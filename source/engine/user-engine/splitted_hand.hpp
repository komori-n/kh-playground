/**
 * @file splitted_hand.hpp
 */
#ifndef KOMORI_SPLITTED_HAND_HPP_
#define KOMORI_SPLITTED_HAND_HPP_

#include <cstdint>

#include "typedefs.hpp"

namespace komori {
/**
 * @brief 駒ごとにバラバラに持ち駒を管理するクラス
 */
class SplittedHand {
 public:
  /// `hand` から持ち駒を分割して初期化する
  explicit SplittedHand(Hand hand) {
    for (PieceType pr = PIECE_HAND_ZERO; pr < PIECE_HAND_NB; ++pr) {
      val_[pr] = hand & PIECE_BIT_MASK2[pr];
    }
  }

  /// 空の持ち駒を返す
  static SplittedHand Zero() { return SplittedHand{HAND_ZERO}; }

  /// 全ての持ち駒を返す
  static SplittedHand Full() { return SplittedHand{static_cast<Hand>(HAND_BIT_MASK)}; }

  /// 現在の持ち駒を返す
  explicit operator Hand() const { return ToHand(); }
  /// 現在の持ち駒を返す
  Hand ToHand() const {
    InternalType hand = HAND_ZERO;
    KOMORI_UNROLL(PIECE_HAND_NB - 1) for (PieceType pr = PIECE_HAND_ZERO; pr < PIECE_HAND_NB; ++pr) {
      hand |= val_[pr];
    }
    return static_cast<Hand>(hand);
  }

  /// `other` との持ち駒の和集合を返す
  void MergeByMax(const Hand& other) {
    KOMORI_UNROLL(PIECE_HAND_NB - 1) for (PieceType pr = PIECE_HAND_ZERO; pr < PIECE_HAND_NB; ++pr) {
      val_[pr] = std::max<InternalType>(val_[pr], other & PIECE_BIT_MASK2[pr]);
    }
  }

  /// `other` との持ち駒の積集合を返す
  void MergeByMin(const Hand& other) {
    KOMORI_UNROLL(PIECE_HAND_NB - 1) for (PieceType pr = PIECE_HAND_ZERO; pr < PIECE_HAND_NB; ++pr) {
      val_[pr] = std::min<InternalType>(val_[pr], other & PIECE_BIT_MASK2[pr]);
    }
  }

 private:
  /// 内部で持つ持ち駒の型
  using InternalType = std::uint_fast32_t;

  /// 駒ごとの持ち駒
  std::array<InternalType, PIECE_HAND_NB> val_;
};
}  // namespace komori

#endif  // KOMORI_SPLITTED_HAND_HPP_
