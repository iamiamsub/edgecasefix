// edgecasefix.dll - an 012 build of beatmania IIDX forced to TDJ stops a still turntable as the 010 builds
// do: after 133 ms, not 67 ms.
//
// The 012 builds keep the last 8 turntable positions, made for LDJ at 60 fps; at TDJ's 120 fps they
// stop the turntable after 8 frames still (turntable.h). After a backspin scratch ended with a
// reverse, an analog turntable that the hand stops and that moves a little again within 133 ms then
// starts afresh - a press, which takes the next scratch too early (BAD, combo break) and leaves the
// real one nothing to hit. The 010 builds, keeping 16, see the same move as the turn going on.
//
// So the game's PlayInput_UpdateTurntable is replaced by the same state machine, line for line,
// except that a side stops when its position has been the same for 8 frames at 60 fps' length (16 at
// 120 fps), counted here. Nothing changes at 60 fps (LDJ) or on any other build: only the function
// exactly as in the 012 builds is replaced. Load it with "-k edgecasefix.dll", before tt_log.dll.
//
// Kept small on purpose (no C runtime, no hook library): the DLL is this file and four kernel32 calls.

#include <windows.h>

namespace
{
    // PlayInput_UpdateTurntable of the 012 builds (2026090400 0x1807509e0, 2026091600 0x18073e080),
    // with zeros for the targets of its calls of the IO devices (+0x3a) and AppConfig (+0xf8).
    unsigned char const original[] = {
        0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c, 0x24, 0x10, 0x48, 0x89, 0x74, 0x24, 0x18, 0x57,
        0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x83, 0xec, 0x20, 0x45, 0x33, 0xe4, 0x48,
        0x8d, 0x59, 0x04, 0x48, 0x8b, 0xf1, 0x41, 0x8b, 0xec, 0x45, 0x8b, 0xf4, 0x45, 0x8d, 0x7c, 0x24,
        0x02, 0x45, 0x8d, 0x6c, 0x24, 0x04, 0x48, 0x63, 0x7b, 0x04, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x41,
        0x8b, 0x4c, 0x06, 0x58, 0x48, 0x8d, 0x04, 0x2f, 0x83, 0xc7, 0x06, 0x89, 0x4c, 0x86, 0x0c, 0x81,
        0xe7, 0x07, 0x00, 0x00, 0x80, 0x7d, 0x07, 0xff, 0xcf, 0x83, 0xcf, 0xf8, 0xff, 0xc7, 0x48, 0x63,
        0xc7, 0x48, 0x03, 0xc5, 0x2b, 0x4c, 0x86, 0x0c, 0x83, 0xf9, 0x7f, 0x8d, 0x81, 0x00, 0xff, 0xff,
        0xff, 0x0f, 0x4e, 0xc1, 0x83, 0xf8, 0x81, 0x44, 0x8d, 0x80, 0x00, 0x01, 0x00, 0x00, 0x44, 0x0f,
        0x4d, 0xc0, 0x8b, 0x43, 0x04, 0xff, 0xc0, 0x25, 0x07, 0x00, 0x00, 0x80, 0x7d, 0x07, 0xff, 0xc8,
        0x83, 0xc8, 0xf8, 0xff, 0xc0, 0x8b, 0x0b, 0x89, 0x43, 0x04, 0x83, 0xf9, 0x01, 0x74, 0x0c, 0x83,
        0xf9, 0x05, 0x74, 0x07, 0x83, 0xf9, 0x02, 0x74, 0x12, 0xeb, 0x0b, 0xc7, 0x03, 0x03, 0x00, 0x00,
        0x00, 0xb9, 0x03, 0x00, 0x00, 0x00, 0x83, 0xf9, 0x06, 0x75, 0x06, 0x44, 0x89, 0x2b, 0x41, 0x8b,
        0xcd, 0x41, 0x8b, 0xc0, 0x99, 0x33, 0xc2, 0x2b, 0xc2, 0x83, 0xf8, 0x02, 0x7c, 0x69, 0x85, 0xc9,
        0x75, 0x12, 0x45, 0x85, 0xc0, 0x41, 0x8b, 0xc4, 0x0f, 0x9e, 0xc0, 0xff, 0xc0, 0x89, 0x03, 0x8b,
        0x06, 0x89, 0x43, 0x28, 0x45, 0x85, 0xc0, 0x7e, 0x08, 0x44, 0x39, 0x2b, 0x74, 0x0a, 0x45, 0x85,
        0xc0, 0x79, 0x65, 0x83, 0x3b, 0x03, 0x75, 0x60, 0xe8, 0x00, 0x00, 0x00, 0x00, 0x8b, 0x13, 0xb9,
        0x02, 0x00, 0x00, 0x00, 0x83, 0x78, 0x08, 0x03, 0x8b, 0x06, 0x41, 0x0f, 0x44, 0xcd, 0x2b, 0x43,
        0x28, 0x3b, 0xc1, 0x41, 0x8b, 0xc4, 0x7d, 0x0d, 0x83, 0xfa, 0x03, 0x0f, 0x94, 0xc0, 0x83, 0xc0,
        0x03, 0x89, 0x03, 0xeb, 0x33, 0x83, 0xfa, 0x03, 0x0f, 0x94, 0xc0, 0x83, 0xc0, 0x05, 0x89, 0x03,
        0x8b, 0x06, 0x89, 0x43, 0x28, 0xeb, 0x21, 0x85, 0xc9, 0x74, 0x1d, 0x8b, 0x53, 0x08, 0x48, 0x8d,
        0x43, 0x08, 0x49, 0x8b, 0xcc, 0x39, 0x10, 0x75, 0x0f, 0x48, 0xff, 0xc1, 0x49, 0x03, 0xc5, 0x48,
        0x83, 0xf9, 0x08, 0x7c, 0xf0, 0x44, 0x89, 0x23, 0x49, 0x83, 0xc6, 0x08, 0x48, 0x83, 0xc5, 0x0b,
        0x48, 0x83, 0xc3, 0x2c, 0x49, 0x83, 0xef, 0x01, 0x0f, 0x85, 0xc8, 0xfe, 0xff, 0xff, 0xff, 0x06,
        0x48, 0x8b, 0x74, 0x24, 0x60, 0x48, 0x8b, 0x5c, 0x24, 0x50, 0x48, 0x8b, 0x6c, 0x24, 0x58, 0x48,
        0x83, 0xc4, 0x20, 0x41, 0x5f, 0x41, 0x5e, 0x41, 0x5d, 0x41, 0x5c, 0x5f, 0xc3,
    };

