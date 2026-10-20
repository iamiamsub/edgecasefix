# edgecasefix

beatmania IIDX の細かい不具合を直す、小さな DLL（5KB、C ランタイムもフックのライブラリも使わない）。
[English](README.md)

## 直すもの

### 012 版を TDJ に強制したときの皿

012 版（LDJ 筐体用）を TDJ に強制して遊ぶと、アナログの皿で BSS・MSS の終点を逆回しで取ったとき、直後の皿が
BAD・ミスになる。IIDX 33 Sparkle Shower（2026090400）と 34 ZINRAI（2026091600）の 012 版で確認。皿を止める
終わらせ方、キーボード、LDJ モード、010 版では起きない。

ゲームの皿の状態機械（`PlayInput_UpdateTurntable`）は直近の皿の位置を持ち、全部同じなら「停止」にする。
停止中に動く（2 フレームで 2 以上）と「回し始め」= 押下になる。010 版は 16 個、012 版は 8 個持ち、どちらも
自分のフレームレート（TDJ 120fps・LDJ 60fps）で 133ms になる。012 版を TDJ に強制すると 120fps で動くので、
67ms で停止になる。逆回しのあと手で止めた皿は少し戻り、67ms 以降だとそれが新しい回し始めになって、直後の
皿ノーツを早押しで取る（本当に回したときにはノーツが無い）。

edgecasefix.dll は、この関数を同じ状態機械を写した自前の関数に差し替える。違うのは停止の判定だけで、
60fps の 8 フレーム分の長さ（120fps なら 16 フレーム = 010 版と同じ）同じ位置なら停止にする。60fps（LDJ）では
ゲームと同じ。差し替えるのは、関数が 012 版のものと 1 バイトも違わないときだけ（上の 2 つの版は同じ関数）で、
それ以外の版では何もしない。

## 使い方

`edgecasefix.dll` をゲームの `modules` フォルダに置き、起動のオプションに `-k edgecasefix.dll` を足す（spice2x）。
ログは出さない。前の名前は `tt_fix.dll` で、置き換えるときはそちらを外す。

2dxtra・altfix・tracker_link と一緒でよい。`tt_log.dll`（下）より先に読み込む。

## ビルド

C++ の開発ツール入りの Visual Studio 2022 以降（CMake・Ninja・vcpkg は付属のもの）:

```powershell
.\build.ps1    # build\edgecasefix.dll, build\tt_log.dll, build\ttlab.exe, dist\edgecasefix_<版>.zip
```

edgecasefix.dll はほかに何も要らない。tt_log.dll は safetyhook と zydis（vcpkg）を使い、
`lib\avs2-core.def` から作った `lib\avs2-core.lib` とリンクする。

## テストの道具

- **ttlab.exe**（`lab\`）: ゲームを起動せずに、ゲーム自身の皿の状態機械にシナリオの皿の動きを流す。手元の
  `bm2dx.dll` を読み込むだけにして、状態機械が呼ぶ 2 つの関数（IO と AppConfig）を偽物に差し替え、指定したフック
  DLL を読み込む。`lab\run.ps1` が全シナリオを流して、状態をフレームごとに比べる:

  ```powershell
  .\lab\run.ps1 -Reference <010 版>\bm2dx.dll -Builds <012 版>\bm2dx.dll, <012 版>\bm2dx.dll
  ```

  - 012 + edgecasefix の TDJ 120fps と 010 の TDJ 120fps: 全フレーム同じ
  - 012 + edgecasefix の LDJ 60fps と 012 だけ、010 + edgecasefix と 010 だけ: 同じ
  - 012 だけと 010: 違う（不具合）

  シナリオ（`lab\scenarios\*.txt`、1P 側、1 回転 256）: `move <フレーム数> <1 フレームの動き>`、`steps <動き>...`、
  `fuzz <フレーム数> <種>`、`mark <名前>`、`reset <動き>`（そこまで回してからゲームの `PlayInput_Reset`。次のステージの
  ように）。報告の場面・手で止める・MSS から BSS・普通の皿・キーボード・途中のリセット・ランダム 9 万フレーム。
- **tt_log.dll**: プレーを記録する。皿の位置と状態（フレームごと）、皿の押下と判定を、DLL の横の
  `tt_log_<日付>_<時刻>.csv` に書く（行の形は `src\tt_log.cc` の先頭）。最後に読み込む。

ゲーム本体はこのリポジトリに含まない。手元のものを使う。

## ライセンス

MIT（[LICENSE](LICENSE)）。tt_log の補助（`src\memory.*`・`src\modules.*`・`src\avs2.h`・`src\hooks.h`・
`src\exceptions.h`）は [omnifix](https://github.com/aixxe/omnifix) から（MIT、[LICENSE-omnifix.txt](LICENSE-omnifix.txt)）。
