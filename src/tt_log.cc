// tt_log.dll - for testing: records what the turntable does in a play, frame by frame, with the
// scratch judgments, to tt_log_<date>_<time>.csv next to the DLL.
//
// Lines (time in ms since the DLL loaded):
//   F,time,frame,side,position,move,state,changed   a frame where the side moved or is not stopped
//                                                    (move: over the last 2 frames, as the game sees it;
//                                                    state 0 stopped, 1/2 start, 3/4 turning, 5/6 reverse)
//   T,time,frame,side,ms,grade                      a scratch press timed against its note (ms: early < 0)
//   J,time,frame,side,grade                         a scratch judgment (grade 4/5 PGREAT .. 1/8 BAD,
//                                                    9 empty POOR, 10 POOR, 11 early charge release)
//   C,time,frame,io_type,fps,ring                    the game's mode, when it changes
//
// Load it last, after edgecasefix.dll to record the fixed states and after autoplay_mix.dll (which finds
// the timing function by its first bytes): "... -k edgecasefix.dll -k tt_log.dll".

#include <array>
#include <chrono>
#include <cstdio>
#include <format>
#include <mutex>
#include <ranges>
#include <stdexcept>
#include <string>
#include <windows.h>
#include <safetyhook.hpp>

#include "avs2.h"
#include "hooks.h"
#include "memory.h"
#include "modules.h"
#include "turntable.h"

using namespace omnifix;

namespace
{
    auto game = tt::game {};
    auto update_hook = SafetyHookInline {};
    auto apply_hook = SafetyHookInline {};
    auto timing_hook = SafetyHookInline {};

    auto file = static_cast<std::FILE*>(nullptr);
    auto lock = std::mutex {};
    auto start = std::chrono::steady_clock::now();
    auto input = static_cast<std::uint8_t*>(nullptr);       // the last input the state machine ran on
    auto mode = std::array<int, 3> {};
    auto last = std::array<int, 2> { -1, -1 };              // state written last, per side

    auto now() -> long long
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count();
    }

    auto frame() -> int { return input ? *reinterpret_cast<int*>(input) : -1; }

    // every line flushed: the game may end without unloading the DLL
    auto write(const std::string& line) -> void
    {
        auto guard = std::scoped_lock { lock };
        std::fputs(line.c_str(), file);
        std::fflush(file);
    }

    auto update_detour(std::uint8_t* in) -> void
    {
        update_hook.call(in);
        input = in;

        auto const config = game.config();
        auto const current = std::array<int, 3> { *reinterpret_cast<int*>(config + 8), tt::fps(game), game.ring };

        if (current != mode)
        {
            mode = current;
            write(std::format("C,{},{},{},{},{}\n", now(), frame(), mode[0], mode[1], mode[2]));
        }

        for (auto s = 0; s < 2; ++s)
        {
            auto const side = tt::at(game, in, s);
            auto const pos = tt::position(game, side);
            auto move = pos - side.ring[(side.slot + game.ring - 3) % game.ring];
            move = move > 127 ? move - 256 : move < -127 ? move + 256 : move;

            if (side.state != 0 || last[s] != 0 || move != 0)
                write(std::format("F,{},{},{},{},{},{},{}\n", now(), frame(), s, pos, move, side.state, side.changed));

            last[s] = side.state;
        }
    }

    /** Judge_Apply(judge, side, grade, lane, measure); what it returns is passed on as it is */
    auto apply_detour(void* judge, int side, int grade, int lane, int measure) -> std::uint64_t
    {
        if (lane == 7)
            write(std::format("J,{},{},{},{}\n", now(), frame(), side, grade));

        return apply_hook.call<std::uint64_t>(judge, side, grade, lane, measure);
    }

    /** Judge_GradeFromTiming(judge, side, lane, ms) -> grade */
    auto timing_detour(void* judge, int side, int lane, float ms) -> int
    {
        auto const grade = timing_hook.call<int>(judge, side, lane, ms);

        if (lane == 7)
            write(std::format("T,{},{},{},{:.2f},{}\n", now(), frame(), side, ms, grade));

        return grade;
    }

    auto init(HMODULE self) -> void
    {
        auto const list = modules::list();
        auto const bm2dx = std::ranges::find_if(list, [] (auto&& entry)
            { return modules::has_export(entry.base, "dll_entry_main"); });

        if (bm2dx == list.end())
            throw std::runtime_error { "game library not found" };

        auto const region = bm2dx->region();
        game = tt::find(region);
        reserve_hook_memory(region);

        // the first instructions are left out: 2dxtra or autoplay_mix may have hooked the functions already
        auto const apply = memory::find(region, "? ? ? ? ? 48 89 6C 24 10 48 89 74 24 18 44 89 4C 24 20 57 41 54 "
            "41 55 41 56 41 57 48 83 EC 20 4C 63 E2 48 8B F1 41 8B CC 41 8B D9 45 8B F0 E8 ? ? ? ? 41 8B D6 "
            "4C 63 F8 48 8B CE E8");
        auto const timing = memory::find(region, "? ? ? ? ? ? ? ? ? ? ? 48 C7 40 88 FE FF FF FF 48 89 58 08 "
            "48 89 70 10 48 8B 05 ? ? ? ? 48 33 C4 48 89 84 24 90 00 00 00 4C 8B C9 48 63 C2 41 83 F8 07");

        wchar_t path[MAX_PATH] {};
        GetModuleFileNameW(self, path, MAX_PATH);
        auto name = std::wstring { path };
        auto t = SYSTEMTIME {};
        GetLocalTime(&t);
        name = name.substr(0, name.find_last_of(L"\\/") + 1) + std::format(L"tt_log_{:04}{:02}{:02}_{:02}{:02}{:02}.csv",
            t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);

        if (_wfopen_s(&file, name.c_str(), L"w") != 0 || !file)
            throw std::runtime_error { "could not open the log file" };

        update_hook = safetyhook::create_inline(reinterpret_cast<void*>(game.update), update_detour);
        apply_hook = safetyhook::create_inline(apply, apply_detour);
        timing_hook = safetyhook::create_inline(timing, timing_detour);

        if (!update_hook || !apply_hook || !timing_hook)
            throw std::runtime_error { "could not hook the turntable or the judgments" };

        std::fputs("# tt_log: F,time,frame,side,position,move,state,changed / T,time,frame,side,ms,grade / "
            "J,time,frame,side,grade / C,time,frame,io_type,fps,ring\n", file);
        std::fflush(file);
        avs2::log::info("tt_log: recording the turntable (the game keeps {} positions)", game.ring);
    }
}

auto DllMain(HINSTANCE module, DWORD reason, LPVOID) -> BOOL
{
    if (reason != DLL_PROCESS_ATTACH)
        return TRUE;

    DisableThreadLibraryCalls(module);

    try
    {
        init(module);
    }
    catch (const std::exception& e)
    {
        avs2::log::warning("tt_log: not started: {}", e.what());
    }

    return TRUE;
}
