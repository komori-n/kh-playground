/**
 * @file komoring_heights.hpp
 */
#ifndef KOMORI_KOMORING_HEIGHTS_HPP_
#define KOMORI_KOMORING_HEIGHTS_HPP_

#include <future>
#include <vector>

#include "engine_option.hpp"
#include "local_expansion.hpp"
#include "mate_len.hpp"
#include "move_path.hpp"
#include "pv_list.hpp"
#include "score.hpp"
#include "search_monitor.hpp"
#include "search_result.hpp"
#include "transposition_table.hpp"
#include "usi_info.hpp"

namespace komori {
/**
 * @brief 詰将棋探索の本体
 */
class KomoringHeights {
 public:
  /// Default constructor(default)
  KomoringHeights() = default;
  /// Copy constructor(delete)
  KomoringHeights(const KomoringHeights&) = delete;
  /// Move constructor(delete)
  KomoringHeights(KomoringHeights&&) = delete;
  /// Copy assign operator(delete)
  KomoringHeights& operator=(const KomoringHeights&) = delete;
  /// Move assign operator(delete)
  KomoringHeights& operator=(KomoringHeights&&) = delete;
  /// Destructor(default)
  ~KomoringHeights() = default;

  /**
   * @brief エンジンを初期化する
   * @param option 探索オプション
   * @param num_threads スレッド数
   */
  void Init(const EngineOption& option, std::uint32_t num_threads);
  /// 置換表の内容をすべて削除する。ベンチマーク用。
  void Clear();

  /// 探索を停止する
  void Stop();

  /**
   * @brief 詰み手順を取得する
   * @pre Search() の戻り値が `NodeState::kProven`
   * @return 詰み手順
   */
  const std::vector<Move>& BestMoves() const { return best_moves_; }

  /**
   * @brief Search() の準備を行う。探索開始直前に main_thread から呼び出すこと。
   * @param n 現局面
   * @param is_root_or_node `n` が OR node かどうか
   * @pre メインスレッドから呼び出すこと
   */
  void NewSearch(const Position& n, bool is_root_or_node);

  /**
   * @brief 詰め探索を行う（メインスレッド）
   * @param n 現局面
   * @param is_root_or_node `n` が OR node かどうか
   * @return 探索結果
   */
  NodeState SearchMainThread(const Position& n, bool is_root_or_node);
  /**
   * @brief 詰め探索を行う（メインスレッド以外）
   * @param n 現局面
   * @param is_root_or_node `n` が OR node かどうか
   * @return 探索結果
   */
  NodeState SearchSubThread(const Position& n, bool is_root_or_node);

 private:
  void ResetFutures();

  /**
   * @brief 局面 `n` が `len` 手以下で詰むかどうかを探索する
   * @param n 現局面
   * @param len 詰み手数
   * @param multi_pv Multi PV の数
   * @return 探索結果
   */
  SearchResult SearchEntry(Node& n, MateLen len, std::uint32_t multi_pv);

  /**
   * @brief `n` に対し `mate_len` 手以下の詰み手順を `pv_moves_` に格納する
   * @param n 現局面
   * @param max_len 現局面の最大詰み手数
   * @pre 現局面が `mate_len` 手以下の詰みであること
   * @return 探索結果
   */
  SearchResult ConstructProvenPv(Node& n, MateLen max_len);

  /**
   * @brief `n` に対し、不詰手順を `best_moves_` に格納する
   * @param n 現局面
   * @pre 現局面が不詰であること
   */
  void ConstructDisprovenPv(Node& n);

  /**
   * @brief 詰め探索の本体。（再帰関数）
   * @param n 現局面
   * @param thpn pn のしきい値
   * @param thdn dn のしきい値
   * @param len  残り手数
   * @param inc_flag TCA の探索延長フラグ
   * @return 探索結果
   */
  SearchResult SearchImpl(Node& n, PnDn thpn, PnDn thdn, MateLen len, std::uint32_t& inc_flag);

  /// 現在の探索情報を取得する
  /// @pre メインスレッドから呼び出すこと
  UsiInfo CurrentInfo() const;

  /**
   * @brief 探索情報を出力する
   * @param n 現局面
   * @pre メインスレッドから呼び出すこと
   */
  void Print(const Node& n);

  tt::TranspositionTable tt_;  ///< 置換表
  EngineOption option_;        ///< エンジンオプション
  /// スレッド同期用バリア
  /// 各ループの末尾で全スレッドが待つために使用する。
  ///
  /// サブスレッドは、 search_promises_ に結果を書いた直後に barrier_ で待機する。
  /// メインスレッドは、 search_futures_ で結果を受け取り、次に探索すべき局面を決めて barrier_ で全スレッドに通知する。
  Barrier barrier_;

  SearchMonitor monitor_;                                     ///< 探索モニター
  ScoreMaker score_maker_{ScoreCalculationMethod::kPonanza};  ///< 評価値を作成するオブジェクト

  std::vector<Move> best_moves_;                                          ///< 詰み手順
  std::vector<InlineStack<LocalExpansion, kDepthMax>> expansion_list_{};  ///< スレッドごとの局面展開のための一時領域
  Score score_{};  ///< 現在の探索評価値。余詰探索中に CurrentInfo() で取得できるようにここにおいておく

  std::mutex mutex_;          ///< pv_list_ の排他制御のための mutex
  PvList pv_list_;            ///< 各手に対する PV の一覧
  bool in_pv_search_{false};  ///< PV探索中かどうか
  MovePath pv_moves_;         ///< PVの手順

  std::atomic<bool> should_break_main_loop_{false};  ///< メインループを抜けるかどうか
  // sub thread が探索すべき局面。サブスレッドは mutex で排他せずに読むため、メインスレッドから起床を命じるときは
  // race condition にならないように注意する必要がある
  std::vector<Move> moves_from_root_;                        ///< 探索開始局面
  MateLen mate_len_{MateLen::Zero()};                        ///< 探索手数
  std::uint32_t multi_pv_{1};                                ///< Multi PV の数
  std::vector<std::promise<SearchResult>> search_promises_;  /// 各スレッドの探索結果を送る promise
  std::vector<std::future<SearchResult>> search_futures_;    /// 各スレッドの探索結果を受け取る future
};
}  // namespace komori

#endif  // KOMORI_KOMORING_HEIGHTS_HPP_
