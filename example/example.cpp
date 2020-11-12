#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <string_view>
#include <utility>
#include <variant>

#include <SDL.h>

#include "fceux.h"

#include "prelude.hpp"

namespace detail {
template <class S, class... Args>
void ENSURE_IMPL(const std::string_view file, const int line, const bool cond, const S& format_str, Args&&... args) {
    if (!cond)
        PANIC_IMPL(file, line, format_str, std::forward<Args>(args)...);
}
}
#define ENSURE(cond, s, ...) detail::ENSURE_IMPL(__FILE__, __LINE__, cond, FMT_STRING(s), ##__VA_ARGS__)

namespace {

class Sdl {
private:
    SDL_Window* win_ {};
    SDL_Renderer* ren_ {};

public:
    Sdl() {
        ENSURE(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0, "SDL_Init() failed");

        ENSURE(win_ = SDL_CreateWindow(
                   "libfceux example",
                   SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                   512, 480,
                   0),
            "SDL_CreateWindow() failed");

        ENSURE(ren_ = SDL_CreateRenderer(win_, -1, 0), "SDL_CreateRenderer() failed");
    }

    ~Sdl() {
        SDL_DestroyRenderer(ren_);
        SDL_DestroyWindow(win_);

        SDL_Quit();
    }

    [[nodiscard]] SDL_Renderer* ren() const { return ren_; }
};

class Texture {
private:
    SDL_Texture* tex_ {};

public:
    Texture(SDL_Renderer* ren, int w, int h) {
        ENSURE(tex_ = SDL_CreateTexture(
                   ren,
                   SDL_PIXELFORMAT_RGBA8888,
                   SDL_TEXTUREACCESS_STREAMING,
                   w, h),
            "SDL_CreateTexture() failed");
    }

    ~Texture() {
        SDL_DestroyTexture(tex_);
    }

    [[nodiscard]] SDL_Texture* get() const { return tex_; }
};

class TextureLock {
private:
    const Texture& tex_;

public:
    TextureLock(const Texture& tex, const SDL_Rect* rect, void** pixels, int* pitch)
        : tex_(tex) {
        ENSURE(SDL_LockTexture(tex.get(), rect, pixels, pitch) == 0, "SDL_LockTexture() failed");
    }

    ~TextureLock() {
        SDL_UnlockTexture(tex_.get());
    }
};

// 60 FPS 固定。
// 時間は 1/6000 秒単位で管理。
class Timer {
private:
    static constexpr u32 FRAME_DUR = 100;

    u32 nxt_; // 次フレームのタイムスタンプ

public:
    static u32 get_timestamp() {
        return 6 * SDL_GetTicks();
    }

    Timer()
        : nxt_(get_timestamp() + FRAME_DUR) {}

