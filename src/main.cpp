#define _USE_MATH_DEFINES
#include <cmath>
#include <Trampoline.h>
#include "MemoryMgr.h"
#include "Patterns.h"
#include "FmvDetection.h"
#include <atomic>

namespace WidescreenFix
{
    static float hFov;
    static float* pWidth;
    static float* pHeight;

    static float aspectPrevious;
    static float hFovPrevious_precalc;
    static float hFovPrevious_postcalc;

    static uint8_t* pConstrainAspectRatio;
    static uint8_t constrainAspectRatioOriginal;
    static std::atomic<bool> fmvPlaying;

    // FMVs are drawn in the player's screen layer, which Unreal only insets to 16:9 while the camera
    // constrains its aspect ratio, so the game's own constraint is restored for as long as one plays
    static void OnFmvPlayingChanged(bool playing) {
        OutputDebugStringA(playing ? "Crash4WidescreenFix: FMV started, constraining to 16:9\n"
                                   : "Crash4WidescreenFix: FMV stopped, releasing 16:9 constraint\n");
        fmvPlaying = playing;
        Memory::VP::Patch<uint8_t>(pConstrainAspectRatio, playing ? constrainAspectRatioOriginal : 0);
        FlushInstructionCache(GetCurrentProcess(), pConstrainAspectRatio, 1);
    }

    static void CalculateNew_hFov() {
        // The view is constrained to 16:9 during FMVs, so the game's FOV is already correct
        if (fmvPlaying)
            return;

        float aspect = *pWidth / *pHeight;

        if (aspect > 1.777778) {
            if (hFovPrevious_precalc == hFov && aspectPrevious == aspect) {
                // caching to not have to calculate on each frame
                hFov = hFovPrevious_postcalc;
                return;
            }

            // for caching
            hFovPrevious_precalc = hFov;
            aspectPrevious = aspect;

            // Calculation taken from https://www.purebasic.fr/english/viewtopic.php?t=37014
            hFov = atan(tan((hFov * M_PI) / 360.0) * aspect / (16.0 / 9.0)) * 360.0 / M_PI;
            hFovPrevious_postcalc = hFov;
        }
    }
}

void OnInitializeHook() {
    using namespace Memory;
    using namespace hook::txn;

    std::unique_ptr<ScopedUnprotect::Unprotect> Protect = ScopedUnprotect::UnprotectSectionOrFullModule( GetModuleHandle( nullptr ), ".text" );

    try {
        using namespace WidescreenFix;

        auto resPattern = pattern ("C7 41 68 00 00 48 8B 05"); // 0x1415AF1DC
        ReadOffsetValue(resPattern.get_first(8), pWidth);
        pHeight = pWidth + 1;
    }
    TXN_CATCH();

    try {
        using namespace WidescreenFix;

        auto cameraPattern = pattern ("F3 0F 11 47 18 8B 83 00 02 00 00"); // 0x141E6C84F

        // First, disable bConstrainAspectRatio, re-enabling it only while an FMV is playing
        pConstrainAspectRatio = cameraPattern.get_first<uint8_t>(26);
        constrainAspectRatioOriginal = *pConstrainAspectRatio;
        Patch<uint8_t>(pConstrainAspectRatio, 0);
        if (!FmvDetection::Install(OnFmvPlayingChanged))
            OutputDebugStringA("Crash4WidescreenFix: MFCreateMediaSession import not found, FMVs will stay stretched\n");

        Trampoline* trampoline = Trampoline::MakeTrampoline(cameraPattern.get_first() );
        auto calculateTrampoline = trampoline->Jump(CalculateNew_hFov);

        auto jmpAddr = cameraPattern.get_first(5);

        const uint8_t shellcode[] = {
                0xF3, 0x0F, 0x11, 0x47, 0x18, // movss, [rdi+18h], xmm0 - original instruction
                0x8B, 0x47, 0x18, // mov eax, [rdi+18h]
                0xA3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // movabs [hFov], eax
                0xE8, 0x00, 0x00, 0x00, 0x00, // call ds:[CalculateNew_hFov]
                0xA1, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // movabs eax, [hFov]
                0x89, 0x47, 0x18, // mov [rdi+18h], eax
                0xE9, 0x00, 0x00, 0x00, 0x00 // jmp jmpAddr
        };

        std::byte* space = trampoline->RawSpace( sizeof(shellcode) );
        memcpy_s( space, sizeof(shellcode), shellcode, sizeof(shellcode) );

        // Fill pointers
        Patch( space + 5 + 3 + 1, &hFov );
        WriteOffsetValue( space + 5 + 3 + 9 + 1, calculateTrampoline );
        Patch( space + 5 + 3 + 9 + 5 + 1, &hFov );
        WriteOffsetValue( space + 5 + 3 + 9 + 5 + 9 + 3 + 1, jmpAddr);

        InjectHook(cameraPattern.get_first(), space, PATCH_JUMP );
    }
    TXN_CATCH();
}