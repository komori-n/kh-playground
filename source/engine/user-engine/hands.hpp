/**
 * @file hands.hpp
 */
#ifndef KOMORI_HANDS_HPP_
#define KOMORI_HANDS_HPP_

#include <algorithm>
#include <array>

#include "typedefs.hpp"

namespace komori {
/// hand から pr を消す
inline void RemoveHand(Hand& hand, PieceType pr) {
  hand = static_cast<Hand>(hand & ~PIECE_BIT_MASK2[pr]);
}

/// 2 つの持ち駒を 1 つにまとめる
inline Hand MergeHand(Hand h1, Hand h2) {
  return static_cast<Hand>(h1 + h2);
}

/// 先後の持ち駒（盤上にない駒）を全てかき集める
inline Hand CollectHand(const Position& n) {
  return MergeHand(n.hand_of(BLACK), n.hand_of(WHITE));
}

/// move 後の手駒を返す
inline Hand AfterHand(const Position& n, Move move, Hand before_hand) {
  if (is_drop(move)) {
    const auto pr = move_dropped_piece(move);
    if (hand_exists(before_hand, pr)) {
      sub_hand(before_hand, move_dropped_piece(move));
    }
  } else {
    if (const auto to_pc = n.piece_on(to_sq(move)); to_pc != NO_PIECE) {
      const auto pr = raw_type_of(to_pc);
      add_hand(before_hand, pr);
      if (before_hand & HAND_BORROW_MASK) {
        sub_hand(before_hand, pr);
      }
    }
  }
  return before_hand;
}

/// move 後の手駒が after_hand のとき、移動前の持ち駒を返す
inline Hand BeforeHand(const Position& n, Move move, Hand after_hand) {
  if (is_drop(move)) {
    const auto pr = move_dropped_piece(move);
    add_hand(after_hand, pr);
    if (after_hand & HAND_BORROW_MASK) {
      sub_hand(after_hand, pr);
    }
  } else {
    const auto to_pc = n.piece_on(to_sq(move));
    if (to_pc != NO_PIECE) {
      const auto pr = raw_type_of(to_pc);
      if (hand_exists(after_hand, pr)) {
        sub_hand(after_hand, pr);
      }
    }
  }
  return after_hand;
}

/**
 * @brief 持ち駒 `target` に `diff_dst - diff_src` を加える
 * @param target 現在の持ち駒
 * @param diff_src 変化量の起点
 * @param diff_dst 変化量の終点
 * @return `target` に `diff_dst - diff_src` を加算した結果
 */
inline Hand ApplyDeltaHand(Hand target, Hand diff_src, Hand diff_dst) {
  // 単純に実装するなら target + (diff_dst - diff_src) だが、オーバーフローの可能性があるので駒種別に判定する
  Hand res = HAND_ZERO;
  for (PieceType pr = PIECE_HAND_ZERO; pr < PIECE_HAND_NB; ++pr) {
    const auto target_cnt = hand_count(target, pr);
    const auto src_cnt = hand_count(diff_src, pr);
    const auto dst_cnt = hand_count(diff_dst, pr);
    if (src_cnt < dst_cnt) {
      const auto result_cnt = std::min(target_cnt + dst_cnt - src_cnt, PIECE_BIT_MASK[pr]);
      add_hand(res, pr, result_cnt);
    } else if (src_cnt > dst_cnt) {
      const auto result_cnt = target_cnt >= (src_cnt - dst_cnt) ? target_cnt - (src_cnt - dst_cnt) : 0;
      add_hand(res, pr, result_cnt);
    } else {
      add_hand(res, pr, target_cnt);
    }
  }

  return res;
}

/**
 * @brief 局面 n の子局面がすべて 反証駒 disproof_hand で不詰であることが既知の場合、もとの局面 n の反証駒を計算する。
 *
 * OrNode の時に限り呼び出せる。
 * disproof_hand をそのまま返すのが基本だが、もし disproof_hand の中に局面 n では持っていない駒が含まれていた場合、
 * その駒を打つ手を初手とした詰みがあるかもしれない。（局面 n に含まれないので、前提となる子局面の探索には含まれない）
 * そのため、現局面で持っていない種別の持ち駒がある場合は、反証駒から消す必要がある。
 *
 * ### 例
 *
 * ```
 * 後手の持駒：飛二 角二 金四 銀四 桂三 香三 歩十六
 *   ９ ８ ７ ６ ５ ４ ３ ２ １
 * +---------------------------+
 * | ・ ・ ・ ・ ・ ・ ・ ・v玉|一
 * | ・ ・ ・ ・ ・ ・ ・ ・ ・|二
 * | ・ ・ ・ ・ ・ ・ ・ ・ 歩|三
 * | ・ ・ ・ ・ ・ ・ ・ ・ ・|四
 * | ・ ・ ・ ・ ・ ・ ・ ・ ・|五
 * | ・ ・ ・ ・ ・ ・ ・ ・ ・|六
 * | ・ ・ ・ ・ ・ ・ ・ ・ ・|七
 * | ・ ・ ・ ・ ・ ・ ・ ・ ・|八
 * | ・ ・ ・ ・ ・ ・ ・ ・ ・|九
 * +---------------------------+
 * 先手の持駒：桂 香 歩
 * ```
 *
 * ↑子局面はすべて金を持っていても詰まないが、現局面で金を持っているなら詰む
 *
 * @param n 現在の局面
 * @param disproof_hand n に対する子局面の探索で得られた反証駒の極大集合
 * @return Hand disproof_hand から n で持っていない　かつ　王手になる持ち駒を除いた持ち駒
 */
inline Hand RemoveIfHandGivesOtherChecks(const Position& n, Hand disproof_hand) {
  const Color us = n.side_to_move();
  const Color them = ~n.side_to_move();
  const Hand hand = n.hand_of(us);
  const Square king_sq = n.king_square(them);
  const auto droppable_bb = ~n.pieces();

  for (PieceType pr = PIECE_HAND_ZERO; pr < PIECE_HAND_NB; ++pr) {
    if (!hand_exists(hand, pr) && hand_exists(disproof_hand, pr)) {
      // 二歩の場合は反証駒を消す必要はない（打てないので）
      if (pr == PAWN && (n.pieces(us, PAWN) & file_bb(file_of(king_sq)))) {
        continue;
      }

      if (n.check_squares(pr) & droppable_bb) {
        // pr を持っていたら王手ができる -> pr は反証駒から除かれるべき
        RemoveHand(disproof_hand, pr);
      }
    }
  }
  return disproof_hand;
}

/**
 * @brief 局面 n の子局面がすべて証明駒 proof_hand で詰みであることが既知の場合、もとの局面 n の証明駒を計算する。
 *
 * AndNode の時に限り呼び出せる。
 * proof_hand をそのまま返すのが基本だが、もし proof_hand の中に局面 n では持っていない駒が含まれていた場合、
 * その駒を打って合駒をすれば詰みを防げたかもしれない。（局面 n に含まれないので、前提となる子局面の探索には含まれない）
 * そのため、現局面で持っていない種別の持ち駒がある場合は、証明駒に加える（合駒がなかった情報を付与する）必要がある。
 *
 * ### 例
 *
 * ```
 * 後手の持駒：香
 *   ９ ８ ７ ６ ５ ４ ３ ２ １
 * +---------------------------+
 * | ・ ・ ・ ・ ・ ・ ・ ・ ・|一
 * | ・ ・ ・ ・ ・ ・ ・ ・ ・|二
 * | ・ ・ ・ ・ ・ ・ ・ ・ ・|三
 * | ・ ・ ・ ・ ・ ・ ・v香 ・|四
 * |v金v銀v角v桂 ・ ・ ・v歩v玉|五
 * |v角v飛v飛v桂 ・ ・ ・v香 ・|六
 * |v桂v桂v金 ・ ・ ・ ・ ・ ・|七
 * |v銀v銀v銀 ・ ・ ・ ・ と ・|八
 * |v金v金 ・ ・ ・ ・ ・ ・ 香|九
 * +---------------------------+
 * 先手の持駒：歩十六
 * ```
 *
 * ↑後手の合駒が悪いので詰んでしまう。つまり、「先手が歩を独占している」という情報も証明駒に含める必要がある。
 *
 * @param n 現在の局面
 * @param proof_hand n に対する子局面の探索で得られた証明駒の極小集合
 * @return Hand proof_hand から n で受け方が持っていない　かつ　合駒で王手を防げる持ち駒を攻め方側に集めた持ち駒
 */
inline Hand AddIfHandGivesOtherEvasions(const Position& n, Hand proof_hand) {
  const auto us = n.side_to_move();
  const auto them = ~us;
  const Hand us_hand = n.hand_of(us);
  const Hand them_hand = n.hand_of(them);
  const auto king_sq = n.king_square(n.side_to_move());
  auto checkers = n.checkers();

  if (checkers.pop_count() != 1) {
    return proof_hand;
  }

  const auto checker_sq = checkers.pop();
  if (!between_bb(king_sq, checker_sq)) {
    return proof_hand;
  }

  // 駒を持っていれば合駒で詰みを防げたかもしれない（合法手が増えるから）
  for (PieceType pr = PIECE_HAND_ZERO; pr < PIECE_HAND_NB; ++pr) {
    if (pr == PAWN) {
      const auto drop_mask =
          (us == BLACK) ? pawn_drop_mask<BLACK>(n.pieces<BLACK>(PAWN)) : pawn_drop_mask<WHITE>(n.pieces<WHITE>(PAWN));
      if (!drop_mask.test(between_bb(king_sq, checker_sq))) {
        continue;
      }
    }

    if (!hand_exists(us_hand, pr)) {
      // pr を持っていれば詰みを防げた（かもしれない）
      RemoveHand(proof_hand, pr);
      proof_hand = MergeHand(proof_hand, static_cast<Hand>(hand_exists(them_hand, pr)));
    }
  }

  return proof_hand;
}

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
    for (PieceType pr = PIECE_HAND_ZERO; pr < PIECE_HAND_NB; ++pr) {
      hand |= val_[pr];
    }
    return static_cast<Hand>(hand);
  }

  /// `other` との持ち駒の和集合を返す
  void MergeByMax(const Hand& other) {
    for (PieceType pr = PIECE_HAND_ZERO; pr < PIECE_HAND_NB; ++pr) {
      val_[pr] = std::max<InternalType>(val_[pr], other & PIECE_BIT_MASK2[pr]);
    }
  }

  /// `other` との持ち駒の積集合を返す
  void MergeByMin(const Hand& other) {
    for (PieceType pr = PIECE_HAND_ZERO; pr < PIECE_HAND_NB; ++pr) {
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

#endif  // KOMORI_HANDS_HPP_
