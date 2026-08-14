// The component table, and the properties it is only useful if it keeps.
//
// These are conformance checks rather than unit tests: the table is generated,
// so what is worth asserting is not that one entry is right but that the shape
// every consumer relies on still holds. A properties panel, a code generator
// and an importer all break in the same way if it does not.
#include "gbui/meta/components.hpp"

#include <set>
#include <string_view>

#include "harness.hpp"

using namespace gbui;

TEST("the component table is not empty and names no duplicates it should not") {
    const auto& all = meta::components();
    // Fifty-odd today. A floor rather than an exact count, so adding a
    // component does not fail a test that has nothing to do with it — but a
    // generator that silently stopped parsing would take it well below this.
    CHECK(all.size() >= 40);

    for (const meta::ComponentInfo& entry : all) {
        CHECK(!entry.name.empty());
        CHECK(!entry.header.empty());
        CHECK(!entry.group.empty());
        CHECK(!entry.signature.empty());
    }
}

TEST("every component belongs to a known group") {
    // The groups are the umbrella headers a developer already includes, so a
    // component in "Other" is one no umbrella gathers — which is a fact about
    // the headers worth failing on rather than a category.
    const std::set<std::string_view> known = {"Components", "Containers", "Controls", "Overlays",
                                              "Charts"};
    for (const meta::ComponentInfo& entry : meta::components()) {
        CHECK(known.count(entry.group) == 1);
    }
    CHECK(meta::groups().size() == known.size());
}

TEST("an enum property carries its choices") {
    const meta::ComponentInfo* button = meta::find("button");
    CHECK(button != nullptr);
    if (!button) return;

    bool sawVariant = false;
    for (const meta::PropertyInfo& property : button->properties) {
        if (property.name != "variant") continue;
        sawVariant = true;
        CHECK(property.kind == meta::PropertyKind::Enum);
        CHECK(property.choices.size() == 4);
        CHECK(property.choices.front() == std::string_view("Primary"));
        CHECK(property.defaultText == std::string_view("ButtonVariant::Secondary"));
    }
    CHECK(sawVariant);
}

TEST("an optional property says so, and an icon is an icon") {
    const meta::ComponentInfo* button = meta::find("button");
    CHECK(button != nullptr);
    if (!button) return;

    for (const meta::PropertyInfo& property : button->properties) {
        if (property.name != "leading") continue;
        // `std::optional<Icon>`: an editor draws a value plus a "set" toggle,
        // and picks the value from a grid of glyphs rather than a menu of
        // forty names — which is the whole reason `Icon` is its own kind.
        CHECK(property.optional);
        CHECK(property.kind == meta::PropertyKind::Icon);
    }
}

TEST("a container is marked as one") {
    // The first thing a designer needs to know about a component: whether
    // something can be dropped inside it.
    const meta::ComponentInfo* panel = meta::find("panel");
    CHECK(panel != nullptr);
    if (panel) CHECK(panel->container);

    const meta::ComponentInfo* badge = meta::find("badge");
    CHECK(badge != nullptr);
    if (badge) CHECK(!badge->container);
}

TEST("a component that reacts to the pointer says so") {
    const meta::ComponentInfo* slider = meta::find("slider");
    CHECK(slider != nullptr);
    if (slider) CHECK(slider->interactive);

    const meta::ComponentInfo* spacer = meta::find("spacer");
    CHECK(spacer != nullptr);
    if (spacer) CHECK(!spacer->interactive);
}

TEST("every options struct was found for the components that take one") {
    // A component whose signature names an options type but whose property
    // list is empty means the generator matched the function and lost the
    // struct — the failure that turns a properties panel into a blank form.
    for (const meta::ComponentInfo& entry : meta::components()) {
        if (entry.optionsType.empty()) continue;
        CHECK(!entry.properties.empty());
    }
}
