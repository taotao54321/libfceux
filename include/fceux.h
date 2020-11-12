#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int fceux_init(const char* path_rom);
void fceux_quit(void);

// joy1, joy2 は RLDUTSBA 形式。
//
// フレーム境界以外から呼び出したときの動作は未定義。
// よって、フック関数内で呼び出してはならない。
// また、フレーム境界でないスナップショットに対して利用してはならない。
void fceux_run_frame(uint8_t joy1, uint8_t joy2, uint8_t** xbuf, int32_t** soundbuf, int32_t* soundbuf_size);

enum FceuxMemoryDomain {
    FCEUX_MEMORY_CPU,
};

uint8_t fceux_mem_read(uint16_t addr, enum FceuxMemoryDomain domain);
void fceux_mem_write(uint16_t addr, uint8_t value, enum FceuxMemoryDomain domain);

struct Snapshot;

struct Snapshot* fceux_snapshot_create();
void fceux_snapshot_destroy(struct Snapshot* snap);
int fceux_snapshot_load(struct Snapshot* snap);
int fceux_snapshot_save(struct Snapshot* snap);

typedef void (*FceuxHookBeforeExec)(uint16_t addr);

// CPU 命令実行前に呼ばれる関数 hook を登録する。
// フック関数は 1 つのみ登録可能。フック関数が既にある場合は単に置き換えられる。
// NULL を渡すと登録解除される。
//
// より複雑なフック機構はこの関数を用いてクライアント側で実装可能なはず。
void fceux_hook_before_exec(FceuxHookBeforeExec hook);

// xbuf の Byte に対応する RGB 値を得る。
void fceux_video_get_palette(uint8_t idx, uint8_t* r, uint8_t* g, uint8_t* b);

// サンプリングレート設定。
// 0, 44100, 48000, 96000 のみが指定できる。
// 0 を指定するとサウンドが無効になる。
int fceux_sound_set_freq(int freq);

#ifdef __cplusplus
}
#endif
