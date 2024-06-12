/**
 * @file expansion_stack.hpp
 */
#ifndef KOMORI_EXPANSION_STACK_HPP_
#define KOMORI_EXPANSION_STACK_HPP_

#include <deque>
#include "local_expansion.hpp"

namespace komori {
/**
 * @brief `LocalExpansion` をスタックで管理するクラス。
 *
 * 基本的には `std::stack<LocalExpansion>` のように振る舞う。`Emplace()` により新たな `LocalExpansion` を
 * 構築し、`Pop()` により構築したインスタンスのうち最も新しいものを消す。最新のインスタンスは `Current()` で取得できる。
 */
class ExpansionStack {
 public:
  /// Default constructor(default)
  ExpansionStack() = default;
  /// Copy constructor(delete)
  ExpansionStack(const ExpansionStack&) = delete;
  /// Move constructor(delete)
  ExpansionStack(ExpansionStack&&) = delete;
  /// Copy assign operator(delete)
  ExpansionStack& operator=(const ExpansionStack&) = delete;
  /// Move assign operator(delete)
  ExpansionStack& operator=(ExpansionStack&&) = delete;
  /// Destructor(default)
  ~ExpansionStack() = default;

  /**
   * @brief スタックの先頭に `LocalExpansion` オブジェクトを構築する。
   * @tparam Args `LocalExpansion` のコンストラクタの引数。詳細は `LocalExpansion` の定義を参照。
   * @param args `LocalExpansion` のコンストラクタの引数。
   * @return 構築した `LocalExpansion` オブジェクト
   */
  template <typename... Args>
  LocalExpansion& Emplace(Args&&... args) {
    auto& expansion = list_.emplace_back(std::forward<Args>(args)...);
    return expansion;
  }

  /**
   * @brief スタック先頭の `LocalExpansion` オブジェクトを開放する。
   */
  void Pop() noexcept { list_.pop_back(); }

  /// スタック先頭要素を返す。
  LocalExpansion& Current() { return list_.back(); }
  /// スタック先頭要素を返す。
  const LocalExpansion& Current() const { return list_.back(); }

  /// スタックが空かどうか判定する
  bool IsEmpty() const { return list_.empty(); }
  /**
   * @brief スタックの最も古い要素を返す
   * @return スタックの最も古い要素
   * @pre `!IsEmpty()`
   */
  const LocalExpansion& Root() const { return list_.front(); }

 private:
  /**
   * @brief 格納データの本体。
   *
   * @note `std::vector` のほうが若干高速に動作すると思われるが、
   *       `LocalExpansion` のような move 不可オブジェクトには用いることができない。
   */
  std::deque<LocalExpansion> list_;
};
}  // namespace komori

#endif  // KOMORI_EXPANSION_STACK_HPP_
