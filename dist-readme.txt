edgecasefix 1.0.0
=================

beatmania IIDX の細かい不具合を直す DLL です。今は次の 1 つを直します。

012 版を TDJ に強制したときの皿
  012 版を TDJ に強制して遊ぶと、BSS・MSS の終点を逆回しで取ったときに直後の皿が BAD・ミスになります
  （IIDX 33 Sparkle Shower / 34 ZINRAI の 012 版で確認）。

  原因: 012 版は皿の位置の履歴を 8 フレーム分しか持たず（LDJ の 60fps 用）、TDJ の 120fps では 67ms で「皿が
  止まった」と判定します。逆回しのあと止めた皿が少し戻ると、それが新しい「回し始め」になり、直後の皿ノーツを
  早押しで取ってしまいます。010 版は 16 フレーム分（120fps で 133ms）なので起きません。

  edgecasefix.dll は、TDJ（120fps）のときだけ、止まったと判定するまでを 010 版と同じ長さにします。LDJ（60fps）や
  010 版では何もしません。ゲームの皿の処理（PlayInput_UpdateTurntable）を同じ処理の自前の関数に差し替えます。
  012 版のその関数と 1 バイトも違わないときだけ差し替え、それ以外の版では何もしません。

使い方
  edgecasefix.dll をゲームの modules フォルダに置き、起動のオプションに -k edgecasefix.dll を足します（spice2x）。
  ログは出しません。

  前の名前は tt_fix.dll（1.0.0・1.0.1）です。置き換えるときは tt_fix.dll と -k tt_fix.dll を外してください。

ソース: https://github.com/iamiamsub/edgecasefix（MIT ライセンス）

------------------------------------------------------------------------------------------------

edgecasefix 1.0.0 fixes edge cases of beatmania IIDX. For now, one:

The turntable of an 012 build forced to TDJ
  When a backspin or multi spin scratch is ended with a reverse, the scratch right after it is judged
  BAD or missed (seen on the 012 builds of IIDX 33 Sparkle Shower and 34 ZINRAI).

  The 012 builds keep 8 frames of turntable positions (made for LDJ at 60 fps) and, at TDJ's 120 fps,
  take the turntable as stopped after 67 ms. A stopped turntable that moves back a little after the
  reverse then starts afresh, which takes the next scratch too early. The 010 builds keep 16 frames
  (133 ms at 120 fps). edgecasefix.dll makes the 012 builds wait as long at 120 fps; at 60 fps (LDJ)
  and on the 010 builds it changes nothing. It replaces the game's turntable function
  (PlayInput_UpdateTurntable) with its own copy of the same state machine, only where that function
  is byte for byte the 012 builds' one.

Use: put edgecasefix.dll in the game's modules folder and add -k edgecasefix.dll (spice2x). It writes
no log. Its earlier name was tt_fix.dll (1.0.0, 1.0.1): take tt_fix.dll and -k tt_fix.dll out.

Source: https://github.com/iamiamsub/edgecasefix (MIT License)
