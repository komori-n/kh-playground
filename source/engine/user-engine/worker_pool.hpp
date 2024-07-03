/**
 * @file thread_pool.hpp
 */

#ifndef KOMORI_WORKER_POOL_HPP_
#define KOMORI_WORKER_POOL_HPP_

#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>

#include "node.hpp"

namespace komori {

/**
 * @brief 複数のスレッドで探索を行うためのクラス
 *
 * Sub Thread （メインスレッド以外のスレッド）のタスク管理に用いる。スレッド生成はやねうら王側でやってくれるが、
 * そのスレッドが何をするかはこのクラスが管理する。Sub Thread は探索開始早々 WorkerPool::Work() を呼び出し、
 * メインスレッドから渡される Task に従い探索を行う。
 *
 * ノードコピーを削減するために、`Work()` では `Node&` を引数に取る。このとき、引数の局面は探索開始局面である。
 * Task では、これを引数に取り、その局面から探索を行う。次回の探索に影響を出さないようにするために、
 * Task 終了時は必ず Task 開始時の局面に戻すこと。
 */
class WorkerPool {
  /// タスクの型。引数には探索開始局面が渡される。タスク終了時は必ず Task 開始時の局面に戻すこと。
  using Task = std::function<void(Node& n)>;

 public:
  /**
   * @brief サブスレッドをworkerとして合流させる
   * @param n 現局面
   *
   * `task_queue_` が空になるまで、タスクを処理し続ける。
   */
  void Work(Node& n) {
    while (!stop_.load(std::memory_order_acquire)) {
      Task task;
      {
        std::unique_lock lock{mutex_};
        cv_.wait(lock, [this] { return !task_queue_.empty() || stop_.load(std::memory_order_acquire); });
        if (stop_.load(std::memory_order_acquire)) {
          return;
        }
        task = std::move(task_queue_.front());
        task_queue_.pop();
      }
      task(n);
    }
  }
  /// タスクを追加する
  void AddTask(const Task& task) {
    {
      std::lock_guard lock{mutex_};
      task_queue_.push(task);
    }
    cv_.notify_one();
  }
  /// 全スレッドを停止する
  void Stop() {
    stop_.store(true, std::memory_order_release);
    cv_.notify_all();
  }
  /// タスクキューをリセットする
  void Reset() {
    std::lock_guard lock{mutex_};
    task_queue_ = std::queue<Task>();
    stop_.store(false, std::memory_order_release);
  }

 private:
  std::mutex mutex_;             ///< タスクキューの排他制御
  std::condition_variable cv_;   ///< スレッドを起こすための条件変数
  std::queue<Task> task_queue_;  ///< タスクキュー

  std::atomic<bool> stop_{false};  ///< スレッド停止フラグ
};
}  // namespace komori

#endif  // KOMORI_WORKER_POOL_HPP_
