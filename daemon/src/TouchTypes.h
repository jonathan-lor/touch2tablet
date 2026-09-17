// Frame-level touch input as delivered by an ITouchSource.
#pragma once

#include <optional>
#include <vector>

namespace t2t {

struct TouchEvent {
    enum class Type { Begin, End, Position };
    Type type;
    int slot = 0;
    std::optional<int> x, y;   // Position only
};

struct TouchFrame {
    std::vector<TouchEvent> events;   // in device order, one SYN_REPORT's worth
};

}  // namespace t2t
