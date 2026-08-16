// Halyard — a trading desk, and the only screen here whose chart is *forming*
// rather than merely scrolling.
//
// The candle on the right-hand edge is incomplete: its close, its high and its
// low are whatever the price has done so far this period, and it is redrawn
// every frame until the period ends and the next one starts. That is the one
// thing a market screen does that no other dashboard does, and it is the reason
// this screen exists — a chart that only ever appends finished data would have
// been the analytics screen with different numbers.
//
// **Where a real feed would go.** Nowhere in this file. `Feed` below has one
// job — turn a time into candles, trades and a book — and an application with a
// websocket would replace exactly that class and change nothing else, because
// nothing above it knows where a `Candle` came from. It is a sampled function
// rather than a socket for three reasons that all matter more than realism
// here: the toolkit has no networking and is not getting any, these screens are
// compiled to WebAssembly and shot headlessly in CI where there is no network
// to have, and a screenshot has to be the same picture on every machine.
//
// Interactive: the timeframe tabs, the candle and volume readouts, the book's
// rows, and the sortable watchlist.

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "gbui/widgets/chart.hpp"
#include "gbui/widgets/components.hpp"
#include "gbui/widgets/containers.hpp"

#include "kit.hpp"
#include "registry.hpp"

namespace gbui::demos {
namespace {

/** One print on the tape. */
struct Trade {
    std::string at;
    double price = 0.0;
    double size = 0.0;
    bool buy = false;
};

/**
 * One card on the ticker: a price as it was when the card was made.
 *
 * Frozen on purpose. A tape is a sequence of *prints* — each one says what a
 * name traded at at a moment, and a card whose number kept changing under the
 * reader would be a live cell that happens to be sliding, which is a different
 * thing and a worse one. It is also what stops the strip twitching: text that
 * gains a sign or a digit changes the content's width, and the width is what
 * the wrap is measured from.
 */
struct Quote {
    std::string symbol;
    std::string price;
    std::string change;
    bool up = false;
};

/** One side of one price level. */
struct Level {
    double price = 0.0;
    double size = 0.0;
};

/**
 * A company's mark, drawn into pixels once.
 *
 * Generated rather than loaded, and that is not a shortcut around the image
 * element — it is the only way a demo can have logos at all. These screens are
 * compiled to WebAssembly and shot headlessly in CI, where there is no file to
 * read; and the toolkit decodes nothing, so a PNG here would mean vendoring a
 * decoder into a library whose whole claim is that it has no dependencies. What
 * an application does instead is exactly what this does: hand `image` the
 * eight-bit RGBA its own decoder produced.
 */
class Logo {
public:
    /** `mark` picks which of four shapes is stamped on the tile, so six
     *  companies are six marks rather than six colours of one. */
    Logo(Color tint, int mark) { draw(tint, mark); }

    Bitmap bitmap() const { return {pixels_.data(), kSide, kSide, 0}; }

private:
    static constexpr int kSide = 44;

    void draw(Color tint, int mark) {
        pixels_.assign(static_cast<std::size_t>(kSide) * kSide * 4, 0);
        const auto put = [&](int x, int y, Color colour, float alpha) {
            if (x < 0 || y < 0 || x >= kSide || y >= kSide || alpha <= 0.0f) return;
            std::uint8_t* p = pixels_.data() +
                              (static_cast<std::size_t>(y) * kSide + static_cast<std::size_t>(x)) * 4;
            const float keep = 1.0f - alpha;
            p[0] = static_cast<std::uint8_t>(colour.r * alpha + static_cast<float>(p[0]) * keep);
            p[1] = static_cast<std::uint8_t>(colour.g * alpha + static_cast<float>(p[1]) * keep);
            p[2] = static_cast<std::uint8_t>(colour.b * alpha + static_cast<float>(p[2]) * keep);
            p[3] = static_cast<std::uint8_t>(255.0f * alpha + static_cast<float>(p[3]) * keep);
        };

        // The tile: a rounded square, antialiased by the same distance test the
        // painter uses, so it sits beside real components without giving itself
        // away at the corners.
        constexpr float kRadius = 11.0f;
        for (int y = 0; y < kSide; ++y) {
            for (int x = 0; x < kSide; ++x) {
                const float px = static_cast<float>(x) + 0.5f;
                const float py = static_cast<float>(y) + 0.5f;
                const float dx = std::max({kRadius - px, px - (kSide - kRadius), 0.0f});
                const float dy = std::max({kRadius - py, py - (kSide - kRadius), 0.0f});
                const float distance = std::hypot(dx, dy) - kRadius;
                put(x, y, tint, std::clamp(0.5f - distance, 0.0f, 1.0f));
            }
        }

        // The mark, built around the tile's own centre rather than from
        // coordinates picked by eye. Every one of these used to be a pixel or
        // three off in one direction, which is exactly the sort of thing nobody
        // can name and everybody can see.
        constexpr float kCentre = kSide / 2.0f;
        const Color ink{255, 255, 255, 1.0f};

        /** A filled rectangle given its centre, which is the only way to place
         *  one symmetrically without arithmetic at every call. */
        const auto slab = [&](float cx, float cy, float w, float h) {
            const int x0 = static_cast<int>(std::lround(cx - w / 2.0f));
            const int y0 = static_cast<int>(std::lround(cy - h / 2.0f));
            for (int j = y0; j < y0 + static_cast<int>(std::lround(h)); ++j) {
                for (int i = x0; i < x0 + static_cast<int>(std::lround(w)); ++i) {
                    put(i, j, ink, 1.0f);
                }
            }
        };

        switch (mark % 4) {
            case 0: {
                // Three columns on a shared baseline, rising. The *group* is
                // centred, so the tallest column being on the right does not
                // drag the mark off to one side.
                constexpr float kWidth = 5.0f;
                constexpr float kGap = 3.0f;
                const std::array<float, 3> heights{9.0f, 15.0f, 21.0f};
                const float span = kWidth * 3.0f + kGap * 2.0f;
                const float baseline = kCentre + heights.back() / 2.0f;
                for (std::size_t i = 0; i < heights.size(); ++i) {
                    const float cx = kCentre - span / 2.0f + kWidth / 2.0f +
                                     static_cast<float>(i) * (kWidth + kGap);
                    slab(cx, baseline - heights[i] / 2.0f, kWidth, heights[i]);
                }
                break;
            }
            case 1: {
                // A ring, antialiased by its distance from the centre like
                // everything else here.
                for (int y = 0; y < kSide; ++y) {
                    for (int x = 0; x < kSide; ++x) {
                        const float d = std::hypot(static_cast<float>(x) + 0.5f - kCentre,
                                                   static_cast<float>(y) + 0.5f - kCentre);
                        put(x, y, ink, std::clamp(2.0f - std::fabs(d - 8.5f), 0.0f, 1.0f));
                    }
                }
                break;
            }
            case 2: {
                // A chevron: two arms meeting on the centre line, each the
                // mirror of the other.
                constexpr float kArm = 9.0f;
                constexpr float kThick = 4.0f;
                for (float step = 0.0f; step <= kArm; step += 0.5f) {
                    slab(kCentre - step, kCentre + 4.0f - step, kThick, kThick);
                    slab(kCentre + step, kCentre + 4.0f - step, kThick, kThick);
                }
                break;
            }
            default: {
                // Three rules, each centred, the middle one short — a document,
                // and symmetric whichever line is which.
                slab(kCentre, kCentre - 7.0f, 21.0f, 6.0f);
                slab(kCentre, kCentre + 2.0f, 21.0f, 3.0f);
                slab(kCentre, kCentre + 9.0f, 13.0f, 3.0f);
                break;
            }
        }
    }

