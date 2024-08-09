/**
 * @file local_expansion.hpp
 */
#ifndef KOMORI_LOCAL_EXPANSION_HPP_
#define KOMORI_LOCAL_EXPANSION_HPP_

#include <algorithm>
#include <optional>
#include <utility>

#include "hands.hpp"
#include "initial_estimation.hpp"
#include "inline_stack.hpp"
#include "komori/saturation_arithmetic.hpp"
#include "lazy_expansion_table.hpp"
#include "move_picker.hpp"
#include "node.hpp"
#include "ranges.hpp"
#include "splitted_hand.hpp"
#include "transposition_table.hpp"
#include "ttquery.hpp"
#include "typedefs.hpp"

namespace komori {
namespace detail {
/**
 * @brief OR node `pos` を `move` した局面が自明な詰み／不詰かどうかを判定する。
 * @param pos   現局面
 * @param move  次の手
 * @return `pos` を `move` で進めた局面が自明な詰みまたは不詰ならその結果を返す。それ以外なら `std::nullopt` を返す。
 *
 * 末端局面における固定深さ探索。詰め探索で必須ではないが、これによって高速化することができる。
 *
 * 高速 1 手詰めルーチンおよび高速 0 手不詰ルーチンにより自明な詰み／不詰を展開することなく検知することができる。
 */
inline std::optional<SearchResult> CheckObviousFinalAfter(const Node& n, Move move) {
  Node& nn = const_cast<Node&>(n);
  // ここの1手詰判定が意外と重たいので、少し泥臭く高速化する
  nn.DoMoveNoRepetition(move);
  Defer undo_defer{[&nn] { nn.UndoMoveNoRepetition(); }};

  if (!DoesHaveMatePossibility(nn.Pos())) {
    const Hand curr_hand = static_cast<Hand>(HAND_BIT_MASK);
    const Hand hand = RemoveIfHandGivesOtherChecks(nn.Pos(), curr_hand);

    return SearchResult::MakeFinal<false>(hand, MateLen::DepthMax(), 1);
  } else if (auto [best_move, proof_hand] = CheckMate1Ply(nn.Pos()); proof_hand != kNullHand) {
    return SearchResult::MakeFinal<true>(proof_hand, MateLen{1}, 1);
  }

  return std::nullopt;
}
}  // namespace detail

/**
 * @brief 局面の局所展開結果を保持する。
 *
 * 現局面の子ノードの展開結果を一時的に保持するクラス。前回の置換表 LookUp 結果を保持しておくことで、
 * 置換表 LookUp 回数をへらすことが目的である。
 *
 * ## 実装詳細
 *
 * ### 生添字（i_raw）
 *
 * 同じ局面 `n` に対し、`MovePicker` は常に同じ順番の指し手を生成する。つまり、`MovePicker` の添字により指し手を
 * 一意に特定できる。このように、任意の指し手 `m` に対し、`MovePicker` における添字を `m` の生添字（raw index）と呼び、
 * 変数 `i_raw` により表現する。
 *
 * ### 添字スタック（idx_）
 *
 * 探索中に「良さげ順」に生添字の並び替えを行いたい。このような生添字のリストが添え字スタック `idx`である。
 * 探索結果の配列 `results_` を直接並び替えるのではなく `idx_` を並び替えることで、並び替えにかかる命令数を
 * 削減することができる。また、添字スタックを経由してアクセスすることで、`mp_`
 * 「良さげ順」でアクセスすることができる。
 *
 * スタック構造を活かして探索中に `idx_` へ生添字を追加することもできる。これは、
 * 指し手の遅延展開（`lazy_expansion_`）に用いられる。
 *
 * ### MultiPV
 *
 * multi_pv > 1 のとき、勝ちになる手が見つかった後も multi_pv 個の勝ちが見つかるまでは探索を続ける。ここで、
 * 勝ちになる手とは OR node では詰む手、AND node では不詰になる手のことである。
 *
 * 勝ちになる手は、以降の探索から除外される。除外されている手の個数は `excluded_moves_` で管理されている。
 * `BestMove()` や `FrontResult()` で現時点の最善手を取得するとき、除外された手は最善手に含まれないので注意すること。
 */
class LocalExpansion {
 private:
  /**
   * @brief  `idx_` の比較器を生成する。
   * @return 関数オブジェクト（比較器）
   * @note ラムダ式を返すために、戻り値を auto にしてクラス先頭で定義している。
   */
  auto MakeComparer() const {
    const SearchResultComparer sr_comparer{or_node_};
    return [this, sr_comparer](std::size_t i_raw, std::size_t j_raw) -> bool {
      // `SearchResultComparer` で大小比較の決着がつくならそれに従う。
      // `SearchResultComparer` で結論がでなければ、指し手自体の評価値（指し手生成時に付与）で大小を決める。
      const auto& left_result = results_[i_raw];
      const auto& right_result = results_[j_raw];
      const auto ordering = sr_comparer(left_result, right_result);
      if (ordering == SearchResultComparer::Ordering::kLess) {
        return true;
      } else if (ordering == SearchResultComparer::Ordering::kGreater) {
        return false;
      }

      return mp_[i_raw].value < mp_[j_raw].value;
    };
  }

