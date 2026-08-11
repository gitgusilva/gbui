// The fallback backend: no window at all.
//
// Compiled when no platform backend is available, so an application still
// links and its offscreen paths — rendering to a canvas, writing a file — keep
// working on a build machine with no display libraries.
#include "gbui/platform/window.hpp"

namespace gbui {

std::unique_ptr<Window> Window::create(const WindowOptions&) { return nullptr; }

}  // namespace gbui
