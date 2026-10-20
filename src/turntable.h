#pragma once

// The turntable state machine of bm2dx, PlayInput_UpdateTurntable(input), for tt_log (edgecasefix has its own copy).
// Once a frame, for each side, it keeps the turntable position in a ring of the last positions and
// sets the side's state from the move over the last 2 frames (|d| < 2 counts as still):
//   0 stopped, 1 / 2 started (+ / -), 3 / 4 turning (+ / -), 5 / 6 reversed to + / -.
// 1, 2, 5 and 6 are presses (PlayInput_IsLanePressed) and last one frame; 3 and 4 are held. A side
// that is still stops when every position in the ring is the same. A reverse within 2 frames of the
// last start or reverse (4 on TDJ, io_type 3) only turns the held direction round, without a press.
//
// The 010 builds keep 16 positions, the 012 builds 8 - the same 133 ms at their own 120 and 60 fps.
// An 012 build forced to TDJ runs at 120 fps and stops a still turntable after 67 ms.
//
// Input (bm2dx 2026081900 0x18081eed0; 2026091600 0x18073e080): +0 frame counter, then per side
// (+4 + side * stride): +0 state, +4 next ring slot, +8 ring[ring], then the frame of the last
// start or reverse.

#include <cstdint>
#include <format>
#include <span>
#include <stdexcept>

#include "exceptions.h"
#include "memory.h"

namespace tt
{
    struct game
    {
        void (*update)(std::uint8_t* input) = nullptr; // PlayInput_UpdateTurntable
        int ring = 0;                                  // positions kept: 16 (010) or 8 (012)
        std::uint8_t* (*config)() = nullptr;           // AppConfig: +8 io_type, +0x18 display refresh
    };

    struct side
    {
        int& state;
        int& slot;
        int* ring;
        int& changed;
    };

    /** Finds the state machine in the game library (bm2dx 32 Pinky Crush and later). */
    inline auto find(std::span<std::uint8_t> region) -> game
    {
        using namespace omnifix;

        // ... MOVSXD RDI,[RBX+4] ; CALL <io> ; MOV ECX,[R14+RAX+58h] ; LEA RAX,[RDI+RBP] ; ADD EDI,ring-2.
        // The first 14 bytes are left out: edgecasefix's jump (JMP [RIP] to its own state machine) may be there.
        auto const at = memory::find(region,
            "? ? ? ? ? ? ? ? ? ? ? ? ? ? 18 57 41 54 41 55 41 56 41 57 48 83 EC 20 45 33 E4 "
            "48 8D 59 04 48 8B F1 41 8B EC 45 8B F4 45 8D 7C 24 02 45 8D 6C 24 04 48 63 7B 04 E8 ? ? ? ? "
            "41 8B 4C 06 58 48 8D 04 2F 83 C7");
        auto out = game {};
        out.update = reinterpret_cast<void (*)(std::uint8_t*)>(at);
        out.ring = at[74] + 2;

        if (out.ring != 8 && out.ring != 16)
            throw error { "unexpected turntable ring of {} positions", out.ring };

        // CALL <AppConfig> ; MOV EDX,[RBX] ; MOV ECX,2 ; CMP DWORD PTR [RAX+8],3 (the reverse window)
        out.config = reinterpret_cast<std::uint8_t* (*)()>(memory::follow(memory::find({ at, at + 0x200 },
            "[E8] ? ? ? ? 8B 13 B9 02 00 00 00 83 78 08 03")));
        return out;
    }

    inline auto at(const game& g, std::uint8_t* input, int s) -> side
    {
        auto const p = reinterpret_cast<int*>(input + 4 + s * (12 + g.ring * 4));
        return { p[0], p[1], p + 2, p[2 + g.ring] };
    }

    /** The position the last update put in the ring. */
    inline auto position(const game& g, const side& s) -> int
        { return s.ring[(s.slot + g.ring - 1) % g.ring]; }

    /** Frames a second the game runs at: 60 on LDJ (io_type 2), else the display's (AppConfig_GetFrameRate). */
    inline auto fps(const game& g) -> int
    {
        auto const config = g.config();
        auto const rate = *reinterpret_cast<int*>(config + 8) == 2 ? 60 : *reinterpret_cast<int*>(config + 0x18);
        return rate >= 30 && rate <= 1000 ? rate : 60;
    }
}
