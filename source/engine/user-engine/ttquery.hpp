/**
 * @file ttquery.hpp
 */
#ifndef KOMORI_TTQUERY_HPP_
#define KOMORI_TTQUERY_HPP_

#include <optional>
#include <shared_mutex>

#include "mate_len.hpp"
#include "regular_table.hpp"
#include "repetition_table.hpp"
#include "search_result.hpp"
#include "ttentry.hpp"
#include "typedefs.hpp"

namespace komori::tt {

/**
 * @brief 連続する複数エントリを束ねてまとめて読み書きするためのクラス。
 *
 * 詰将棋エンジンでは置換表を何回も何回も読み書きする。このクラスでは、置換表の読み書きに必要となる
 * 情報（ハッシュ値など）をキャッシュしておき、高速化することが目的である。
 *
 * ## 実装詳細
 *
 * 置換表から結果を読む際、以下の情報を利用して探索結果を復元する。
 *
 * 1. 同一局面（盤面も持ち駒も一致）
 * 2. 優等局面（盤面が一致していて持ち駒が現局面より多い）
 * 3. 劣等局面（盤面が一致していて持ち駒が現局面より少ない）
 */
class Query {
 public:
  /**
   * @brief Query の構築を行う。
   * @param rep_table   千日手テーブル
   * @param initial_entry_pointer 通常テーブルの探索開始位置への循環ポインタ
   * @param path_key    経路ハッシュ値
   * @param board_key   盤面ハッシュ値
   * @param hand        持ち駒
   * @param depth       探索深さ
   */
  constexpr Query(RepetitionTable& rep_table,
                  CircularEntryPointer initial_entry_pointer,
                  Key path_key,
                  Key board_key,
                  Hand hand,
                  Depth depth)
      : rep_table_{&rep_table},
        initial_entry_pointer_{initial_entry_pointer},
        path_key_{path_key},
        board_key_{board_key},
        hand_{hand},
        depth_{depth},
        cached_entry_{&*initial_entry_pointer} {};

  /**
   * @brief Default constructor(default)
   *
   * 配列で領域を確保したいためデフォルトコンストラクト可能にしておく。デフォルトコンストラクト状態では
   * メンバ関数の呼び出しは完全に禁止である。そのため、使用する前に必ず引数つきのコンストラクタで初期化を行うこと。
   */
  Query() = default;
  /// Copy constructor(delete). 使うことはないと思われるので封じておく。
  Query(const Query&) = delete;
  /// Move constructor(default)
  constexpr Query(Query&&) noexcept = delete;
  /// Copy assign operator(delete)
  Query& operator=(const Query&) = delete;
  /// Move assign operator(default)
  constexpr Query& operator=(Query&&) noexcept = default;
  /// Destructor
  ~Query() noexcept = default;

  // テンプレート関数のカバレッジは悲しいことになるので取らない
  // LCOV_EXCL_START NOLINTBEGIN

  /**
   * @brief 置換表から結果を集めてきて返す関数。
   * @tparam InitialEvalFunc 初期値関数の型
   * @param does_have_old_child  unproven old child の結果を使った場合 `true` が書かれる変数
   * @param len 探している詰み手数
   * @param eval_func 初期値を計算する関数。エントリが見つからなかったときのみ呼ばれる。
   * @param strict_lookup 厳密な探索を行うかどうか。false のときは、詰みを見つけたらすぐにそれを返す。
   * @return Look Up 結果
   *
   * 置換表を探索して現局面の探索情報を返す関数。
   *
   * 一般に、初期化関数の呼び出しは実行コストがかかる。そのため、初期化関数は `eval_func` に包んで渡す。
   * 必要な時のみ `eval_func` の呼び出しを行うことで、呼び出さないパスの高速化ができる。
   */
  template <typename InitialEvalFunc>
  SearchResult LookUp(bool& does_have_old_child,
                      MateLen len,
                      InitialEvalFunc&& eval_func,
                      bool strict_lookup = false) const {
    PnDn pn = 1;
    PnDn dn = 1;
    SearchAmount amount = 1;

    bool found_exact = false;

    for (auto itr = initial_entry_pointer_; !itr->IsNull(); ++itr) {
      std::shared_lock lock(*itr);
      // 本来は lock 後にも !itr->Null() のチェックが必要だが、itr->Hand() == kNullHand のとき itr->LookUp() が必ず
      // 失敗するので、このタイミングでのチェックは省略できる。
      if (itr->IsFor(board_key_)) {
        if (itr->LookUp(hand_, depth_, len, pn, dn, does_have_old_child)) {
          amount = std::max(amount, itr->Amount());
          if (pn == 0) {
            Hand proof_hand = itr->GetHand();
            MateLen proven_len = itr->ProvenLen();
            if (strict_lookup) {
              for (auto itr2 = ++itr; !itr2->IsNull(); ++itr2) {
                std::shared_lock lock2{*itr2};
                if (itr2->IsFor(board_key_) && itr2->UpdateProvenLen(hand_, proven_len)) {
                  proof_hand = itr2->GetHand();
                }
              }
            }
            return SearchResult::MakeFinal<true>(proof_hand, proven_len, amount);
          } else if (dn == 0) {
            Hand disproof_hand = itr->GetHand();
            MateLen disproven_len = itr->DisprovenLen();
            if (strict_lookup) {
              for (auto itr2 = ++itr; !itr2->IsNull(); ++itr2) {
                std::shared_lock lock2{*itr2};
                if (itr2->IsFor(board_key_) && itr2->UpdateDisprovenLen(hand_, disproven_len)) {
                  disproof_hand = itr2->GetHand();
                }
              }
            }
            return SearchResult::MakeFinal<false>(disproof_hand, disproven_len, amount);
          } else if (itr->GetHand() == hand_) {
            if (itr->IsPossibleRepetition()) {
              if (const auto opt = rep_table_->Contains(path_key_, len)) {
                const auto [depth, table_len] = opt.value();
                return SearchResult::MakeRepetition(hand_, table_len, amount, depth);
              }
            }

            found_exact = true;
            cached_entry_ = &*itr;
          }
        }
      }
    }

    if (found_exact) {
      return SearchResult::MakeUnknown(pn, dn, len, amount);
    }

    const auto [init_pn, init_dn] = std::forward<InitialEvalFunc>(eval_func)();
    pn = std::max(pn, init_pn);
    dn = std::max(dn, init_dn);

    return SearchResult::MakeFirstVisit(pn, dn, len, amount);
  }
  // LCOV_EXCL_STOP NOLINTEND