    void delay() {
        const u32 now = get_timestamp();
        if (now < nxt_) {
            SDL_Delay((nxt_ - now) / 6);
            nxt_ += FRAME_DUR;
        }
        else {
            // 処理が追いつかない場合、諦めて次から 60 FPS を目指す。
            // ここを nxt_ += FRAME_DIR とすると遅れを(可能なら)挽回できるが、
            // 挽回している間 FPS が 60 を超えてしまうのは望ましくないと考えた。
            nxt_ = now + FRAME_DUR;
        }
    }
};

struct CmdQuit {};
struct CmdSave {};
struct CmdLoad {};
struct CmdDump {};
struct CmdInput {
    u8 buttons;
};

using Cmd = std::variant<CmdQuit, CmdInput, CmdSave, CmdLoad, CmdDump>;

Cmd event() {
    u8 buttons = 0;

    for (SDL_Event ev; SDL_PollEvent(&ev) != 0;) {
        switch (ev.type) {
        case SDL_QUIT: return CmdQuit {};
        case SDL_KEYDOWN: {
            switch (ev.key.keysym.sym) {
            case SDLK_s: return CmdSave {};
            case SDLK_l: return CmdLoad {};
            case SDLK_d: return CmdDump {};
            case SDLK_q: return CmdQuit {};
            default: break;
            }
            break;
        }
        }
    }

    SDL_PumpEvents();
    const u8* keys = SDL_GetKeyboardState(nullptr);
    const auto joykey = [&buttons, &keys](int scancode, int bit) {
        if (bool(keys[scancode]))
            buttons |= 1 << bit;
    };
    joykey(SDL_SCANCODE_Z, 0);
    joykey(SDL_SCANCODE_X, 1);
    joykey(SDL_SCANCODE_V, 2);
    joykey(SDL_SCANCODE_C, 3);
    joykey(SDL_SCANCODE_UP, 4);
    joykey(SDL_SCANCODE_DOWN, 5);
    joykey(SDL_SCANCODE_LEFT, 6);
    joykey(SDL_SCANCODE_RIGHT, 7);

    return CmdInput { buttons };
}

void draw(const Texture& tex, u8* xbuf) {
    u32* p = nullptr;
    int pitch = -1;
    TextureLock lock(tex, nullptr, reinterpret_cast<void**>(&p), &pitch);

    for (const auto y : IRANGE(240)) {
        for (const auto x : IRANGE(256)) {
            u8 r, g, b;
            fceux_palette_get(*xbuf++, &r, &g, &b);
            p[x] = (r << 24) | (g << 16) | (b << 8) | 0xFF;
        }
        p += pitch / sizeof(p[0]);
    }
}

void cmd_save(Snapshot* snap) {
    if (bool(fceux_snapshot_save(snap)))
        EPRINTLN("saved snapshot");
    else
        EPRINTLN("cannot save snapshot");
}

void cmd_load(Snapshot* snap) {
    if (bool(fceux_snapshot_load(snap)))
        EPRINTLN("loaded snapshot");
    else
        EPRINTLN("cannot load snapshot");
}

void cmd_dump() {
    PRINTLN("");
    for (const auto hi : IRANGE(16)) {
        for (const auto lo : IRANGE(16)) {
            const auto addr = (hi << 4) | lo;
            PRINT("{:02X} ", fceux_mem_read(addr, FCEUX_MEMORY_CPU));
        }
        PRINTLN("");
    }
    PRINTLN("");
}

void cmd_input(const Sdl& sdl, const Texture& tex, u8 buttons) {
    u8* xbuf;
    i32* soundbuf;
    i32 soundbuf_size;
    fceux_run_frame(buttons, 0, &xbuf, &soundbuf, &soundbuf_size);

    draw(tex, xbuf);

    SDL_RenderCopy(sdl.ren(), tex.get(), nullptr, nullptr);
    SDL_RenderPresent(sdl.ren());
}

void mainloop(const Sdl& sdl, const Texture& tex) {
    auto snap = fceux_snapshot_create();

    Timer timer;
    for (bool running = true; running;) {
        const auto cmd = event();
        std::visit(overload {
                       [&](CmdQuit) { running = false; },
                       [&](CmdSave) { cmd_save(snap); },
                       [&](CmdLoad) { cmd_load(snap); },
                       [&](CmdDump) { cmd_dump(); },
                       [&](CmdInput inp) { cmd_input(sdl, tex, inp.buttons); },
                   },
            cmd);

        timer.delay();
    }

    fceux_snapshot_destroy(snap);
}

void print_instruction() {
    PRINTLN(R"EOS(Instruction:

Arrow keys      D-pad
z               A
X               B
c               Start
v               Select
s               Save state
l               Load state
d               Dump zero page
q               quit
)EOS");
}

[[noreturn]] void usage() {
    EPRINTLN("Usage: example <game.nes>");
    std::exit(1);
}

} // anonymous namespace

int main(int argc, char** argv) {
    if (argc != 2) usage();
    const auto path_rom = argv[1];

    Sdl sdl;
    Texture tex(sdl.ren(), 256, 240);

    ENSURE(fceux_init(path_rom) != 0, "fceux_init() failed");

    print_instruction();

    mainloop(sdl, tex);

    fceux_quit();

    return 0;
}
