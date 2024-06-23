#include "komoring_heights.hpp"

#include "../../usi.h"
#include "local_expansion.hpp"
#include "mate_len.hpp"
#include "node.hpp"
#include "score.hpp"
#include "search_result.hpp"
#include "typedefs.hpp"

namespace komori {
namespace {
/// GC で削除するエントリの割合
constexpr double kGcRemovalRatio = 0.5;
static_assert(kGcRemovalRatio > 0 && kGcRemovalRatio < 1.0, "kGcRemovalRatio must be greater than 0 and less than 1");

// 反復深化のしきい値を適当に伸ばす
std::pair<PnDn, PnDn> NextPnDnThresholds(PnDn pn, PnDn dn, PnDn curr_thpn, PnDn curr_thdn) {
  const auto thpn = static_cast<PnDn>(static_cast<double>(pn) * (1.7 + 0.3 * tl_thread_id)) + 1;
  const auto thdn = static_cast<PnDn>(static_cast<double>(dn) * (1.7 + 0.3 * tl_thread_id)) + 1;

  return std::make_pair(ClampPnDn(curr_thpn, thpn, kInfinitePnDn), ClampPnDn(curr_thdn, thdn, kInfinitePnDn));
}

/**
 * @brief AND node `n` において、詰みを逃れる手を1つ任意に選んで返す
 * @param tt  置換表
 * @param n   現局面（AND node）
 * @return `n` において詰みを逃れる手（あれば）
 */
std::optional<Move> GetEvasion(tt::TranspositionTable& tt, Node& n) {
  for (const auto move : MovePicker{n}) {
    const auto query = tt.BuildChildQuery(n, move);
    bool does_have_old_child = false;
    const auto result =
        query.LookUp(does_have_old_child, kDepthMaxMateLen, [&n, &move = move]() { return InitialPnDn(n, move.move); });
    if (result.Dn() == 0) {
      return {move};
    }
  }

  return std::nullopt;
}
}  // namespace

void KomoringHeights::Init(const EngineOption& option, std::uint32_t num_threads) {
  option_ = option;
  barrier_.Initialize(option_.threads);
  tt_.Resize(option_.hash_mb);
  expansion_list_.resize(num_threads);
  expansion_list_.shrink_to_fit();

#if defined(USE_TT_SAVE_AND_LOAD)
  const auto& tt_read_path = option_.tt_read_path;
  if (!tt_read_path.empty()) {
    std::ifstream ifs(tt_read_path, std::ios::binary);
    if (ifs) {
      sync_cout << "info string load_path: " << tt_read_path << sync_endl;
      tt_.Load(ifs);
    }
  }
#endif  // defined(USE_TT_SAVE_AND_LOAD)
}

void KomoringHeights::Clear() {
  tt_.Clear();
}

void KomoringHeights::Stop() {
  monitor_.Stop();
}

void KomoringHeights::NewSearch(const Position& n, bool is_root_or_node) {
  auto& nn = const_cast<Position&>(n);
  const Node node{nn, is_root_or_node};

  tt_.NewSearch();
  monitor_.NewSearch(tt_.Capacity(), option_.pv_interval, option_.nodes_limit);
  best_moves_.clear();
  score_ = Score{};
  pv_list_.NewSearch(node);

  if (tt_.Hashfull() >= kExecuteGcHashfullThreshold) {
    tt_.Clear();
  }

  moves_from_root_.clear();
  mate_len_ = kDepthMaxMateLen;
  should_break_main_loop_.store(false, std::memory_order_relaxed);
  search_results_.resize(option_.threads);
  search_results_.shrink_to_fit();
}

NodeState KomoringHeights::SearchMainThread(const Position& n, bool is_root_or_node) {
  auto& nn = const_cast<Position&>(n);
  Node node{nn, is_root_or_node};

  // ひとまず開始局面を探索する
  barrier_.Await();  // await-a
  SearchResult result = SearchEntry(node, kDepthMaxMateLen);
  monitor_.Stop();
  barrier_.Await();      // await-b
  monitor_.ResetStop();  // stopフラグが立っているのでリセット

  // 全スレッドの探索結果をマージする
  const SearchResultComparer comparer{node.IsOrNode()};
  for (int i = 1; i < option_.threads; ++i) {
    if (search_results_[i].IsFinal() && comparer(search_results_[i], result) == SearchResultComparer::Ordering::kLess) {
      result = search_results_[i];
    }
  }

  sync_cout << CurrentInfo() << result << sync_endl;
  if (result.Pn() == 0) {
    // pv作成
    MovePath path{};

    result = ConstructPv(node, result.Len(), path);
    score_ = Score::Make(option_.score_method, result, node.IsRootOrNode());

    UsiInfo info = CurrentInfo();
    info.PushPVBack(0, score_.ToString(), path.ToString());
    sync_cout << info << sync_endl;

    best_moves_ = path.Moves();
  }

  // 待機している sub thread を解放する
  should_break_main_loop_ = true;
  barrier_.Await();  // await-a

  return result.GetNodeState();
}

NodeState KomoringHeights::SearchSubThread(const Position& n, bool is_root_or_node) {
  auto& nn = const_cast<Position&>(n);
  Node node{nn, is_root_or_node};

  barrier_.Await();  // await-a
  while (!should_break_main_loop_) {
    // 探索本体
    // await-a ~ await-b の区間は moves_from_root_ と mate_len_ は読み取り専用なので、排他を取る必要はない
    RollForward(node, moves_from_root_);
    const SearchResult result = SearchEntry(node, mate_len_);
    search_results_[tl_thread_id] = result;

    RollBack(node, moves_from_root_);
    monitor_.Stop();
    barrier_.Await();  // await-b
    barrier_.Await();  // await-a
  }

  return NodeState::kUnknown;
}

SearchResult KomoringHeights::SearchEntry(Node& n, MateLen len) {
  expansion_list_[tl_thread_id].Emplace(tt_, n, len, true, option_.multi_pv, len != kDepthMaxMateLen);

  PnDn thpn = (tl_thread_id + 1) * kPnDnUnit;
  PnDn thdn = (tl_thread_id + 1) * kPnDnUnit;
  SearchResult result;
  do {
    std::uint32_t inc_flag = 0;
    result = SearchImpl(n, thpn, thdn, kDepthMaxMateLen, inc_flag);
    if (result.IsFinal()) {
      break;
    }

    if (tl_thread_id == 0 && !score_.IsFinal()) {
      score_ = Score::Make(option_.score_method, result, n.IsRootOrNode());
    }

    std::tie(thpn, thdn) = NextPnDnThresholds(result.Pn(), result.Dn(), thpn, thdn);
  } while (!monitor_.ShouldStop() && thpn <= kInfinitePnDn && thdn <= kInfinitePnDn);

  expansion_list_[tl_thread_id].Pop();
  return result;
}

SearchResult KomoringHeights::ConstructPv(Node& n, MateLen max_len, MovePath& move_path) {
  // 証明駒を使わずにできるだけ短い詰みを LookUp してほしいので、strict_lookup=true にしている
  expansion_list_[tl_thread_id].Emplace(tt_, n, max_len, false, 1, true);
  LocalExpansion& local_expansion = expansion_list_[tl_thread_id].back();
  // early exit するときに忘れずに local_expansion を開放するための RAII
  Defer release_expansion([this]() { expansion_list_[tl_thread_id].Pop(); });

  SearchResult result = local_expansion.CurrentResult(n);
  while (!result.IsFinal() && !monitor_.ShouldStop()) {
    // mate_len 以下の詰みがあるはずなので頑張って探す
    // sub thread たちにも `moves_from_root_` 以下 `mate_len_` 手詰めを見つけるのを手伝ってもらう
    moves_from_root_ = move_path.Moves();
    mate_len_ = max_len;
    barrier_.Await();  // await-a

    // expansion 済なので、SearchEntryではなく SearchImpl を呼ぶ
    std::uint32_t inc_flag = 0;
    result = SearchImpl(n, kInfinitePnDn, kInfinitePnDn, max_len, inc_flag);
    monitor_.Stop();
    barrier_.Await();  // await-b
    monitor_.ResetStop();

    // sub thread の結果を result にコピーすることもできるが、メインスレッドの local expansion の状態が
    // 狂ってしまうので、あえて何もしない
  }

  if (result.Dn() == 0) {
    // mate_len 以下の詰みがあるはずなので、ここに到達するのはおかしい
    n.UndoMoveNoRepetition();
    sync_cout << n.GetDepth() << " " << n.Pos() << sync_endl;
    sync_cout << "info string unexpected disproven: " << result << sync_endl;
    std::terminate();
  }

  if (result.Len() <= kZeroMateLen) {
    // 現局面で詰みだった
    return result;
  }

  if (n.IsOrNode()) {
    if (const auto [best_move, proof_hand] = CheckMate1Ply(n.Pos()); proof_hand != kNullHand) {
      move_path.AddMove(best_move, n.GetDepth());
      return SearchResult::MakeFinal<true>(proof_hand, MateLen{1}, 1);
    }
  }

  SearchResult child_result;
  do {
    const Move best_move = local_expansion.BestMove();
    move_path.AddMove(best_move, n.GetDepth());

    // DoMove() をするとループに遭遇したときに回避できないので、千日手判定なし版を使う
    n.DoMoveNoRepetition(best_move);
    child_result = ConstructPv(n, result.Len() - 1, move_path);
    n.UndoMoveNoRepetition();

    local_expansion.UpdateBestChild(child_result);
    result = local_expansion.CurrentResult(n);

    // 子局面で見つけた手数（mate_path の depth+1 以降に書かれた手数）が現局面の詰み手数と一致しているか確認する
    // もし差異があったら、現局面の詰み手数が間違っていた可能性があるのでもう一度探索する
  } while (result.Len() != child_result.Len() + 1 && !monitor_.ShouldStop());

  return result;
}

SearchResult KomoringHeights::SearchImpl(Node& n, PnDn thpn, PnDn thdn, MateLen len, std::uint32_t& inc_flag) {
  const PnDn orig_thpn = thpn;
  const PnDn orig_thdn = thdn;
  const std::uint32_t orig_inc_flag = inc_flag;

  auto& local_expansion = expansion_list_[tl_thread_id].back();
  monitor_.Visit(n.GetDepth());
  if (tl_thread_id == 0 && monitor_.ShouldPrint()) {
    Print(n);
  }

  if (n.GetDepth() >= kDepthMax) {
    return SearchResult::MakeRepetition(n.OrHand(), len, 1, 0);
  }

  // 必要があれば TCA による探索延長をしたいので、このタイミングで現局面の pn/dn を取得する。
  auto curr_result = local_expansion.CurrentResult(n);
  // Threshold Controlling Algorithm(TCA).
  // 浅い結果を参照している場合、無限ループになる可能性があるので少しだけ探索を延長する
  if (local_expansion.DoesHaveOldChild()) {
    inc_flag++;
  }

  if (inc_flag > 0) {
    ExtendSearchThreshold(curr_result, thpn, thdn);
  }

  if (tl_gc_thread && monitor_.ShouldCheckHashfull()) {
    if (tt_.Hashfull() >= kExecuteGcHashfullThreshold) {
      tt_.CollectGarbage(kGcRemovalRatio);
    }
    monitor_.ResetNextHashfullCheck();
  }

  while (!monitor_.ShouldStop() && (curr_result.Pn() < thpn && curr_result.Dn() < thdn)) {
    // local_expansion.BestMove() にしたがい子局面を展開する
    // （curr_result.Pn() > 0 && curr_result.Dn() > 0 なので、BestMove が必ず存在する）
    const auto best_move = local_expansion.BestMove();
    const bool is_first_search = local_expansion.FrontIsFirstVisit();
    const auto [child_thpn, child_thdn] = local_expansion.FrontPnDnThresholds(thpn, thdn);

    n.DoMove(best_move);

    // 子局面を展開する。展開した expansion は UndoMove() の直前に忘れずに開放しなければならない。
    expansion_list_[tl_thread_id].Emplace(tt_, n, len - 1, is_first_search);
    auto& child_expansion = expansion_list_[tl_thread_id].back();

    SearchResult child_result;
    if (is_first_search) {
      child_result = child_expansion.CurrentResult(n);
      // 新規局面を展開したので、inc_flag を 1 つ減らしておく
      // オリジナルの TCA では inc_flag は bool 値なので単に false を代入していたが、KomoringHeights では
      // 非負整数へ拡張している。
      if (inc_flag > 0) {
        inc_flag--;
      }

      // 子局面を初展開する場合、child_result を計算した時点で threshold を超過する可能性がある
      // しかし、SearchImpl をコールしてしまうと TCA の探索延長によりすぐに返ってこない可能性がある
      // ゆえに、この時点で Exceed している場合は SearchImpl を呼ばないようにする。
      if (child_result.Pn() >= child_thpn || child_result.Dn() >= child_thdn) {
        goto CHILD_SEARCH_END;
      }
    }
    child_result = SearchImpl(n, child_thpn, child_thdn, len - 1, inc_flag);

  CHILD_SEARCH_END:
    expansion_list_[tl_thread_id].Pop();
    n.UndoMove();

    local_expansion.UpdateBestChild(child_result);
    curr_result = local_expansion.CurrentResult(n);

    // TCA で延長したしきい値はいったん戻す
    thpn = orig_thpn;
    thdn = orig_thdn;
    if (inc_flag > 0) {
      // TCA 継続中ならしきい値を伸ばす
      ExtendSearchThreshold(curr_result, thpn, thdn);
    } else if (inc_flag == 0 && orig_inc_flag > 0) {
      // TCA の展開が終わったので、いったん親局面に戻る
      break;
    }
  }

  /// `inc_flag` の値は探索前より小さくなっているはず
  inc_flag = std::min(inc_flag, orig_inc_flag);
  return curr_result;
}

UsiInfo KomoringHeights::CurrentInfo() const {
  UsiInfo usi_output = monitor_.GetInfo();
  usi_output.Set(UsiInfoKey::kHashfull, tt_.Hashfull());
  usi_output.Set(UsiInfoKey::kScore, score_.ToString());

  return usi_output;
}

void KomoringHeights::Print(const Node& n) {
  if (option_.silent) {
    return;
  }

  UsiInfo info = CurrentInfo();
  info.PushPVBack(n.GetDepth(), score_.ToString(), ToString(n.MovesFromStart()));
  sync_cout << info << sync_endl;
}
}  // namespace komori
