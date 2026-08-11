// Which URLs are handed to the desktop, and which are refused.
#include "gbui/platform/shell.hpp"

#include "harness.hpp"

using namespace gbui;

/**
 * These assert only the *refusals*, and deliberately: a case that returns true
 * would launch a browser on the machine running the tests. The refusals are
 * where the consequence is, so they are what is pinned.
 */
TEST("a scheme the desktop should not be handed is refused") {
    // `xdg-open` will happily run a .desktop entry, open a file manager, or
    // hand a path to whatever claims it. A URL usually arrives from a commit
    // message, a remote or a document — somewhere the person running the
    // application did not choose it.
    CHECK(!openUrl("file:///etc/passwd"));
    CHECK(!openUrl("javascript:alert(1)"));
    CHECK(!openUrl("data:text/html,<script>alert(1)</script>"));
    CHECK(!openUrl("ssh://box/repo.git"));
    CHECK(!openUrl("vscode://file/etc/shadow"));
}

TEST("something with no scheme at all is refused") {
    CHECK(!openUrl(""));
    CHECK(!openUrl("github.com"));
    CHECK(!openUrl("/usr/bin/id"));
    CHECK(!openUrl("./relative/path"));
    // A Windows path's "C:" is not a scheme, and must not be read as one.
    CHECK(!openUrl("C:/Windows/System32"));
}

/** Whitespace and control characters are either a mistake or an attempt to
 *  smuggle a second argument past something downstream. */
TEST("a url carrying whitespace or control characters is refused") {
    CHECK(!openUrl("https://example.com /etc/passwd"));
    CHECK(!openUrl("https://example.com\n--version"));
    CHECK(!openUrl("https://example.com\ttrailing"));
    CHECK(!openUrl(std::string("https://example.com\0hidden", 25)));
}

TEST("an absurdly long url is refused rather than passed on") {
    CHECK(!openUrl("https://example.com/" + std::string(5000, 'a')));
}

/** Case in the scheme is not significant, so a refusal cannot be dodged by
 *  spelling it differently. */
TEST("the scheme is matched without regard to case") {
    CHECK(!openUrl("FILE:///etc/passwd"));
    CHECK(!openUrl("JavaScript:alert(1)"));
}
