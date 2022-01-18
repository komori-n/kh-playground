#include "transposition_table.hpp"

#include "hands.hpp"
#include "node.hpp"
#include "path_keys.hpp"

namespace komori {
namespace {
/// ハッシュ使用率を計算するために見るエントリの数。
constexpr std::size_t kHashfullCalcEntries = 10000;
/// USI_Hash のうちどの程度を NormalTable に使用するかを示す割合
constexpr double kNormalRepetitionRatio = 0.95;

/// state の内容に沿って補正した amount を返す。例えば、ProvenState の Amount を大きくしたりする。
inline SearchedAmount GetAdjustedAmount(NodeState state, SearchedAmount amount) {
  // proven state は他のノードと比べて 10 倍探索したことにする（GCで消されづらくする）
  constexpr SearchedAmount kProvenAmountIncrease = 10;

  if (state == NodeState::kProvenState) {
    // 単純に掛け算をするとオーバーフローする可能性があるので、範囲チェックをする
    if (amount >= std::numeric_limits<SearchedAmount>::max() / kProvenAmountIncrease) {
      amount = std::numeric_limits<SearchedAmount>::max();
    } else {
      amount *= kProvenAmountIncrease;
    }
  }
  return amount;
}
}  // namespace

template <bool kProven>
CommonEntry* BoardCluster::SetFinal(Hand hand, Move16 move, MateLen mate_len, SearchedAmount amount) const {
  std::uint32_t hash_high = hash_high_;
  CommonEntry* ret = nullptr;

  for (auto& entry : *this) {
    if (entry.IsNull() || entry.HashHigh() != hash_high) {
      continue;
    }

    if ((kProven && entry.UpdateWithProofHand(hand)) || (!kProven && entry.UpdateWithDisproofHand(hand))) {
      // entry はもういらない
      entry.Clear();
    } else {
      if (ret != nullptr) {
        // hand を格納済みなら以下の保存処理をスキップ
        // ret=&entry の行で return していないのは、不必要になったエントリをすべて Clear() したいから。
        continue;
      }

      if constexpr (kProven) {
        if (auto proven = entry.TryGetProven(); proven != nullptr && !proven->IsFull()) {
          // 証明済局面に空きがあるならそこに証明駒を書く
          proven->Add(hand, move, mate_len);
          entry.UpdateSearchedAmount(amount);
          ret = &entry;
        }
      } else {
        if (auto disproven = entry.TryGetDisproven(); disproven != nullptr && !disproven->IsFull()) {
          disproven->Add(hand, move, mate_len);
          entry.UpdateSearchedAmount(amount);
          ret = &entry;
        }
      }
    }
  }

  if (ret != nullptr) {
    return ret;
  }

  return Add({hash_high, amount, HandsData<kProven>{hand, move, mate_len}});
}

CommonEntry* BoardCluster::Add(CommonEntry&& entry) const {
  CommonEntry* removed_entry = nullptr;
  SearchedAmount removed_amount = std::numeric_limits<SearchedAmount>::max();

  for (auto& e : *this) {
    if (e.IsNull()) {
      e = std::move(entry);
      return &e;
    }

    auto amount = GetAdjustedAmount(e.GetNodeState(), e.GetSearchedAmount());
    if (removed_amount > amount) {
      removed_amount = amount;
      removed_entry = &e;
    }
  }

  // 空きエントリが見つからなかったので、いちばんいらなさそうなエントリを上書きする
  *removed_entry = std::move(entry);
  return removed_entry;
}

void LookUpQuery::SetResult(const SearchResult& result) {
  auto amount = result.GetSearchedAmount();
  switch (result.GetNodeState()) {
    case NodeState::kProvenState:
      SetProven(result.ProperHand(), result.BestMove(), result.GetMateLen(), amount);
      break;
    case NodeState::kDisprovenState:
      SetDisproven(result.ProperHand(), result.BestMove(), result.GetMateLen(), amount);
      break;
    case NodeState::kRepetitionState:
      SetRepetition(amount);
      break;
    default:
      auto entry = LookUpWithCreation();
      entry->UpdatePnDn(result.Pn(), result.Dn(), amount);
  }
}

void LookUpQuery::SetRepetition(SearchedAmount amount) {
  LookUpWithCreation();
  if (entry_->GetNodeState() == NodeState::kOtherState) {
    entry_->SetMaybeRepetition();
  }

  rep_table_->Insert(path_key_);
  entry_ = const_cast<CommonEntry*>(&kRepetitionEntry);
}

TranspositionTable::TranspositionTable(int gc_hashfull) : gc_hashfull_{gc_hashfull} {};

void TranspositionTable::Resize(std::uint64_t hash_size_mb) {
  std::uint64_t new_bytes = hash_size_mb * 1024 * 1024;
  std::uint64_t normal_bytes = static_cast<std::uint64_t>(static_cast<double>(new_bytes) * kNormalRepetitionRatio);
  std::uint64_t rep_bytes = static_cast<std::uint64_t>(static_cast<double>(new_bytes) * (1.0 - kNormalRepetitionRatio));
  // board_cluster 分だけ少し余分にメモリ確保が必要
  std::uint64_t new_num_entries =
      std::max(static_cast<std::uint64_t>(BoardCluster::kClusterSize + 1), normal_bytes / sizeof(CommonEntry));
  if (tt_.size() == new_num_entries) {
    return;
  }

  tt_.resize(new_num_entries);
  tt_.shrink_to_fit();
  entry_mod_ = new_num_entries - BoardCluster::kClusterSize;

  std::uint64_t rep_entry_max = rep_bytes / sizeof(Key);
  rep_table_.SetTableSizeMax(rep_entry_max);

  NewSearch();
}

void TranspositionTable::NewSearch() {
  for (auto& entry : tt_) {
    entry.Clear();
  }
  rep_table_.Clear();
}

std::size_t TranspositionTable::CollectGarbage() {
  rep_table_.CollectGarbage();

  std::size_t removed_num = 0;
  std::size_t obj_value = tt_.size() * gc_hashfull_ / 1000;

  for (;;) {
    for (auto& entry : tt_) {
      if (entry.IsNull()) {
        continue;
      }

      if (GetAdjustedAmount(entry.GetNodeState(), entry.GetSearchedAmount()) < threshold_) {
        removed_num++;
        entry.Clear();
      }
    }

    if (removed_num >= obj_value) {
      break;
    }

    threshold_++;
  }
  return removed_num;
}

LookUpQuery TranspositionTable::GetQuery(const Node& n) {
  Key board_key = n.Pos().state()->board_key();
  std::uint32_t hash_high = board_key >> 32;
  CommonEntry* head_entry = HeadOf(board_key);
  BoardCluster board_cluster{head_entry, hash_high};

  return {rep_table_, std::move(board_cluster), n.OrHand(), n.GetDepth(), n.GetPathKey()};
}

LookUpQuery TranspositionTable::GetChildQuery(const Node& n, Move move) {
  Key board_key = n.Pos().board_key_after(move);
  std::uint32_t hash_high = board_key >> 32;
  CommonEntry* head_entry = HeadOf(board_key);
  BoardCluster board_cluster{head_entry, hash_high};

  return {rep_table_, std::move(board_cluster), n.OrHandAfter(move), n.GetDepth() + 1, n.PathKeyAfter(move)};
}

Move TranspositionTable::LookUpBestMove(const Node& n) {
  auto query = GetQuery(n);
  auto entry = query.LookUpWithoutCreation();
  return n.Pos().to_move(entry->BestMove(n.OrHand()));
}

int TranspositionTable::Hashfull() const {
  std::size_t used = 0;

  // tt_ の最初と最後はエントリ数が若干少ないので、真ん中から kHashfullCalcEntries 個のエントリを調べる
  std::size_t begin_idx = BoardCluster::kClusterSize;
  std::size_t end_idx = std::min(begin_idx + kHashfullCalcEntries, static_cast<std::size_t>(entry_mod_));

  std::size_t num_entries = end_idx - begin_idx;
  std::size_t idx = begin_idx;
  for (std::size_t i = 0; i < num_entries; ++i) {
    if (!tt_[idx].IsNull()) {
      used++;
    }
    idx += 334;
    if (idx > end_idx) {
      idx -= end_idx - begin_idx;
    }
  }
  return static_cast<int>(used * 1000 / num_entries);
}

template CommonEntry* BoardCluster::SetFinal<false>(Hand hand,
                                                    Move16 move,
                                                    MateLen mate_len,
                                                    SearchedAmount amount) const;
template CommonEntry* BoardCluster::SetFinal<true>(Hand hand,
                                                   Move16 move,
                                                   MateLen mate_len,
                                                   SearchedAmount amount) const;
}  // namespace komori