    std::vector<std::uint8_t> pixels_;
};

/** A company on the desk: what it is called, what its chart is made of, and
 *  the mark that stands for it in a list. */
struct Watch {
    std::string_view symbol;
    std::string_view name;
    kit::Rolling history;
    /** What the price chart is built from when this one is selected. */
    double base = 100.0;
    float seed = 1.0f;
    Logo logo;
};

/**
 * The market, as a function of time.
 *
 * Sampled, not accumulated: every candle is recomputed from the price function
 * on every advance rather than being appended to and remembered. That is what
 * makes the forming candle fall out for free — the newest one is simply sampled
 * up to *now* instead of up to the end of its period — and it is what keeps a
 * screenshot at t=12s identical on every machine, which a series built by
 * accumulating frames could never be.
 */
class Feed {
public:
    /** The price at a moment. Three octaves: a slow tide worth minutes of
     *  chart, a swing worth a few candles, and the jitter inside one. Scaled
     *  to the company, so a stock at fourteen dollars moves in cents and one at
     *  two hundred moves in dollars. */
    double priceAt(float t) const {
        const double swing = base_ * 0.04;
        return base_ + swing * static_cast<double>(kit::noise(t * 0.045f, seed_)) +
               swing * 0.25 * static_cast<double>(kit::noise(t * 0.31f, seed_ + 4.6f)) +
               swing * 0.06 * static_cast<double>(kit::noise(t * 1.45f, seed_ + 8.2f));
    }

    /** Which company the desk is looking at. Changing it changes everything
     *  below, because everything below is a function of the price. */
    void follow(double base, float seed) {
        base_ = base;
        seed_ = seed;
    }

    void advance(float time, float period) {
        now_ = time;
        period_ = period;

        // Which candles the window shows. The newest is the one `now` falls
        // inside, and it is the only one that is not finished.
        const long newest = static_cast<long>(std::floor(time / period));
        const long oldest = newest - static_cast<long>(kCandles) + 1;

        candles_.clear();
        volume_.clear();
        settled_.clear();
        times_.clear();
        for (long index = oldest; index <= newest; ++index) {
            const float start = static_cast<float>(index) * period;
            const float finish = std::min(time, start + period);

            Candle candle{};
            candle.open = priceAt(start);
            candle.close = priceAt(finish);
            candle.high = std::max(candle.open, candle.close);
            candle.low = std::min(candle.open, candle.close);
            // Eight samples inside the period, which is what puts a wick on a
            // candle at all: open and close alone describe a rectangle.
            constexpr int kSamples = 8;
            for (int step = 1; step < kSamples; ++step) {
                const float at =
                    start + (finish - start) * static_cast<float>(step) / kSamples;
                const double price = priceAt(at);
                candle.high = std::max(candle.high, price);
                candle.low = std::min(candle.low, price);
            }
            candles_.push_back(candle);

            // Volume follows the range — a period that went nowhere traded
            // little — but it follows it, it is not made of it.
            //
            // Built as a floor times two factors rather than as a sum with the
            // range dominating it. A minute that trades a fifth of the next
            // one's does not happen, and the arithmetic that let it made the
            // series swing forty to one between neighbours: drawn as a line it
            // came out a comb, every segment running from the baseline to the
            // top and back, because that is what the numbers said.
            const double range = candle.high - candle.low;
            const double quiet = base_ * 0.010;   // what a dull period moves
            const double activity =
                0.5 + 0.5 * std::clamp(quiet > 0.0 ? range / quiet : 1.0, 0.0, 2.0);
            // The session's own rhythm, slower than a candle, so neighbouring
            // periods belong to the same stretch of the day.
            const double rhythm = kit::wave(start * 0.4f, seed_ + 2.2f, 0.8, 1.3);
            const double full = 3'200.0 * activity * rhythm;
            // The forming period has only earned its elapsed share.
            const double share = period > 0.0f ? (finish - start) / period : 1.0;
            volume_.push_back(full * std::max(0.05, static_cast<double>(share)));
            // And the same series without the period that is still filling.
            // A tile reading "volume per period" that ends on a bar a tenth
            // built ends on a dive every time anyone glances at it, and the
            // dive is about the clock rather than about the market.
            if (index < newest) settled_.push_back(volume_.back());

            times_.push_back(clockAt(index, period));
        }
    }

