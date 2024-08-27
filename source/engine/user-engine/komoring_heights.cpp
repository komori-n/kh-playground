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

Move SelectNextBestmove(const LocalExpansion& local_expansion, const std::vector<Move>& searched_moves) {
  for (const auto& [move, result] : local_expansion.GetAllResults()) {
    if (std::find(searched_moves.begin(), searched_moves.end(), move) == searched_moves.end()) {
      return move;
    }
  }

  return MOVE_NONE;
}
}  // namespace

void KomoringHeights::Init(const EngineOption& option, std::uint32_t num_threads) {
  option_ = option;
  tt_.Resize(option_.hash_mb);
  expansion_list_.resize(num_threads);
  expansion_list_.shrink_to_fit();
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
  in_pv_search_ = false;
  pv_moves_.Clear();

  if (tt_.Hashfull() >= kExecuteGcHashfullThreshold) {
    tt_.Clear();
  }

  worker_pool_.Reset();
  barrier_.Initialize(option_.threads);
}

NodeState KomoringHeights::SearchMainThread(const Position& n, bool is_root_or_node) {
  auto& nn = const_cast<Position&>(n);
  Node node{nn, is_root_or_node};

  NodeState node_state = NodeState::kUnknown;
  SearchResult best_child_result;

  std::vector<Move> searched_moves{};
  std::size_t num_found_win_moves = 0;
  const std::size_t num_legal_moves = MovePicker{node}.size();
  const std::size_t max_num_win_moves = std::min<std::size_t>(num_legal_moves, option_.multi_pv);
  while (num_found_win_moves < max_num_win_moves && searched_moves.size() < num_legal_moves && !monitor_.ShouldStop()) {
    const std::uint32_t multi_pv = num_found_win_moves + 1;
    in_pv_search_ = false;
    expansion_list_[tl_thread_id].Emplace(tt_, node, MateLen::DepthMax(), true, multi_pv);
    LocalExpansion& local_expansion = expansion_list_[tl_thread_id].back();

    SearchResult result;
    while (!monitor_.ShouldStop()) {
      DispatchSearch(node, MateLen::DepthMax(), multi_pv);
      result = local_expansion.CurrentResult(node);

      if (result.IsFinal()) {
        break;
      }
      local_expansion.Relookup(tt_, node, MateLen::DepthMax(), false, multi_pv, false);
    }

    in_pv_search_ = true;
    for (auto [move, child_result] : local_expansion.GetAllResults()) {
      if (!child_result.IsFinal() ||
          std::find(searched_moves.begin(), searched_moves.end(), move) != searched_moves.end()) {
        continue;
      }
      searched_moves.push_back(move);

      if (searched_moves.size() == 1) {
        score_ = score_maker_.Make(child_result, node.IsRootOrNode());
        score_.AddOneIfFinal();
        node_state = child_result.GetNodeState();
      }
      sync_cout << CurrentInfo() << move << " " << child_result << sync_endl;

      node.DoMoveNoRepetition(move);
      pv_moves_.AddMove(move, 0);
      if (child_result.Pn() == 0) {
        ++num_found_win_moves;
        child_result = ConstructProvenPv(node, child_result.Len());
      } else if (child_result.Dn() == 0) {
        if (!node.IsOrNode()) {
          const Move evasion = GetEvasion(node);
          pv_moves_.AddMove(evasion, 1);
        }
      }

      if (child_result.IsFinal()) {
        local_expansion.UpdateFinal(child_result, move);
      }
      pv_list_.Update(move, child_result, 0, pv_moves_.Moves());
      if (searched_moves.size() == 1 || SearchResultComparer{node.IsRootOrNode()}(child_result, best_child_result) ==
                                            SearchResultComparer::Ordering::kLess) {
        best_child_result = child_result;

        best_moves_ = pv_moves_.Moves();
        score_ = score_maker_.Make(child_result, node.IsRootOrNode());
        score_.AddOneIfFinal();
      }
      Print(node);

      node.UndoMoveNoRepetition();
    }

    expansion_list_[tl_thread_id].Pop();
  }

  // 待機している sub thread を解放する
  worker_pool_.Stop();

  if (num_legal_moves == 0) {
    node_state = node.IsOrNode() ? NodeState::kDisproven : NodeState::kProven;
  }

  return node_state;
}

