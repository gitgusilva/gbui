#include "gbui_demos/host.hpp"

#include <algorithm>
#include <fstream>

#include "gbui/layout/layout.hpp"
#include "gbui/paint/paint.hpp"
#include "gbui/scene/ui.hpp"

namespace gbui::demos {

const std::vector<Skin>& skins() {
    static const std::vector<Skin> list = {
        {"gitbox", "GitBox"},
        {"material", "Material 3"},
        {"cupertino", "Cupertino"},
        {"fluent", "Fluent"},
    };
    return list;
}

Host::Host(const HostOptions& options)
    : width_(std::max(320, options.width)),
      height_(std::max(240, options.height)),
      scale_(options.scale > 0.0f ? options.scale : 1.0f),
      darkMode_(options.darkMode) {
    arena_.reserve(2048);
    setSkin(options.skin);
    canvas_.resize(static_cast<int>(static_cast<float>(width_) * scale_),
                   static_cast<int>(static_cast<float>(height_) * scale_));

    const std::string& wanted = options.demo;
    if (wanted.empty() || !select(wanted)) {
        if (!catalogue().empty()) select(catalogue().front().id);
    }
}

Host::~Host() = default;

// ---------------------------------------------------------------------------
// Fonts
// ---------------------------------------------------------------------------

void Host::useBundledFontsOnly() { fonts_.clearSearchPaths(); }

bool Host::addFont(std::string_view family, const std::string& path, FontWeight weight,
                   FontSlant slant) {
    return fonts_.addFontFile(family, path, weight, slant);
}

void Host::setFontFamilies(std::string_view ui, std::string_view mono) {
    uiFamily_ = std::string(ui);
    monoFamily_ = std::string(mono);
}

// ---------------------------------------------------------------------------
// What is on screen
// ---------------------------------------------------------------------------

bool Host::select(std::string_view id) {
    const DemoInfo* entry = find(id);
    if (!entry || !entry->create) return false;

    demo_ = entry->create();
    info_ = entry;
    selectedId_ = std::string(entry->id);
    restart();
    return true;
}

void Host::setSkin(std::string_view id) {
    const auto& list = skins();
    const bool known =
        std::any_of(list.begin(), list.end(), [&](const Skin& skin) { return skin.id == id; });
    if (known) skinId_ = std::string(id);
}

void Host::setDarkMode(bool dark) { darkMode_ = dark; }

void Host::setFontSize(float size) { fontSize_ = std::clamp(size, 9.0f, 24.0f); }

void Host::resize(int width, int height, float scale) {
    width_ = std::max(320, width);
    height_ = std::max(240, height);
    scale_ = scale > 0.0f ? scale : 1.0f;
    canvas_.resize(static_cast<int>(static_cast<float>(width_) * scale_),
                   static_cast<int>(static_cast<float>(height_) * scale_));
}

void Host::restart() {
    time_ = 0.0f;
    // Not the demo's state, which `select` has already replaced: this is the
    // toolkit's own memory of a tree that no longer exists. Left alone, the row
    // hovered in the last demo stays hovered in the next one, because a tag is
    // only a string and two screens can spell one the same way.
    interaction_ = Interaction{};
    animator_ = Animator{};
    arena_.reset();
    root_ = NodeId{};
    pending_ = InputFrame{};
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void Host::pointerMove(float x, float y) { pending_.pointer = {x, y}; }

void Host::pointerButton(bool down) { pending_.pointerDown = down; }

void Host::pointerLeave() {
    pending_.pointer = {-1.0f, -1.0f};
    pending_.pointerDown = false;
}

void Host::wheel(float lines) { pending_.wheel += lines; }

void Host::setModifiers(Modifiers modifiers) { pending_.modifiers = modifiers; }

void Host::key(Key key, bool repeat) { pending_.keys.push_back({key, pending_.modifiers, repeat}); }

void Host::type(std::string_view utf8) { pending_.text.append(utf8); }

void Host::submit(const InputFrame& input) {
    // Pointer and modifiers are levels and are taken as they are; wheel, keys
    // and text are events and accumulate, because a host may take input more
    // than once between two frames.
    pending_.pointer = input.pointer;
    pending_.pointerDown = input.pointerDown;
    pending_.modifiers = input.modifiers;
    pending_.wheel += input.wheel;
    pending_.keys.insert(pending_.keys.end(), input.keys.begin(), input.keys.end());
    pending_.text.append(input.text);
}

// ---------------------------------------------------------------------------
// The frame
// ---------------------------------------------------------------------------

Theme Host::themeFor() const {
    // A screen designed for one palette keeps it: a control room is dark at
    // noon, and a weather desk on a wall is light at midnight.
    bool dark = darkMode_;
    if (info_) {
        if (info_->palette == Palette::Dark) dark = true;
        if (info_->palette == Palette::Light) dark = false;
    }

    Theme theme = skinId_ == "material"    ? Theme::material(dark)
                  : skinId_ == "cupertino" ? Theme::cupertino(dark)
                  : skinId_ == "fluent"    ? Theme::fluent(dark)
                                           : (dark ? Theme::dark() : Theme::light());

    Typography& type = theme.typography();
    if (!uiFamily_.empty()) type.uiFont = uiFamily_;
    if (!monoFamily_.empty()) {
        type.monoFont = monoFamily_;
        type.editorFont = monoFamily_;
    }
    type.uiFontSize = fontSize_;
    type.editorFontSize = fontSize_ - 1.0f;
    return theme;
}

Design Host::designFor() const {
    if (skinId_ == "material") return Design::material();
    if (skinId_ == "cupertino") return Design::cupertino();
    if (skinId_ == "fluent") return Design::fluent();
    return Design::gitbox();
}

NodeId Host::buildTree(MeasureText& measure, const Theme& theme) {
    measure = measureWith(fonts_, scale_);

    arena_.reset();  // O(1): last frame's nodes are simply forgotten
    Ui ui(arena_);
    ui.setDesign(designFor());
    ui.setMeasure(measure, theme.typography());
    ui.setAnimator(&animator_);

    Frame frame{ui,
                interaction_,
                time_,
                lastDelta_,
                {static_cast<float>(width_), static_cast<float>(height_)}};
    const NodeId root = demo_ ? demo_->build(frame) : ui.begin(Style{}).id();

    LayoutContext context;
    context.theme = &theme;
    context.measure = measure;
    layout(arena_, root, Rect{0.0f, 0.0f, static_cast<float>(width_), static_cast<float>(height_)},
           context);
    return root;
}

void Host::frame(float delta) {
    lastDelta_ = delta;
    time_ += delta;
    animator_.tick(delta);

    // Resolved against *last* frame's tree, which is the tree the user was
    // looking at when they clicked. See interaction.hpp.
    InputFrame events;
    events.swap_with(pending_);
    interaction_.update(arena_, root_, events);

    const Theme theme = themeFor();
    MeasureText measure;
    root_ = buildTree(measure, theme);

    DisplayList list;
    list.setScale(scale_);  // the one place logical units become device pixels
    list.reserve(arena_.size() * 2);
    record(arena_, root_, theme, list, measure);

    canvas_.clear(theme.color(Token::Bg));
    SoftwarePainter painter(canvas_, fonts_, theme.typography());
    painter.paint(list);
}

std::string Host::toSvg() {
    const Theme theme = themeFor();
    MeasureText measure;
    root_ = buildTree(measure, theme);

    DisplayList list;
    list.reserve(arena_.size() * 2);
    record(arena_, root_, theme, list, measure);

    SvgPainter painter(static_cast<float>(width_), static_cast<float>(height_),
                       theme.color(Token::Bg));
    painter.paint(list);
    return painter.finish();
}

bool Host::writePpm(const std::string& path) const {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out << "P6\n" << canvas_.width() << " " << canvas_.height() << "\n255\n";
    const std::uint8_t* pixels = canvas_.pixels();
    for (int y = 0; y < canvas_.height(); ++y) {
        for (int x = 0; x < canvas_.width(); ++x) {
            const std::size_t index =
                static_cast<std::size_t>(y) * canvas_.pitch() + static_cast<std::size_t>(x) * 4;
            out.put(static_cast<char>(pixels[index]));
            out.put(static_cast<char>(pixels[index + 1]));
            out.put(static_cast<char>(pixels[index + 2]));
        }
    }
    return out.good();
}

}  // namespace gbui::demos
