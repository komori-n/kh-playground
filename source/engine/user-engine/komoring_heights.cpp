#include "komoring_heights.hpp"

#include <fstream>
#include <regex>

namespace komori {
namespace {
inline std::uint64_t GcInterval(std::uint64_t hash_mb) {
  const std::uint64_t entry_num = hash_mb * 1024 * 1024 / sizeof(tt::Entry);

  return entry_num / 2 * 3;
}

/**
 * @brief "name (1).bin" のようにファイル名がかぶらないような番号を付与する
 * @param path 書き込みたいファイルへのパス
 * @return ファイルパス
 *
 * 既存のファイルと衝突しなくなるまで、以下の要領でパスを巡回する。
 *
 * - "name" --> "name (1)" --> "name (2)" --> ...
 * - "hoge/name.bin" --> "hoge/name (1).bin" --> "hoge/name (2).bin" --> ...
 */
inline std::filesystem::path GetNoOverwritePath(std::filesystem::path path) {
  while (std::filesystem::exists(path)) {
    std::string filename = path.stem();
    const std::string ext = path.extension();
    const std::regex reg{R"((.* )\((\d+)\))"};
    std::smatch match;

    if (std::regex_match(filename, match, reg)) {
      auto num = stoi(match[2].str());
      filename = match[1].str();
      filename += "(";
      filename += std::to_string(num + 1);
      filename += ")";
      filename += ext;
    } else {
      filename += " (1)";
      filename += ext;
    }
    path.replace_filename(filename);
  }

  return path;
}
}  // namespace

namespace detail {
void SearchMonitor::NewSearch(std::uint64_t gc_interval) {
  start_time_ = std::chrono::system_clock::now();
  depth_ = 0;

  tp_hist_.Clear();
  mc_hist_.Clear();
  hist_idx_ = 0;

  move_limit_ = std::numeric_limits<std::uint64_t>::max();
  limit_stack_ = {};

  gc_interval_ = gc_interval;
  ResetNextGc();
}

void SearchMonitor::Tick() {
  tp_hist_[hist_idx_] = std::chrono::system_clock::now();
  mc_hist_[hist_idx_] = MoveCount();
  hist_idx_++;
}

UsiInfo SearchMonitor::GetInfo() const {
  auto curr_time = std::chrono::system_clock::now();
  auto time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(curr_time - start_time_).count();

  const auto move_count = MoveCount();
  std::uint64_t nps = 0;
  if (hist_idx_ >= kHistLen) {
    const auto tp = tp_hist_[hist_idx_];
    const auto mc = mc_hist_[hist_idx_];
    const auto tp_diff = std::chrono::duration_cast<std::chrono::milliseconds>(curr_time - tp).count();
    nps = (move_count - mc) * 1000ULL / tp_diff;
  } else {
    if (time_ms > 0) {
      nps = move_count * 1000ULL / time_ms;
    }
  }

  UsiInfo output;
  output.Set(UsiInfoKey::kSelDepth, depth_);
  output.Set(UsiInfoKey::kTime, time_ms);
  output.Set(UsiInfoKey::kNodes, move_count);
  output.Set(UsiInfoKey::kNps, nps);

  return output;
}

void SearchMonitor::ResetNextGc() {
  next_gc_count_ = MoveCount() + gc_interval_;
}

void SearchMonitor::PushLimit(std::uint64_t move_limit) {
  limit_stack_.push(move_limit_);
  move_limit_ = std::min(move_limit_, move_limit);
}

void SearchMonitor::PopLimit() {
  if (!limit_stack_.empty()) {
    move_limit_ = limit_stack_.top();
    limit_stack_.pop();
  }
}
}  // namespace detail

void KomoringHeights::Init(const EngineOption& option, Thread* thread) {
  option_ = option;
  tt_.Resize(option_.hash_mb);
  monitor_.Init(thread);

  const auto& tt_read_path = option_.tt_read_path;
  if (!tt_read_path.empty() && std::filesystem::exists(tt_read_path)) {
    std::ifstream ifs(tt_read_path, std::ios::binary);
    if (ifs) {
      sync_cout << "info string load_path: " << tt_read_path << sync_endl;
      tt_.Load(ifs);
    }
  }
}

UsiInfo KomoringHeights::CurrentInfo() const {
  UsiInfo usi_output = monitor_.GetInfo();
  usi_output.Set(UsiInfoKey::kHashfull, tt_.Hashfull());
  usi_output.Set(UsiInfoKey::kScore, score_.ToString());

  return usi_output;
}

NodeState KomoringHeights::Search(const Position& n, bool is_root_or_node) {
  // <初期化>
  tt_.NewSearch();
  monitor_.NewSearch(GcInterval(option_.hash_mb));
  monitor_.PushLimit(option_.nodes_limit);
  best_moves_.clear();
  // </初期化>

  auto& nn = const_cast<Position&>(n);
  Node node{nn, is_root_or_node};

  auto [state, len] = SearchMainLoop(node, is_root_or_node);
  const bool proven = (state == NodeState::kProven);

  auto tt_write_path = option_.tt_write_path;
  if (!tt_write_path.empty()) {
    if (option_.tt_no_overwrite) {
      tt_write_path = GetNoOverwritePath(tt_write_path);
    }
    std::ofstream ofs(tt_write_path, std::ios::binary);
    if (ofs) {
      sync_cout << "info string save_path: " << tt_write_path << sync_endl;
      tt_.Save(ofs);
    }
  }

  if (proven) {
    if (best_moves_.size() % 2 != static_cast<int>(is_root_or_node)) {
      sync_cout << "info string Failed to detect PV" << sync_endl;
    }
    return NodeState::kProven;
  } else {
    return NodeState::kDisproven;
  }
}

std::pair<NodeState, MateLen> KomoringHeights::SearchMainLoop(Node& n, bool is_root_or_node) {
  auto node_state{NodeState::kUnknown};
  auto len{kDepthMaxMateLen};

  for (int i = 0; i < 128; ++i) {
    // `result` が余詰探索による不詰だったとき、後から元の状態（詰み）に戻せるようにしておく
    const auto old_score = score_;
    const auto result = SearchEntry(n, len);
    score_ = Score::Make(option_.score_method, result, is_root_or_node);

    auto info = CurrentInfo();

    if (result.Pn() == 0) {
      // len 以下の手数の詰みが帰ってくるはず
      KOMORI_PRECONDITION(result.Len().Len() <= len.Len());
      best_moves_ = GetMatePath(n, result.Len());

      sync_cout << info << "# " << OrdinalNumber(i + 1) << " result: mate in " << best_moves_.size()
                << "(upper_bound:" << result.Len() << ")" << sync_endl;
      std::ostringstream oss;
      for (const auto move : best_moves_) {
        oss << move << " ";
      }
      info.Set(UsiInfoKey::kPv, oss.str());
      sync_cout << info << sync_endl;
      node_state = NodeState::kProven;
      if (result.Len().Len() <= 1) {
        break;
      }

      if (option_.post_search_level == PostSearchLevel::kNone ||
          (option_.post_search_level == PostSearchLevel::kUpperBound && best_moves_.size() == result.Len().Len())) {
        break;
      }

      len = result.Len() - 2;
    } else {
      sync_cout << info << "# " << OrdinalNumber(i + 1) << " result: " << result << sync_endl;
      if (result.Dn() == 0 && result.Len() < len) {
        sync_cout << info << "Failed to detect PV" << sync_endl;
      }
      if (node_state == NodeState::kProven) {
        len = len + 2;
        score_ = old_score;
        if (best_moves_.size() != len.Len()) {
          best_moves_ = GetMatePath(n, len);
        }
      }
      break;
    }
  }

  return {node_state, len};
}

SearchResult KomoringHeights::SearchEntry(Node& n, MateLen len) {
  SearchResult result{};
  PnDn thpn = (len == kDepthMaxMateLen) ? 1 : kInfinitePnDn;
  PnDn thdn = (len == kDepthMaxMateLen) ? 1 : kInfinitePnDn;

  expansion_list_.Emplace(tt_, n, len, true);
  while (!monitor_.ShouldStop()) {
    result = SearchImpl(n, thpn, thdn, len, false);
    if (result.IsFinal()) {
      break;
    }

    if (result.Pn() >= kInfinitePnDn || result.Dn() >= kInfinitePnDn) {
      auto info = CurrentInfo();
      sync_cout << info << "error: " << (result.Pn() >= kInfinitePnDn ? "pn" : "dn") << " overflow detected"
                << sync_endl;
      break;
    }

    score_ = Score::Make(option_.score_method, result, n.IsRootOrNode());
    // 反復深化のしきい値を適当に伸ばす
    thpn = Clamp(thpn, 2 * result.Pn(), kInfinitePnDn);
    thdn = Clamp(thdn, 2 * result.Dn(), kInfinitePnDn);
  }
  expansion_list_.Pop();

  auto query = tt_.BuildQuery(n);
  query.SetResult(result);

  return result;
}

SearchResult KomoringHeights::SearchImpl(Node& n, PnDn thpn, PnDn thdn, MateLen len, bool inc_flag) {
  auto& local_expansion = expansion_list_.Current();
  monitor_.Visit(n.GetDepth());
  PrintIfNeeded(n);

  expansion_list_.EliminateDoubleCount(tt_, n);

  // 必要があれば TCA による探索延長をしたいので、このタイミングで現局面の pn/dn を取得する。
  auto curr_result = local_expansion.CurrentResult(n);
  // Threshold Controlling Algorithm(TCA).
  // 浅い結果を参照している場合、無限ループになる可能性があるので少しだけ探索を延長する
  inc_flag = inc_flag || local_expansion.DoesHaveOldChild();
  if (inc_flag && !curr_result.IsFinal()) {
    if (curr_result.Pn() < kInfinitePnDn) {
      thpn = Clamp(thpn, curr_result.Pn() + 1);
    }

    if (curr_result.Dn() < kInfinitePnDn) {
      thdn = Clamp(thdn, curr_result.Dn() + 1);
    }
  }

  if (n.GetDepth() > 0 && monitor_.ShouldGc()) {
    tt_.CollectGarbage();
    tt_.CompactEntries();
    monitor_.ResetNextGc();
  }

  while (!monitor_.ShouldStop() && (curr_result.Pn() < thpn && curr_result.Dn() < thdn)) {
    // local_expansion.BestMove() にしたがい子局面を展開する
    // （curr_result.Pn() > 0 && curr_result.Dn() > 0 なので、BestMove が必ず存在する）
    const auto best_move = local_expansion.BestMove();
    const bool is_first_search = local_expansion.FrontIsFirstVisit();
    const BitSet64 sum_mask = local_expansion.FrontSumMask();
    const auto [child_thpn, child_thdn] = local_expansion.PnDnThresholds(thpn, thdn);

    n.DoMove(best_move);

    // 子局面を展開する。展開した expansion は UndoMove() の直前に忘れずに開放しなければならない。
    auto& child_expansion = expansion_list_.Emplace(tt_, n, len - 1, is_first_search, sum_mask);

    SearchResult child_result;
    if (is_first_search) {
      child_result = child_expansion.CurrentResult(n);
      // 新規局面を展開したので、TCA による探索延長をこれ以上続ける必要はない
      inc_flag = false;

      // 子局面を初展開する場合、child_result を計算した時点で threshold を超過する可能性がある
      // しかし、SearchImpl をコールしてしまうと TCA の探索延長によりすぐに返ってこない可能性がある
      // ゆえに、この時点で Exceed している場合は SearchImpl を呼ばないようにする。
      if (child_result.Pn() >= child_thpn || child_result.Dn() >= child_thdn) {
        goto CHILD_SEARCH_END;
      }
    }
    child_result = SearchImpl(n, child_thpn, child_thdn, len - 1, inc_flag);

  CHILD_SEARCH_END:
    expansion_list_.Pop();
    n.UndoMove();

    local_expansion.UpdateBestChild(child_result, n.GetBoardKeyHandPair());
    curr_result = local_expansion.CurrentResult(n);
  }

  return curr_result;
}

std::vector<Move> KomoringHeights::GetMatePath(Node& n, MateLen len) {
  std::vector<Move> best_moves;
  while (len.Len() > 0) {
    // 1手詰はTTに書かれていない可能性があるので先にチェックする
    const auto [move, hand] = CheckMate1Ply(n);
    if (move != MOVE_NONE) {
      best_moves.push_back(move);
      n.DoMove(move);
      break;
    }

    const auto result = SearchEntry(n, len);
    if (result.Pn() != 0) {
      // `n` は詰みのはずなのに探索で詰みを示せなかった。このような現象は、余詰め探索に千日手が絡んでいるケースで
      // しばしば発生する。千日手テーブルだけ消去して探索し直すことでこれを回避できる。
      tt_.NewSearch();
      SearchEntry(n, len);
    }

    // 子ノードの中から最善っぽい手を選ぶ
    Move best_move = MOVE_NONE;
    MateLen best_len = n.IsOrNode() ? kDepthMaxMateLen : kZeroMateLen;
    MateLen best_disproven_len = kZeroMateLen;
    for (const auto move : MovePicker{n}) {
      const auto query = tt_.BuildChildQuery(n, move.move);
      const auto [disproven_len, proven_len] = query.FinalRange();
      if (n.IsOrNode() && proven_len < best_len) {
        best_move = move.move;
        best_len = proven_len;
        best_disproven_len = disproven_len;
      } else if (!n.IsOrNode()) {
        if (proven_len > best_len || (proven_len == best_len && best_disproven_len < disproven_len)) {
          best_move = move.move;
          best_len = proven_len;
          best_disproven_len = disproven_len;
        }
      }
    }

    if (best_move == MOVE_NONE) {
      break;
    }

    len = len - 1;
    n.DoMove(best_move);
    best_moves.push_back(best_move);
  }

  RollBack(n, best_moves);
  return best_moves;
}

void KomoringHeights::PrintIfNeeded(const Node& n) {
  if (!print_flag_) {
    return;
  }
  print_flag_ = false;

  auto usi_output = CurrentInfo();
  usi_output.Set(UsiInfoKey::kDepth, n.GetDepth());
#if defined(KEEP_LAST_MOVE)
  const auto moves = n.Pos().moves_from_start();
  usi_output.Set(UsiInfoKey::kPv, moves);
  if (const auto p = moves.find_first_of(' '); p != std::string::npos) {
    const auto best_move = moves.substr(0, p);
    usi_output.Set(UsiInfoKey::kCurrMove, best_move);
  }
#endif

  sync_cout << usi_output << sync_endl;

  monitor_.Tick();
}
}  // namespace komori