void KomoringHeights::SearchSubThread(const Position& n, bool is_root_or_node) {
  auto& nn = const_cast<Position&>(n);
  Node node{nn, is_root_or_node};

  // メインスレッドの言いなりになって働く
  worker_pool_.Work(node);
}

void KomoringHeights::DispatchSearch(Node& n, MateLen len, std::uint32_t multi_pv) {
  const auto& moves = n.MovesFromStart();
  const std::vector<Move> moves_from_root(moves.begin(), moves.end());

  for (int i = 1; i < option_.threads; ++i) {
    worker_pool_.AddTask([this, len, multi_pv, &moves_from_root, i](Node& n) {
      RollForward(n, moves_from_root);
      SearchEntry(n, len, multi_pv);
      monitor_.Stop();

      RollBack(n, moves_from_root);
      barrier_.Await();
    });
  }

  sync_cout << "info string start searching" << sync_endl;
  SearchEntryNoEmplace(n, len);
  monitor_.Stop();
  sync_cout << "info string await start" << sync_endl;
  barrier_.Await();
  sync_cout << "info string await end" << sync_endl;
  monitor_.ResetStop();
}

SearchResult KomoringHeights::SearchEntry(Node& n, MateLen len, std::uint32_t multi_pv) {
  expansion_list_[tl_thread_id].Emplace(tt_, n, len, true, multi_pv, len != MateLen::DepthMax());
  Defer release_expansion([this]() { expansion_list_[tl_thread_id].Pop(); });
  return SearchEntryNoEmplace(n, len);
}

SearchResult KomoringHeights::SearchEntryNoEmplace(Node& n, MateLen max_len) {
  PnDn thpn = (tl_thread_id + 1) * kPnDnUnit;
  PnDn thdn = (tl_thread_id + 1) * kPnDnUnit;
  SearchResult result;
  do {
    std::uint32_t inc_flag = 0;
    result = SearchImpl(n, thpn, thdn, max_len, inc_flag);
    if (result.IsFinal()) {
      break;
    }

    if (tl_thread_id == 0 && !score_.IsFinal()) {
      score_ = score_maker_.Make(result, n.IsRootOrNode());
    }

    std::tie(thpn, thdn) = NextPnDnThresholds(result.Pn(), result.Dn(), thpn, thdn);
  } while (!monitor_.ShouldStop() && thpn <= kInfinitePnDn && thdn <= kInfinitePnDn);
  return result;
}