 public:
  /**
   * @brief LocalExpansion を構築する。
   * @param tt  置換表
   * @param n   現局面
   * @param len 残り詰み手数
   * @param first_search 初回探索なら `true`。`true` なら高速 1 手詰めルーチンを走らせる。
   * @param multi_pv 勝ちになる手をいくつ見つけるか。1以上でなければならない
   * @param strict_lookup できるだけ短い詰み手順がほしいなら true。無駄合のような重い処理が有効になる
   */
  LocalExpansion(tt::TranspositionTable& tt,
                 const Node& n,
                 MateLen len,
                 bool first_search,
                 std::uint32_t multi_pv = 1,
                 bool strict_lookup = false)
      : or_node_{n.IsOrNode()}, mp_{n, true} {
    for (const auto& [i_raw, move] : WithIndex<std::uint32_t>(mp_)) {
      queries_[i_raw] = tt.BuildChildQuery(n, move.move);
    }

    Relookup(tt, n, len, first_search, multi_pv, strict_lookup);
  }

  /// Copy constructor(delete)
  LocalExpansion(const LocalExpansion&) = delete;
  /// Move constructor(delete)
  LocalExpansion(LocalExpansion&&) = delete;
  /// Copy assign operator(delete)
  LocalExpansion& operator=(const LocalExpansion&) = delete;
  /// Move assign operator(delete)
  LocalExpansion& operator=(LocalExpansion&&) = delete;
  /// Destructor(default)
  ~LocalExpansion() = default;

  /**
   * @brief LocalExpansion を構築しなおす
   * @param tt  置換表
   * @param n   現局面
   * @param len 残り詰み手数
   * @param first_search 初回探索なら `true`。`true` なら高速 1 手詰めルーチンを走らせる。
   * @param multi_pv 勝ちになる手をいくつ見つけるか。1以上でなければならない
   * @param strict_lookup できるだけ短い詰み手順がほしいなら true。無駄合のような重い処理が有効になる
   */
  void Relookup(tt::TranspositionTable& tt,
                const Node& n,
                MateLen len,
                bool first_search,
                std::uint32_t multi_pv,
                bool strict_lookup) {
    len_ = len;
    multi_pv_ = multi_pv;
    lazy_expansion_ = LazyExpansionTable{n, mp_};
    does_have_old_child_ = false;
    excluded_moves_ = 0;
    idx_.clear();
    for (const auto [i_raw, move] : WithIndex<std::uint32_t>(mp_)) {
      const tt::Query& query = queries_[i_raw];
      auto& result = results_[i_raw];

      if (const auto maybe_depth = n.IsRepetitionOrInferiorAfter(move.move)) {
        result = SearchResult::MakeRepetition(n.OrHandAfter(move.move), len_ - 1, 1, *maybe_depth);
      } else {
        const MateLen lookup_len = len_ < MateLen{1} ? MateLen::DepthMax() : len_ - 1;
        result = query.LookUp(does_have_old_child_, lookup_len, MakeInitialEvaluationFunc(n, move), strict_lookup);

        if (!result.IsFinal() && lazy_expansion_.HasPrev(i_raw)) {
          // prev がいる non-final な手は、prev が final になるまで探索を後回しにする
          continue;
        }
      }

      idx_.Push(i_raw);
      if (!or_node_ && is_drop(move.move) && strict_lookup && IsRedundantDrop(tt, n, i_raw)) {
        result = SearchResult::MakeFinal<true>(n.OrHand(), len_ - 1, result.Amount());
        idx_.Pop();
        redundant_drop_idx_.Push(i_raw);
        goto FOUND_FINAL;
      }

      if (!result.IsFinal()) {
        if (!or_node_ && first_search && result.GetUnknownData().is_first_visit) {
          if (const auto maybe_res = detail::CheckObviousFinalAfter(n, move.move)) {
            query.SetResult(*maybe_res);
            result = *maybe_res;
            goto FOUND_FINAL;
          }
        }
      } else {
      FOUND_FINAL:
        lazy_expansion_.Remove(i_raw);
        if (result.Phi(or_node_) == 0) {
          if (excluded_moves_ >= multi_pv_ - 1) {
            if (strict_lookup) {
              continue;
            } else {
              break;
            }
          }

          excluded_moves_++;
        }
      }
    }

    std::sort(idx_.begin(), idx_.end(), MakeComparer());
    RecalcDelta();
  }

