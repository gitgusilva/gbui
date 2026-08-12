// The building API.
//
// C++ has no JSX, and a tree written as nested function calls reads inside-out.
// So containers are opened with a scope guard that closes them on destruction,
// which puts the nesting of the code and the nesting of the UI in the same
// shape:
//
//     Ui ui(arena);
//     auto root = ui.beginColumn({.gap = 8});
//     {
//         auto bar = ui.beginRow({.gap = 6, .height = 40});
//         ui.label("History");
//     }                                    // <- the row closes here
//     ui.label("Local Changes");
//
// The Ui object owns nothing: every node lives in the Arena that was handed in,
// so a tree outlives the builder that wrote it.
#pragma once

#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "gbui/anim/animator.hpp"
#include "gbui/layout/layout.hpp"
#include "gbui/scene/tree.hpp"
#include "gbui/style/design.hpp"

namespace gbui {

class Ui {
public:
    explicit Ui(Arena& arena) : arena_(arena) {}

    Ui(const Ui&) = delete;
    Ui& operator=(const Ui&) = delete;

    /**
     * Closes the container it was opened with.
     *
     * Move-only, and closing twice is impossible, so a container cannot be left
     * open by an early return.
     *
     * **A scope closes when it leaves scope — not where it is cast to void.**
     * The `(void)scope;` at the end of a block only silences the unused-variable
     * warning; it is not a close, and anything built after it is still a child.
     * That has been the cause of four separate layout bugs here, the last being
     * a date picker whose two arrows came out different sizes because the second
     * was built inside the month label. When a container must end before the
     * enclosing block does, put it in braces of its own or call `close()`.
     */
    class [[nodiscard]] Scope {
    public:
        Scope(Ui& ui, NodeId id) : ui_(&ui), id_(id) {}
        Scope(Scope&& other) noexcept : ui_(other.ui_), id_(other.id_), extra_(other.extra_) {
            other.ui_ = nullptr;
        }
        Scope& operator=(Scope&&) = delete;
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        ~Scope() {
            if (!ui_) return;
            for (int i = 0; i <= extra_; ++i) ui_->pop();
        }

        NodeId id() const { return id_; }
        operator NodeId() const { return id_; }

        /** Makes this scope close one extra container when it dies.
         *
         *  A component that opens a viewport and a content box has to hand back
         *  one scope, and both have to close. The *inner* scope takes on the
         *  extra pop and the outer one is disowned — the other way round leaves
         *  the outer popping the moment the component returns, which unbalances
         *  the stack and puts everything after it in the wrong parent. */
        void adopt() {
            if (ui_) ++extra_;
        }

        /** Gives up responsibility for closing. Used on the scope whose pop
         *  another scope has adopted. */
        void disown() { ui_ = nullptr; }

        /**
         * Closes the container now, rather than at the end of the block.
         *
         * For when the two do not coincide and braces would be awkward. Safe to
         * call more than once, and safe not to call at all — the destructor
         * does the same thing.
         */
        void close() {
            if (!ui_) return;
            for (int i = 0; i <= extra_; ++i) ui_->pop();
            ui_ = nullptr;
        }

    private:
        Ui* ui_;
        NodeId id_;
        int extra_ = 0;
    };

    /** Opens a container and makes it the parent of everything until the
     *  returned Scope dies. */
    Scope begin(const Style& style);
    Scope beginRow(Style style = {});
    Scope beginColumn(Style style = {});

    /** Adds a childless node — a spacer, a rule, a swatch. */
    NodeId add(const Style& style);

    /** Adds a run of text. The string is copied into the arena, so a temporary
     *  is safe to pass. */
    NodeId label(std::string_view text, TextStyle textStyle = {}, Style style = {});

    /** Adds a node drawing path data — an icon. The data is not copied: it is
     *  expected to live as long as the frame, which the generated icon table
     *  does trivially. */
    NodeId vector(std::string_view pathData, Style style = {}, Fill color = Fill{Token::Text},
                  float stroke = 2.0f);

    /**
     * A leaf the application draws into: a rectangle and whatever vector art it
     * wants inside it.
     *
     * This is the primitive a chart, a commit graph or a sparkline is built
     * from, rather than a widget per picture. The shapes are in the node's own
     * coordinates — (0,0) is its content box — so the caller works in the box it
     * asked for and never learns where that box ended up. How *big* the box is
     * comes from `Interaction::frameOf`, the same way every other component
     * that needs last frame's geometry finds out.
     */
    NodeId draw(const Style& style, std::vector<Shape> shapes);

    /** Tags the most recently added node, for hit testing and tests. */
    Ui& tag(std::string_view id);

    /**
     * Opens a naming scope: `qualify` prefixes with it until the scope dies.
     *
     * Identity here is a string, because the tree is rebuilt every frame and a
     * string is the only thing that survives that. The cost lands on the
     * caller, who ends up writing `"scada.tank.clearwell"` at one site and
     * `"scada.tank.backwash"` at the next, and misspelling one of them is a
     * control that silently stops responding.
     *
     *     auto ids = ui.beginIds("scada.tank");
     *     meter(ui, ui.qualify("clearwell"), …);   // "scada.tank.clearwell"
     *
     * Scopes nest, and the separator is supplied here rather than by the
     * caller, so a name cannot end up with two dots in it or none.
     */
    class [[nodiscard]] IdScope {
    public:
        IdScope(Ui& ui, std::size_t restoreTo) : ui_(&ui), restoreTo_(restoreTo) {}
        IdScope(IdScope&& other) noexcept : ui_(other.ui_), restoreTo_(other.restoreTo_) {
            other.ui_ = nullptr;
        }
        IdScope& operator=(IdScope&&) = delete;
        IdScope(const IdScope&) = delete;
        IdScope& operator=(const IdScope&) = delete;
        ~IdScope() { close(); }