SearchResult KomoringHeights::ConstructProvenPv(Node& n, MateLen max_len) {
  // 証明駒を使わずにできるだけ短い詰みを LookUp してほしいので、strict_lookup=true にしている
  const std::uint32_t multi_pv = 1;
  expansion_list_[tl_thread_id].Emplace(tt_, n, max_len, false, multi_pv, true);
  LocalExpansion& local_expansion = expansion_list_[tl_thread_id].back();
  // early exit するときに忘れずに local_expansion を開放するための RAII
  Defer release_expansion([this]() { expansion_list_[tl_thread_id].Pop(); });

  SearchResult result = local_expansion.CurrentResult(n);
  std::uint32_t loop_count = 0;
  while (!result.IsFinal() && !monitor_.ShouldStop()) {
    // mate_len 以下の詰みがあるはずなので頑張って探す
    // 前の週で別スレッドが詰みを見つけているかもしれないので、改めて展開しなおす
    local_expansion.Relookup(tt_, n, max_len, false, multi_pv, true);

    // sub thread たちにも `n` 以下 `mate_len_` 手詰めを見つけるのを手伝ってもらう
    const auto r = SearchEntryNoEmplace(n, max_len);
    const MateLen max_mate_len = r.Pn() == 0 ? r.Len() : max_len + 2;

    // sub thread の結果を result にコピーすることもできるが、メインスレッドの local expansion の状態が
    // 狂ってしまうので、あえて何もしない
    result = local_expansion.CurrentResult(n);

    if (++loop_count % 30 == 0) {
      // 無駄合は確率で消える可能性があるので、max_len で詰むはずでも詰みを見つけれられないことがある。
      // そんなときは、詰み手数を伸ばして親局面から探索をやり直す

      max_len = max_mate_len;
    }
  }

  if (result.Dn() == 0) {
    // mate_len 以下の詰みがあるはずなので、ここに到達するのはおかしい
    sync_cout << n.GetDepth() << " " << n.Pos() << sync_endl;
    sync_cout << "info string unexpected disproven: " << result << sync_endl;
    std::terminate();
  }

  if (result.Len() <= MateLen::Zero()) {
    // 現局面で詰みだった
    return result;
  }

  if (n.IsOrNode() && n.GetDepth() > 0) {
    if (const auto [best_move, proof_hand] = CheckMate1Ply(n.Pos()); proof_hand != kNullHand) {
      pv_moves_.AddMove(best_move, n.GetDepth());
      return SearchResult::MakeFinal<true>(proof_hand, MateLen{1}, 1);
    }
  }

  SearchResult child_result;
  do {
    const Move best_move = local_expansion.BestMove();
    pv_moves_.AddMove(best_move, n.GetDepth());

    // DoMove() をするとループに遭遇したときに回避できないので、千日手判定なし版を使う
    n.DoMoveNoRepetition(best_move);
    child_result = ConstructProvenPv(n, result.Len() - 1);
    n.UndoMoveNoRepetition();

    local_expansion.UpdateBestChild(child_result);
    result = local_expansion.CurrentResult(n);

    // 子局面で見つけた手数（mate_path の depth+1 以降に書かれた手数）が現局面の詰み手数と一致しているか確認する
    // もし差異があったら、現局面の詰み手数が間違っていた可能性があるのでもう一度探索する
  } while ((result.Len() != child_result.Len() + 1) && !monitor_.ShouldStop());

  return result;
}

Move KomoringHeights::GetEvasion(Node& n) {
  expansion_list_[tl_thread_id].Emplace(tt_, n, MateLen::DepthMax(), false, 1, true);
  LocalExpansion& local_expansion = expansion_list_[tl_thread_id].back();
  while (!monitor_.ShouldStop()) {
    SearchEntryNoEmplace(n, MateLen::DepthMax());
    if (local_expansion.CurrentResult(n).IsFinal()) {
      break;
    }

    local_expansion.Relookup(tt_, n, MateLen::DepthMax(), false, 1, true);
  }

  const Move evasion = local_expansion.BestMove();
  expansion_list_[tl_thread_id].Pop();
  return evasion;
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

  if (n.GetDepth() >= kDepthMax - 1) {
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
    expansion_list_[tl_thread_id].Emplace(tt_, n, len - 1, is_first_search, 1);
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
  if (!expansion_list_[0].empty()) {
    const LocalExpansion& root = expansion_list_[0].front();
    const SearchResult result = root.FrontResult();
    if (!in_pv_search_) {
      const auto& pv = n.MovesFromStart();
      std::vector<Move> pv_moves(pv.begin(), pv.end());
      pv_list_.Update(root.BestMove(), result, n.GetDepth(), std::move(pv_moves));
    }

    info.Set(UsiInfoKey::kCurrMove, USI::move(root.BestMove()));
    for (const auto& [i, mr] : WithIndex(root.GetAllResults())) {
      const auto& [move, result] = mr;
      if (i >= option_.multi_pv) {
        break;
      }
      const PvList::PvInfo pv_info = pv_list_.GetPvInfo(move);
      Score score = score_maker_.Make(pv_info.result, n.IsRootOrNode());
      score.AddOneIfFinal();
      info.PushPVBack(pv_info.depth, score.ToString(), ToString(pv_info.pv));
    }
  }

  sync_cout << info << sync_endl;
}
}  // namespace komori