    /** The tape: the last prints, newest first, on a tick faster than a
     *  candle. Derived from the same function, so a trade and the candle it
     *  belongs to can never disagree. */
    std::vector<Trade> tape() const {
        constexpr float kTick = 0.28f;
        std::vector<Trade> out;
        const long newest = static_cast<long>(std::floor(now_ / kTick));
        for (long index = newest; index > newest - static_cast<long>(kPrints) && index >= 0;
             --index) {
            const float at = static_cast<float>(index) * kTick;
            const double price = priceAt(at);
            const double before = priceAt(at - kTick);
            out.push_back({clockAt(index, kTick), price,
                           std::round(kit::wave(at, 2.7f, 20.0, 640.0) / 10.0) * 10.0,
                           price >= before});
        }
        return out;
    }

    /** The book, built outward from the touch. Sizes wobble on their own
     *  waves, because a ladder where every level moves together reads as one
     *  bar chart rather than as a queue. */
    void book(std::vector<Level>& bids, std::vector<Level>& asks) const {
        bids.clear();
        asks.clear();
        const double mid = last();
        const double half = spread() / 2.0;
        for (std::size_t level = 0; level < kLevels; ++level) {
            const double step = static_cast<double>(level) * 0.02;
            const auto seed = static_cast<float>(level) * 1.7f;
            bids.push_back({mid - half - step,
                            std::round(kit::wave(now_ * 0.9f, seed + 0.3f, 120.0, 1'900.0))});
            asks.push_back({mid + half + step,
                            std::round(kit::wave(now_ * 0.9f, seed + 9.1f, 120.0, 1'900.0))});
        }
    }

    const std::vector<Candle>& candles() const { return candles_; }
    const std::vector<double>& volume() const { return volume_; }
    /** The same, without the period still filling. */
    const std::vector<double>& settledVolume() const { return settled_; }
    const std::vector<std::string>& times() const { return times_; }

    double last() const { return candles_.empty() ? 0.0 : candles_.back().close; }
    /** Where the session opened — the oldest candle the window holds, which is
     *  what "the change" on a screen like this is measured against. */
    double sessionOpen() const { return candles_.empty() ? 0.0 : candles_.front().open; }
    double change() const { return last() - sessionOpen(); }
    double changePercent() const {
        const double open = sessionOpen();
        return std::fabs(open) < 1e-9 ? 0.0 : change() / open * 100.0;
    }
    double high() const {
        double top = last();
        for (const Candle& candle : candles_) top = std::max(top, candle.high);
        return top;
    }
    double low() const {
        double bottom = last();
        for (const Candle& candle : candles_) bottom = std::min(bottom, candle.low);
        return bottom;
    }
    double turnover() const {
        double total = 0.0;
        for (const double one : volume_) total += one;
        return total;
    }
    /** Volume-weighted average price, which is the number a desk actually
     *  benchmarks a fill against. */
    double vwap() const {
        double weighted = 0.0;
        double shares = 0.0;
        for (std::size_t i = 0; i < candles_.size() && i < volume_.size(); ++i) {
            const double typical =
                (candles_[i].high + candles_[i].low + candles_[i].close) / 3.0;
            weighted += typical * volume_[i];
            shares += volume_[i];
        }
        return shares > 0.0 ? weighted / shares : last();
    }
    double spread() const { return 0.01 + 0.03 * kit::wave(now_ * 1.3f, 4.4f, 0.0, 1.0); }

private:
    /** A wall clock for a tick, so the axis reads like a session rather than
     *  like a sample number. Opens at 09:30 and runs on the chart's own
     *  period, which is why changing the timeframe changes the labels. */
    static std::string clockAt(long index, float period) {
        // Floored, not truncated. A window that reaches back past t=0 has
        // negative ticks in it — the session's first candles, drawn before the
        // demo's clock started — and C++ division rounds those towards zero,
        // which is how the axis came to read "09:30:-13".
        const long seconds = static_cast<long>(std::floor(static_cast<float>(index) * period));
        const long floored = seconds >= 0 ? seconds / 60 : (seconds - 59) / 60;
        const long minutes = 570 + floored;   // the bell, at 09:30
        char buffer[16];
        std::snprintf(buffer, sizeof(buffer), "%02ld:%02ld:%02ld", ((minutes / 60) % 24 + 24) % 24,
                      ((minutes % 60) + 60) % 60, ((seconds % 60) + 60) % 60);
        return buffer;
    }

    static constexpr std::size_t kCandles = 44;
    static constexpr std::size_t kPrints = 13;
    static constexpr std::size_t kLevels = 6;

    float now_ = 0.0f;
    float period_ = 1.6f;
    double base_ = 184.0;
    float seed_ = 3.1f;
    std::vector<Candle> candles_;
    std::vector<double> volume_;
    std::vector<double> settled_;
    std::vector<std::string> times_;
};

class Markets final : public Demo {
public:
    NodeId build(Frame& frame) override;

private:
    void ticker(Ui& ui, const Interaction& input, float seconds, float delta);
    void tiles(Ui& ui);
    void pricePanel(Ui& ui, const Interaction& input);
    void bookPanel(Ui& ui);
    void tapePanel(Ui& ui, const Interaction& input);
    void watchPanel(Ui& ui, const Interaction& input);

    Feed feed_{};
    // The price and the volume under it are one instrument. They point at one
    // sample — hovering either draws the crosshair and the readout on both —
    // and they show one window, so sweeping a range on either zooms the pair.
    ChartLink crosshair_{};
    ChartView view_{};
    MarqueeState ticker_{};
    /**
     * What the ticker is showing, and what it will show next.
     *
     * Two lists rather than one, and this is the whole of what makes the strip
     * hold still: prints arrive whenever the market says so, but a strip that
     * takes them *while it is passing* slides everything already on screen
     * sideways by the width of whatever was added or dropped — which is what a
     * reader sees as the thing resetting. So the incoming list is built up
     * quietly and swapped in on the one frame the strip comes round, where a
     * different pass beginning reads as the next lap.
     */
    std::vector<Quote> board_;
    std::vector<Quote> incoming_;
    /** The strip came round on the last frame, so the next list may be taken
     *  on now. */
    bool cameRound_ = false;
    /** When the last card was printed, on the demo's own clock. */
    float lastPrint_ = -1.0f;
    std::size_t nextSymbol_ = 0;
    std::size_t timeframe_ = 0;
    ScrollState page_{};
    TableState tape_{};
    TableState watch_{};
    std::vector<Trade> prints_;
    std::vector<Level> bids_;
    std::vector<Level> asks_;

    /** Which company the desk is on. The chart, the tiles, the book and the
     *  tape are all this one number. */
    std::size_t company_ = 0;

    std::vector<Watch> watchlist_ = {
        {"HLYD", "Halyard Industries", kit::Rolling{28, 176.0, 193.0, 3.1f, 0.9f}, 184.0, 3.1f,
         Logo{Color{0x2f, 0x7d, 0xf6, 1.0f}, 0}},
        {"NVAX", "Novatech Systems", kit::Rolling{28, 41.0, 48.0, 8.4f, 1.1f}, 44.0, 8.4f,
         Logo{Color{0x1f, 0xa8, 0x6b, 1.0f}, 1}},
        {"ORCX", "Orcus Energy", kit::Rolling{28, 88.0, 97.0, 2.2f, 0.8f}, 92.0, 2.2f,
         Logo{Color{0xe0, 0x8b, 0x1f, 1.0f}, 2}},
        {"TRMU", "Terramune", kit::Rolling{28, 12.4, 15.8, 6.9f, 1.3f}, 14.0, 6.9f,
         Logo{Color{0xb0, 0x4c, 0xd6, 1.0f}, 3}},
        {"KSTL", "Kestrel Freight", kit::Rolling{28, 63.0, 71.0, 4.7f, 1.0f}, 67.0, 4.7f,
         Logo{Color{0xd2, 0x44, 0x4a, 1.0f}, 2}},
        {"VRTY", "Verity Health", kit::Rolling{28, 209.0, 228.0, 9.8f, 0.7f}, 218.0, 9.8f,
         Logo{Color{0x18, 0x9c, 0xc4, 1.0f}, 0}},
    };
};

/** The timeframes, and what each one means to the candle clock. The labels are
 *  a trader's; the periods are what makes the chart visibly coarser, which is
 *  the part a reader can actually see in a demo. */
constexpr std::array<std::string_view, 4> kTimeframes = {"1m", "5m", "15m", "1h"};
constexpr std::array<float, 4> kPeriods = {1.1f, 1.7f, 2.6f, 4.0f};

/** A price, always with its two decimals: a ladder where 184.5 sits above
 *  184.48 is a ladder a reader has to parse rather than scan. */
std::string money(double value) { return kit::format("%.2f", value); }

/**
 * The tape along the top: every name on the desk, sliding past.
 *
 * The one thing on this screen that is not about the company selected — a
 * trading floor has a board, and the board carries the market whatever the
 * desk is looking at. It stops while the pointer is over it, which is the only
 * interaction a ticker has and the reason `marquee` takes the clock rather
 * than keeping one.
 */
void Markets::ticker(Ui& ui, const Interaction& input, float seconds, float delta) {
    // A print every couple of seconds, one name at a time, round the desk. Ten
    // cards is a little more than the strip can show, so there is always one
    // arriving and one about to leave.
    constexpr float kEvery = 1.8f;
    constexpr std::size_t kCards = 10;
    while (lastPrint_ < 0.0f || seconds - lastPrint_ >= kEvery) {
        const Watch& one = watchlist_[nextSymbol_ % watchlist_.size()];
        const double trend = one.history.trend();
        incoming_.push_back({std::string(one.symbol), "USD " + money(one.history.latest()),
                             kit::signedPercent(trend), trend >= 0.0});
        if (incoming_.size() > kCards) incoming_.erase(incoming_.begin());
        ++nextSymbol_;
        lastPrint_ = lastPrint_ < 0.0f ? seconds : lastPrint_ + kEvery;
        // A demo left open for an hour must not spend it catching up, the same
        // guard `kit::Rolling` makes for the same reason.
        if (seconds - lastPrint_ > kEvery * static_cast<float>(kCards)) lastPrint_ = seconds;
    }

    // Taken on at the seam, and on the first frame there is nothing to disturb.
    if (board_.empty() || cameRound_) board_ = incoming_;

    Style bar;
    bar.direction = Direction::Row;
    bar.align = Align::Center;
    bar.gap = 14.0f;
    bar.height = 38.0f;
    bar.shrink = 0.0f;
    bar.padding = Edges::symmetric(0.0f, 14.0f);
    bar.background = Fill{Token::BgElevated};
    auto scope = ui.scope(bar);

    kit::pill(ui, "LIVE", {.tone = kit::Tone::Ok});
    kit::field(ui, "LAST UPDATED", feed_.times().empty() ? "" : feed_.times().back(),
               Token::Text, FontRole::Mono);
    kit::rule(ui, Direction::Row);

    // Held while the pointer is over the strip, so a reader can stop it on a
    // name instead of chasing it — which is the whole of what `delta` is for.
    const bool held = input.isHovered("markets.ticker");

    cameRound_ = marquee(
        ui, input, "markets.ticker", ticker_, held ? 0.0f : delta,
        [&](Ui& lane) {
            for (const Quote& quote : board_) {
                Style card;
                card.direction = Direction::Row;
                card.align = Align::Center;
                card.gap = 8.0f;
                card.shrink = 0.0f;
                card.height = 24.0f;
                card.padding = Edges::symmetric(0.0f, 12.0f);
                card.margin = Edges::symmetric(0.0f, 5.0f);
                card.radius = 12.0f;
                card.background = Fill{Token::BgOverlay, 0.7f};
                card.border = Border{1.0f, Fill{Token::Border}};
                auto cardScope = lane.scope(card);

                text(lane, quote.symbol,
                     {.color = Token::TextMuted,
                      .weight = FontWeight::SemiBold,
                      .role = FontRole::Mono,
                      .size = 10.5f});
                text(lane, quote.price,
                     {.color = Token::TextStrong,
                      .weight = FontWeight::SemiBold,
                      .role = FontRole::Mono,
                      .size = 11.5f});
                kit::pill(lane, quote.change,
                          {.tone = quote.up ? kit::Tone::Ok : kit::Tone::Alarm, .size = 9.5f});
                (void)cardScope;
            }
        },
        // No gap of its own: the cards carry five pixels of margin either side,
        // so ten between them everywhere — and a gap here would apply *only* at
        // the seam, where it reads as a hole that comes round once a lap.
        {.speed = 34.0f, .gap = 0.0f});
}

void Markets::tiles(Ui& ui) {
    // Keyed by the company, not by the tile. An eased value slides from where
    // it was to where it is, which is right while one price moves and a lie
    // across two: switching from a stock at 184 to one at 14 animated the
    // headline through a hundred and twenty, a price neither of them ever had.
    // A key the animator has not seen starts where it is asked to.
    const std::string valueKey =
        "markets.last." + std::string(watchlist_[std::min(company_, watchlist_.size() - 1)].symbol);
    Style row;
    row.direction = Direction::Row;
    row.gap = 12.0f;
    row.shrink = 0.0f;
    auto scope = ui.scope(row);

    const bool up = feed_.change() >= 0.0;
    kit::statTile(ui, {.label = "LAST",
                       .value = money(kit::eased(ui, valueKey, "v", feed_.last())),
                       .tint = true,
                       .trend = kit::signedPercent(feed_.changePercent()),
                       .trendTone = up ? kit::Tone::Ok : kit::Tone::Alarm,
                       .tone = up ? kit::Tone::Ok : kit::Tone::Alarm});
    kit::statTile(ui, {.label = "SESSION CHANGE",
                       .value = kit::format("%+.2f", feed_.change()),
                       .unit = "USD",
                       .tone = up ? kit::Tone::Ok : kit::Tone::Alarm});
    kit::statTile(ui, {.label = "VWAP",
                       .value = money(feed_.vwap()),
                       .unit = feed_.last() >= feed_.vwap() ? "above" : "below",
                       .tone = kit::Tone::Info});
    kit::statTile(ui, {.label = "SESSION RANGE",
                       .value = money(feed_.low()) + " – " + money(feed_.high()),
                       .valueSize = 19.0f});
    kit::statTile(ui, {.label = "VOLUME",
                       .value = kit::compact(feed_.turnover()),
                       .unit = "sh",
                       .history = &feed_.settledVolume()});
    kit::statTile(ui, {.label = "SPREAD",
                       .value = kit::format("%.2f", feed_.spread()),
                       .unit = "USD",
                       .tone = feed_.spread() > 0.03 ? kit::Tone::Warn : kit::Tone::Ok});
}

void Markets::pricePanel(Ui& ui, const Interaction& input) {
    const Watch& company = watchlist_[std::min(company_, watchlist_.size() - 1)];
    const std::string heading = std::string(company.symbol) + " · " + std::string(company.name);
    // Two panes of one chart, and the column they sit in has no gap: the plots
    // meet on a hairline, so the crosshair and a swept range cross from one to
    // the other without anything in between having to be drawn. A gap here is
    // what made them read as two charts that agree rather than as one.
    auto card = kit::card(
        ui, {.title = heading,
             .note = std::string(kTimeframes[timeframe_]) + " candles · scroll to zoom",
             // The buttons drive the same view the gestures do, so a reader who
             // has lost the thread has a way back that needs no aim.
             .actions = [&](Ui& header) { chartToolbar(header, input, "markets.zoom", view_); },
             .gap = 0.0f,
             .grow = 2.4f,
             .minWidth = 420.0f});

    // One axis width for both charts, so a candle sits over its own volume bar.
    // Two charts that each size their own axis line up only by luck.
    constexpr float kAxis = 52.0f;
    candlestickChart(ui, input, "markets.candles", feed_.candles(), view_,
                     {.axisWidth = kAxis,
                      .height = 232.0f,
                      .valueFormat = "%.2f",
                      .categories = feed_.times(),
                      .categoryAxis = 0.0f,
                      .link = &crosshair_},
                     // The plain wheel, with no modifier: this chart is what the
                     // reader came to the screen for, and the page around it has
                     // somewhere else to be scrolled from.
                     {.wheel = true, .wheelModifier = false});

    const std::vector<Series> volume = {
        {.name = "Volume", .values = feed_.volume(), .color = Token::Accent}};

    // Forty-four times along an axis this wide is forty-four ellipses. Every
    // eighth is labelled and the rest are blank — which is what the axis of a
    // dense series is for, and the readout still names every one of them
    // through `tooltip.title`, out of the feed's own clock.
    std::vector<std::string> sparse(feed_.times().size());
    for (std::size_t i = 0; i < sparse.size(); ++i) {
        if (i % 8 == 0) sparse[i] = feed_.times()[i];
    }
    const std::vector<std::string>& stamps = feed_.times();

    barChart(ui, input, "markets.volume", volume, view_,
             {.tickCount = 2,
              .axisWidth = kAxis,
              .height = 74.0f,
              .grid = false,
              .valueFormat = "%.0f",
              // The same padding the candles use, which is what keeps the two
              // rows of marks on the same grid.
              .categoryPadding = 0.3f,
              .radius = 1.0f,
              .categories = sparse,
              .categoryAxis = 18.0f,
              .tooltip = {.title = [&stamps](const TooltipContext& at) {
                  return at.index < stamps.size() ? stamps[at.index] : std::string{};
              }},
              .link = &crosshair_,
              // No key under it: one series named "Volume" under a pane that is
              // visibly the volume is a line of type for nothing.
              .legend = {.show = false}},
             {.wheel = true, .wheelModifier = false});

    (void)card;
}

void Markets::bookPanel(Ui& ui) {
    auto card = kit::card(ui, {.title = "ORDER BOOK",
                                    .note = "depth · 6 levels",
                                    .gap = 1.0f,
                                    .width = 250.0f});

    double deepest = 1.0;
    for (const Level& level : asks_) deepest = std::max(deepest, level.size);
    for (const Level& level : bids_) deepest = std::max(deepest, level.size);

    // One row of the ladder: the depth bar is the row's own background, drawn
    // as a child under the text rather than beside it. A bar in a column of its
    // own would make the reader's eye travel to read one number.
    const auto ladderRow = [&](const Level& level, bool ask) {
        Style line;
        line.direction = Direction::Row;
        line.align = Align::Center;
        line.height = 20.0f;
        line.shrink = 0.0f;
        auto lineScope = ui.scope(line);

        Style depth;
        depth.position = Position::Absolute;
        depth.left = 0.0f;
        depth.top = 0.0f;
        depth.height = Length::percent(100);
        depth.width = Length::percent(
            static_cast<float>(std::clamp(level.size / deepest, 0.02, 1.0) * 100.0));
        depth.radius = 2.0f;
        depth.background = Fill{ask ? Token::Removed : Token::Added, 0.18f};
        ui.add(depth);

        text(ui, money(level.price),
             {.color = ask ? Token::Removed : Token::Added,
              .weight = FontWeight::Medium,
              .role = FontRole::Mono,
              .size = 11.5f,
              .grow = 1.0f});
        text(ui, kit::format("%.0f", level.size),
             {.color = Token::Text, .role = FontRole::Mono, .size = 11.5f});
        (void)lineScope;
    };

    // Asks from the outside in, so the two sides meet at the spread in the
    // middle — the shape of every book a trader has ever read.
    for (std::size_t level = asks_.size(); level-- > 0;) ladderRow(asks_[level], true);

    {
        Style touch;
        touch.direction = Direction::Row;
        touch.align = Align::Center;
        touch.gap = 6.0f;
        touch.height = 26.0f;
        touch.shrink = 0.0f;
        touch.padding = Edges::symmetric(0.0f, 6.0f);
        touch.background = Fill{Token::BgElevated};
        touch.radius = 4.0f;
        auto touchScope = ui.scope(touch);
        text(ui, "SPREAD", {.color = Token::TextMuted, .size = 10.0f, .grow = 1.0f});
        text(ui, kit::format("%.2f", feed_.spread()),
             {.color = Token::TextStrong,
              .weight = FontWeight::SemiBold,
              .role = FontRole::Mono,
              .size = 12.0f});
        (void)touchScope;
    }

    for (const Level& level : bids_) ladderRow(level, false);

    spacer(ui);
    kit::field(ui, "IMBALANCE", [&] {
        double bid = 0.0;
        double ask = 0.0;
        for (const Level& one : bids_) bid += one.size;
        for (const Level& one : asks_) ask += one.size;
        const double total = bid + ask;
        return kit::format("%.0f%% bid", total > 0.0 ? bid / total * 100.0 : 50.0);
    }());
}

void Markets::tapePanel(Ui& ui, const Interaction& input) {
    auto card = kit::card(ui, {.title = "TIME & SALES",
                                    .note = "every print, newest first",
                                    .gap = 0.0f,
                                    .grow = 1.0f});

    const std::vector<Column> columns = {
        {.title = "Time",
         .sizing = ColumnSize::FitContent,
         .fitSample = "99:99:99",
         .fitStyle = {.role = FontRole::Mono}},
        {.title = "Price",
         .sizing = ColumnSize::FitContent,
         .fitSample = "999.99",
         .fitStyle = {.role = FontRole::Mono},
         .align = TextAlign::End},
        {.title = "Size",
         .sizing = ColumnSize::FitContent,
         .fitSample = "9 999",
         .fitStyle = {.role = FontRole::Mono},
         .align = TextAlign::End},
        {.title = "Side", .width = 1.0f},
    };

    table(
        ui, input, "markets.tape", columns, prints_.size(), tape_,
        [&](Ui& cellUi, std::size_t row, std::size_t column) {
            const Trade& print = prints_[row];
            switch (column) {
                case 0:
                    text(cellUi, print.at,
                         {.color = Token::TextMuted, .role = FontRole::Mono, .size = 11.5f});
                    break;
                case 1:
                    text(cellUi, money(print.price),
                         {.color = print.buy ? Token::Added : Token::Removed,
                          .weight = FontWeight::Medium,
                          .role = FontRole::Mono,
                          .size = 11.5f,
                          .align = TextAlign::End,
                          .grow = 1.0f});
                    break;
                case 2:
                    text(cellUi, kit::format("%.0f", print.size),
                         {.color = Token::Text,
                          .role = FontRole::Mono,
                          .size = 11.5f,
                          .align = TextAlign::End,
                          .grow = 1.0f});
                    break;
                default:
                    badge(cellUi, print.buy ? "BUY" : "SELL",
                          {.background = print.buy ? Token::Added : Token::Removed,
                           .foreground = Token::TextStrong});
                    break;
            }
        },
        {.rowHeight = 26.0f, .zebra = true, .name = "Time and sales"});
}

void Markets::watchPanel(Ui& ui, const Interaction& input) {
    auto card = kit::card(ui, {.title = "WATCHLIST",
                                    .note = "click a column to sort",
                                    .gap = 0.0f,
                                    .grow = 1.0f});

    const std::vector<Column> columns = {
        // The mark, at a fixed width: it is a picture of a known size, and a
        // column that fitted to a header of no text would come out at nothing.
        {.title = "", .sizing = ColumnSize::Fixed, .width = 30.0f, .minWidth = 30.0f},
        {.title = "Symbol", .sizing = ColumnSize::FitContent, .fitSample = "MMMM", .sortable = true},
        {.title = "Name", .width = 1.4f},
        {.title = "Last",
         .sizing = ColumnSize::FitContent,
         .fitSample = "9 999.99",
         .fitStyle = {.role = FontRole::Mono},
         .align = TextAlign::End,
         .sortable = true},
        {.title = "Change",
         .sizing = ColumnSize::FitContent,
         .fitSample = "-99.9%",
         .fitStyle = {.role = FontRole::Mono},
         .align = TextAlign::End,
         .sortable = true},
        {.title = "Session", .width = 1.0f},
    };

    const TableResult result = table(
        ui, input, "markets.watch", columns, watchlist_.size(), watch_,
        [&](Ui& cellUi, std::size_t row, std::size_t column) {
            const Watch& one = watchlist_[row];
            const double trend = one.history.trend();
            switch (column) {
                case 0:
                    // The company's mark, at the size a row has room for. The
                    // pixels are the demo's own and outlive the frame, which is
                    // the whole of what `image` asks of a caller.
                    image(cellUi, one.logo.bitmap(),
                          {.width = 22.0f, .height = 22.0f, .radius = 6.0f});
                    break;
                case 1:
                    text(cellUi, one.symbol,
                         {.color = row == company_ ? Token::Accent : Token::TextStrong,
                          .weight = FontWeight::SemiBold,
                          .role = FontRole::Mono,
                          .size = 11.5f});
                    break;
                case 2:
                    text(cellUi, one.name, {.color = Token::TextMuted, .grow = 1.0f});
                    break;
                case 3:
                    text(cellUi, money(one.history.latest()),
                         {.color = Token::Text,
                          .role = FontRole::Mono,
                          .align = TextAlign::End,
                          .grow = 1.0f});
                    break;
                case 4:
                    text(cellUi, kit::signedPercent(trend),
                         {.color = trend >= 0.0 ? Token::Added : Token::Removed,
                          .role = FontRole::Mono,
                          .align = TextAlign::End,
                          .grow = 1.0f});
                    break;
                default:
                    kit::sparkline(cellUi, one.history.values(),
                                   {.width = 92.0f,
                                    .height = 20.0f,
                                    .tone = trend >= 0.0 ? kit::Tone::Ok : kit::Tone::Alarm});
                    break;
            }
        },
        {.rowHeight = 34.0f, .zebra = true, .name = "Watchlist"});

    // Picking a row is picking the company the whole desk is on.
    if (result.selectionChanged && watch_.selected < watchlist_.size()) {
        company_ = watch_.selected;
        // A different company is a different series, so the window on it starts
        // over — a range swept on one stock means nothing on the next.
        view_.reset();
    }

    if (result.sortChanged && watch_.sortColumn >= 0) {
        const int column = watch_.sortColumn;
        const bool ascending = watch_.ascending;
        std::stable_sort(watchlist_.begin(), watchlist_.end(),
                         [&](const Watch& a, const Watch& b) {
                             bool less = false;
                             switch (column) {
                                 case 1:
                                     less = a.symbol < b.symbol;
                                     break;
                                 case 4:
                                     less = a.history.trend() < b.history.trend();
                                     break;
                                 default:
                                     less = a.history.latest() < b.history.latest();
                                     break;
                             }
                             return ascending ? less : !less;
                         });
    }
}

NodeId Markets::build(Frame& frame) {
    Ui& ui = frame.ui;
    const Interaction& input = frame.input;

    const Watch& company = watchlist_[std::min(company_, watchlist_.size() - 1)];
    feed_.follow(company.base, company.seed);
    feed_.advance(frame.time, kPeriods[timeframe_]);
    prints_ = feed_.tape();
    feed_.book(bids_, asks_);
    for (Watch& one : watchlist_) one.history.advance(frame.time);

    Style window;
    window.direction = Direction::Column;
    window.background = Fill{Token::Bg};
    window.radius = 0.0f;
    auto root = ui.scope(window);

    {
        const bool up = feed_.change() >= 0.0;
        auto header = kit::header(ui, up ? Icon::TrendingUp : Icon::TrendingDown, "Halyard",
                                       "Equities desk · NYSE · consolidated tape");
        kit::hspace(ui, 4.0f);
        image(ui, company.logo.bitmap(), {.width = 26.0f, .height = 26.0f, .radius = 7.0f});
        kit::field(ui, std::string(company.symbol), company.name);
        spacer(ui);
        kit::pill(ui, "MARKET OPEN", {.tone = kit::Tone::Ok});
        {
            Style strip;
            strip.width = 200.0f;
            strip.shrink = 0.0f;
            auto stripScope = ui.scope(strip);
            std::vector<TabItem> items;
            for (std::string_view label : kTimeframes) items.push_back({.label = label});
            if (const auto chosen = tabs(ui, input, "markets.timeframe", items, timeframe_,
                                         {.thickness = 30.0f, .rule = false})) {
                timeframe_ = *chosen;
            }
        }
        button(ui, input, "BUY",
               {.variant = ButtonVariant::Primary, .leading = Icon::Plus, .id = "markets.buy"});
        button(ui, input, "SELL",
               {.variant = ButtonVariant::Ghost, .leading = Icon::Minus, .id = "markets.sell"});
    }
    ticker(ui, input, frame.time, frame.delta);
    kit::rule(ui, Direction::Column);

    {
        Style body;
        body.direction = Direction::Column;
        body.grow = 1.0f;
        body.basis = 0.0f;
        auto bodyScope = ui.scope(body);
        auto page = scrollArea(ui, input, "markets.page", page_,
                               {.padding = Edges::all(14.0f), .gap = 12.0f});

        tiles(ui);
        {
            Style row;
            row.direction = Direction::Row;
            row.gap = 12.0f;
            row.height = 386.0f;
            row.shrink = 0.0f;
            auto rowScope = ui.scope(row);
            pricePanel(ui, input);
            bookPanel(ui);
        }
        {
            Style row;
            row.direction = Direction::Row;
            row.gap = 12.0f;
            row.height = 262.0f;
            row.shrink = 0.0f;
            auto rowScope = ui.scope(row);
            tapePanel(ui, input);
            watchPanel(ui, input);
        }
    }

    kit::rule(ui, Direction::Column);
    {
        auto bar = kit::statusBar(ui);
        kit::statusItem(ui, Icon::TrendingUp, "feed: consolidated · 2 ms", kit::Tone::Ok);
        kit::statusItem(ui, Icon::ClockFading, feed_.times().empty() ? "" : feed_.times().back());
        spacer(ui);
        kit::statusItem(ui, Icon::Terminal,
                        kit::format("%.0f prints", static_cast<double>(prints_.size())));
        text(ui, "HALYARD 2.1", {.color = Token::TextMuted, .size = 10.5f});
    }
    return root.id();
}

}  // namespace

DemoInfo marketsDemo() {
    return {
        .id = "markets",
        .title = "Halyard Trading Desk",
        .sector = "Finance · Equities",
        .summary =
            "A live market screen: a candlestick chart whose newest candle is still "
            "forming, volume under it on the same grid, a depth ladder, the tape and a "
            "sortable watchlist.",
        .highlights = {"Forming candle", "Linked crosshair", "Zoom the pair", "Depth ladder"},
        .tryThis =
            "Watch the right-hand candle grow a wick and then roll over into a new one, "
            "hover anywhere for a crosshair that runs through both charts at once, and "
            "drag across either of them to zoom the pair to that range.",
        .design = {1280.0f, 824.0f},
        .palette = Palette::Follow,
        .create = [] { return std::unique_ptr<Demo>(new Markets()); }};
}

}  // namespace gbui::demos
