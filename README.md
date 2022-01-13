![Make CI (MSYS2 for Windows)](https://github.com/komori-n/KomoringHeights/workflows/Make%20CI%20(MSYS2%20for%20Windows)/badge.svg?event=push)
![Make CI (MinGW for Windows)](https://github.com/komori-n/KomoringHeights/workflows/Make%20CI%20(MinGW%20for%20Windows)/badge.svg?event=push)
![Make CI (for Ubuntu Linux)](https://github.com/komori-n/KomoringHeights/workflows/Make%20CI%20(for%20Ubuntu%20Linux)/badge.svg?event=push)
![Make CI (for Mac)](https://github.com/komori-n/KomoringHeights/workflows/Make%20CI%20(for%20Mac)/badge.svg?event=push)
![NDK CI (for Android)](https://github.com/komori-n/KomoringHeights/workflows/NDK%20CI%20(for%20Android)/badge.svg?event=push)

# About this project

KomoringHeights は、df-pn+アルゴリズムを用いたやねうら王ベースの詰将棋ソルバーです。
中〜長編の詰将棋を高速に解くことを目指して開発をしています。

KomoringHeights 本体は `source/engine/user-engine` 以下に格納されています。

# How to use

[Releases](https://github.com/komori-n/KomoringHeights/releases) からお使いのOSに合ったバイナリをダウンロードしてください。
KomoringHeightsを動かすには、[将棋所](http://shogidokoro.starfree.jp/)、[ShogiGUI](http://shogigui.siganus.com/)、
[ShogiDroid](http://shogidroid.siganus.com/)などのUSIプロトコルに対応したGUIを利用してください。

# Options

* `USI_Hash`: 置換表で使用するメモリ量
* `DepthLimit`: 探索深さ制限。この深さに達したら探索を打ち切る（0: 無制限）
  * 0固定を推奨。この値を変更すると探索性能が劣化する
* `NodesLimit`: 探索ノード数（`DoMove()` 回数）制限（0: 無制限）
* `PvInterval`: PVの出力周期（ミリ秒）
* `YozumeNodeCount`: 余詰め探索局面数
* `YozumePath`: 余詰め探索回数制限
* `Threads`: unimplemented

# 技術解説

* [詰将棋に対するdf-pnアルゴリズムの解説 | コウモリのちょーおんぱ](https://komorinfo.com/blog/df-pn-basics/)
* [詰将棋探索における証明駒／反証駒の活用方法 | コウモリのちょーおんぱ](https://komorinfo.com/blog/proof-piece-and-disproof-piece/)
* [高速な詰将棋ソルバー『KomoringHeights』v0.4.0を公開した | コウモリのちょーおんぱ](https://komorinfo.com/blog/komoring-heights-v040/)

# Contributing

バグの報告や機能要望などはIssueへお願いします。
Pull Requestも大歓迎です。

# ライセンス

Licensed under GPLv3.

やねうら王プロジェクトのソースコードはStockfishをそのまま用いている部分が多々あり、Apery/SilentMajorityを参考にしている部分もありますので、やねうら王プロジェクトは、それらのプロジェクトのライセンス(GPLv3)に従うものとします。

「リゼロ評価関数ファイル」については、やねうら王プロジェクトのオリジナルですが、一切の権利は主張しませんのでご自由にお使いください。
