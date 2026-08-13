// The interactive group, and the shape every one of them shares:
//
//     if (checkbox(ui, input, "id", value, {…})) value = !value;
//
// The component never writes to the model. It draws the value it was handed
// and reports what the user did, which is what makes undo, validation and
// "are you sure?" possible without the toolkit knowing about any of them.
// Every example below is that sentence with a different noun.
#include "catalog.hpp"

namespace gbui::demos::catalog {

void addControlExamples(std::vector<Example>& out) {
    out.push_back(
        {"checkbox", [](Ui& ui, const Interaction& input, State& state) {
             if (checkbox(ui, input, "catalog.checkbox", state.on, {.label = "Show tags"})) {
                 state.on = !state.on;
             }
         }});

    out.push_back({"radio", [](Ui& ui, const Interaction& input, State& state) {
                       const char* names[] = {"Merge", "Rebase", "Fast-forward"};
                       for (std::size_t i = 0; i < 3; ++i) {
                           const std::string tag = "catalog.radio." + std::to_string(i);
                           if (radio(ui, input, tag, state.choice == i, {.label = names[i]})) {
                               state.choice = i;
                           }
                       }
                   }});

    out.push_back(
        {"switchToggle", [](Ui& ui, const Interaction& input, State& state) {
             // A switch reads as "on now" and a checkbox as "will
             // apply" — the only difference between the two, and the
             // reason both exist.
             if (switchToggle(ui, input, "catalog.switch", state.on, {.label = "Auto fetch"})) {
                 state.on = !state.on;
             }
         }});

    out.push_back({"slider", [](Ui& ui, const Interaction& input, State& state) {
                       if (const SliderResult result = slider(ui, input, "catalog.slider",
                                                              state.fraction, {.showValue = true});
                           result.changed) {
                           state.fraction = result.value;
                       }
                   }});

    out.push_back({"numberField", [](Ui& ui, const Interaction& input, State& state) {
                       if (const NumberFieldResult result = numberField(
                               ui, input, "catalog.number", state.number,
                               {.minimum = 0.0, .maximum = 100.0, .step = 1.0, .suffix = " min"});
                           result.changed) {
                           state.number = result.value;
                       }
                   }});

    out.push_back({"textField", [](Ui& ui, const Interaction& input, State& state) {
                       textField(ui, input, "catalog.field", state.text,
                                 {.placeholder = "Repository name", .leading = Icon::Search});
                   }});

    out.push_back({"progressBar", [](Ui& ui, const Interaction&, State& state) {
                       progressBar(ui, {.value = state.fraction});
                       // Below zero is the indeterminate form, which needs a
                       // clock the application supplies.
                       progressBar(ui, {.value = -1.0, .phase = state.clock});
                   }});

    out.push_back({"label", [](Ui& ui, const Interaction& input, State& state) {
                       // A label knows what it labels, so clicking it moves
                       // the keyboard there — and draws no focus ring, because
                       // the pointer already told the user where they went.
                       label(ui, input, "catalog.field", "Repository");
                       textField(ui, input, "catalog.field", state.text,
                                 {.placeholder = "Repository name"});
                   }});

    out.push_back(
        {"hyperlink", [](Ui& ui, const Interaction& input, State&) {
             if (hyperlink(ui, input, "catalog.link", "Open on GitHub", {.trailing = Icon::Link})) {
                 // An application would open a browser here.
             }
         }});

    out.push_back({"colorPicker", [](Ui& ui, const Interaction& input, State& state) {
                       colorPicker(ui, input, "catalog.colorPicker", state.colour);
                   }});

    out.push_back({"colorField", [](Ui& ui, const Interaction& input, State& state) {
                       // The same picker behind a swatch, for a form.
                       colorField(ui, input, "catalog.colorField", state.colour);
                   }});

    out.push_back({"datePicker", [](Ui& ui, const Interaction& input, State& state) {
                       if (const DatePickerResult result = datePicker(
                               ui, input, "catalog.datePicker", state.date, state.calendar);
                           result.chosen) {
                           state.date = result.date;
                       }
                   }});

    out.push_back({"dateField", [](Ui& ui, const Interaction& input, State& state) {
                       if (const DateFieldResult result = dateField(ui, input, "catalog.dateField",
                                                                    state.date, state.calendar);
                           result.changed) {
                           state.date = result.date;
                       }
                   }});

    out.push_back({"timePicker", [](Ui& ui, const Interaction& input, State& state) {
                       if (const TimePickerResult result = timePicker(
                               ui, input, "catalog.timePicker", state.time, state.clock2);
                           result.changed) {
                           state.time = result.time;
                       }
                   }});

    out.push_back({"dateTimePicker", [](Ui& ui, const Interaction& input, State& state) {
                       if (const DateTimePickerResult result = dateTimePicker(
                               ui, input, "catalog.dateTimePicker", state.stamp, state.stampState);
                           result.changed) {
                           state.stamp = result.when;
                       }
                   }});

    out.push_back({"dateTimeField", [](Ui& ui, const Interaction& input, State& state) {
                       if (const DateTimeFieldResult result = dateTimeField(
                               ui, input, "catalog.dateTimeField", state.stamp, state.stampState);
                           result.changed) {
                           state.stamp = result.when;
                       }
                   }});

    out.push_back({"richEditor", [](Ui& ui, const Interaction& input, State& state) {
                       richEditor(ui, input, "catalog.richEditor", state.document, state.rich);
                   }});
}

}  // namespace gbui::demos::catalog