  /**
   * @brief 合法手がないかどうか
   * @see CurrentResult
   */
  bool empty() const noexcept { return idx_.empty(); }
  /**
   * @brief 現時点の最善手
   * @pre !Current().IsFinal()
   */
  Move BestMove() const { return mp_[idx_[excluded_moves_]].move; }
  /// 現局面の合法手の数（無駄合除く）
  std::size_t Size() const { return idx_.size(); }
  /**
   * @brief 最善の子の探索結果を取得する
   * @pre !Current().IsFinal()
   */
  const SearchResult& FrontResult() const { return results_[idx_[excluded_moves_]]; }
  /// `move` に対応する探索結果を取得する
  const SearchResult& ResultFor(Move move) const {
    return results_[std::find(mp_.begin(), mp_.end(), move) - mp_.begin()];
  }
  /**
   * @brief unproven old child がいるかどうか
   */
  bool DoesHaveOldChild() const { return does_have_old_child_; }
  /**
   * @brief 最善手の子ノードが初探索かどうか
   * @pre !CurrentResult().IsFinal()
   */
  bool FrontIsFirstVisit() const { return FrontResult().GetUnknownData().is_first_visit; }

  /**
   * @brief (Move, SearchResult) のペアを良さげ順にすべて取得する
   */
  auto GetAllResults() const {
    return Zip(Apply(idx_, [this](const std::size_t i_raw) { return mp_[i_raw].move; }),
               Apply(idx_, [this](const std::size_t i_raw) { return results_[i_raw]; }));
  }

  /**
   * @brief 現局面における探索結果を返す
   * @param n 現局面
   * @pre `n` がコンストラクト時に渡されたものと同じ局面
   * @note `!CurrentResult(n).IsFinal()` ならば `!empty()` が成り立つ。これを利用して `!empty()` のチェックを
   * スキップすることがある。
   */
  SearchResult CurrentResult(const Node& n) const {
    if (GetPhi() == 0) {
      return GetWinResult(n);
    } else if (GetDelta() == 0) {
      return GetLoseResult(n);
    } else {
      return GetUnknownResult(n);
    }
  }

