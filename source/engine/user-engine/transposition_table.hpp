/**
 * @note
 * 置換表のデータ構造の全体像をここで述べる。
 *
 * - NodeTable: 通常置換表
 *   - 未証明局面、詰み局面、不詰局面
 * - RepetitionTable: 千日手置換表
 *
 * 置換表へのアクセスは詰将棋探索で最も重たい処理であるため、アクセスする際は LookUpQuery を介して LookUp を行う。
 *
 * ## NodeTable
 *
 * NodeTable は node の探索結果を保存するための置換表である。格納される探索結果は、局面に至る経路に依存しない。
 * もし評価値が経路に依存する疑いがある場合、千日手フラグを立てたエントリを格納する。LookUp() 時に千日手疑いの
 * フラグが立っていた場合、RepetitionTable を用いて千日手チェックを行う。
 *
 * NodeTable は std::vector<CommonEntry> により実現している。盤面ハッシュ（持ち駒を考慮しないハッシュ値）が
 * `board_key` の局面は、
 *     [board_key % mod, (board_key % mod) + BoardCluster::kSize)
 * の半開区間のどこかに保存される。ただし、
 *     mod := table_size - BoardCluster::kSize
 * である。
 *
 * 置換表として確保した領域の一部を `board_key` に用いるという構造は直感的に分かりづらいため、`BoardCluster` という
 * クラスで wrap している。`BoardCluster` は盤面ハッシュ値が同じ局面にアクセスするための構造体で、
 * 置換表の LookUp や証明駒／反証駒の登録を行う役割を担う。
 *
 * 置換表エントリは CommonEntry というデータ構造で格納している。CommonEntry の詳細については ttentry.hpp のコメントを
 * 参照。
 *
 * もし千日手を発見した場合は千日手フラグをエントリに記録する。次回 LookUp 時、千日手フラグが立てられた
 * エントリが返却された場合は、RepetitionTable を介して千日手の判定も行う。
 *
 * ## RepetitionTable
 *
 * RepetitionTable は、千日手局面を保存するための置換表である。千日手に至る経路（をエンコードした `path_key`）を
 * 複数個保存できる。文献によっては twin table とも呼ばれる。
 *
 * RepetitionTable はシンプルな std::unordered_set<Key> により実現している。
 */
#ifndef TRANSPOSITION_TABLE_HPP_
#define TRANSPOSITION_TABLE_HPP_

#include <unordered_set>
#include <vector>

#include "ttentry.hpp"
#include "typedefs.hpp"

namespace komori {

// forward declaration
class CommonEntry;
// forward declaration
class Node;

/**
 * @brief 同じ駒配置の局面の探索結果をまとめて管理するクラス
 */
class BoardCluster {
 public:
  static constexpr inline std::size_t kClusterSize = 16;

  constexpr BoardCluster(CommonEntry* head_entry, std::uint32_t hash_high)
      : head_entry_{head_entry}, hash_high_{hash_high} {}

  /// 事前に std::veector などで領域を確保できるようにするために、デフォルトコンストラクタを定義しておく
  BoardCluster() = default;
  constexpr BoardCluster(const BoardCluster&) = delete;
  constexpr BoardCluster(BoardCluster&&) noexcept = default;
  constexpr BoardCluster& operator=(const BoardCluster&) = delete;
  constexpr BoardCluster& operator=(BoardCluster&&) noexcept = default;
  ~BoardCluster() = default;

  /**
   * @brief 条件に合致するエントリを探す。
   *
   * もしそのようなエントリが見つからなかった場合、新規作成して tt に追加する。
   */
  CommonEntry* LookUpWithCreation(Hand hand, Depth depth) const { return LookUp<true>(hand, depth); }
  /**
   * @brief 条件に合致するエントリを探す。
   *
   * もしそのようなエントリが見つからなかった倍、tt には追加せずダミーの entry を返す。
   * ダミーの entry は、次回の LookUpWithoutCreation() 呼び出しするまでの間だけ有効である。
   */
  CommonEntry* LookUpWithoutCreation(Hand hand, Depth depth) const { return LookUp<false>(hand, depth); }

