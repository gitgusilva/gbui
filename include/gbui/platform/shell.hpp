// Handing something to the desktop: a URL to whatever opens URLs.
#pragma once

#include <string_view>

namespace gbui {

/**
 * Opens `url` in whatever the desktop has registered for it.
 *
 * `xdg-open` on Linux and the BSDs, `open` on macOS, `ShellExecute` on Windows.
 * Returns false when the scheme is one this refuses, when no opener could be
 * started, or when the platform has none — never because the browser itself
 * failed, which happens after this has returned and cannot be observed.
 *
 * **Only `http`, `https` and `mailto` are opened.** The desktop's opener will
 * happily launch a `file://` path, a `.desktop` entry or anything else it has a
 * handler for, and a URL usually arrives from a document, a commit message or a
 * remote — places where the person running the application did not choose it.
 * Widening the list is a decision for whoever knows where their URLs come from,
 * so it is not made here.
 *
 * The child is started with `fork` and `exec`, never through a shell, so a URL
 * containing quotes, semicolons or backticks is an argument and not a command.
 */
bool openUrl(std::string_view url);

}  // namespace gbui
