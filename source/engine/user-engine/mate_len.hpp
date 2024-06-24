/**
 * @file mate_len.hpp
 */
#ifndef KOMORI_MATE_LEN_HPP_
#define KOMORI_MATE_LEN_HPP_

#include <ostream>

#include "typedefs.hpp"

namespace komori {
namespace detail {
/**
 * @brief `MateLen` と `MateLen16` の実装本体。中身はほぼ同じなので一箇所にまとめる。
 * @tparam T 16ビット以上の符号なし整数型
 *
 * @note 初期値として「0手よりも小さい手数」を表現したいので、実際の手数に kOffset を加えた値を保持する。
 */
template <typename T>
class MateLenImpl : DefineNotEqualByEqual<MateLenImpl<T>>, DefineComparisonOperatorsByLess<MateLenImpl<T>> {
  /**
   * @brief 任意の整数型を基底に持つ `MateLenImpl` をフレンド指定する。
   * @tparam S 整数型
   *
   * コンストラクト時に `len_plus_ofs_` に直接アクセスするために必要。`Len()` は -Offset 手を 0 手に
   * 切り上げてしまうので、 friend 指定なしだと実装がやや難しい。
   */
  template <typename S>
  friend class MateLenImpl;

  static_assert(std::is_integral_v<T> && std::is_unsigned_v<T>, "T must be an integer");
  static_assert(sizeof(T) >= 2, "The size of T must be greater than or equal to 2");

 public:
  /**
   * @brief `len` 手詰み（不詰）で初期化する
   * @param len 詰み（不詰）手数
   */
  constexpr explicit MateLenImpl(T len) noexcept : len_plus_ofs_{static_cast<T>(len + kOffset)} {}
  /**
   * @brief 他の整数を基底に持つ `len` から初期化する
   * @tparam S  整数型
   * @param len 詰み（不詰）手数
   */
  template <typename S, Constraints<std::enable_if_t<!IsNarrowingConversion<S, T>::value>> = nullptr>
  constexpr MateLenImpl(const MateLenImpl<S>& len) noexcept : len_plus_ofs_{len.len_plus_ofs_} {}
  template <typename S, Constraints<std::enable_if_t<IsNarrowingConversion<S, T>::value>> = nullptr>
  constexpr explicit MateLenImpl(const MateLenImpl<S>& len) noexcept
      : len_plus_ofs_{static_cast<T>(len.len_plus_ofs_)} {}
  /// Default constructor(default)
  MateLenImpl() noexcept = default;
  /// Copy constructor(default)
  constexpr MateLenImpl(const MateLenImpl&) noexcept = default;
  /// Move constructor(default)
  constexpr MateLenImpl(MateLenImpl&&) noexcept = default;
  /// Copy assign operator(default)
  constexpr MateLenImpl& operator=(const MateLenImpl&) noexcept = default;
  /// Move assign operator(default)
  constexpr MateLenImpl& operator=(MateLenImpl&&) noexcept = default;
  /// Destructor(default)
  ~MateLenImpl() = default;

  /// MateLenImpl で表せる最小値
  static constexpr MateLenImpl Min() noexcept { return MateLenImpl{DirectConstructTag{}, T{0}}; }

  /// MateLenImpl で表せる最大値
  static constexpr MateLenImpl Max() noexcept { return MateLenImpl{kDepthMax + 1}; }

  /// -1 手
  static constexpr MateLenImpl Minus1() noexcept {
    return MateLenImpl{DirectConstructTag{}, static_cast<T>(kOffset - 1)};
  }

  /// 0 手
  static constexpr MateLenImpl Zero() noexcept { return MateLenImpl{0}; }

  /// kDepthMax 手
  static constexpr MateLenImpl DepthMax() noexcept { return MateLenImpl{kDepthMax}; }

  /// 詰み手数を返す
  constexpr T Len() const noexcept { return SaturatedSubtract(len_plus_ofs_, kOffset); }

