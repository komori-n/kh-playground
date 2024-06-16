/**
 * @file delayed_move_list.hpp
 */
#ifndef KOMORI_DELAYED_LIST_HPP_
#define KOMORI_DELAYED_LIST_HPP_

#include <array>
#include <optional>
#include <utility>

#include "move_picker.hpp"
#include "node.hpp"
#include "ranges.hpp"
#include "typedefs.hpp"

namespace komori {
/**
 * @brief 指し手の遅延展開を判断するクラス。
 *
 * 同じ地点への合駒などのすぐに展開する必要のない局面を特定し、その依存関係を提供する。
 *
 * もし合駒手を等しく均等に調べると、pn が過大評価される可能性があり、探索性能の劣化につながる。
 * そのため、他の指し手の結果を見てから局面を読み進めたいことがしばしばある。
 *
 * このクラスでは、遅延展開すべき手で単方向リストを構成する。例えば、`SQ_52` への合駒であれば、
 *
 * - △５二歩 -> △５二香 -> △５二桂 -> ... -> △５二金
 *
 * のような単方向リストを形成する。単方向リストの次の要素は `Next()` で取得できる。
 * また、`Remove()` により要素の削除も可能で、その場合、削除された要素は `Next()` の候補から除外される。
 */
class LazyExpansionTable {
 public:
  /**
   * @brief 局面 `n` の遅延展開すべき手を調べる
   * @param n   現局面
   * @param mp  `n` における合法手
   */
  LazyExpansionTable(const Node& n, const MovePicker& mp) {
    for (std::uint32_t i_raw = 0; i_raw < mp.size(); ++i_raw) {
      next_plus1_[i_raw] = 0;
      has_prev_[i_raw] = false;
      is_removed_[i_raw] = false;
    }

    for (std::uint32_t i_raw = 0; i_raw + 1 < mp.size(); ++i_raw) {
      const Move move1 = mp[i_raw];
      for (std::size_t j_raw = i_raw + 1; j_raw < mp.size(); ++j_raw) {
        const Move move2 = mp[j_raw];
        if (IsSame(n, move1, move2)) {
          next_plus1_[i_raw] = j_raw + 1;
          has_prev_[j_raw] = true;
          break;
        }
      }
    }
  }

  /**
   * @brief `i_raw` の直後に展開すべき手のインデックスを返す。
   * @param i_raw 手の `index`
   * @return 直後に展開すべき手があればその `i_raw`。なければ `std::nullopt`。
   */
  std::optional<std::uint32_t> Next(std::uint32_t i_raw) const {
    while (std::uint32_t next_plus_1 = next_plus1_[i_raw]) {
      const std::uint32_t i_raw_next = next_plus_1 - 1;
      if (!is_removed_[i_raw_next]) {
        return {i_raw_next};
      }
      i_raw = i_raw_next;
    }
    return std::nullopt;
  }

  /**
   * @brief `i_raw` の直前に展開すべき手があるかどうかを返す。
   */
  bool HasPrev(std::uint32_t i_raw) const { return has_prev_[i_raw]; }

  /**
   * @brief `i_raw` が削除されたことを記録する。
   */
  void Remove(std::uint32_t i_raw) {
    is_removed_[i_raw] = true;
    if (!has_prev_[i_raw] && next_plus1_[i_raw] > 0) {
      const std::uint32_t i_next = next_plus1_[i_raw] - 1;
      has_prev_[i_next] = false;
    }
  }

 private:
  bool IsSame(const Node& n, Move move1, Move move2) const {
    if (is_drop(move1) && is_drop(move2)) {
      return IsSameDrop(n, move1, move2);
    } else if (!is_drop(move1) && !is_drop(move2)) {
      return IsSameNonDrop(n, move1, move2);
    } else {
      return false;
    }
  }

  bool IsSameDrop(const Node& n, Move move1, Move move2) const {
    if (n.IsOrNode()) {
      return false;
    }

    const Square to1 = to_sq(move1);
    const Square to2 = to_sq(move2);
    if (to1 == to2) {
      return true;
    }

    // 中合いはだいたい無意味なので後回し
    const auto support_cnt1 = n.Pos().attackers_to(n.Us(), to1).pop_count();
    const auto support_cnt2 = n.Pos().attackers_to(n.Us(), to2).pop_count();
    if (support_cnt1 == 0 && support_cnt2 == 0) {
      // NOTE: 逆王手の場合に true を返すと、IsSameが同値関係にならないので注意（1敗）
      return true;  // NOLINT(readability-simplify-boolean-expr)
    }

    return false;
  }

  bool IsSameNonDrop(const Node& n, Move move1, Move move2) const {
    const Square from1 = from_sq(move1);
    const Square from2 = from_sq(move2);
    const Square to1 = to_sq(move1);
    const Square to2 = to_sq(move2);
    if (!n.IsOrNode()) {
      if (from1 != n.KingSquare() && from2 != n.KingSquare() && !n.Pos().capture(move1) && !n.Pos().capture(move2)) {
        return true;
      }

      const Bitboard bb = n.Pos().pieces<PRO_PAWN>();
      if (bb.test(from1) && bb.test(from2) && n.Pos().capture(move1) && n.Pos().capture(move2)) {
        return true;
      }
    }

    if (from1 == from2 && to1 == to2) {
      // 移動方法は同じで成・不成だけが異なる場合
      const PieceType pt1 = type_of(n.Pos().piece_on(from1));
      if (pt1 == PAWN || pt1 == BISHOP || pt1 == ROOK) {
        return true;
      } else if (pt1 == LANCE) {
        const Rank rank = relative_rank(n.Us(), rank_of(to1));
        if (rank == RANK_2) {
          return true;
        }
      }
    }

    return false;
  }

  std::array<std::uint32_t, kMaxCheckMovesPerNode> next_plus1_;  ///< 直後に展開すべき手 + 1。なければ0。
  std::array<bool, kMaxCheckMovesPerNode> has_prev_;             ///< 直前に展開すべき手があるかどうか
  std::array<bool, kMaxCheckMovesPerNode> is_removed_;           ///< 削除されたかどうか
};

}  // namespace komori

#endif  // KOMORI_DELAYED_LIST_HPP_
