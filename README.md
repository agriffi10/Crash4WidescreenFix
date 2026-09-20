# Crash Bandicoot 4 - Widescreen Fix
## Description
This is a small mod that allows _Crash Bandicoot 4: It's About Time_ to run at aspect ratios wider than 16:9. Unlike
previous hex-editor based fixes, this correctly scales the field of view at all times for a perfect Hor+ widescreen
scale. FMV cutscenes are displayed at their original 16:9 aspect ratio with black bars rather than stretched.

## Instructions
1. Download the [.zip file containing the mod](https://github.com/RibShark/Crash4WidescreenFix/releases/latest).
2. Extract all files to `<game folder>\Lava\Binaries\Win64`, so that `dsound.dll` resides in the same folder as 
`CrashBandicoot4.exe` or `Lava-Win64-Shipping.exe`.

## Building
Requires Visual Studio 2022 or newer with the _Desktop development with C++_ workload, which includes both MSVC and
CMake, and Git. MinGW is not supported, as ModUtils is MSVC-only.

From the _x64 Native Tools Command Prompt for VS_, in a folder you can write to:
```
git clone https://github.com/agriffi10/Crash4WidescreenFix.git
cd Crash4WidescreenFix
git submodule update --init
cmake -B build -A x64
cmake --build build --config Release
```
This produces `build\Release\Crash4WidescreenFix.asi`. To test it, copy it over the `.asi` from an existing
installation of the mod, so that `dsound.dll` is present alongside it. The mod logs to the debugger, so
[DebugView](https://learn.microsoft.com/sysinternals/downloads/debugview) will show which patches were applied and
when a cutscene starts and stops.

Note that `src/stdafx.h` includes the standard headers that ModUtils itself relies on, and provides a replacement for
`stdext::make_checked_array_iterator`, which current MSVC no longer ships. Without these the pinned version of
ModUtils no longer compiles.

## Credits
[ThirteenAG](https://github.com/ThirteenAG) - [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader)\
[Silent](https://github.com/CookiePLMonster) - [ModUtils](https://github.com/CookiePLMonster/ModUtils)