        /** Ends the scope now rather than at the end of the block. */
        void close() {
            if (!ui_) return;
            ui_->idPrefix_.resize(restoreTo_);
            ui_ = nullptr;
        }

    private:
        Ui* ui_;
        std::size_t restoreTo_;
    };

    IdScope beginIds(std::string_view name);

    /**
     * `name` with the open naming scopes in front of it, interned so the view
     * outlives the call.
     *
     * The *same* string goes to the component and to the `Interaction` the
     * caller then asks about it — which is why this returns the qualified name
     * rather than quietly prefixing inside `tag`. A component that was handed
     * one name and registered another would be a control the application can
     * build and never hear from again.
     */
    std::string_view qualify(std::string_view name);

    /** Everything `qualify` would put in front of a name. Empty at the top. */
    std::string_view idPrefix() const { return idPrefix_; }

    /** Marks the most recently added node as a place Tab can land. */
    Ui& focusable(bool value = true);

    /** Says the most recent tag names geometry, not a control: `frameOf` still
     *  finds it, and hit testing walks past it to the tagged ancestor. */
    Ui& ignoresPointer(bool value = true);

    /** Sets what the pointer looks like over the most recently added node. */
    Ui& cursor(Cursor value);

    /** The node currently being filled, or the root once building has finished. */
    NodeId current() const { return stack_.empty() ? root_ : stack_.back(); }
    NodeId root() const { return root_; }
    NodeId last() const { return last_; }

    /**
     * Lets components measure text while they are being built.
     *
     * A caret has to sit at a byte offset inside a run, and the only way to
     * know where that is on screen is to measure the prefix. Layout measures
     * too, but it runs after the tree exists — so the measurer is handed to the
     * builder as well, and the two agree because it is the same function.
     */
    void setMeasure(MeasureText measure, Typography typography) {
        measure_ = std::move(measure);
        typography_ = std::move(typography);
    }

    /** Width and height of a run, or zero when no measurer was supplied. */
    TextMetrics measure(std::string_view text, const TextStyle& style) const {
        if (!measure_) return {};
        return measure_(text, style, typography_, std::numeric_limits<float>::infinity());
    }

    /** The same, in a given width — which is the only way to ask how tall a
     *  wrapped run will be, and therefore how big the box around it must be
     *  *before* layout has run. A floating box that cannot ask this has to
     *  guess its own size and correct it a frame later. */
    TextMetrics measure(std::string_view text, const TextStyle& style, float maxWidth) const {
        if (!measure_) return {};
        return measure_(text, style, typography_, maxWidth);
    }

    bool canMeasure() const { return static_cast<bool>(measure_); }
    const Typography& typography() const { return typography_; }

    /**
     * The design system in force: shape, sizing and press behaviour.
     *
     * Handed to the builder rather than read from the theme at paint time,
     * because these decisions change the *tree* — a control's height and radius
     * are laid out, and whether a press throws ink decides whether a node
     * exists at all. Unset, it is the toolkit's own, so nothing has to opt in.
     */
    void setDesign(Design design) { design_ = std::move(design); }
    const Design& design() const { return design_; }

    /**
     * Gives components somewhere to animate.
     *
     * The animator outlives the arena — it has to, since it is what remembers
     * where a value was last frame — so it is handed over as a pointer the
     * application owns, exactly as the measurer is. Without one, `animate`
     * returns the target and every component behaves as it did before: an
     * application that has not opted in gets no animation and no surprises.
     */
    void setAnimator(Animator* animator) { animator_ = animator; }
    Animator* animator() const { return animator_; }

    /** Where `property` of `id` is on its way to `target`. */
    float animate(std::string_view id, std::string_view property, float target,
                  const Transition& transition = {}) const {
        return animator_ ? animator_->animate(id, property, target, transition) : target;
    }
    Color animate(std::string_view id, std::string_view property, Color target,
                  const Transition& transition = {}) const {
        return animator_ ? animator_->animate(id, property, target, transition) : target;
    }

    /** How far through a one-shot, or 1 when none is running — and 1 always,
     *  with no animator, so an effect that only draws while `< 1` draws
     *  nothing rather than drawing forever. */
    float pulse(std::string_view id, std::string_view property, bool trigger,
                const Transition& transition = {}) const {
        return animator_ ? animator_->pulse(id, property, trigger, transition) : 1.0f;
    }

    /** Seconds on the animation clock, or zero with no animator — which is what
     *  makes anything reading it stand still rather than flicker. */
    float now() const { return animator_ ? animator_->now() : 0.0f; }

    /** A number kept across frames, replaced only when `set`. */
    float latch(std::string_view id, std::string_view property, float value, bool set) const {
        return animator_ ? animator_->latch(id, property, value, set) : value;
    }

    Arena& arena() { return arena_; }
    const Arena& arena() const { return arena_; }

private:
    friend class Scope;

    NodeId attach(const Style& style);
    void pop();

    friend class IdScope;

    Arena& arena_;
    std::string idPrefix_;
    MeasureText measure_;
    Animator* animator_ = nullptr;
    Design design_ = Design::gitbox();
    Typography typography_;
    std::vector<NodeId> stack_;
    NodeId root_{};
    NodeId last_{};
};

}  // namespace gbui