  /// proof_hand により詰みであることを報告する
  CommonEntry* SetProven(Hand proof_hand, Move16 move, Depth mate_len, SearchedAmount amount) const {
    return SetFinal<true>(proof_hand, move, mate_len, amount);
  }
  /// disproof_hand により不詰であることを報告する
  CommonEntry* SetDisproven(Hand disproof_hand, Move16 move, Depth mate_len, SearchedAmount amount) const {
    return SetFinal<false>(disproof_hand, move, mate_len, amount);
  }

  std::uint32_t HashHigh() const { return hash_high_; }
  /// entry　が cluster に含まれるなら true
  bool IsStored(CommonEntry* entry) const { return begin() <= entry && entry < end(); }

  constexpr CommonEntry* begin() const { return head_entry_; }
  constexpr CommonEntry* end() const { return head_entry_ + kClusterSize; };

 private:
  /// LookUpWithCreation() と LookUpWithoutCreation() の実装本体。
  template <bool kCreateIfNotExist>
  CommonEntry* LookUp(Hand hand, Depth depth) const;
  /// SetProven() と SetDisproven() の実装本体。
  template <bool kProven>
  CommonEntry* SetFinal(Hand hand, Move16 move, Depth mate_len, SearchedAmount amount) const;

  /// 新たな entry をクラスタに追加する。クラスタに空きがない場合は、最も必要なさそうなエントリを削除する
  CommonEntry* Add(CommonEntry&& entry) const;

  /// BoardCluster が管理する NormalTable の先頭要素。ここから kClusterSize 個のエントリが cluster に含まれる
  CommonEntry* head_entry_{nullptr};
  /// 盤面ハッシュの上位32ビット
  std::uint32_t hash_high_;
};

/**
 * @brief 千日手局面の置換表
 */
class RepetitionTable {
 public:
  /// 置換表に保存された path key をすべて削除する
  constexpr void Clear() { keys_.clear(); }

  /// `path_key` を千日手として登録する
  void Insert(Key path_key) { keys_.insert(path_key); }
  /// `path_key` が保存されていれば true
  bool Contains(Key path_key) { return keys_.find(path_key) != keys_.end(); }
  /// 現在の置換表サイズ
  std::size_t Size() const { return keys_.size(); }

 private:
  std::unordered_set<Key> keys_;
};

/**
 * @brief 置換表の LookUp するためのキャッシュするクラス
 *
 * 置換表の LookUp を行うためには、ハッシュ値 `key`、 持ち駒 `hand` などの情報を渡す必要がある。置換表 LookUp() のたびに
 * これらの値を指定させると呼び出し側のコードが複雑になってしまうため、LookUp を行うためのクラスを用意する。
 */
class LookUpQuery {
 public:
  LookUpQuery(RepetitionTable& rep_table, BoardCluster&& board_claster, Hand hand, Depth depth, Key path_key)
      : rep_table_{&rep_table},
        board_cluster_{std::move(board_claster)},
        hand_{hand},
        depth_{depth},
        path_key_{path_key},
        entry_{board_cluster_.begin()} {}

  /// キャッシュに使用するため、デフォルトコンストラクタを有効にする
  LookUpQuery() = default;
  LookUpQuery(const LookUpQuery&) = delete;
  LookUpQuery(LookUpQuery&&) noexcept = default;
  LookUpQuery& operator=(const LookUpQuery&) = delete;
  LookUpQuery& operator=(LookUpQuery&&) noexcept = default;
  ~LookUpQuery() = default;

  /// Query によるエントリ問い合わせを行う。もし見つからなかった場合は新規作成して cluster に追加する
  CommonEntry* LookUpWithCreation();
  /**
   * @brief  Query によるエントリ問い合わせを行う。もし見つからなかった場合はダミーのエントリを返す。
   *
   * ダミーエントリが返されたかどうかは IsStored() により判定可能である。このエントリは次回の LookUp までの間まで
   * 有効である。
   */
  CommonEntry* LookUpWithoutCreation();