    unsigned char* (*io)();     // +0x58 + side * 8: the turntable position
    unsigned char* (*config)(); // +8 io_type, +0x18 display refresh

    int last[2];                // per side, the last position
    int same[2];                // and for how many updates before it has been the same

    auto matches(unsigned char const* p) -> bool
    {
        for (auto i = 0; i < static_cast<int>(sizeof original); ++i)
            if (p[i] != original[i] && !(i > 0x3a && i < 0x3f) && !(i > 0xf8 && i < 0xfd))
                return false;

        return true;
    }

    auto target(unsigned char* call) -> unsigned char*
        { return call + 5 + *reinterpret_cast<int*>(call + 1); }

    // input: +0 frame (0 again after PlayInput_Reset), then per side (+4 + side * 0x2c) state, next ring
    // slot, ring[8] and the frame of the last start or reverse; the states are turntable.h's
    auto update(unsigned char* input) -> void
    {
        auto const frame = *reinterpret_cast<int*>(input);
        auto const io_type = *reinterpret_cast<int*>(config() + 8);
        auto const fps = io_type == 2 ? 60 : *reinterpret_cast<int*>(config() + 0x18);
        auto const frames = fps > 60 && fps <= 1000 ? (8 * fps + 30) / 60 : 8;

        for (auto s = 0; s < 2; ++s)
        {
            auto const side = reinterpret_cast<int*>(input + 4 + s * 0x2c);
            auto& state = side[0];
            auto& slot = side[1];
            auto const ring = side + 2;
            auto& changed = side[10];
            auto const position = *reinterpret_cast<int*>(io() + 0x58 + s * 8);

            if (frame == 0) // PlayInput_Reset filled the ring with the position of then
                last[s] = ring[slot], same[s] = 1 << 20;

            same[s] = position == last[s] ? same[s] + (same[s] < 1 << 20) : 0;
            last[s] = position;

            ring[slot] = position;
            auto d = position - ring[(slot + 6) & 7];
            d = d <= 127 ? d : d - 256;
            d = d >= -127 ? d : d + 256;
            slot = (slot + 1) & 7;

            if (state == 1 || state == 5)
                state = 3;
            else if (state == 2 || state == 6)
                state = 4;

            if (d >= 2 || d <= -2)
            {
                if (state == 0)
                    state = d > 0 ? 1 : 2, changed = frame;

                if ((d > 0 && state == 4) || (d < 0 && state == 3))
                {
                    if (frame - changed < (io_type == 3 ? 4 : 2))
                        state = state == 3 ? 4 : 3;
                    else
                        state = state == 3 ? 6 : 5, changed = frame;
                }
            }
            else if (state != 0 && same[s] >= frames - 1) // the game: all 8 in the ring the same
                state = 0;
        }

        *reinterpret_cast<int*>(input) = frame + 1;
    }

    auto install(unsigned char* base) -> void
    {
        auto const nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
        auto section = IMAGE_FIRST_SECTION(nt);

        for (auto n = 0; n < nt->FileHeader.NumberOfSections; ++n, ++section)
        {
            if (!(section->Characteristics & IMAGE_SCN_MEM_EXECUTE))
                continue;

            auto const start = base + section->VirtualAddress;
            auto const end = start + section->Misc.VirtualSize;

            for (auto p = start; p + sizeof original <= end; ++p)
            {
                if (!matches(p))
                    continue;

                io = reinterpret_cast<unsigned char* (*)()>(target(p + 0x3a));
                config = reinterpret_cast<unsigned char* (*)()>(target(p + 0xf8));

                // JMP [RIP] to update; the rest of the game's function is never run
                auto protect = DWORD {};

                if (!VirtualProtect(p, 14, PAGE_EXECUTE_READWRITE, &protect))
                    return;

                p[0] = 0xff, p[1] = 0x25;
                *reinterpret_cast<int*>(p + 2) = 0;
                *reinterpret_cast<void**>(p + 6) = reinterpret_cast<void*>(&update);
                VirtualProtect(p, 14, protect, &protect);
                FlushInstructionCache(GetCurrentProcess(), p, 14);
                return;
            }
        }
    }
}

extern "C" auto WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) -> BOOL
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        auto game = GetModuleHandleA("bm2dx.dll");

        if (!game)
            game = GetModuleHandleA("bm2dx_omni.dll");

        if (game)
            install(reinterpret_cast<unsigned char*>(game));
    }

    return TRUE;
}
