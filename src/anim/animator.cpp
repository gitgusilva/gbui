#include "gbui/anim/animator.hpp"

#include <algorithm>
#include <cmath>

namespace gbui {
namespace {

/** FNV-1a over the tag and the property name.
 *
 * A hash rather than a string key because this is asked once per animated
 * property per frame, and a map of `std::string` would allocate on every
 * lookup for something the caller already holds. Two different properties
 * colliding is a 2^-64 event; the alternative costs an allocation per frame
 * per control. */
std::uint64_t keyOf(std::string_view id, std::string_view property, std::uint64_t salt = 0) {
    std::uint64_t hash = 1469598103934665603ULL ^ salt;
    const auto mix = [&hash](std::string_view text) {
        for (const char c : text) {
            hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c));
            hash *= 1099511628211ULL;
        }
    };
    mix(id);
    mix("\x1f");  // a separator no tag contains, so "ab"+"c" and "a"+"bc" differ
    mix(property);
    return hash;
}

/** Long enough that anything reappearing within it keeps its position — a menu
 *  closed and reopened, a row scrolled out and back — and short enough that a
 *  virtualised list does not accumulate state for rows nobody will see again. */
constexpr std::uint64_t kForgetAfterFrames = 120;

}  // namespace

void Animator::tick(float deltaSeconds) {
    ++frame_;
    // A frame that took longer than this was not a frame — it was a drag, a
    // breakpoint or a machine asleep. Stepping by the real number would finish
    // every animation at once, which is worse than running slightly slow.
    const float delta = std::clamp(deltaSeconds, 0.0f, 0.1f);
    clock_ += delta;

    running_ = 0;
    for (auto it = states_.begin(); it != states_.end();) {
        State& state = it->second;
        if (frame_ - state.touched > kForgetAfterFrames) {
            it = states_.erase(it);
            continue;
        }

        if (!state.done && state.oneShot) {
            // A pulse reports raw progress: the curve belongs to whatever it
            // drives — a radius eases, the alpha under it usually does not.
            state.elapsed += delta;
            const float span = std::max(1e-6f, state.transition.duration);
            const float active = state.elapsed - state.transition.delay;
            state.current = std::clamp(active / span, 0.0f, 1.0f);
            if (active >= span) state.done = true;
            if (!state.done) ++running_;
            ++it;
            continue;
        }

        if (!state.done) {
            state.elapsed += delta;
            const float span = std::max(0.0f, state.transition.duration);
            const float active = state.elapsed - state.transition.delay;
            if (active <= 0.0f) {
                state.current = state.from;
            } else if (span <= 0.0f || active >= span) {
                state.current = state.to;
                state.done = true;
            } else {
                state.current =
                    state.from + (state.to - state.from) * state.transition.shape(active / span);
            }
            if (!state.done) ++running_;
        }
        ++it;
    }
}

float Animator::track(std::uint64_t key, float target, const Transition& transition) {
    const auto [it, inserted] = states_.try_emplace(key);
    State& state = it->second;
    state.touched = frame_;

    if (inserted) {
        // First sight of this property: it *is* its target. A control does not
        // animate into existence, the same rule CSS applies to an element that
        // has only ever had one value.
        state.from = target;
        state.to = target;
        state.current = target;
        state.done = true;
        return target;
    }

    if (state.to != target) {
        state.from = state.current;  // from where it actually is, not where it was headed
        state.to = target;
        state.elapsed = 0.0f;
        state.transition = transition;
        state.done = transition.duration <= 0.0f && transition.delay <= 0.0f;
        if (state.done) state.current = target;
    }
    return state.current;
}

float Animator::animate(std::string_view id, std::string_view property, float target,
                        const Transition& transition) {
    return track(keyOf(id, property), target, transition);
}

float Animator::pulse(std::string_view id, std::string_view property, bool trigger,
                      const Transition& transition) {
    const auto [it, inserted] = states_.try_emplace(keyOf(id, property, 0x51ED270B'2B1D4F1FULL));
    State& state = it->second;
    state.touched = frame_;

    if (inserted) {
        // A pulse that has never fired is over, not about to start: a button
        // does not ripple because it appeared.
        state.oneShot = true;
        state.from = 0.0f;
        state.to = 1.0f;
        state.current = 1.0f;
        state.done = true;
    }

    if (trigger) {
        state.transition = transition;
        state.elapsed = 0.0f;
        state.current = 0.0f;
        // A zero-length pulse is one that never plays, rather than one that
        // plays instantly and cannot be seen.
        state.done = transition.duration <= 0.0f;
        if (state.done) state.current = 1.0f;
    }
    return state.current;
}

float Animator::latch(std::string_view id, std::string_view property, float value, bool set) {
    const auto [it, inserted] = states_.try_emplace(keyOf(id, property, 0x2545F491'4F6CDD1DULL));
    State& state = it->second;
    state.touched = frame_;
    if (inserted || set) {
        state.current = value;
        state.from = value;
        state.to = value;
        state.done = true;
    }
    return state.current;
}

Color Animator::animate(std::string_view id, std::string_view property, Color target,
                        const Transition& transition) {
    // One sub-animation per channel, salted so they cannot collide with each
    // other or with the scalar of the same name.
    const std::uint64_t base = keyOf(id, property, 0x9E3779B97F4A7C15ULL);
    // The channels are bytes; they travel as floats and are rounded on the way
    // back, so a two-step fade does not quantise to a stair.
    const auto channel = [&](std::uint64_t slot, std::uint8_t value) {
        const float moved = track(base + slot, static_cast<float>(value), transition);
        return static_cast<std::uint8_t>(std::clamp(std::lround(moved), 0L, 255L));
    };
    Color out;
    out.r = channel(1, target.r);
    out.g = channel(2, target.g);
    out.b = channel(3, target.b);
    out.a = std::clamp(track(base + 4, target.a, transition), 0.0f, 1.0f);
    return out;
}

}  // namespace gbui
