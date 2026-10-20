// ttlab - runs the game's own turntable state machine (PlayInput_UpdateTurntable, src/turntable.h)
// on a scripted turntable, without the game: the game library is mapped (not started), the two
// functions the state machine asks (the IO devices and AppConfig) are made to return made-up ones,
// and the hook DLLs given are loaded into the process as the game would load them.
//
//   ttlab <bm2dx.dll> <scenario> [--io N] [--fps N] [-k <hook.dll>]... [--out <trace>]
//
// --io: AppConfig io_type (3 TDJ, 2 LDJ; default 3), --fps: the display's refresh (default 120).
// The trace has a line a frame: frame position state [mark]. The presses and stops go to stdout.
//
// Scenario lines (side 1; side 2 stays still), positions in turntable counts (256 a turn):
//   move <frames> <counts per frame>   turn at that speed (fractions add up)
//   steps <d> <d> ...                  one frame per move
//   fuzz <frames> <seed>               turns, flicks, stops and small moves at random
//   reset <counts>                     turn that far, then the game's PlayInput_Reset (a new stage)
//   mark <label>                       labels the next frame
//   # ...                              a comment

#define NOMINMAX
#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    auto find(std::uint8_t* base, std::size_t size, const char* pattern) -> std::uint8_t*
    {
        auto bytes = std::vector<int> {};
        auto in = std::istringstream { pattern };

        for (auto token = std::string {}; in >> token;)
            bytes.push_back(token == "?" ? -1 : std::stoi(token, nullptr, 16));

        for (auto i = std::size_t {}; i + bytes.size() <= size; ++i)
        {
            auto j = std::size_t {};

            while (j < bytes.size() && (bytes[j] < 0 || base[i + j] == bytes[j]))
                ++j;

            if (j == bytes.size())
                return base + i;
        }

        return nullptr;
    }

    auto follow(std::uint8_t* call) -> std::uint8_t*
        { return call + 5 + *reinterpret_cast<std::int32_t*>(call + 1); }

    /** Makes a function return a fixed pointer: MOV RAX, imm64 ; RET */
    auto returns(std::uint8_t* fn, void* value) -> void
    {
        auto protect = DWORD {};
        VirtualProtect(fn, 11, PAGE_EXECUTE_READWRITE, &protect);
        fn[0] = 0x48, fn[1] = 0xb8;
        *reinterpret_cast<void**>(fn + 2) = value;
        fn[10] = 0xc3;
        VirtualProtect(fn, 11, protect, &protect);
        FlushInstructionCache(GetCurrentProcess(), fn, 11);
    }

    alignas(16) std::uint8_t io[0x100] {};      // +0x58 + side * 8: turntable position
    alignas(16) std::uint8_t config[0x100] {};  // +8 io_type, +0x14 / +0x18 frame rate
    alignas(16) std::uint8_t input[0x400] {};   // the state machine's own
}