  /// 調べていた局面が証明駒 `proof_hand` で詰みであることを報告する
  void SetProven(Hand proof_hand, Move16 move, Depth mate_len, SearchedAmount amount) {
    entry_ = board_cluster_.SetProven(proof_hand, move, mate_len, amount);
  }
  /// 調べていた局面が反証駒 `disproof_hand` で不詰であることを報告する
  void SetDisproven(Hand disproof_hand, Move16 move, Depth mate_len, SearchedAmount amount) {
    entry_ = board_cluster_.SetDisproven(disproof_hand, move, mate_len, amount);
  }
  /// 調べていた局面が千日手による不詰であることを報告する
  void SetRepetition(SearchedAmount amount);
  /// 調べていた局面が勝ちであることを報告する
  template <bool kOrNode>
  void SetWin(Hand hand, Move move, Depth len, SearchedAmount amount) {
    if constexpr (kOrNode) {
      SetProven(hand, move, len, amount);
    } else {
      SetDisproven(hand, move, len, amount);
    }
  }
  /// 調べていた局面が負けであることを報告する
  template <bool kOrNode>
  void SetLose(Hand hand, Move move, Depth len, SearchedAmount amount) {
    if constexpr (kOrNode) {
      SetDisproven(hand, move, len, amount);
    } else {
      SetProven(hand, move, len, amount);
    }
  }

 private:
  /// `entry_` が有効（前回呼び出しから移動していない）かどうかをチェックする
  bool IsValid() const;

  /// 千日手置換表へのポインタ。デフォルトコンストラクト可能にするために参照ではなくポインタで持つ。
  RepetitionTable* rep_table_;
  /// 通常置換表の LookUp 対象。
  BoardCluster board_cluster_;
  /// （先手側の）持ち駒
  Hand hand_;
  /// 探索深さ
  Depth depth_;
  /// 経路ハッシュ値
  Key path_key_;

  /// entry のキャッシュ。毎回 LookUp をすると時間がかかるので、サボれる時はこれを返す。
  CommonEntry* entry_;
};

/**
 * @brief 詰将棋探索における置換表本体
 *
 * 高速化のために、直接 LookUp させるのではなく、LookUpQuery を返すことで結果のキャッシュが可能にしている。
 */
class TranspositionTable {
 public:
  explicit TranspositionTable(int gc_hashfull);

  /// ハッシュサイズを `hash_size_mb` （以下）に変更する。以前に保存されていた結果は削除される
  void Resize(std::uint64_t hash_size_mb);
  /// 以前の探索結果をすべて削除し、新たな探索をを始める
  void NewSearch();
  /// GCを実行する
  std::size_t CollectGarbage();

  /// 局面 `n` の LookUp 用の構造体を取得する
  LookUpQuery GetQuery(const Node& n);
  /// 局面 `n` から `move` で進めた局面の、LookUp 用の構造体を取得する
  LookUpQuery GetChildQuery(const Node& n, Move move);
  /// 局面 `n` の最善手を取得する。探索中の場合、MOVE_NONE が返る可能性がある
  Move LookUpBestMove(const Node& n);

  /// ハッシュ使用率を返す（戻り値は千分率）
  int Hashfull() const;

 private:
  /// NormalTable の board_key の先頭要素へのポインタを返す
  CommonEntry* HeadOf(Key board_key) { return &tt_[board_key % entry_mod_]; }

  /// NormalTable 本体
  std::vector<CommonEntry> tt_{};
  /// RepetitionTable 本体
  RepetitionTable rep_table_{};
  /// tt_.size() - BoardCluster::kClusterSize。
  /// (board_key % entry_mod_) により先頭要素の index を求められる
  std::uint64_t entry_mod_{1};
  /// GC で削除したい要素数の割合（千分率）
  int gc_hashfull_;
  /// 前回の GC で用いたしきい値
  SearchedAmount threshold_{kMinimumSearchedAmount};
};
}  // namespace komori
#endif  // TRANSPOSITION_TABLE_HPP_