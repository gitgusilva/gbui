// The SDL2 backend: a window, an event pump, and a streaming texture the
// software canvas is uploaded into once a frame.
//
// SDL is used for the platform, not for drawing. Everything visible is
// rasterised by Canvas, which keeps the output identical to what the SVG
// backend produces and means a second platform is a copy of this file with the
// SDL calls swapped.
#include "gbui/platform/window.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstdio>
#include <map>

namespace gbui {
namespace {

Key translate(SDL_Keycode code) {
    switch (code) {
        case SDLK_LEFT: return Key::Left;
        case SDLK_RIGHT: return Key::Right;
        case SDLK_UP: return Key::Up;
        case SDLK_DOWN: return Key::Down;
        case SDLK_HOME: return Key::Home;
        case SDLK_END: return Key::End;
        case SDLK_PAGEUP: return Key::PageUp;
        case SDLK_PAGEDOWN: return Key::PageDown;
        case SDLK_BACKSPACE: return Key::Backspace;
        case SDLK_DELETE: return Key::Delete;
        case SDLK_RETURN:
        case SDLK_KP_ENTER: return Key::Return;
        case SDLK_TAB: return Key::Tab;
        case SDLK_ESCAPE: return Key::Escape;
        case SDLK_SPACE: return Key::Space;
        case SDLK_a: return Key::A;
        case SDLK_c: return Key::C;
        case SDLK_v: return Key::V;
        case SDLK_x: return Key::X;
        case SDLK_z: return Key::Z;
        case SDLK_y: return Key::Y;
        case SDLK_t: return Key::T;
        case SDLK_d: return Key::D;
        case SDLK_PLUS:
        case SDLK_EQUALS:
        case SDLK_KP_PLUS: return Key::Plus;
        case SDLK_MINUS:
        case SDLK_KP_MINUS: return Key::Minus;
        default: return Key::Unknown;
    }
}

Modifiers modifiersOf(SDL_Keymod state) {
    Modifiers modifiers;
    modifiers.shift = (state & KMOD_SHIFT) != 0;
    modifiers.ctrl = (state & KMOD_CTRL) != 0;
    modifiers.alt = (state & KMOD_ALT) != 0;
    modifiers.super = (state & KMOD_GUI) != 0;
    return modifiers;
}

class Sdl2Window final : public Window {
public:
    ~Sdl2Window() override {
        // Before SDL_Quit, and before the renderer: a cursor outlives neither.
        SDL_SetCursor(SDL_GetDefaultCursor());
        for (auto& [cursor, handle] : cursors_) {
            if (handle) SDL_FreeCursor(handle);
        }
        if (texture_) SDL_DestroyTexture(texture_);
        if (renderer_) SDL_DestroyRenderer(renderer_);
        if (window_) SDL_DestroyWindow(window_);
        SDL_Quit();
    }

    bool open(const WindowOptions& options) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            std::fprintf(stderr, "gbui: SDL_Init failed: %s\n", SDL_GetError());
            return false;
        }
        window_ = SDL_CreateWindow(options.title.c_str(), SDL_WINDOWPOS_CENTERED,
                                   SDL_WINDOWPOS_CENTERED, options.width, options.height,
                                   SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE |
                                       SDL_WINDOW_ALLOW_HIGHDPI);
        if (!window_) {
            std::fprintf(stderr, "gbui: SDL_CreateWindow failed: %s\n", SDL_GetError());
            return false;
        }
        SDL_SetWindowMinimumSize(window_, options.minWidth, options.minHeight);
        // Without this SDL never sends SDL_TEXTINPUT, and a text field would
        // have to guess a layout from key codes — wrong for every keyboard that
        // is not the one the developer owns.
        SDL_StartTextInput();

