#include "gbui_demos/host.hpp"

#include <algorithm>
#include <fstream>

#include "gbui/layout/layout.hpp"
#include "gbui/meta/components.hpp"
#include "gbui/paint/paint.hpp"
#include "gbui/scene/ui.hpp"
#include "gbui/widgets/components.hpp"
#include "gbui_demos/catalog.hpp"

namespace gbui::demos {

namespace {

/**
 * The frame one component is previewed in.
 *
 * Wide enough for the stage below plus its margins and no wider: the stage is
 * capped at 560, so every pixel past 600 is empty space with a component
 * huddled at one end of it. The height is the web gallery's preview strip,
 * which is what the same examples are seen in on the documentation site — a
 * table wants the room and a checkbox does not, and one frame for all of them
 * is what makes fifty previews read as a set.
 */
constexpr float kPreviewWidth = 600.0f;
constexpr float kPreviewHeight = 460.0f;

/**
 * One component, on its own, in a frame that says what it is.
 *
 * A `Demo` like any other — which is the point. The host has one pipeline and
 * one input path, and a component preview earns nothing by having a second.
 */
class ComponentDemo final : public Demo {
public:
    ComponentDemo(const catalog::Example& example, const meta::ComponentInfo* info)
        : example_(example), info_(info), state_(catalog::freshState()) {}

    /**
     * The sentence a reader wants above a preview.
     *
     * The header's own — "A button, in the four variants the design system
     * has." — and not the declaration's, which is per overload and often
     * explains a distinction rather than the component: `button`'s first is
     * documented by what it cannot do without an `Interaction`. The
     * declaration doc is the right thing next to a *signature*, and the wrong
     * thing next to a picture.
     */
    static std::string_view describe(const meta::ComponentInfo* info) {
        if (!info) return {};
        return info->headerDoc.empty() ? info->summary : info->headerDoc;
    }

    NodeId build(Frame& frame) override {
        Ui& ui = frame.ui;
        state_.clock = frame.time;
        state_.delta = frame.delta;

        Style page;
        page.direction = Direction::Column;
        page.gap = 12.0f;
        page.padding = Edges::all(20.0f);
        page.background = Fill{Token::Bg};
        page.radius = 0.0f;
        auto root = ui.scope(page);

        {
            Style header;
            header.direction = Direction::Column;
            header.gap = 4.0f;
            header.shrink = 0.0f;
            auto headerScope = ui.scope(header);
            text(ui, example_.component,
                 {.color = Token::TextStrong,
                  .weight = FontWeight::SemiBold,
                  .role = FontRole::Mono,
                  .size = 15.0f});
            if (const std::string_view summary = describe(info_); !summary.empty()) {
                text(ui, summary,
                     {.color = Token::TextMuted,
                      .size = 12.0f,
                      .overflow = TextOverflow::Wrap,
                      .maxLines = 2});
            }
        }

        // The example sits on a surface of its own, so what the reader is
        // looking at is unambiguous: everything inside the border is the
        // component, everything outside it is this frame.
        {
            Style stage;
            stage.direction = Direction::Column;
            stage.gap = 10.0f;
            // Stretch, not Start. A slider's track, a table's columns and a
            // virtualised list all take their width from the box they are
            // given — hand them a content-sized one and they collapse to
            // nothing, which is what the first render of this gallery did.
            stage.align = Align::Stretch;
            stage.maxWidth = 560.0f;
            stage.padding = Edges::all(18.0f);
            stage.grow = 1.0f;
            stage.basis = 0.0f;
            stage.minHeight = 0.0f;
            stage.background = Fill{Token::BgElevated};
            stage.border = Border{1.0f, Fill{Token::Border}};
            auto stageScope = ui.scope(stage);
            example_.build(ui, frame.input, state_);
            // Handed on rather than acted on here, because only the host owns
            // the `Interaction` — the same relay the components themselves use.
            if (!state_.focusRequest.empty()) {
                frame.focus = state_.focusRequest;
                state_.focusRequest.clear();
            }
        }

        if (info_) {
            text(ui, info_->header,
                 {.color = Token::TextMuted, .role = FontRole::Mono, .size = 10.5f});
        }
        return root.id();
    }

private:
    catalog::Example example_;
    const meta::ComponentInfo* info_ = nullptr;
    catalog::State state_;
};

}  // namespace

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

bool Host::selectComponent(std::string_view component) {
    const catalog::Example* example = catalog::find(component);
    if (!example) return false;

    demo_ = std::make_unique<ComponentDemo>(*example, meta::find(component));
    // No `DemoInfo`: a component has no sector, no fixed palette — so it
    // follows the reader's preference like anything else would — and no design
    // size of its own, which is what `designSize` answers for it.
    info_ = nullptr;
    selectedId_ = std::string(component);
    restart();
    return true;
}

Vec2 Host::designSize() const {
    if (info_) return info_->design;
    return {kPreviewWidth, kPreviewHeight};
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

    Theme theme = themeOverride_           ? *themeOverride_
                  : skinId_ == "material"  ? Theme::material(dark)
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

void Host::setThemeOverride(std::optional<Theme> theme) { themeOverride_ = std::move(theme); }

Design Host::designFor() const {
    Design chosen = Design::gitbox();
    if (skinId_ == "material") chosen = Design::material();
    else if (skinId_ == "cupertino") chosen = Design::cupertino();
    else if (skinId_ == "fluent") chosen = Design::fluent();

    // ---- the skin changes colour and behaviour, not the layout -------------
    //
    // Each design carries its own control height — Material's is 40 against
    // this one's 30, which is Material 3's own number and correct for
    // Material. In a *demo* it is the wrong thing to vary: switching skin
    // reflows every screen, rows change height, a dense table becomes a sparse
    // one, and what the reader is trying to compare — the palette, the corner,
    // the press — moves out from under them while they compare it.
    //
    // So the geometry that reflows is pinned to the reference design and
    // everything that does not is the skin's: the radius, the fills, the
    // borders, the ripple, the motion. An application shipping one skin should
    // *not* do this; it wants the design's own metrics, which is why this lives
    // in the demo host and not in `Design`.
    const Design reference = Design::gitbox();
    chosen.controlHeight = reference.controlHeight;
    return chosen;
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
    const NodeId root = demo_ ? demo_->build(frame) : ui.scope(Style{}).id();

    // Applied after the build rather than during it, which is the only order
    // that works: a component asks for focus while it is being built, and the
    // node it is asking for may not exist until the build has finished.
    if (!frame.focus.empty()) interaction_.focus(frame.focus);

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
