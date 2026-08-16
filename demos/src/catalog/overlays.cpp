// The floating group. All of them are positioned by the placement engine
// rather than by the flex flow, all of them sit in a layer above the content —
// and all of them share one rule: **the application owns whether they are
// open.** Every example below therefore has a `bool` in `State` behind it.
#include "catalog.hpp"

namespace gbui::demos::catalog {

void addOverlayExamples(std::vector<Example>& out) {
    out.push_back({"toast", [](Ui& ui, const Interaction& input, State& state) {
                       // Seeded once and then left alone: the queue belongs to
                       // the application, and the reader dismissing one is the
                       // application's answer, not the component's.
                       // One sticky and one on a timer, so the preview shows
                       // both halves: an error that waits to be dealt with, and
                       // a success whose bar drains and takes it away.
                       const auto present = [&](std::string_view id) {
                           for (const ToastEntry& entry : state.toasts.items) {
                               if (entry.id == id) return true;
                           }
                           return false;
                       };
                       if (!present("fetch")) {
                           state.toasts.push({.id = "fetch",
                                              .kind = ToastKind::Error,
                                              .title = "Fetch failed",
                                              .message = "Could not reach origin.",
                                              .duration = 0.0,
                                              .action = "Retry"});
                       }
                       if (!present("pushed")) {
                           state.toasts.push({.id = "pushed",
                                              .kind = ToastKind::Success,
                                              .title = "Pushed",
                                              .message = "3 commits to origin/main.",
                                              .duration = 8.0});
                       }
                       toast(ui, input, state.toasts, state.delta,
                             {.placement = ToastPlacement::BottomRight,
                              .bounds = input.viewport(),
                              .width = 250.0f,
                              .margin = 14.0f});
                   }});

    out.push_back({"select", [](Ui& ui, const Interaction& input, State& state) {
                       static const std::vector<std::string> branches = {"main", "feat/nord-tuning",
                                                                         "fix/ci-headless"};
                       SelectOptions options;
                       options.name = "Branch";
                       if (const SelectResult result =
                               select(ui, input, "catalog.select", branches, state.choice,
                                      state.select, options);
                           result.chosen) {
                           state.choice = *result.chosen;
                       }
                   }});

    out.push_back({"menuItem", [](Ui& ui, const Interaction& input, State& state) {
                       // Drawn inline here rather than inside a popover, which
                       // is what a menu item *is*: a row, not a window.
                       auto panelScope = panel(ui, {.padding = Edges::symmetric(4.0f, 0.0f)});
                       if (menuItem(ui, input, "catalog.menu.stage", "Stage file",
                                    {.leading = Icon::FilePlus, .shortcut = "Ctrl+S"})) {
                           state.on = !state.on;
                       }
                       menuItem(ui, input, "catalog.menu.tags", "Show tags",
                                {.selected = state.on});
                       menuItem(ui, input, "catalog.menu.discard", "Discard changes",
                                {.leading = Icon::RotateCcw, .disabled = true});
                   }});

    out.push_back({"menuSeparator", [](Ui& ui, const Interaction& input, State&) {
                       auto panelScope = panel(ui, {.padding = Edges::symmetric(4.0f, 0.0f)});
                       menuItem(ui, input, "catalog.sep.copy", "Copy");
                       menuSeparator(ui);
                       menuItem(ui, input, "catalog.sep.delete", "Delete");
                   }});

    out.push_back({"tooltip", [](Ui& ui, const Interaction& input, State&) {
                       button(ui, input, "HOVER ME",
                              {.variant = ButtonVariant::Secondary, .id = "catalog.tip.anchor"});
                       // Anchored by the *tag* of whatever it explains, so the
                       // two need not be siblings and the button knows nothing
                       // about it.
                       tooltip(ui, input, "catalog.tip.anchor", "Fetches every remote");
                   }});

    out.push_back({"popover", [](Ui& ui, const Interaction& input, State& state) {
                       button(
                           ui, input, state.popoverOpen ? "CLOSE" : "OPEN",
                           {.variant = ButtonVariant::Secondary, .id = "catalog.popover.anchor"});
                       if (input.clicked("catalog.popover.anchor")) {
                           state.popoverOpen = !state.popoverOpen;
                       }
                       if (state.popoverOpen) {
                           auto popoverScope =
                               popover(ui, input, "catalog.popover", "catalog.popover.anchor");
                           sectionHeading(ui, "REMOTE");
                           text(ui, "origin/main", {.color = Token::TextMuted});
                       }
                   }});

    out.push_back({"modal", [](Ui& ui, const Interaction& input, State& state) {
                       button(ui, input, "DISCARD ALL",
                              {.variant = ButtonVariant::Danger, .id = "catalog.modal.open"});
                       if (input.clicked("catalog.modal.open")) state.modalOpen = true;
                       if (!state.modalOpen) return;

                       auto dialog = modal(
                           ui, input, "catalog.modal", "Discard all changes?", state.modalAt,
                           {.width = 360.0f, .icon = Icon::CircleAlert, .danger = true});
                       // The position comes back updated, so dragging survives
                       // the tree being rebuilt every frame.
                       state.modalAt = dialog.result.position;
                       if (dialog.result.dismissed) state.modalOpen = false;

                       auto body = ui.column({.padding = Edges::all(16.0f)});
                       text(ui, "This cannot be undone.",
                            {.overflow = TextOverflow::Wrap, .lineHeight = 1.5f});
                   }});

    out.push_back({"modalActions", [](Ui& ui, const Interaction& input, State& state) {
                       button(ui, input, "OPEN DIALOG",
                              {.variant = ButtonVariant::Secondary, .id = "catalog.actions.open"});
                       if (input.clicked("catalog.actions.open")) state.modalOpen = true;
                       if (!state.modalOpen) return;

                       auto dialog = modal(ui, input, "catalog.actions", "Push to origin?",
                                           state.modalAt, {.width = 340.0f});
                       state.modalAt = dialog.result.position;
                       if (dialog.result.dismissed) state.modalOpen = false;
                       {
                           auto body = ui.column({.padding = Edges::all(16.0f)});
                           text(ui, "3 commits ahead.", {.color = Token::TextMuted});
                       }
                       // The row of buttons at the foot, pushed to the right.
                       auto actions = modalActions(ui);
                       button(
                           ui, input, "CANCEL",
                           {.variant = ButtonVariant::Secondary, .id = "catalog.actions.cancel"});
                       if (input.clicked("catalog.actions.cancel")) state.modalOpen = false;
                       button(ui, input, "PUSH",
                              {.variant = ButtonVariant::Primary, .id = "catalog.actions.ok"});
                       if (input.clicked("catalog.actions.ok")) state.modalOpen = false;
                   }});
}

}  // namespace gbui::demos::catalog
