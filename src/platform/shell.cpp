#include "gbui/platform/shell.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace gbui {
namespace {

/** The scheme of `url`, lowercased, or empty when it has none. */
std::string schemeOf(std::string_view url) {
    const std::size_t colon = url.find(':');
    if (colon == std::string_view::npos || colon == 0) return {};
    std::string scheme(url.substr(0, colon));
    for (char& c : scheme) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    // A scheme is letters, digits, '+', '-' and '.'; anything else means this
    // is not a scheme at all — a Windows path's "C:" lands here.
    const bool wellFormed =
        std::isalpha(static_cast<unsigned char>(scheme.front())) != 0 &&
        std::all_of(scheme.begin(), scheme.end(), [](char c) {
            return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '+' || c == '-' ||
                   c == '.';
        });
    return wellFormed ? scheme : std::string{};
}

bool allowed(const std::string& scheme) {
    return scheme == "http" || scheme == "https" || scheme == "mailto";
}

/** A control character or a space in a URL is either a mistake or an attempt to
 *  smuggle a second argument past something downstream. */
bool clean(std::string_view url) {
    return !url.empty() && url.size() < 4096 &&
           std::none_of(url.begin(), url.end(), [](char c) {
               const auto byte = static_cast<unsigned char>(c);
               return byte <= 0x20 || byte == 0x7F;
           });
}

}  // namespace

bool openUrl(std::string_view url) {
    if (!clean(url) || !allowed(schemeOf(url))) return false;
    const std::string target(url);

#if defined(_WIN32)
    // `ShellExecuteA` takes the URL as one argument, so there is no shell and
    // nothing to quote. Anything above 32 is success, by its own convention.
    const HINSTANCE result =
        ShellExecuteA(nullptr, "open", target.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<INT_PTR>(result) > 32;
#else
#if defined(__APPLE__)
    const char* opener = "open";
#else
    const char* opener = "xdg-open";
#endif
    const pid_t child = ::fork();
    if (child < 0) return false;
    if (child == 0) {
        // Double fork, so the opener is not left as a zombie for a caller that
        // never reaps and does not become a child of a process that may exit
        // first. The middle process leaves immediately.
        const pid_t grandchild = ::fork();
        if (grandchild == 0) {
            // `execlp`, not `system`: the URL is an argument vector entry, so a
            // semicolon or a backtick in it is a character and not syntax.
            ::execlp(opener, opener, target.c_str(), static_cast<char*>(nullptr));
            ::_exit(127);
        }
        ::_exit(grandchild < 0 ? 127 : 0);
    }
    int status = 0;
    // Only the middle process is waited for, and it exits at once.
    while (::waitpid(child, &status, 0) < 0) {
        // Interrupted by a signal; ask again.
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#endif
}

}  // namespace gbui
