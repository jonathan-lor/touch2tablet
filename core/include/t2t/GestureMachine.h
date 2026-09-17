// touch2tablet core: multitouch slots -> pen events.
//
// Feed it evdev-style slot events (trackingBegin / trackingEnd / position) followed by frame()
// at each SYN_REPORT.  It emits abstract PenEvents; how they are framed on a real device is the
// sink's business.
//
// Behavior (tip-only):
//   * the first slot to appear is the primary and drives the position; the tip goes down on the
//     first frame that carries its coordinates (GT911 sometimes reports a tracking id before the
//     slot's coordinates; that frame is simply waited out)
//   * extra contacts change nothing; when the primary lifts while others remain the stroke ends
//     and the leftovers are ignored until everything lifts
//   * with `limit` on, a sequence that starts outside the tablet area is ignored entirely
//   * the tip and proximity are released when the last finger lifts
#pragma once

#include "t2t/Mapper.h"
#include "t2t/Settings.h"

#include <functional>
#include <optional>
#include <vector>

namespace t2t {

struct PenEvent {
    enum class Type { ProximityIn, Move, Down, Up, ProximityOut };
    Type type;
    int x = 0;   // output px (Move, Down)
    int y = 0;
};

class GestureMachine {
public:
    using Emit = std::function<void(const PenEvent&)>;

    /// Snapshot for status / GUI position streams; read after frame().
    struct State {
        int fingers = 0;             // slots currently down
        bool primaryReady = false;   // primary slot has coordinates
        int rawX = 0, rawY = 0;      // primary raw position
        MapResult mapped;            // primary mapped (valid when primaryReady)
        bool active = false;         // tip is down
        bool ignored = false;        // rest of this sequence is dropped
    };

    GestureMachine(const Settings& s, const RawRange& raw, Emit sink);

    void setSettings(const Settings& s);
    void setRawRange(const RawRange& raw);

    void trackingBegin(int slot);
    void trackingEnd(int slot);
    void position(int slot, std::optional<int> x, std::optional<int> y);
    void frame();   // SYN_REPORT

    /// Release everything and leave proximity (device going away, stream lost sync).
    void reset();

    const State& state() const { return state_; }

private:
    struct Slot {
        int id;
        std::optional<int> x, y;
        bool ready() const { return x && y; }
    };
    Slot* find(int slot);
    Slot* primary();
    void release();
    void updateState();

    Settings settings_;
    RawRange raw_;
    Mapper mapper_;
    Emit emit_;

    std::vector<Slot> slots_;   // insertion order
    std::optional<int> primary_;
    bool active_ = false;
    bool ignored_ = false;
    State state_;
};

}  // namespace t2t
