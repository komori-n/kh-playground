/**
 * @file move_path.hpp
 */
#ifndef KOMORI_MATE_PATH_HPP_
#define KOMORI_MATE_PATH_HPP_

#include "typedefs.hpp"

namespace komori {
/**
 * @brief 手順を保持するクラス
 */
class MovePath {
 public:
  MovePath() = default;
  MovePath(const MovePath&) = delete;
  MovePath(MovePath&& rhs) noexcept = default;
  MovePath& operator=(const MovePath&) = delete;
  MovePath& operator=(MovePath&& rhs) noexcept = default;
  ~MovePath() = default;

  /**
   * @brief `move` を探す `depth` 手目に追加する。ただし、`depth` より深い手順は削除する。
   * @param move 次の手
   * @param depth 手数
   */
  void AddMove(Move move, Depth depth) {
    moves_.resize(depth);
    moves_.push_back(move);
  }

  /// 手順を取得する
  const std::vector<Move>& Moves() const { return moves_; }
  /// 手順の文字列を取得する
  std::string ToString() const { return ::komori::ToString(moves_); }

 private:
  std::vector<Move> moves_;  ///< 手順
};
}  // namespace komori

#endif  // KOMORI_MATE_PATH_HPP_
