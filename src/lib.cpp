#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <iterator>

#include <zlib.h>

#include "debug.h"
#include "driver.h"
#include "emufile.h"
#include "fceu.h"
#include "state.h"

#include "fceux.h"
#include "lib-driver.hpp"

#define LIBFCEUX extern "C"

namespace {

std::uint32_t joypad_data = 0;

} // anonymous namespace

struct Snapshot {
    EMUFILE_MEMORY file {};

    Snapshot() = default;
};

LIBFCEUX int fceux_init(const char* path_rom) {
    if (!FCEUI_Initialize()) return 0;

    if (LoadGame(path_rom, true) == 0) return 0;

    // 標準コントローラーのみサポート
    FCEUI_SetInput(0, SI_GAMEPAD, &joypad_data, 0);
    FCEUI_SetInput(1, SI_GAMEPAD, &joypad_data, 0);
    FCEUI_SetInputFC(SIFC_NONE, nullptr, 0);
    FCEUI_SetInputFourscore(false);

    return 1;
}

LIBFCEUX void fceux_quit(void) {
    FCEUI_Kill();
}

LIBFCEUX void fceux_run_frame(
    std::uint8_t joy1, std::uint8_t joy2,
    std::uint8_t** xbuf, std::int32_t** soundbuf, std::int32_t* soundbuf_size)
{
    joypad_data = joy1 | (joy2<<8);

    FCEUI_Emulate(xbuf, soundbuf, soundbuf_size, 0);
}

LIBFCEUX std::uint8_t fceux_mem_read(std::uint16_t addr, enum FceuxMemoryDomain domain) {
    switch(domain) {
    case FCEUX_MEMORY_CPU:
        return GetMem(addr);
    default: assert(false);
    }
}

LIBFCEUX void fceux_mem_write(std::uint16_t addr, std::uint8_t value, enum FceuxMemoryDomain domain) {
    switch(domain) {
    case FCEUX_MEMORY_CPU:
        BWrite[addr](addr, value);
        break;
    default: assert(false);
    }
}

LIBFCEUX struct Snapshot* fceux_snapshot_create() {
    return new Snapshot;
}

LIBFCEUX void fceux_snapshot_destroy(struct Snapshot* snap) {
    delete snap;
}

LIBFCEUX int fceux_snapshot_load(struct Snapshot* snap) {
    snap->file.fseek(0, SEEK_SET);

    return FCEUSS_LoadFP(&snap->file, SSLOADPARAM_NOBACKUP) ? 1 : 0;
}

LIBFCEUX int fceux_snapshot_save(struct Snapshot* snap) {
    snap->file.truncate(0);

    return FCEUSS_SaveMS(&snap->file, Z_NO_COMPRESSION) ? 1 : 0;
}

LIBFCEUX void fceux_hook_before_exec(FceuxHookBeforeExec hook, void* userdata) {
    hook_before_exec = hook;
    hook_before_exec_userdata = userdata;
}

LIBFCEUX void fceux_video_get_palette(std::uint8_t idx, std::uint8_t* r, std::uint8_t* g, std::uint8_t* b) {
    FCEUD_GetPalette(idx, r, g, b);
}

LIBFCEUX int fceux_sound_set_freq(int freq) {
    using std::begin;
    using std::end;

    constexpr int FREQS_VALID[] { 0, 44100, 48000, 96000 };
    if(std::find(begin(FREQS_VALID), end(FREQS_VALID), freq) == end(FREQS_VALID))
        return 0;

    FCEUI_Sound(freq);

    return 1;
}