  /// 後置インクリメント
  constexpr MateLenImpl& operator++() noexcept {
    ++len_plus_ofs_;
    return *this;
  }
  /// 前置インクリメント
  constexpr MateLenImpl operator++(int) noexcept {
    const MateLenImpl tmp{*this};
    ++len_plus_ofs_;
    return tmp;
  }
  /// 後置デクリメント
  constexpr MateLenImpl& operator--() noexcept {
    --len_plus_ofs_;
    return *this;
  }
  /// 前置デクリメント
  constexpr MateLenImpl operator--(int) noexcept {
    const MateLenImpl tmp{*this};
    --len_plus_ofs_;
    return tmp;
  }

  /// `lhs` と `rhs` が同じ手数かどうか
  friend constexpr bool operator==(const MateLenImpl& lhs, const MateLenImpl& rhs) noexcept {
    return lhs.len_plus_ofs_ == rhs.len_plus_ofs_;
  }

  /// `lhs` より `rhs` のほうが大きい手数かどうか
  friend constexpr bool operator<(const MateLenImpl& lhs, const MateLenImpl& rhs) noexcept {
    return lhs.len_plus_ofs_ < rhs.len_plus_ofs_;
  }

  /// `lhs` に `rhs` を加えた手数
  friend constexpr MateLenImpl operator+(const MateLenImpl& lhs, const T& rhs) noexcept {
    return MateLenImpl{DirectConstructTag{}, static_cast<T>(lhs.len_plus_ofs_ + rhs)};
  }

  /// `lhs` に `rhs` を加えた手数
  friend constexpr MateLenImpl operator+(const T& lhs, const MateLenImpl& rhs) noexcept { return rhs + lhs; }

  /// `lhs` から `rhs` を引いた手数
  friend constexpr MateLenImpl operator-(const MateLenImpl& lhs, const T& rhs) noexcept {
    T new_len_plus_ofs = static_cast<T>(lhs.len_plus_ofs_ - rhs);
    // 無限からは何を引いても無限
    const auto depth_max_plus_ofs = static_cast<T>(kDepthMax + kOffset);
    if (lhs.len_plus_ofs_ >= depth_max_plus_ofs && new_len_plus_ofs < depth_max_plus_ofs) {
      new_len_plus_ofs = depth_max_plus_ofs;
    }
    return MateLenImpl{DirectConstructTag{}, new_len_plus_ofs};
  }

  /// 出力ストリームへの出力
  friend std::ostream& operator<<(std::ostream& os, const MateLenImpl len) {
    if (len.len_plus_ofs_ >= kOffset) {
      return os << len.len_plus_ofs_ - kOffset;
    } else {
      return os << "-" << static_cast<int>(-(len.len_plus_ofs_ - kOffset));
    }
  }

 private:
  /// 詰み／不詰手数のオフセット。マイナス kOffset を表せるようにする
  static constexpr T kOffset = 10;

  /// `len_plus_ofs` からコンストラクトすることを示すタグ
  struct DirectConstructTag {};
  /**
   * @brief `len_plus_ofs` から直接構築するためのコンストラクタ
   * @param len_plus_ofs 詰み／不詰手数 + kOffset
   */
  constexpr MateLenImpl(DirectConstructTag, T len_plus_ofs) : len_plus_ofs_{len_plus_ofs} {}

  /// 詰み／不詰手数 + kOffset。「0手より小さい手」を初期値として使いたいので kOffset を加える。
  T len_plus_ofs_;
};
}  // namespace detail

/**
 * @brief 詰み／不詰手数
 */
using MateLen = detail::MateLenImpl<std::uint32_t>;

/**
 * @brief 詰み／不詰手数（`MateLen` の16ビット版）
 *
 * 実はエンジン全体で `MateLen16` を使ってもパフォーマンス的にはそれほど影響ないのだが、いつか最終局面の駒あまり枚数を
 * 考慮したくなった時に差が出るかもしれないので、型をちゃんと使い分ける。
 */
using MateLen16 = detail::MateLenImpl<std::uint16_t>;

}  // namespace komori

#endif  // KOMORI_MATE_LEN_HPP_
