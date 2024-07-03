#include "../worker_pool.hpp"
#include "test_lib.hpp"

#include <gtest/gtest.h>

using komori::Node;
using komori::WorkerPool;

TEST(WorkerPoolTest, Work) {
  using std::chrono::operator""ms;

  TestNode n{"startpos", true};
  WorkerPool pool;
  std::thread th{[&pool, &n] { pool.Work(*n); }};

  const std::string expected_sfen = n->Pos().sfen();
  std::string result_sfen;
  pool.AddTask([&result_sfen](Node& n) { result_sfen = n.Pos().sfen(); });
  std::this_thread::sleep_for(10ms);
  pool.Stop();
  th.join();

  EXPECT_EQ(expected_sfen, result_sfen);
}
