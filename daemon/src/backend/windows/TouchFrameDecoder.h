#pragma once

#include "TouchTypes.h"
#include <set>

namespace t2t::windows {

struct ContactSample {
    int id, x, y;
    bool touching;
};

// Decode a complete Windows contact snapshot, not individual WM_POINTER messages.
// Native collection/historical-data handling stays outside this state machine.
class TouchFrameDecoder {
public:
    TouchFrame decode(const std::vector<ContactSample>& contacts)
    {
        TouchFrame frame;
        std::set<int> current;
        for (const auto& c : contacts) {
            if (!c.touching || !current.insert(c.id).second) continue;
            if (!active_.contains(c.id))
                frame.events.push_back({TouchEvent::Type::Begin, c.id, {}, {}});
            frame.events.push_back({TouchEvent::Type::Position, c.id, c.x, c.y});
        }
        // Introduce new fingers before ending old ones: a replacement finger in
        // the same frame must not inherit an existing primary's stroke.
        for (int id : active_)
            if (!current.contains(id)) frame.events.push_back({TouchEvent::Type::End, id, {}, {}});
        active_ = std::move(current);
        return frame;
    }
    void reset() { active_.clear(); }
private:
    std::set<int> active_;
};

} // namespace t2t::windows