int main(int argc, char** argv)
{
    auto game = std::string {}, scenario = std::string {}, out = std::string {};
    auto hooks = std::vector<std::string> {};
    auto io_type = 3, fps = 120;

    for (auto i = 1; i < argc; ++i)
    {
        auto const arg = std::string { argv[i] };

        if (arg == "--io" && i + 1 < argc) io_type = std::stoi(argv[++i]);
        else if (arg == "--fps" && i + 1 < argc) fps = std::stoi(argv[++i]);
        else if (arg == "-k" && i + 1 < argc) hooks.push_back(argv[++i]);
        else if (arg == "--out" && i + 1 < argc) out = argv[++i];
        else if (game.empty()) game = arg;
        else scenario = arg;
    }

    if (game.empty() || scenario.empty())
    {
        std::printf("usage: ttlab <bm2dx.dll> <scenario> [--io N] [--fps N] [-k <hook.dll>]... [--out <trace>]\n");
        return 2;
    }

    auto const module = LoadLibraryExA(game.c_str(), nullptr, DONT_RESOLVE_DLL_REFERENCES);

    if (!module)
    {
        std::printf("cannot map %s (%lu)\n", game.c_str(), GetLastError());
        return 2;
    }

    auto const base = reinterpret_cast<std::uint8_t*>(module);
    auto const nt = reinterpret_cast<PIMAGE_NT_HEADERS>(base + reinterpret_cast<PIMAGE_DOS_HEADER>(base)->e_lfanew);
    auto const size = std::size_t { nt->OptionalHeader.SizeOfImage };

    auto const update = find(base, size,
        "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 54 41 55 41 56 41 57 48 83 EC 20 45 33 E4 "
        "48 8D 59 04 48 8B F1 41 8B EC 45 8B F4 45 8D 7C 24 02 45 8D 6C 24 04 48 63 7B 04 E8 ? ? ? ? "
        "41 8B 4C 06 58 48 8D 04 2F 83 C7");

    if (!update)
    {
        std::printf("no turntable state machine in %s\n", game.c_str());
        return 2;
    }

    auto const ring = update[74] + 2;

    // PlayInput_Reset: frame 0, and each side stopped with the ring full of the position of then
    auto const reset = find(base, size,
        "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 20 45 33 FF 48 8D 79 ? 44 89 39");

    if (!reset)
    {
        std::printf("no turntable reset in %s\n", game.c_str());
        return 2;
    }
    auto const config_call = find(update, 0x200, "E8 ? ? ? ? 8B 13 B9 02 00 00 00 83 78 08 03");

    if (!config_call)
    {
        std::printf("no AppConfig call in the state machine\n");
        return 2;
    }

    *reinterpret_cast<int*>(config + 8) = io_type;
    *reinterpret_cast<int*>(config + 0x14) = fps;
    *reinterpret_cast<int*>(config + 0x18) = fps;
    returns(follow(update + 58), io);
    returns(follow(config_call), config);

    for (auto&& hook: hooks)
        if (!LoadLibraryA(hook.c_str()))
        {
            std::printf("cannot load %s (%lu)\n", hook.c_str(), GetLastError());
            return 2;
        }

    std::printf("%s: %d positions, io_type %d, %d fps%s\n", game.c_str(), ring, io_type, fps, hooks.empty() ? "" : ", hooked");

    // the turntable, a frame at a time
    auto moves = std::vector<double> {};
    auto marks = std::vector<std::string> {};
    auto resets = std::map<std::size_t, double> {}; // before frame: the turn first
    auto file = std::ifstream { scenario };

    for (auto line = std::string {}; std::getline(file, line);)
    {
        auto in = std::istringstream { line.substr(0, line.find('#')) };
        auto word = std::string {};

        if (!(in >> word))
            continue;

        auto mark = [&] { marks.resize(moves.size() + 1); };

        if (word == "move")
        {
            auto frames = 0;
            auto speed = 0.0;
            in >> frames >> speed;
            moves.insert(moves.end(), frames, speed);
        }
        else if (word == "steps")
        {
            for (auto d = 0.0; in >> d;)
                moves.push_back(d);
        }
        else if (word == "fuzz")
        {
            auto frames = 0;
            auto seed = 0u;
            in >> frames >> seed;
            auto random = std::mt19937 { seed };
            auto pick = [&] (int n) { return static_cast<int>(random() % n); };

            while (frames > 0)
            {
                auto const kind = pick(5);
                auto const n = std::min(frames, 1 + pick(kind == 0 ? 60 : 12));
                auto const speed = kind == 0 ? 0.0 : kind == 1 ? (pick(2) ? 1 : -1) * (2 + pick(8))
                    : kind == 2 ? (pick(2) ? 1 : -1) * 0.5 : kind == 3 ? 0.0 : (pick(3) - 1) * 1.0;
                moves.insert(moves.end(), n, speed);
                frames -= n;
            }
        }
        else if (word == "reset")
            in >> resets[moves.size()];
        else if (word == "mark")
        {
            mark();
            in >> marks.back();
        }
    }

    marks.resize(moves.size());

    auto trace = out.empty() ? nullptr : std::fopen(out.c_str(), "w");
    auto turned = 0.0;
    auto was = 0;

    for (auto f = std::size_t {}; f < moves.size(); ++f)
    {
        if (auto const r = resets.find(f); r != resets.end())
        {
            turned += r->second;
            *reinterpret_cast<int*>(io + 0x58) = static_cast<int>(std::lround(turned)) & 0xff;
            reinterpret_cast<void (*)(std::uint8_t*)>(reset)(input);
            std::printf("  %5zu  -- reset\n", f);
        }

        turned += moves[f];
        auto const position = static_cast<int>(std::lround(turned)) & 0xff;
        *reinterpret_cast<int*>(io + 0x58) = position;
        reinterpret_cast<void (*)(std::uint8_t*)>(update)(input);

        auto const state = *reinterpret_cast<int*>(input + 4);

        if (trace)
            std::fprintf(trace, "%zu %d %d%s%s\n", f, position, state, marks[f].empty() ? "" : " ", marks[f].c_str());

        if (!marks[f].empty())
            std::printf("  %5zu  -- %s\n", f, marks[f].c_str());

        if (state == 1 || state == 2 || state == 5 || state == 6)
            std::printf("  %5zu  press: %s %c\n", f, state <= 2 ? "start" : "reverse", state % 2 ? '+' : '-');
        else if (state == 0 && was != 0)
            std::printf("  %5zu  stop\n", f);

        was = state;
    }

    if (trace)
        std::fclose(trace);

    std::fflush(stdout);
    TerminateProcess(GetCurrentProcess(), 0); // no DLL detach for a game library that never started
    return 0;
}
