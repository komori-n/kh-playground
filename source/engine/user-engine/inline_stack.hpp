/**
 * @file inline_stack.hpp
 */
#ifndef KOMORI_INLINE_STACK_HPP_
#define KOMORI_INLINE_STACK_HPP_

#include <array>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>
#include "type_traits.hpp"

namespace komori {
/**
 * @brief サイズ固定のスタック。
 * @tparam T 保存する要素の型
 * @tparam kSize 保存可能な添字の最大個数（`kSize`>0）
 *
 * `Push()` および `Pop()` により要素を追加および削除ができるスタック。動的メモリ確保は行わず、`std::array` にて
 * 実装されている。
 *
 * スタックは配列の手前から順に詰める形で実現されている。スタックに積まれた要素は `operator[]` で
 * アクセスすることができ、その添字は古い順に 0, 1, ... と振られている。同様に、イテレータ (`begin()` `end()`) により
 * 古い順にイテレートすることができる。
 *
 * @note テンプレートパラメータ `kSize` でサイズの上限を指定できるが、高速化のために範囲チェックは一切行っていない。
 */
template <typename T, std::size_t kSize>
class InlineStack {
 public:
  static_assert(kSize > 0, "kSize shall be greater than 0");

  InlineStack() = default;
  InlineStack(std::initializer_list<T> init) {
    for (auto&& val : init) {
      Emplace(std::move(val));
    }
  }
  InlineStack(const InlineStack&) = default;
  InlineStack(InlineStack&&) noexcept = default;
  InlineStack& operator=(const InlineStack&) = default;
  InlineStack& operator=(InlineStack&&) noexcept = default;
  ~InlineStack() { clear(); }

  template <bool kIsConst>
  class Iterator {
   private:
    using parent = std::conditional_t<kIsConst, const InlineStack, InlineStack>;

   public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::conditional_t<kIsConst, const T, T>;
    using pointer = std::conditional_t<kIsConst, const T*, T*>;
    using reference = std::conditional_t<kIsConst, const T&, T>&;
    using iterator_category = std::random_access_iterator_tag;

    Iterator() = default;
    Iterator(std::reference_wrapper<parent> stack, std::uint32_t index) : stack_{stack}, index_{index} {}
    Iterator(const Iterator&) = default;
    Iterator(Iterator&&) noexcept = default;
    Iterator& operator=(const Iterator&) = default;
    Iterator& operator=(Iterator&&) noexcept = default;
    ~Iterator() = default;

    Iterator& operator++() {
      ++index_;
      return *this;
    }
    Iterator operator++(int) {
      const auto tmp = *this;
      ++index_;
      return tmp;
    }
    Iterator& operator--() {
      --index_;
      return *this;
    }
    Iterator operator--(int) {
      const auto tmp = *this;
      --index_;
      return tmp;
    }

    reference operator*() { return stack_.get()[index_]; }
    pointer operator->() const { return &stack_.get()[index_]; }
    reference operator[](difference_type n) const { return stack_.get()[index_ + n]; }
    friend difference_type operator-(const Iterator& lhs, const Iterator& rhs) { return lhs.index_ - rhs.index_; }
    friend Iterator& operator+=(Iterator& lhs, difference_type rhs) {
      lhs.index_ += rhs;
      return lhs;
    }
    friend Iterator operator+(const Iterator& lhs, difference_type rhs) {
      return Iterator(lhs.stack_.get(), lhs.index_ + rhs);
    }
    friend Iterator operator+(difference_type lhs, const Iterator& rhs) {
      return Iterator(rhs.stack_.get(), lhs + rhs.index_);
    }
    friend Iterator& operator-=(Iterator& lhs, difference_type rhs) {
      lhs.index_ -= rhs;
      return lhs;
    }
    friend Iterator operator-(const Iterator& lhs, difference_type rhs) {
      return Iterator(lhs.stack_.get(), lhs.index_ - rhs);
    }
    friend bool operator==(const Iterator& lhs, const Iterator& rhs) { return lhs.index_ == rhs.index_; }
    friend bool operator!=(const Iterator& lhs, const Iterator& rhs) { return lhs.index_ != rhs.index_; }
    friend bool operator<(const Iterator& lhs, const Iterator& rhs) { return lhs.index_ < rhs.index_; }
    friend bool operator>(const Iterator& lhs, const Iterator& rhs) { return lhs.index_ > rhs.index_; }
    friend bool operator<=(const Iterator& lhs, const Iterator& rhs) { return lhs.index_ <= rhs.index_; }
    friend bool operator>=(const Iterator& lhs, const Iterator& rhs) { return lhs.index_ >= rhs.index_; }

   private:
    std::reference_wrapper<parent> stack_;
    std::uint32_t index_;
  };

  /// `val` をスタックに追加する
  std::uint32_t Push(T val) {
    const auto i = len_++;

    new (&data_[i].bytes) T(std::move(val));
    return i;
  }
  /// `args` を使ってスタックに要素を追加する
  template <typename... Args>
  std::uint32_t Emplace(Args&&... args) {
    const auto i = len_++;

    new (&data_[i].bytes) T(std::forward<Args>(args)...);
    return i;
  }
  /// スタックから要素を1つ削除する
  void Pop() {
    --len_;
    reinterpret_cast<T*>(&data_[len_].bytes)->~T();
  }

  auto begin() { return Iterator<false>(*this, 0); }
  auto begin() const { return Iterator<true>(*this, 0); }
  auto cbegin() const { return Iterator<true>(*this, 0); }
  auto end() { return Iterator<false>(*this, len_); }
  auto end() const { return Iterator<true>(*this, len_); }
  auto cend() const { return Iterator<true>(*this, len_); }
  T& front() { return *std::launder(reinterpret_cast<T*>(&data_[0].bytes)); }
  const T& front() const { return *std::launder(reinterpret_cast<const T*>(&data_[0].bytes)); }
  T& back() { return *std::launder(reinterpret_cast<T*>(&data_[len_ - 1].bytes)); }
  const T& back() const { return *std::launder(reinterpret_cast<const T*>(&data_[len_ - 1].bytes)); }
  std::uint32_t size() const { return len_; }
  std::uint32_t max_size() const { return kSize; }
  bool empty() const { return len_ == 0; }
  T& operator[](std::uint32_t i) { return *std::launder(reinterpret_cast<T*>(&data_[i].bytes)); }
  const T& operator[](std::uint32_t i) const { return *std::launder(reinterpret_cast<const T*>(&data_[i].bytes)); }

  /// スタックを空にする
  void clear() {
    while (!empty()) {
      Pop();
    }
  }

 private:
  struct alignas(alignof(T)) Data {
    std::byte bytes[sizeof(T)];
  };

  std::array<Data, kSize> data_;  ///< スタックを保存する領域
  std::uint32_t len_{0};          ///< スタックに現在格納されている要素数
};
}  // namespace komori

#endif  // KOMORI_INLINE_STACK_HPP_
