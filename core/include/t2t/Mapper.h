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
    Mapper() = default;
    Mapper(const Settings& s, const RawRange& raw);

    MapResult map(int rx, int ry) const;
    void toMm(int rx, int ry, double& mmX, double& mmY) const;

private:
    RawRange raw_;
    double tabletW_ = 165.0, tabletH_ = 100.0;
    double areaW_ = 165.0, areaH_ = 100.0, areaX_ = 82.5, areaY_ = 50.0;
    double cos_ = 1.0, sin_ = 0.0;
    double dispW_ = 2560.0, dispH_ = 1440.0, dispX0_ = 0.0, dispY0_ = 0.0;
    int screenW_ = 2560, screenH_ = 1440;
    bool clip_ = true;
};

}  // namespace t2t
