#include "t2t/Mapper.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace t2t {

Mapper::Mapper(const Settings& s, const RawRange& raw)
    : raw_(raw),
      tabletW_(s.tablet.width), tabletH_(s.tablet.height),
      areaW_(s.tabletArea.width), areaH_(s.tabletArea.height),
      areaX_(s.tabletArea.x), areaY_(s.tabletArea.y),
      dispW_(s.displayArea.width), dispH_(s.displayArea.height),
      dispX0_(s.displayArea.x - s.displayArea.width / 2.0),
      dispY0_(s.displayArea.y - s.displayArea.height / 2.0),
      screenW_(s.display.width), screenH_(s.display.height),
      clip_(s.clip)
{
    const double r = s.tabletArea.rotation * std::numbers::pi / 180.0;
    cos_ = std::cos(r);
    sin_ = std::sin(r);
}

void Mapper::toMm(int rx, int ry, double& mmX, double& mmY) const
{
    const double xr = std::max(1, raw_.xmax - raw_.xmin);
    const double yr = std::max(1, raw_.ymax - raw_.ymin);
    mmX = (rx - raw_.xmin) / xr * tabletW_;
    mmY = (ry - raw_.ymin) / yr * tabletH_;
}

MapResult Mapper::map(int rx, int ry) const
{
    MapResult m;
    toMm(rx, ry, m.mmX, m.mmY);
    const double dx = m.mmX - areaX_, dy = m.mmY - areaY_;
    // area is rotated clockwise by `rotation`; bring the point into the area's frame
    const double lx = dx * cos_ + dy * sin_;
    const double ly = -dx * sin_ + dy * cos_;
    double u = lx / areaW_ + 0.5;
    double v = ly / areaH_ + 0.5;
    m.inside = u >= 0.0 && u <= 1.0 && v >= 0.0 && v <= 1.0;
    if (clip_) {
        u = std::clamp(u, 0.0, 1.0);
        v = std::clamp(v, 0.0, 1.0);
    }
    const double ox = std::clamp(dispX0_ + u * dispW_, 0.0, double(screenW_ - 1));
    const double oy = std::clamp(dispY0_ + v * dispH_, 0.0, double(screenH_ - 1));
    m.x = static_cast<int>(std::lround(ox));
    m.y = static_cast<int>(std::lround(oy));
    return m;
}

}  // namespace t2t