  /**
   * @brief 最善手の子の評価値を更新する
   * @param search_result 最善手の子の評価値
   * @pre !empty()
   */
  void UpdateBestChild(const SearchResult& search_result) {
    const auto old_i_raw = idx_[excluded_moves_];
    const auto& query = queries_[old_i_raw];
    auto& result = results_[old_i_raw];
    const PnDn orig_delta = result.Delta(or_node_);
    const bool orig_was_final = result.IsFinal();
    result = search_result;
    query.SetResult(search_result);

    if (!orig_was_final && result.IsFinal()) {
      valid_child_num_--;
    }

    if (!orig_was_final && search_result.Phi(or_node_) == 0) {
      // 後から見つかった手のほうがいい手かもしれないので、前半部分をソートし直しておく
      ResortExcludedBack();
      if (search_result.Pn() == 0 && search_result.Len() > len_) {
        multi_pv_++;
      }
      if (excluded_moves_ >= multi_pv_ - 1) {
        // multi_pv_ 個の勝ちになる手を見つけたので、これ以上探索を続ける必要はない
        return;
      }
      excluded_moves_++;
      if (excluded_moves_ >= idx_.size()) {
        // 全合法手が勝ちだとわかったので、これ以上探索を続ける必要はない
        return;
      }
    }

    bool needs_recalc_delta = (orig_delta == delta_max_);
    if (!orig_was_final && search_result.IsFinal()) {
      ResortFront();

      std::uint32_t j_raw = old_i_raw;
      if (std::optional<std::uint32_t> maybe_next = lazy_expansion_.Next(j_raw)) {
        const std::uint32_t next_j_raw = *maybe_next;
        valid_child_num_++;
        idx_.Push(next_j_raw);

        const PnDn new_delta = results_[next_j_raw].Delta(or_node_);
        if (delta_max_ <= new_delta) {
          delta_max_ = new_delta;
          needs_recalc_delta = false;
        }
        ResortBack();
      }
    } else {
      const PnDn new_delta = search_result.Delta(or_node_);
      if (new_delta != kInfinitePnDn && delta_max_ <= new_delta) {
        delta_max_ = new_delta;
        needs_recalc_delta = false;
      }
      ResortFront();
    }

    if (needs_recalc_delta) {
      RecalcDelta();
    }
  }

  /// Final だとわかっている `move` に対する探索結果を更新する
  void UpdateFinal(const SearchResult& search_result, Move move) {
    const auto old_i_raw = std::find(mp_.begin(), mp_.end(), move) - mp_.begin();
    const auto& query = queries_[old_i_raw];
    auto& result = results_[old_i_raw];
    result = search_result;
    query.SetResult(search_result);
    std::sort(idx_.begin(), idx_.end(), MakeComparer());
  }

  /**
   * @brief 最善手の子の探索で用いる pn と dn のしきい値
   * @pre !empty()
   * @param thpn 現局面の pn のしきい値
   * @param thdn 現局面の dn のしきい値
   * @return 子局面の pn, dn のしきい値のペア
   */
  std::pair<PnDn, PnDn> FrontPnDnThresholds(PnDn thpn, PnDn thdn) const {
    // pn/dn で考えるよりも phi/delta で考えたほうがわかりやすい
    // そのため、いったん phi/delta の世界に変換して、最後にもとに戻す

    const auto thphi = Phi(thpn, thdn, or_node_);
    const auto thdelta = Delta(thpn, thdn, or_node_);
    const auto child_thphi = std::min<PnDn>(thphi, GetSecondPhi() + 1);
    const auto child_thdelta = NewThdeltaForBestMove(thdelta);

    if (or_node_) {
      return {child_thphi, child_thdelta};
    } else {
      return {child_thdelta, child_thphi};
    }
  }

  /// デバッグ用
  void DebugPrint() {
    // print 中は割り込まれたくないので、最初と最後を sync_cout ~ sync_endl で囲む
    sync_cout << "info string LocalExpansion DebugPrint\n";
    std::cout << "info string multi_pv: " << multi_pv_ << "\n";
    std::cout << "info string excluded_moves: " << excluded_moves_ << "\n";
    for (const std::uint32_t i_raw : idx_) {
      const auto& move = mp_[i_raw];
      const auto& result = results_[i_raw];
      std::cout << "info string move: " << move.move << " result: " << result << "\n";
    }
    std::cout << "info string -----------------" << sync_endl;
  }

