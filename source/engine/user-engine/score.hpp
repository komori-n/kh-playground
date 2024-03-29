/**
 * @file score.hpp
 */
#ifndef KOMORI_SCORE_HPP_
#define KOMORI_SCORE_HPP_

#include <cmath>

#include "engine_option.hpp"
#include "search_result.hpp"
#include "typedefs.hpp"

namespace komori {
/**
 * @brief 現在の探索状況に基づく評価値。
 */
class Score : DefineNotEqualByEqual<Score> {
  /// 内部で用いる整数型
  using ScoreValue = std::int64_t;

 public:
  /// デフォルトコンストラクタ
  constexpr Score() noexcept = default;

  /**
   * @brief 詰み状態の `Score` オブジェクトを構築する。
   * @param method `Score` の計算方法
   * @param mate_len 詰み手数
   */
  static Score MakeProven(ScoreCalculationMethod /* method */, std::size_t mate_len, bool is_root_or_node) {
    const auto score = Score(Kind::kWin, static_cast<ScoreValue>(mate_len));
    return is_root_or_node ? score : -score;
  }

  /**
   * @brief `Score` オブジェクトを構築する。
   * @param method `Score` の計算方法
   * @param result `result` 現在の探索結果
   * @param is_root_or_node 開始局面が OR node かどうか
   * @return `Score` オブジェクト
   *
   * `method` の値に応じて構築方法を変えたいので `static` メソッドとして公開する。
   */
  static Score Make(ScoreCalculationMethod method, const SearchResult& result, bool is_root_or_node) {
    constexpr double kPonanza = 600.0;  // ポナンザ定数

    Score score{};  // 開始局面が AND node なら正負を反転したいので一旦変数に格納する
    if (result.IsFinal()) {
      // 開始局面の手番を基準に評価値を計算しなければならない
      if (result.Pn() == 0) {
        score = Score(Kind::kWin, static_cast<ScoreValue>(result.Len().Len()));
      } else {
        score = Score(Kind::kLose, static_cast<ScoreValue>(result.Len().Len()));
      }
    } else {
      switch (method) {
        case ScoreCalculationMethod::kDn:
          score = Score(Kind::kUnknown, static_cast<ScoreValue>(result.Dn()));
          break;
        case ScoreCalculationMethod::kMinusPn:
          score = Score(Kind::kUnknown, -static_cast<ScoreValue>(result.Pn()));
          break;
        case ScoreCalculationMethod::kPonanza: {
          const double r = static_cast<double>(result.Dn()) / static_cast<double>(result.Pn() + result.Dn());
          const double val_real = -kPonanza * std::log((1 - r) / r);
          const auto val = static_cast<ScoreValue>(val_real);
          score = Score(Kind::kUnknown, val);
        } break;
        default:
          score = Score(Kind::kUnknown, 0);
      }
    }

    return is_root_or_node ? score : -score;
  }

  /// 現在の評価値を USI 文字列で返す
  std::string ToString() const {
    const auto depth_max_to_print_max = [](ScoreValue value) { return value >= kDepthMax ? kMatePrintMax : value; };
    switch (kind_) {
      case Kind::kWin:
        return std::string{"mate "} + std::to_string(depth_max_to_print_max(value_));
      case Kind::kLose:
        return std::string{"mate -"} + std::to_string(depth_max_to_print_max(value_));
      default:
        return std::string{"cp "} + std::to_string(value_);
    }
  }

  /// 評価値が詰み／不詰かどうか。
  bool IsFinal() const { return kind_ != Kind::kUnknown; }

  /**
   * @brief 評価値が詰みまたは不詰のとき、手数を1手伸ばす
   *
   * 探索開始局面で評価値を出力するとき、詰み手数が1手ズレてしまうのを直す用。
   */
  void AddOneIfFinal() {
    if (kind_ == Kind::kWin || kind_ == Kind::kLose) {
      value_ = std::min<ScoreValue>(value_ + 1, kDepthMax);
    }
  }

  /// 評価値の正負を反転させる
  Score operator-() const {
    switch (kind_) {
      case Kind::kWin:
        return Score{Kind::kLose, value_};
      case Kind::kLose:
        return Score{Kind::kWin, value_};
      default:
        return Score{Kind::kUnknown, -value_};
    }
  }

  /// `lhs` と `rhs` が等しいかどうか
  friend bool operator==(const Score& lhs, const Score& rhs) noexcept {
    return lhs.kind_ == rhs.kind_ && lhs.value_ == rhs.value_;
  }

 private:
  /// 詰まなかったときに表示する詰み手数
  static constexpr ScoreValue kMatePrintMax = 9999;
  /// 評価値の種別（勝ちとか負けとか）
  enum class Kind {
    kUnknown,  ///< 詰み／不詰未確定
    kWin,      ///< （開始局面の手番から見て）勝ち
    kLose,     ///< （開始局面の手番から見て）負け
  };

  /// コンストラクタ。`Make()` 以外では構築できないように private に隠しておく
  Score(Kind kind, ScoreValue value) : kind_{kind}, value_{value} {}

  Kind kind_{Kind::kUnknown};  ///< 評価値諸別
  ScoreValue value_{};         ///< 評価値（kUnknown） or 詰み手数（kWin/kLose）
};
}  // namespace komori

#endif  // KOMORI_SCORE_HPP_
