# edgecasefix

beatmania IIDX の細かい不具合を直す、小さな DLL（5KB、C ランタイムもフックのライブラリも使わない）。
[English](README.md)

## 直すもの

### 012 版を TDJ に強制したときの皿

012 版（LDJ 筐体用）を TDJ に強制して遊ぶと、アナログの皿で BSS・MSS の終点を逆回しで取ったとき、直後の皿が
BAD・ミスになる。IIDX 33 Sparkle Shower（2026090400）と 34 ZINRAI（2026091600）の 012 版で確認した。皿を止める
終わらせ方、キーボード、LDJ モード、010 版では起きない。

ゲームの皿の状態機械（`PlayInput_UpdateTurntable`）は直近の皿の位置を持ち、全部同じなら「停止」にする。
停止中に動く（2 フレームで 2 以上）と、「回し始め」という押下になる。010 版は 16 個、012 版は 8 個持ち、どちらも
それぞれのフレームレート（010 版は TDJ の 120fps、012 版は LDJ の 60fps）で 133ms 分になる。012 版を TDJ に強制すると
120fps で動くので、67ms で停止になる。

逆回しのあと手で止めた皿は少し戻る。止まってから 67ms 以上たって戻ると、それが新しい回し始めになり、直後の
皿ノーツを早押しで取る。本当に回したときにはノーツが無い。

edgecasefix.dll はこの関数を、同じ状態機械を写した自前の関数に差し替える。違うのは停止の判定だけで、皿の位置が
60fps の 8 フレーム分の時間変わらなければ停止にする。120fps なら 16 フレームで、010 版と同じになる。60fps（LDJ）では
ゲームと同じ。

差し替えるのは、関数が 012 版のものと 1 バイトも違わないときだけで、それ以外のバージョンでは何もしない。
上の 2 つのバージョンはこの関数が同じ。

## 使い方

[Releases](../../releases) の zip を展開し、`edgecasefix.dll` をゲームの `modules` フォルダに置き、起動のオプション
（spice2x）に `-k edgecasefix.dll` を足す。ログは出さない。前の名前は `tt_fix.dll` で、置き換えるときは `tt_fix.dll` と
`-k tt_fix.dll` を外す。

2dxtra・altfix・tracker_link と一緒に読み込んでよい。下で説明する `tt_log.dll` より先に読み込む。

## ビルド

ビルドには C++ の開発ツール入りの Visual Studio 2022 以降が要る。CMake・Ninja・vcpkg は付属のものを使う。

```powershell
.\build.ps1                  # バージョンは CMakeLists.txt の project VERSION
.\build.ps1 -Version 1.0.1   # build\edgecasefix.dll, build\tt_log.dll, build\ttlab.exe, dist\edgecasefix-1.0.1.zip
```

edgecasefix.dll はほかに何も要らない。tt_log.dll は vcpkg で入れる safetyhook と zydis を使い、
`lib\avs2-core.def` から作った `lib\avs2-core.lib` とリンクする。

GitHub のリリースはタグを付けたソースを GitHub Actions（[.github/workflows/build.yml](.github/workflows/build.yml)）で
ビルドして作る。リリースごとに SHA-256 のチェックサムを SHA256SUMS.txt に載せる。
`gh attestation verify <ファイル> -R iamiamsub/edgecasefix` で、そのファイルが GitHub Actions でビルドされたことを確かめられる。

## テストの道具

- ttlab.exe（`lab\`）: ゲームを起動せずに、ゲーム自身の皿の状態機械にシナリオの皿の動きを流す。手元の
  `bm2dx.dll` を読み込むだけにして、状態機械が呼ぶ 2 つの関数（IO と AppConfig）を偽物に差し替え、指定したフック
  DLL を読み込む。`lab\run.ps1` を次のように実行すると、全シナリオを流して状態をフレームごとに比べる。

  ```powershell
  .\lab\run.ps1 -Reference <010 版>\bm2dx.dll -Builds <012 版>\bm2dx.dll, <012 版>\bm2dx.dll
  ```

  - 012 + edgecasefix の TDJ 120fps と 010 の TDJ 120fps: 全フレーム同じ
  - 012 + edgecasefix の LDJ 60fps と 012 だけ、010 + edgecasefix と 010 だけ: 同じ
  - 012 だけと 010: 違う（不具合）

  シナリオは `lab\scenarios\*.txt` に、1P 側の皿の動きを 1 回転 256 として書く。使える行は
  `move <フレーム数> <1 フレームの動き>`、`steps <動き>...`、`fuzz <フレーム数> <種>`、`mark <名前>`、`reset <動き>` で、
  `reset` はそこまで回してから、次のステージのようにゲームの `PlayInput_Reset` を呼ぶ。入っているシナリオは報告の場面・
  手で止めたとき・MSS から BSS・普通の皿・キーボード・途中のリセット・ランダムな 9 万フレーム。
- tt_log.dll: プレーを記録する。フレームごとの皿の位置と状態、皿の押下と判定を、DLL の横の
  `tt_log_<日付>_<時刻>.csv` に書く。行の形は `src\tt_log.cc` の先頭に書いてある。最後に読み込む。

ゲーム本体はこのリポジトリに含まない。手元のものを使う。

## ライセンス

ライセンスは MIT（[LICENSE](LICENSE)）。tt_log の補助のファイル（`src\memory.*`・`src\modules.*`・`src\avs2.h`・
`src\hooks.h`・`src\exceptions.h`）は [omnifix](https://github.com/aixxe/omnifix) から取ったもので、ライセンスは MIT
（[LICENSE-omnifix.txt](LICENSE-omnifix.txt)）。