 private:
  /**
   * @brief Andnode `n` において、mp_[i_raw] が無駄合かどうか判定する
   * @param tt 置換表
   * @param n  現局面
   * @param i_raw 生添字
   * @return 無駄合なら `true`、そうでなければ `false`
   */
  bool IsRedundantDrop(tt::TranspositionTable& tt, const Node& n, std::uint32_t i_raw) {
    const SearchResult& result = results_[i_raw];
    const Move move = mp_[i_raw];
    const Square to = to_sq(move);
    const PieceType pr = move_dropped_piece(move);
    const Key board_key = n.BoardKey();
    const Hand hand = n.OrHand();
    const std::string sfen = n.Pos().sfen();

    if (tt.IsRedundant(board_key, hand, move)) {
      return true;
    }

    Node& nn = const_cast<Node&>(n);
    nn.DoMoveNoRepetition(move);
    Defer undo{[&nn] { nn.UndoMoveNoRepetition(); }};

    for (const ExtMove m2 : MovePicker{nn}) {
      if (to_sq(m2) != to) {
        continue;
      }

      // to に動く指し手で、`pr` がなくても詰む手であれば、`pr` は無駄合である
      const tt::Query& query2 = tt.BuildChildQuery(nn, m2.move);
      if (query2.IsRedundantProven(pr)) {
        tt.InsertRedundant(board_key, hand, move);
        return true;
      }

      // 1手詰のとき、置換表には0手詰として書かれていない場合がある
      // そのため、`m2` を指したときに詰むかどうかを確認する
      if (result.Len() == MateLen{1}) {
        nn.DoMoveNoRepetition(m2);
        Defer undo2{[&nn] { nn.UndoMoveNoRepetition(); }};

        if (nn.Pos().is_mated()) {
          tt.InsertRedundant(board_key, hand, move);
          return true;
        }
      }
    }

    return false;
  }

  // <PnDn>
  /// Pn を計算する
  PnDn GetPn() const {
    if (or_node_) {
      return GetPhi();
    } else {
      return GetDelta();
    }
  }

  /// Dn を計算する
  PnDn GetDn() const {
    if (or_node_) {
      return GetDelta();
    } else {
      return GetPhi();
    }
  }

  /// phi 値を計算する
  PnDn GetPhi() const {
    PnDn front_phi = 0;
    if (excluded_moves_ < idx_.size()) {
      front_phi = FrontResult().Phi(or_node_);
    } else {
      front_phi = kInfinitePnDn;
    }

    if (front_phi >= kInfinitePnDn && excluded_moves_ > 0) {
      return 0;
    }
    return front_phi;
  }

  /// delta 値を計算する
  PnDn GetDelta() const {
    const PnDn phi = GetPhi();
    if (phi == 0) {
      return kInfinitePnDn;
    } else if (phi == kInfinitePnDn) {
      return 0;
    }

    return delta_max_ + kPnDnUnit * (valid_child_num_ - 1);
  }

  /// 2番目の子の phi 値を計算する
  PnDn GetSecondPhi() const {
    if (idx_.size() <= excluded_moves_ + 1) {
      return kInfinitePnDn;
    }
    const auto& second_best_result = results_[idx_[excluded_moves_ + 1]];
    return second_best_result.Phi(or_node_);
  }

  /**
   * @brief 現局面の delta しきい値が `thdelta` のとき、子局面の delta しきい値を計算する
   * @param thdelta 現局面の delta しきい値
   */
  PnDn NewThdeltaForBestMove(PnDn thdelta) const { return sub_sat<PnDn>(thdelta, kPnDnUnit * (valid_child_num_ - 1)); }
  // </PnDn>

  /**
   * @brief
   */
  void RecalcDelta() {
    delta_max_ = 0;
    valid_child_num_ = 0;
    for (const auto i_raw : Skip(idx_, excluded_moves_)) {
      const auto& result = results_[i_raw];
      if (result.IsFinal()) {
        break;
      }

      const auto delta = result.Delta(or_node_);
      delta_max_ = std::max(delta_max_, delta);
      valid_child_num_++;
    }
  }

