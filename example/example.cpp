// TODO: 実際に操作できる簡易エミュレータにする(絵と音が出る)
// ステートセーブ/ロード、ゼロページのダンプなどを付ける

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "fceux.h"

namespace {

void hook(std::uint16_t addr) {
    std::fprintf(stderr, "%04X\n", addr);
}

void ERROR(const std::string& msg) {
    std::fputs(msg.c_str(), stderr);
    std::putc('\n', stderr);
    std::exit(1);
}

void usage() {
    ERROR("Usage: example <game.nes>");
}

} // anonymous namespace

int main(int argc, char** argv) {
    if(argc != 2) usage();
    const auto path_rom = argv[1];

    if(fceux_init(path_rom) == 0) ERROR("fceux_init() failed");

    std::uint8_t* xbuf;
    std::int32_t* soundbuf;
    std::int32_t soundbuf_size;
    for(int i = 0; i < 30; ++i)
        fceux_run_frame(0, 0, &xbuf, &soundbuf, &soundbuf_size);

    auto snap = fceux_snapshot_create();
    fceux_snapshot_save(snap);

    std::fputs("--- hook start ---\n", stderr);
    fceux_hook_before_exec(hook);
    fceux_run_frame(0, 0, &xbuf, &soundbuf, &soundbuf_size);
    fceux_hook_before_exec(nullptr);
    std::fputs("--- hook end ---\n", stderr);
    fceux_run_frame(0, 0, &xbuf, &soundbuf, &soundbuf_size);

    std::printf("%02X\n", fceux_read(0x90, FCEUX_MEMORY_CPU));
    fceux_write(0x90, 1, FCEUX_MEMORY_CPU);
    std::printf("%02X\n", fceux_read(0x90, FCEUX_MEMORY_CPU));

    fceux_snapshot_load(snap);
    std::printf("%02X\n", fceux_read(0x90, FCEUX_MEMORY_CPU));

    fceux_snapshot_destroy(snap);

    fceux_quit();

    return 0;
}
