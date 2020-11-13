#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <string_view>
#include <utility>
#include <variant>

#include <boost/core/noncopyable.hpp>
#include <boost/lockfree/spsc_queue.hpp>

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

constexpr int MY_AUDIO_FREQ = 44100;

class Sdl : private boost::noncopyable {
private:
    SDL_Window* win_ {};
    SDL_Renderer* ren_ {};

public:
    Sdl() {
        ENSURE(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) == 0, "SDL_Init() failed");

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

class Texture : private boost::noncopyable {
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

class TextureLock : private boost::noncopyable {
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

using AudioQueue = boost::lockfree::spsc_queue<i16, boost::lockfree::capacity<16 * (MY_AUDIO_FREQ / 60)>>;

class Audio : private boost::noncopyable {
private:
    AudioQueue queue_ {};

    SDL_AudioDeviceID audio_;
    SDL_AudioSpec spec_;

public:
    explicit Audio(const SDL_AudioCallback callback) {
        SDL_AudioSpec want {};
        want.freq = MY_AUDIO_FREQ;
        want.format = AUDIO_S16SYS;
        want.channels = 1;
        want.samples = 4096;
        want.callback = callback;
        want.userdata = &queue_;

        ENSURE((audio_ = SDL_OpenAudioDevice(nullptr, 0, &want, &spec_, 0)) > 0, "SDL_OpenAudioDevice() failed");

        ENSURE(want.freq == spec_.freq && want.format == spec_.format && want.channels == spec_.channels,
            "spec changed");
    }

    ~Audio() {
        SDL_CloseAudioDevice(audio_);
    }

    [[nodiscard]] AudioQueue& queue() { return queue_; }

    [[nodiscard]] SDL_AudioDeviceID get() const { return audio_; }

    [[nodiscard]] const SDL_AudioSpec& spec() const { return spec_; }

    void pause() const {
        SDL_PauseAudioDevice(audio_, 1);
    }

    void unpause() const {
        SDL_PauseAudioDevice(audio_, 0);
    }
};

void audio_pull(void* const userdata, u8* const stream, const int len) {
    auto& queue = *static_cast<AudioQueue*>(userdata);
    const auto n_sample = len / 2;

    i16* const p = reinterpret_cast<i16*>(stream);
    const auto n_pop = queue.pop(p, n_sample);

    if (n_pop < n_sample) {
        // underflow
        std::fill_n(p + n_pop, n_sample - n_pop, 0);
    }
}

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
            // ここを nxt_ += FRAME_DUR とすると遅れを(可能なら)挽回できるが、
            // 挽回している間 FPS が 60 を超えてしまうのは望ましくないと考えた。
            nxt_ = now + FRAME_DUR;
        }
    }
};

struct CmdQuit {};
struct CmdSave {};
struct CmdLoad {};
struct CmdDump {};
struct CmdEmulate {
    u8 buttons;
};

using Cmd = std::variant<CmdQuit, CmdEmulate, CmdSave, CmdLoad, CmdDump>;

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

    return CmdEmulate { buttons };
}

void draw(const Texture& tex, u8* xbuf) {
    u32* p = nullptr;
    int pitch = -1;
    TextureLock lock(tex, nullptr, reinterpret_cast<void**>(&p), &pitch);

    LOOP(240) {
        for (const auto x : IRANGE(256)) {
            u8 r, g, b;
            fceux_video_get_palette(*xbuf++, &r, &g, &b);
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

void cmd_emulate(const Sdl& sdl, const Texture& tex, Audio& audio, u8 buttons) {
    u8* xbuf;
    i32* soundbuf;
    i32 soundbuf_size;
    fceux_run_frame(buttons, 0, &xbuf, &soundbuf, &soundbuf_size);

    LOOP(soundbuf_size) {
        const i16 sample = (*soundbuf++) & 0xFFFF;
        if (!audio.queue().push(sample)) {
            // overflow
            break;
        }
    }
    draw(tex, xbuf);

    SDL_RenderCopy(sdl.ren(), tex.get(), nullptr, nullptr);
    SDL_RenderPresent(sdl.ren());
}

void mainloop(const Sdl& sdl, const Texture& tex, Audio& audio) {
    auto snap = fceux_snapshot_create();

    audio.unpause();
    Timer timer;
    for (bool running = true; running;) {
        const auto cmd = event();
        std::visit(overload {
                       [&](CmdQuit) { running = false; },
                       [&](CmdSave) { cmd_save(snap); },
                       [&](CmdLoad) { cmd_load(snap); },
                       [&](CmdDump) { cmd_dump(); },
                       [&](CmdEmulate inp) { cmd_emulate(sdl, tex, audio, inp.buttons); },
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

    const Sdl sdl;
    const Texture tex(sdl.ren(), 256, 240);
    Audio audio(audio_pull);

    ENSURE(fceux_init(path_rom) != 0, "fceux_init() failed");
    ENSURE(fceux_sound_set_freq(MY_AUDIO_FREQ) != 0, "fceux_sound_set_freq() failed");

    print_instruction();

    mainloop(sdl, tex, audio);

    fceux_quit();

    return 0;
}