  /// 探索結果を取得する（手番側から見て勝ち局面）
  SearchResult GetWinResult(const Node& n) const {
    // excluded_moves_ に関係なく最も良い手がほしいので FrontResult() は使えない
    const auto& result = results_[idx_.front()];
    const auto best_move = mp_[idx_[0]];
    const auto mate_len = result.Len() + 1;
    const auto amount = result.Amount() + mp_.size() - 1;
    const auto after_hand = result.GetFinalData().hand;

    if (or_node_) {
      const auto proof_hand = BeforeHand(n.Pos(), best_move, after_hand);
      return SearchResult::MakeFinal<true>(proof_hand, mate_len, amount);
    } else {
      auto disproof_hand = after_hand;

      // 駒打ちならその駒を持っていないといけない
      if (is_drop(best_move)) {
        const auto pr = move_dropped_piece(best_move);
        const auto pr_cnt = hand_count(MergeHand(n.OrHand(), n.AndHand()), pr);
        const auto disproof_pr_cnt = hand_count(disproof_hand, pr);
        if (pr_cnt - disproof_pr_cnt <= 0) {
          // もし現局面の攻め方の持ち駒が disproof_hand だった場合、打とうとしている駒 pr が攻め方に独占されているため
          // 受け方は BestMove() を着手することができない。そのため、攻め方の持ち駒を1枚受け方に渡す必要がある。

          // 現局面で OR node + AND node の pr の持ち駒枚数を N とすると、OR node 側の pr の枚数が N-1 になるように
          // disproof_hand を調整する。このとき、disproof_hand の pr 枚数が N より大きい可能性があるので注意。
          sub_hand(disproof_hand, pr, disproof_pr_cnt - pr_cnt + 1);
        }
      }

      // 千日手のときは MakeRepetition で返す
      if (result.GetFinalData().IsRepetition()) {
        const auto depth = result.GetFinalData().repetition_start;
        if (depth < n.GetDepth()) {
          return SearchResult::MakeRepetition(n.OrHand(), mate_len, amount, depth);
        }
      }
      return SearchResult::MakeFinal<false>(disproof_hand, mate_len, amount);
    }
  }

  /// 探索結果を取得する（手番側から見て負け局面）
  SearchResult GetLoseResult(const Node& n) const {
    if (or_node_) {
      SplittedHand splitted_hand = SplittedHand::Full();
      // 子局面の反証駒の極小集合を計算する
      MateLen mate_len = len_;
      SearchAmount amount = 1;
      for (const auto i_raw : idx_) {
        const auto& result = results_[i_raw];
        const auto child_move = mp_[i_raw];
        const auto child_disproof_hand = BeforeHand(n.Pos(), child_move, result.GetFinalData().hand);

        splitted_hand.MergeByMin(child_disproof_hand);
        amount = std::max(amount, result.Amount());
        if (result.Len() < mate_len) {
          mate_len = result.Len();
        }
      }
      amount += std::max<SearchAmount>(mp_.size(), 1) - 1;

      // 千日手のときは MakeRepetition で返す
      if (!mp_.empty()) {
        // children_ は千日手エントリが手前に来るようにソートされているので、以下のようにして千日手判定ができる
        if (const auto& result = results_[idx_.front()]; result.GetFinalData().IsRepetition()) {
          const auto depth = result.GetFinalData().repetition_start;
          if (depth < n.GetDepth()) {
            return SearchResult::MakeRepetition(n.OrHand(), mate_len + 1, amount, depth);
          }
        }
      }
      const auto disproof_hand = RemoveIfHandGivesOtherChecks(n.Pos(), splitted_hand.ToHand());
      return SearchResult::MakeFinal<false>(disproof_hand, mate_len + 1, amount);
    } else {
      SplittedHand splitted_hand = SplittedHand::Zero();
      // 子局面の証明駒の極大集合を計算する
      MateLen mate_len = MateLen::Minus1();
      SearchAmount amount = 1;
      for (const auto i_raw : idx_) {
        const auto& result = results_[i_raw];
        splitted_hand.MergeByMax(result.GetFinalData().hand);
        amount = std::max(amount, result.Amount());
        if (result.Len() > mate_len) {
          mate_len = result.Len();
        }
      }

      // 無駄合で除外した指し手は、証明駒の計算には考慮にいれる必要がある
      for (const auto i_raw : redundant_drop_idx_) {
        const auto& result = results_[i_raw];
        splitted_hand.MergeByMax(result.GetFinalData().hand);
      }
      amount += std::max<SearchAmount>(mp_.size(), 1) - 1;

      const auto proof_hand = AddIfHandGivesOtherEvasions(n.Pos(), splitted_hand.ToHand());
      return SearchResult::MakeFinal<true>(proof_hand, mate_len + 1, amount);
    }
  }