  /**
   * @brief 探索結果 `result` を置換表に書き込む
   * @param result 探索結果
   * @note 実際の処理は `SetProven()`, `SetDisproven()`, `SetRepetition()`, `SetUnknown()` を参照。
   */
  void SetResult(const SearchResult& result) const noexcept {
    if (result.Pn() == 0) {
      SetFinal<true>(result);
    } else if (result.Dn() == 0) {
      if (result.GetFinalData().IsRepetition()) {
        SetRepetition(result);
      } else {
        SetFinal<false>(result);
      }
    } else {
      SetUnknown(result);
    }
  }

 private:
  /**
   * @brief 置換表に `hand` に一致するエントリがあればそれを返し、なければ作って返す
   * @param hand 持ち駒
   * @return 見つけた or 作成したエントリ。`lock()` された状態で返るので、参照が完了したら必ず `unlock()` を呼ぶこと。
   */
  Entry* FindOrCreate(Hand hand) const noexcept {
    if (!cached_entry_->IsNull()) {
      cached_entry_->lock();
      if (cached_entry_->IsFor(board_key_, hand)) {
        return cached_entry_;
      }
      cached_entry_->unlock();
    }

    for (auto itr = initial_entry_pointer_;; ++itr) {
      itr->lock();
      if (itr->IsNull()) {
        itr->Init(board_key_, hand);
        return cached_entry_ = &*itr;
      }

      if (itr->IsFor(board_key_, hand)) {
        return cached_entry_ = &*itr;
      }
      itr->unlock();
    }
  }

  /**
   * @brief 詰みまたは不詰の探索結果 `result` を置換表に書き込む
   * @tparam kIsProven true: 詰み／false: 不詰
   * @param result 探索結果（詰み or 不詰）
   */
  template <bool kIsProven>
  void SetFinal(const SearchResult& result) const noexcept {
    const auto hand = result.GetFinalData().hand;
    auto* const entry = FindOrCreate(hand);
    const auto len = result.Len();
    const auto amount = result.Amount();

    if constexpr (kIsProven) {
      entry->UpdateProven(len, amount);
    } else {
      entry->UpdateDisproven(len, amount);
    }
    entry->unlock();
  }

  /**
   * @brief
   * @param result 探索結果（千日手）
   */
  void SetRepetition(const SearchResult& result) const noexcept {
    auto* const entry = FindOrCreate(hand_);

    entry->SetPossibleRepetition();
    entry->unlock();
    rep_table_->Insert(path_key_, result.GetFinalData().repetition_start, result.Len());
  }

  /**
   * @brief 探索中の探索結果 `result` を置換表に書き込む関数
   * @param result 探索結果（探索中）
   */
  void SetUnknown(const SearchResult& result) const noexcept {
    const auto pn = result.Pn();
    const auto dn = result.Dn();
    const auto amount = result.Amount();

    auto* const entry = FindOrCreate(hand_);
    entry->UpdateUnknown(depth_, pn, dn, amount);
    entry->unlock();
  }

  RepetitionTable* rep_table_;                  ///< 千日手テーブル。千日手判定に用いる。
  CircularEntryPointer initial_entry_pointer_;  ///< 通常テーブルの探索開始位置への循環ポインタ
  Key path_key_;                                ///< 現局面の経路ハッシュ値
  Key board_key_;                               ///< 現局面の盤面ハッシュ値
  Hand hand_;                                   ///< 現局面の持ち駒
  Depth depth_;                                 ///< 現局面の探索深さ
  mutable Entry* cached_entry_;                 ///< 前回アクセスしたエントリ
};
}  // namespace komori::tt

#endif  // KOMORI_TTQUERY_HPP_