        // No vsync flag: the loop above decides when to redraw, and a UI that
        // only paints on change should not be pinned to the refresh rate.
        const Uint32 pacing = options.vsync ? static_cast<Uint32>(SDL_RENDERER_PRESENTVSYNC) : 0u;
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | pacing);
        if (!renderer_) renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE | pacing);
        if (!renderer_) {
            std::fprintf(stderr, "gbui: SDL_CreateRenderer failed: %s\n", SDL_GetError());
            return false;
        }

        int drawableWidth = 0;
        int drawableHeight = 0;
        SDL_GetRendererOutputSize(renderer_, &drawableWidth, &drawableHeight);
        resizeSurface(drawableWidth, drawableHeight);
        return true;
    }

    bool pumpEvents() override {
        resized_ = false;
        // Read as a level once per frame rather than tracked through key
        // events: a wheel turned with Ctrl held generates no key event at all.
        input_.modifiers = modifiersOf(SDL_GetModState());
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT: return false;
                case SDL_WINDOWEVENT:
                    // Noted, not acted on. A drag on a window edge delivers a
                    // size event per compositor tick, and answering each one
                    // means reallocating and re-laying out for sizes that were
                    // never on screen. Only the last one this frame is real.
                    if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED ||
                        event.window.event == SDL_WINDOWEVENT_RESIZED) {
                        sizeChanged_ = true;
                    }
                    break;
                case SDL_MOUSEMOTION:
                    // Left in window coordinates on purpose: those *are* the
                    // logical units everything above the backend works in.
                    mouse_.position = {static_cast<float>(event.motion.x),
                                       static_cast<float>(event.motion.y)};
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        mouse_.leftDown = true;
                        input_.pointerDown = true;
                    } else if (event.button.button == SDL_BUTTON_RIGHT) {
                        // The secondary button, which is what a context menu is
                        // opened by. Kept level like the primary one, and
                        // re-asserted below for the same reason.
                        mouse_.rightDown = true;
                        input_.secondaryDown = true;
                    }
                    break;
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        mouse_.leftDown = false;
                        input_.pointerDown = false;
                    } else if (event.button.button == SDL_BUTTON_RIGHT) {
                        mouse_.rightDown = false;
                        input_.secondaryDown = false;
                    }
                    break;
                case SDL_MOUSEWHEEL:
                    input_.wheel += static_cast<float>(event.wheel.y);
                    break;
                case SDL_TEXTINPUT:
                    // Already composed by the IME: one event can be several
                    // code points, or none at all mid-composition.
                    input_.text += event.text.text;
                    break;
                case SDL_KEYDOWN:
                    if (const Key key = translate(event.key.keysym.sym); key != Key::Unknown) {
                        input_.keys.push_back(KeyEvent{key, modifiersOf(SDL_GetModState()),
                                                       event.key.repeat != 0});
                    }
                    break;
                default: break;
            }
        }
        if (sizeChanged_) {
            sizeChanged_ = false;
            int width = 0;
            int height = 0;
            SDL_GetRendererOutputSize(renderer_, &width, &height);
            resizeSurface(width, height);
        }
        return true;
    }

    Canvas& canvas() override { return canvas_; }

    void present() override {
        if (!texture_ || canvas_.width() == 0) return;
        // The texture is at least the canvas and often larger — see
        // `resizeSurface` — so both the upload and the blit name the part in
        // use rather than the whole of it.
        const SDL_Rect used{0, 0, canvas_.width(), canvas_.height()};
        SDL_UpdateTexture(texture_, &used, canvas_.pixels(), static_cast<int>(canvas_.pitch()));
        SDL_RenderClear(renderer_);
        SDL_RenderCopy(renderer_, texture_, &used, nullptr);
        SDL_RenderPresent(renderer_);
    }

    Vec2 size() const override {
        // Logical, so a caller lays out in the same units it receives events
        // in. The canvas underneath is the drawable, which is `scale()` times
        // larger on a HiDPI screen.
        return {static_cast<float>(canvas_.width()) / scaleX_,
                static_cast<float>(canvas_.height()) / scaleY_};
    }

    float scale() const override { return scaleX_; }

    MouseState mouse() const override { return mouse_; }

    InputFrame takeInput() override {
        input_.pointer = mouse_.position;
        input_.pointerDown = mouse_.leftDown;
        input_.secondaryDown = mouse_.rightDown;
        InputFrame frame;
        frame.swap_with(input_);
        return frame;
    }

    bool resized() const override { return resized_; }

    void setCursor(Cursor cursor) override {
        if (cursor == cursor_) return;  // asking SDL every frame would be waste
        SDL_Cursor* handle = systemCursor(cursor);
        if (!handle) return;  // keep whatever is showing rather than flash to an arrow
        cursor_ = cursor;
        SDL_SetCursor(handle);
    }