  /// 探索結果を取得する（不明局面）
  SearchResult GetUnknownResult(const Node& /* n */) const {
    const auto& result = FrontResult();
    const SearchAmount amount = result.Amount() + mp_.size() - 1;
    return SearchResult::MakeUnknown(GetPn(), GetDn(), len_, amount);
  }

  /**
   * @brief 先頭以外がソートされた状態のとき、先頭要素を適切な位置に挿入する
   */
  void ResortFront() {
    if (idx_.size() > excluded_moves_ + 1) {
      const auto comparer = MakeComparer();
      const auto begin = idx_.begin() + excluded_moves_;
      const auto itr = std::lower_bound(begin + 1, idx_.end(), idx_[excluded_moves_], comparer);
      std::rotate(begin, begin + 1, itr);
    }
  }

  /**
   * @brief 末尾以外がソートされた状態のとき、末尾要素を適切な位置に挿入する
   */
  void ResortBack() {
    if (idx_.size() > excluded_moves_ + 1) {
      const auto comparer = MakeComparer();
      const auto begin = idx_.begin() + excluded_moves_;
      const auto itr = std::lower_bound(begin, idx_.end() - 1, idx_.back(), comparer);
      std::rotate(itr, idx_.end() - 1, idx_.end());
    }
  }

  /**
   * @brief [0, excluded_moves_] の末尾以外がソートされた状態のとき、末尾要素を適切な位置に挿入する
   */
  void ResortExcludedBack() {
    if (excluded_moves_ > 0) {
      const auto comparer = MakeComparer();
      const auto end = idx_.begin() + excluded_moves_ + 1;
      const auto itr = std::lower_bound(idx_.begin(), end - 1, idx_[excluded_moves_], comparer);
      std::rotate(itr, end - 1, end);
    }
  }

  const bool or_node_;                 ///< 現局面が OR node かどうか
  const MovePicker mp_;                ///< 現局面の合法手
  MateLen len_;                        ///< 現局面における残り探索手数
  std::uint32_t multi_pv_;             ///< MultiPv の値。1以上でなければならない
  LazyExpansionTable lazy_expansion_;  ///< 後回しにしている手のグラフ構造

  /// 子の現在の評価値結果一覧
  std::array<SearchResult, kMaxCheckMovesPerNode> results_;
  /// 子のクエリ一覧。コンストラクト時に作ったクエリを使い回すことで高速化できる
  std::array<tt::Query, kMaxCheckMovesPerNode> queries_;

  /// 現局面の評価値が古い探索情報に基づくものかどうか。TCA の探索延長の判断に用いる。
  bool does_have_old_child_;

  PnDn delta_max_;        ///< δ == kInfinitePnDn でない子の中で最大の δ 値
  PnDn valid_child_num_;  ///< 有効な子（idx_ に入っていて、かつfinalでない子）の数

  /// 現在有効な生添字の一覧。「良さ順」で並んでいる。
  InlineStack<std::uint32_t, kMaxCheckMovesPerNode> idx_;
  /// 無駄合で除外された指し手の生添字の一覧
  InlineStack<std::uint32_t, kMaxCheckMovesPerNode> redundant_drop_idx_;

  /// 勝ちになる手を見つけた個数
  /// multi_pv_ == 1 のときは、この値は常に 0 である。multi_pv_ > 1 のとき、勝ち（phi==0）を見つけた後に探索を続ける
  /// 際に用いる。常に excluded_moves_ <= multi_pv_ - 1 かつ excluded_moves_ <= mp_.size() である。
  std::uint32_t excluded_moves_;
};
}  // namespace komori

#endif  // KOMORI_LOCAL_EXPANSION_HPP_
