/**
 * @file redundant_move_table.hpp
 */
#ifndef KOMORI_REDUNDANT_MOVE_TABLE_HPP_
#define KOMORI_REDUNDANT_MOVE_TABLE_HPP_

#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include "ranges.hpp"
#include "shared_exclusive_lock.hpp"
#include "typedefs.hpp"

namespace komori::tt {
/**
 * @brief 無駄合テーブル
 */
class RedundantMoveTable {
 public:
  RedundantMoveTable() = default;
  RedundantMoveTable(const RedundantMoveTable&) = delete;
  RedundantMoveTable(RedundantMoveTable&&) = delete;
  RedundantMoveTable& operator=(const RedundantMoveTable&) = delete;
  RedundantMoveTable& operator=(RedundantMoveTable&&) = delete;
  ~RedundantMoveTable() = default;

  /// テーブルをクリアする
  void Clear() {
    std::lock_guard lock{mutex_};
    table_.clear();
  }

  /**
   * @brief 局面 (`board_key`, `hand) で駒打ち `move` は無駄合か？
   * @param board_key 盤面ハッシュ
   * @param hand 攻め方の持ち駒
   * @param move 駒打ち
   * @return true 無駄合, false 無駄合でない
   */
  bool Contains(Key board_key, Hand hand, Move move) const {
    std::shared_lock lock{mutex_};
    for (auto [_, hand_i] : AsRange(table_.equal_range(CalcKey(board_key, move)))) {
      if (hand_is_equal_or_superior(hand, hand_i)) {
        return true;
      }
    }
    return false;
  }

  /**
   * @brief 局面 `board_key` で駒打ち `move` が無駄合であることを登録する
   * @param board_key 盤面ハッシュ
   * @param hand 攻め方の持ち駒
   * @param move 駒打ち
   */
  void Insert(Key board_key, Hand hand, Move move) {
    std::lock_guard lock{mutex_};
    for (auto& [_, hand_i] : AsRange(table_.equal_range(CalcKey(board_key, move)))) {
      if (hand_is_equal_or_superior(hand_i, hand)) {
        hand_i = hand;
        return;
      }
    }

    table_.emplace(CalcKey(board_key, move), hand);
  }

  /// テーブルのサイズを返す
  std::size_t Size() const {
    std::shared_lock lock{mutex_};
    return table_.size();
  }

 private:
  /// 盤面ハッシュと駒打ちからキーを計算する
  Key CalcKey(Key board_key, Move move) const { return board_key ^ static_cast<Key>(to_sq(move)); }

  mutable SharedExclusiveLock<std::int8_t> mutex_;  ///< ミューテックス
  std::unordered_multimap<Key, Hand> table_;        ///< テーブル本体
};
}  // namespace komori::tt

#endif  // KOMORI_REDUNDANT_MOVE_TABLE_HPP_