private:
    /** SDL's nearest system cursor, created once and kept for the window's
     *  life. The ones SDL2 has no shape for fall back to the closest thing it
     *  does: `Hand` and `Progress` are the two, and an arrow-with-nothing would
     *  say less than a hand and a busy pointer do. */
    SDL_Cursor* systemCursor(Cursor cursor) {
        const auto existing = cursors_.find(cursor);
        if (existing != cursors_.end()) return existing->second;

        SDL_SystemCursor id = SDL_SYSTEM_CURSOR_ARROW;
        switch (cursor) {
            case Cursor::Default: id = SDL_SYSTEM_CURSOR_ARROW; break;
            case Cursor::Pointer:
            case Cursor::Hand: id = SDL_SYSTEM_CURSOR_HAND; break;
            case Cursor::Text: id = SDL_SYSTEM_CURSOR_IBEAM; break;
            // SDL2 has no open or closed hand, and a hand that does not change
            // on press is still the right shape for something draggable.
            case Cursor::Grab:
            case Cursor::Grabbing: id = SDL_SYSTEM_CURSOR_SIZEALL; break;
            case Cursor::ResizeHorizontal: id = SDL_SYSTEM_CURSOR_SIZEWE; break;
            case Cursor::ResizeVertical: id = SDL_SYSTEM_CURSOR_SIZENS; break;
            case Cursor::ResizeDiagonalUp: id = SDL_SYSTEM_CURSOR_SIZENESW; break;
            case Cursor::ResizeDiagonalDown: id = SDL_SYSTEM_CURSOR_SIZENWSE; break;
            case Cursor::NotAllowed: id = SDL_SYSTEM_CURSOR_NO; break;
            case Cursor::Progress:
            case Cursor::Wait: id = SDL_SYSTEM_CURSOR_WAITARROW; break;
            case Cursor::Crosshair: id = SDL_SYSTEM_CURSOR_CROSSHAIR; break;
            case Cursor::Help: id = SDL_SYSTEM_CURSOR_ARROW; break;
        }

        SDL_Cursor* handle = SDL_CreateSystemCursor(id);
        cursors_.emplace(cursor, handle);  // cached even when null, so it is tried once
        return handle;
    }

    void resizeSurface(int width, int height) {
        if (width == canvas_.width() && height == canvas_.height()) return;
        canvas_.resize(width, height);

        // The texture only ever *grows*, and in steps.
        //
        // Creating one is a driver allocation and destroying one can make the
        // driver wait for frames still using it. A window dragged across the
        // screen changes size a hundred times, and answering each with a fresh
        // texture is what made the drag stutter while the same drag in a
        // browser does not. Rounded up so a slow drag does not reallocate on
        // every second pixel either.
        constexpr int kStep = 256;
        if (!texture_ || width > textureWidth_ || height > textureHeight_) {
            const int wide = ((std::max(width, textureWidth_) + kStep - 1) / kStep) * kStep;
            const int tall = ((std::max(height, textureHeight_) + kStep - 1) / kStep) * kStep;
            if (texture_) SDL_DestroyTexture(texture_);
            texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ABGR8888,
                                         SDL_TEXTUREACCESS_STREAMING, wide, tall);
            textureWidth_ = wide;
            textureHeight_ = tall;
        }
        resized_ = true;

        // The pointer arrives in window coordinates; the canvas is in drawable
        // pixels, and on a HiDPI screen those differ by the display scale.
        int windowWidth = 0;
        int windowHeight = 0;
        SDL_GetWindowSize(window_, &windowWidth, &windowHeight);
        scaleX_ = windowWidth > 0 ? static_cast<float>(width) / static_cast<float>(windowWidth) : 1.0f;
        scaleY_ =
            windowHeight > 0 ? static_cast<float>(height) / static_cast<float>(windowHeight) : 1.0f;
    }

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    /** What the texture actually is, which is at least the canvas. */
    int textureWidth_ = 0;
    int textureHeight_ = 0;
    bool sizeChanged_ = false;
    Canvas canvas_;
    MouseState mouse_{};
    InputFrame input_{};
    bool resized_ = false;
    Cursor cursor_ = Cursor::Default;
    std::map<Cursor, SDL_Cursor*> cursors_;
    float scaleX_ = 1.0f;
    float scaleY_ = 1.0f;
};

}  // namespace

std::unique_ptr<Window> Window::create(const WindowOptions& options) {
    auto window = std::make_unique<Sdl2Window>();
    if (!window->open(options)) return nullptr;
    return window;
}

}  // namespace gbui
