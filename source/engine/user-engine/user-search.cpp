#include <cmath>
#include <condition_variable>
#include <mutex>

#include "../../extra/all.h"

#include "komoring_heights.hpp"
#include "path_keys.hpp"
#include "typedefs.hpp"

#if defined(USER_ENGINE)

namespace {
komori::KomoringHeights g_searcher;
komori::EngineOption g_option;
std::atomic_bool g_path_key_init_flag;

// <探索終了同期>
bool g_search_end = false;
std::mutex g_end_mtx;
std::condition_variable g_end_cv;
// </探索終了同期>

komori::NodeState g_search_result = komori::NodeState::kUnknown;

/// 局面が OR node っぽいかどうかを調べる。困ったら OR node として処理する。
bool IsPosOrNode(const Position& root_pos) {
  Color us = root_pos.side_to_move();
  Color them = ~us;

  if (root_pos.king_square(us) == SQ_NB) {
    return true;
  } else if (root_pos.king_square(them) == SQ_NB) {
    return false;
  }

  if (root_pos.in_check() && g_option.root_is_and_node_if_checked) {
    return false;
  }
  return true;
}

enum class LoseKind {
  kTimeout,
  kNoMate,
  kMate,
};

void PrintResult(bool is_mate_search, LoseKind kind, const std::string& pv_moves = "resign") {
  if (is_mate_search) {
    switch (kind) {
      case LoseKind::kTimeout:
        sync_cout << "checkmate timeout" << sync_endl;
        break;
      case LoseKind::kNoMate:
        sync_cout << "checkmate nomate" << sync_endl;
        break;
      default:
        sync_cout << "checkmate " << pv_moves << sync_endl;
    }
  } else {
    auto usi_output = g_searcher.CurrentInfo();
    usi_output.Set(komori::UsiInfoKey::kDepth, 0);
    usi_output.Set(komori::UsiInfoKey::kPv, pv_moves);
    sync_cout << usi_output << sync_endl;
  }
}

void ShowCommand(Position& pos, std::istringstream& is) {
  // unimplemented
}

void PvCommand(Position& pos, std::istringstream& /* is */) {
  // unimplemented
}

void WaitSearchEnd() {
  Timer timer;
  timer.reset();

  const bool is_mate_search = Search::Limits.mate != 0;
  const auto is_end = [&]() {
    return Threads.stop || g_search_end || (is_mate_search && timer.elapsed() >= Search::Limits.mate);
  };
  constexpr TimePoint kTimePointMax = std::numeric_limits<TimePoint>::max();
  const TimePoint pv_interval =
      g_option.pv_interval > kTimePointMax ? kTimePointMax : static_cast<TimePoint>(g_option.pv_interval);

  TimePoint next_pv_out = pv_interval;
  std::unique_lock<std::mutex> lock(g_end_mtx);
  while (!is_end()) {
    auto sleep_duration = 100;
    if (next_pv_out < timer.elapsed() + sleep_duration) {
      // このまま sleep_duration だけ寝ると予定時刻を過ぎてしまう
      if (next_pv_out > timer.elapsed()) {
        sleep_duration = next_pv_out - timer.elapsed();
      } else {
        sleep_duration = 1;
      }
    }

    g_end_cv.wait_for(lock, std::chrono::milliseconds(sleep_duration), is_end);
    if (pv_interval > 0 && timer.elapsed() >= next_pv_out) {
      g_searcher.RequestPrint();
      next_pv_out = timer.elapsed() + pv_interval;
    }
  }
}
}  // namespace

// USI拡張コマンド"user"が送られてくるとこの関数が呼び出される。実験に使ってください。
void user_test(Position& pos, std::istringstream& is) {
  std::string cmd;
  is >> cmd;
  if (cmd == "show") {
    ShowCommand(pos, is);
  } else if (cmd == "pv") {
    PvCommand(pos, is);
  }
}

// USIに追加オプションを設定したいときは、この関数を定義すること。
// USI::init()のなかからコールバックされる。
void USI::extra_option(USI::OptionsMap& o) {
  komori::EngineOption::Init(o);
}

// 起動時に呼び出される。時間のかからない探索関係の初期化処理はここに書くこと。
void Search::init() {}

// isreadyコマンドの応答中に呼び出される。時間のかかる処理はここに書くこと。
void Search::clear() {
  if (!g_path_key_init_flag) {
    g_path_key_init_flag = true;
    komori::PathKeyInit();
  }
  g_option.Reload(Options);

#if defined(USE_DEEP_DFPN)
  auto d = g_option.deep_dfpn_d_;
  auto e = g_option.deep_dfpn_e_;
  komori::DeepDfpnInit(d, e);
#endif  // defined(USE_DEEP_DFPN)

  g_searcher.Init(g_option, Threads.main());
}

// 探索開始時に呼び出される。
// この関数内で初期化を終わらせ、slaveスレッドを起動してThread::search()を呼び出す。
// そのあとslaveスレッドを終了させ、ベストな指し手を返すこと。
void MainThread::search() {
  // `go mate` で探索開始したときは true、`go` で探索開始したときは false
  bool is_mate_search = Search::Limits.mate != 0;
  bool is_root_or_node = IsPosOrNode(rootPos);

  g_searcher.ResetStop();
  g_search_end = false;
  // thread が 2 つ以上使える場合、main thread ではない方をタイマースレッドとして使いたい
  if (Threads.size() > 1) {
    Threads[1]->start_searching();

    g_search_result = g_searcher.Search(rootPos, is_root_or_node);
    {
      std::lock_guard<std::mutex> lock(g_end_mtx);
      g_search_end = true;
    }
    g_end_cv.notify_one();

    Threads[1]->wait_for_search_finished();
  } else {
    // thread が 1 つしか使いない場合、しれっと thread を起動してタイマー役をしてもらう
    std::thread th{[this]() { Thread::search(); }};

    g_search_result = g_searcher.Search(rootPos, is_root_or_node);
    {
      std::lock_guard<std::mutex> lock(g_end_mtx);
      g_search_end = true;
    }
    g_end_cv.notify_one();

    th.join();
  }

  Move best_move = MOVE_NONE;
  if (g_search_result == komori::NodeState::kProven) {
    auto best_moves = g_searcher.BestMoves();
    std::ostringstream oss;
    for (const auto& move : best_moves) {
      oss << move << " ";
    }
    PrintResult(is_mate_search, LoseKind::kMate, oss.str());

    if (!best_moves.empty()) {
      best_move = best_moves[0];
    }
  } else {
    if (g_search_result == komori::NodeState::kDisproven || g_search_result == komori::NodeState::kRepetition) {
      PrintResult(is_mate_search, LoseKind::kNoMate);
    } else {
      PrintResult(is_mate_search, LoseKind::kTimeout);
    }
  }

  // 通常の go コマンドで呼ばれたときは resign を返す
  if (Search::Limits.mate == 0) {
    // "go infinite"に対してはstopが送られてくるまで待つ。
    while (!Threads.stop && Search::Limits.infinite) {
      Tools::sleep(1);
    }
    if (best_move == MOVE_NONE) {
      sync_cout << "bestmove resign" << sync_endl;
    } else {
      sync_cout << "bestmove " << best_move << sync_endl;
    }
    return;
  }
}

// 探索本体。並列化している場合、ここがslaveのエントリーポイント。
void Thread::search() {
  WaitSearchEnd();
  g_searcher.SetStop();
}

#endif  // USER_ENGINE
