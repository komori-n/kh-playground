#ifndef KOMORI_TEST_LIB_HPP_
#define KOMORI_TEST_LIB_HPP_

#include <memory>
#include <string>
#include <thread>

#include "../../../thread.h"
#include "../move_picker.hpp"
#include "../node.hpp"
#include "../typedefs.hpp"

using komori::Barrier;

template <typename... Tasks>
inline bool ParallelExecute(std::chrono::milliseconds time_limit, Tasks&&... tasks) {
  std::atomic<std::size_t> num_finished{};
  std::thread threads[] = {std::thread([&]() {
    std::forward<Tasks>(tasks)();
    num_finished++;
  })...};

  const auto end_tp = std::chrono::steady_clock::now() + time_limit;
  do {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < end_tp && num_finished < sizeof...(Tasks));

  const bool all_finished = num_finished == sizeof...(Tasks);
  for (auto& t : threads) {
    if (t.joinable()) {
      if (all_finished) {
        t.join();
      } else {
        t.detach();
      }
    }
  }

  return all_finished;
}

struct TestNode {
  TestNode(const std::string& sfen, bool root_is_or_node) {
    p_.set(sfen, &si_, Threads[0]);
    n_ = std::make_unique<komori::Node>(p_, root_is_or_node, 33, 4);
    mp_ = std::make_unique<komori::MovePicker>(*n_);
  }

  komori::Node* operator->() { return &*n_; }
  komori::Node& operator*() { return *n_; }

  Position& Pos() { return n_->Pos(); }
  const Position& Pos() const { return n_->Pos(); }

  komori::MovePicker& MovePicker() { return *mp_; }

 private:
  Position p_;
  StateInfo si_;
  std::unique_ptr<komori::Node> n_;
  std::unique_ptr<komori::MovePicker> mp_;
};

template <PieceType... Pts>
inline constexpr Hand MakeHand() {
  PieceType pts[sizeof...(Pts)]{Pts...};
  Hand hand = HAND_ZERO;

  for (const auto& pt : pts) {
    add_hand(hand, pt);
  }
  return hand;
}

#endif  // KOMORI_TEST_LIB_HPP_
