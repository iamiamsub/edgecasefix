# edgecasefix

Fixes for edge cases of beatmania IIDX, as one small DLL (5 KB, no C runtime, no hook library).
[日本語](README.ja.md)

## What it fixes

### The turntable of an 012 build forced to TDJ

On an 012 build (LDJ cabinet) forced to TDJ, a backspin or multi spin scratch ended with a reverse
makes the scratch right after it BAD or a miss, with an analog turntable. Seen on the 012 builds of
IIDX 33 Sparkle Shower (2026090400) and 34 ZINRAI (2026091600). Ending the scratch by stopping the
turntable, a keyboard, LDJ mode and the 010 builds are not affected.

The game's turntable state machine (`PlayInput_UpdateTurntable`) keeps the last turntable positions
and takes the turntable as stopped when all of them are the same; a stopped turntable that moves
(2 counts over 2 frames) starts afresh, which is a press. The 010 builds keep 16 positions, the 012
builds 8: the same 133 ms at their own frame rates (TDJ 120 fps, LDJ 60 fps). Forced to TDJ, an 012
build runs at 120 fps and stops the turntable after 67 ms. A turntable stopped by hand after a reverse
moves back a little, and after 67 ms that is a new start: it takes the next scratch too early and
leaves the real one nothing to hit.

edgecasefix.dll replaces that function with its own copy of the same state machine, which stops a
still turntable after 8 frames at 60 fps' length (16 at 120 fps), as the 010 builds do. At 60 fps (LDJ)
it does what the game does. It replaces the function only where it is byte for byte the 012 builds'
one (both builds above have the same function), so nothing changes on any other build.

## Use

Extract the zip from [Releases](../../releases), put `edgecasefix.dll` in the game's `modules` folder
and add `-k edgecasefix.dll` (spice2x). It writes no log. Its earlier name was `tt_fix.dll`: take that
out when you switch.

Works with 2dxtra, altfix and tracker_link loaded too. Load it before `tt_log.dll` (below).

## Build

Visual Studio 2022 or later with the C++ tools (CMake, Ninja and vcpkg come with it):

```powershell
.\build.ps1                  # version = the project VERSION in CMakeLists.txt
.\build.ps1 -Version 1.0.1   # build\edgecasefix.dll, build\tt_log.dll, build\ttlab.exe, dist\edgecasefix-1.0.1.zip
```

edgecasefix.dll needs nothing else. tt_log.dll uses safetyhook and zydis (vcpkg) and links against
`lib\avs2-core.lib`, made from `lib\avs2-core.def`.

Releases on GitHub are built by GitHub Actions ([.github/workflows/build.yml](.github/workflows/build.yml))
from the tagged source. Each release lists its SHA-256 checksums in SHA256SUMS.txt, and
`gh attestation verify <file> -R iamiamsub/edgecasefix` confirms that a file was built there.

## Test tools

- **ttlab.exe** (`lab\`) runs the game's own turntable state machine on a scripted turntable, without
  starting the game: it maps your copy of `bm2dx.dll`, makes the two functions the state machine asks
  (the IO devices and AppConfig) return made-up ones, and loads the hook DLLs given.
  `lab\run.ps1` runs every scenario and compares the states frame by frame:

  ```powershell
  .\lab\run.ps1 -Reference <010 build>\bm2dx.dll -Builds <012 build>\bm2dx.dll, <012 build>\bm2dx.dll
  ```

  - 012 + edgecasefix at TDJ 120 fps against 010 at TDJ 120 fps: the same in every frame
  - 012 + edgecasefix at LDJ 60 fps against 012 alone, and 010 + edgecasefix against 010 alone: the same
  - 012 alone against 010: different (the bug)

  Scenarios (`lab\scenarios\*.txt`, side 1, 256 counts a turn): `move <frames> <counts a frame>`,
  `steps <move>...`, `fuzz <frames> <seed>`, `mark <label>`, `reset <counts>` (turn that far, then the
  game's `PlayInput_Reset`, as at a new stage). The scenarios cover the reported case, stopping by
  hand, MSS into BSS, plain scratches, a keyboard, a reset in the middle and 90,000 random frames.
- **tt_log.dll** records a play: the turntable position and state in each frame, and the scratch
  presses and judgments, to `tt_log_<date>_<time>.csv` next to it (the lines are described at the top
  of `src\tt_log.cc`). Load it last.

The game libraries are not in this repository; use your own copies.

## License

MIT ([LICENSE](LICENSE)). tt_log's helpers (`src\memory.*`, `src\modules.*`, `src\avs2.h`,
`src\hooks.h`, `src\exceptions.h`) come from [omnifix](https://github.com/aixxe/omnifix)
(MIT, [LICENSE-omnifix.txt](LICENSE-omnifix.txt)).
