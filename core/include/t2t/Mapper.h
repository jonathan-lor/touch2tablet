// touch2tablet core: raw touch coordinates -> output pixels.
//
//   raw -> mm on the glass -> rotate into the tablet-area frame -> normalize to the area
//   (clip) -> display area in px -> clamp to the display.
//
// Rotation is clockwise on screen (y down), matching the GUI canvas.
#pragma once

#include "t2t/Settings.h"

namespace t2t {

struct RawRange {   // defaults: what the GT911 panel reports
    int xmin = 0;
    int xmax = 1024;
    int ymin = 0;
    int ymax = 600;
    bool operator==(const RawRange&) const = default;
};

struct MapResult {
    int x = 0;          // output px, clamped to [0, display.width-1]
    int y = 0;          // output px, clamped to [0, display.height-1]
    bool inside = true; // touch was inside the tablet area before clipping
    double mmX = 0.0;   // position on the glass, mm
    double mmY = 0.0;
};

class Mapper {
public:
    Mapper(const Settings& s, const RawRange& raw);

    MapResult map(int rx, int ry) const;

private:
    RawRange raw_;
    double tabletW_, tabletH_;
    double areaW_, areaH_, areaX_, areaY_;
    double cos_, sin_;
    double dispW_, dispH_, dispX0_, dispY0_;
    int screenW_, screenH_;
    bool clip_;
};

}  // namespace t2t
