#include <array>
#include <chrono>
#include <cstdint>

#include "driver.h"

#include "fceux.h"
#include "lib-driver.hpp"

namespace {

std::array<std::array<std::uint8_t, 3>, 256> palette {};

} // anonymous namespace

FceuxHookBeforeExec hook_before_exec = nullptr;

// とりあえず定数とする
int KillFCEUXonFrame = 0;
int closeFinishedMovie = 0;
int dendy = 0;
int pal_emulation = 0;
bool swapDuty = false;
bool turbo = false;

//--------------------------------------------------------------------
// hook
//--------------------------------------------------------------------

void FCEUD_CallHookBeforeExec(std::uint16_t addr) {
    if(hook_before_exec)
        hook_before_exec(addr);
}

//--------------------------------------------------------------------
// message
//--------------------------------------------------------------------

// TODO: 本来はコールバックなどを通して出力が得られるべき

void FCEUD_PrintError(const char* s) {}
void FCEUD_Message(const char* s) {}

//--------------------------------------------------------------------
// Lua
//--------------------------------------------------------------------

// Lua はサポートしない

void WinLuaOnStart(intptr_t) {}
void WinLuaOnStop(intptr_t) {}

int LuaKillMessageBox() {
    return 0;
}

void PrintToWindowConsole(intptr_t, const char* str) {}

int LuaPrintfToWindowConsole(const char* format, ...) {
    return -1;
}

//--------------------------------------------------------------------
// file I/O
//--------------------------------------------------------------------

// とりあえずアーカイブは未サポート

std::FILE* FCEUD_UTF8fopen(const char* fn, const char* mode) {
    return std::fopen(fn, mode);
}

EMUFILE_FILE* FCEUD_UTF8_fstream(const char* n, const char* m) {
    return new EMUFILE_FILE(n, m);
}

FCEUFILE* FCEUD_OpenArchiveIndex(ArchiveScanRecord&, std::string&, int) {
    return nullptr;
}

FCEUFILE* FCEUD_OpenArchiveIndex(ArchiveScanRecord&, std::string&, int, int*) {
    return nullptr;
}

FCEUFILE* FCEUD_OpenArchive(ArchiveScanRecord&, std::string&, std::string*) {
    return nullptr;
}

FCEUFILE* FCEUD_OpenArchive(ArchiveScanRecord&, std::string&, std::string*, int*) {
    return nullptr;
}

ArchiveScanRecord FCEUD_ScanArchive(std::string) {
    return {};
}

//--------------------------------------------------------------------
// load
//--------------------------------------------------------------------

int LoadGame(const char* path, bool silent) {
    static bool is_loaded = false;

    if (is_loaded) FCEUI_CloseGame();

    if (!FCEUI_LoadGame(path, 1, silent)) return 0;

    is_loaded = true;

    return 1;
}

// サポートしない
int reloadLastGame() {
    return 0;
}

//--------------------------------------------------------------------
// video
//--------------------------------------------------------------------

void FCEUD_SetPalette(uint8 index, uint8 r, uint8 g, uint8 b) {
    palette[index] = {r, g, b};
}

void FCEUD_GetPalette(uint8 i, uint8* r, uint8* g, uint8* b) {
    *r = palette[i][0];
    *g = palette[i][1];
    *b = palette[i][2];
}

//--------------------------------------------------------------------
// netplay
//--------------------------------------------------------------------

// サポートしない

int FCEUD_SendData(void*, uint32) {
    return 0;
}

int FCEUD_RecvData(void*, uint32) {
    return 0;
}

void FCEUD_NetplayText(uint8*) {}

void FCEUD_NetworkClose() {}

//--------------------------------------------------------------------
// sound
//--------------------------------------------------------------------

// とりあえず未サポート

void FCEUD_SoundToggle() {}
void FCEUD_SoundVolumeAdjust(int) {}

//--------------------------------------------------------------------
// savestate
//--------------------------------------------------------------------

// ライブラリでは使わない

void FCEUD_SaveStateAs() {}
void FCEUD_LoadStateFrom() {}

//--------------------------------------------------------------------
// input
//--------------------------------------------------------------------

// とりあえず標準コントローラーのみサポート

void FCEUI_UseInputPreset(int) {}

void FCEUD_SetInput(bool fourscore, bool microphone, ESI port0, ESI port1, ESIFC fcexp) {}

bool FCEUD_ShouldDrawInputAids() {
    return false;
}

void GetMouseData(uint32 (&md)[3]) {
    md[0] = 0;
    md[1] = 0;
    md[2] = 0;
}

unsigned int* GetKeyboard() {
    return nullptr;
}

//--------------------------------------------------------------------
// movie
//--------------------------------------------------------------------

// ライブラリでは使わない

void FCEUD_MovieRecordTo() {}
void FCEUD_MovieReplayFrom() {}

//--------------------------------------------------------------------
// avi
//--------------------------------------------------------------------

// ライブラリでは使わない

void FCEUI_AviVideoUpdate(const unsigned char*) {}

bool FCEUI_AviEnableHUDrecording() {
    return false;
}

bool FCEUI_AviDisableMovieMessages() {
    return false;
}

bool FCEUI_AviIsRecording() {
    return false;
}

void FCEUD_AviRecordTo() {}
void FCEUD_AviStop() {}

int FCEUD_ShowStatusIcon() {
    return 0;
}
void FCEUD_ToggleStatusIcon() {}
void FCEUD_HideMenuToggle() {}

void FCEUD_DebugBreakpoint(int) {}
void FCEUD_TraceInstruction(uint8*, int) {}
void FCEUD_UpdateNTView(int, bool) {}
void FCEUD_UpdatePPUView(int, int) {}

bool FCEUD_PauseAfterPlayback() {
    return false;
}

//--------------------------------------------------------------------
// time
//--------------------------------------------------------------------

uint64 FCEUD_GetTime() {
    using Clock = std::chrono::steady_clock;

    static const auto START = Clock::now();

    const auto now = Clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - START).count();
}

uint64 FCEUD_GetTimeFreq() {
    return 1000;
}

void FCEUD_SetEmulationSpeed(int) {}
void FCEUD_TurboOn() {}
void FCEUD_TurboOff() {}
void FCEUD_TurboToggle() {}

void RefreshThrottleFPS() {}

//--------------------------------------------------------------------
// misc
//--------------------------------------------------------------------

const char* FCEUD_GetCompilerString() {
    return "g++";
}

// とりあえず NTSC 決め打ち
void FCEUD_VideoChanged() {}
