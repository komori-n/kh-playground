# References

KomoringHeights ではDF-WPN（Depth-First Weak Proof-Number）アルゴリズム[^wpns]を基に探索を行う。
以前の詰将棋探索では、df-pn[^df-pn]アルゴリズムという探索手法が主流だったが、
「二重カウント問題」の存在により、稀にpn/dnの値がオーバーフローするという問題があった。
これに対しDF-WPNは、pn/dnの計算方法を工夫することで二重カウント問題を回避できるという利点がある。

なお、kh-v1.1.1まではdf-pn+アルゴリズムを用いて探索を行っていた。
df-pn+アルゴリズムにおける二重カウント問題対策のコードはkh-v1.1.1を参照のこと。

詰将棋探索特有の問題を回避するために、以下の機能が実装されている。

- Repetition Table：GHI問題（Graph History Interaction Problem）対策[^ghi]
- TCA（Threshold Controlling Algorithm）[^tca-and-double-count]： 無限ループ対策

また、探索性能を改善するために、以下の機能が実装されている。

- df-pn+[^df-pn-plus]
- 末端節点における固定深さの探索[^fix-depth-search]
- 証明駒／反証駒[^superiority]

開発に際しては以下の情報を参考にした。

- 1局面の合法王手の最大数[^legal-check]
- やねうら王 詰将棋500万問問題集[^yane-5-million]
- shtsume[^shtsume]

[^superiority]: 脊尾昌宏. (1999). 詰将棋を解くアルゴリズムにおける優越関係の効率的な利用について. ゲームプログラミングワークショップ 1999 論文集, 1999(14), 129-136.
[^wpns]: Ueda, Toru, et al. "Weak proof-number search." Computers and Games: 6th International Conference, CG 2008, Beijing, China, September 29-October 1, 2008. Proceedings 6. Springer Berlin Heidelberg, 2008.
[^df-pn]: 長井歩, & 今井浩. (2002). df-pn アルゴリズムの詰将棋を解くプログラムへの応用. 情報処理学会論文誌, 43(6), 1769-1777.
[^df-pn-plus]: Nagai, A. (2002). Df-pn algorithm for searching AND/OR trees and its applications. PhD thesis, Department of Information Science, University of Tokyo.
[^ghi]: Kishimoto, A., & Müller, M. (2004, July). A general solution to the graph history interaction problem. In AAAI (Vol. 4, pp. 644-649).
[^fix-depth-search]: 金子知適, 田中哲朗, 山口和紀, & 川合慧. (2010). 新規節点で固定深さの探索を行う df-pn の拡張. 情報処理学会論文誌, 51(11), 2040-2047.
[^tca-and-double-count]: Kishimoto, A. (2010, July). Dealing with infinite loops, underestimation, and overestimation of depth-first proof-number search. In Proceedings of the AAAI Conference on Artificial Intelligence (Vol. 24, No. 1, pp. 108-113).
[^legal-check]: TadaoYamaoka. (2018-06-03). [王手生成の最大数 - TadaoYamaokaの開発日記](https://tadaoyamaoka.hatenablog.com/entry/2018/06/03/225012).
[^yane-5-million]: Yaneurao. (2020-12-25). [やねうら王公式からクリスマスプレゼントに詰将棋500万問を謹呈 | やねうら王 公式サイト](https://yaneuraou.yaneu.com/2020/12/25/christmas-present/)
[^shtsume]: <https://github.com/hkijin/shtsume>
