#include "gbui/a11y/role.hpp"

namespace gbui {

std::string_view roleName(Role role) {
    switch (role) {
        case Role::None: return "none";

        case Role::Label: return "label";
        case Role::Heading: return "heading";
        case Role::Paragraph: return "paragraph";
        case Role::Image: return "img";
        case Role::Link: return "link";
        case Role::Figure: return "figure";

        case Role::Button: return "button";
        case Role::Checkbox: return "checkbox";
        case Role::Radio: return "radio";
        case Role::RadioGroup: return "radiogroup";
        case Role::Switch: return "switch";
        case Role::Slider: return "slider";
        case Role::SpinButton: return "spinbutton";
        case Role::TextInput: return "textbox";
        case Role::ComboBox: return "combobox";
        case Role::ListBox: return "listbox";
        case Role::Option: return "option";
        case Role::ProgressBar: return "progressbar";

        case Role::Group: return "group";
        case Role::Form: return "form";
        case Role::Toolbar: return "toolbar";
        case Role::Separator: return "separator";
        case Role::ScrollView: return "scrollview";
        case Role::List: return "list";
        case Role::ListItem: return "listitem";
        case Role::Table: return "table";
        case Role::Row: return "row";
        case Role::Cell: return "cell";
        case Role::ColumnHeader: return "columnheader";
        case Role::Tree: return "tree";
        case Role::TreeItem: return "treeitem";
        case Role::TabList: return "tablist";
        case Role::Tab: return "tab";
        case Role::TabPanel: return "tabpanel";

        case Role::Menu: return "menu";
        case Role::MenuBar: return "menubar";
        case Role::MenuItem: return "menuitem";
        case Role::MenuItemCheckbox: return "menuitemcheckbox";
        case Role::MenuItemRadio: return "menuitemradio";
        case Role::Dialog: return "dialog";
        case Role::AlertDialog: return "alertdialog";
        case Role::Tooltip: return "tooltip";

        case Role::Status: return "status";
        case Role::Alert: return "alert";
    }
    // Unreachable for any value of the enumeration, and deliberately not a
    // default label: adding a role without a name should fail the build here
    // rather than silently answer "none" at runtime.
    return "none";
}

}  // namespace gbui
