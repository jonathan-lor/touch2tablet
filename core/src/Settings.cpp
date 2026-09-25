#include "t2t/Settings.h"

#include <QJsonValue>
#include <cmath>

namespace t2t {

namespace {

// Numeric field: present + finite -> clamped value, else `current`.
double num(const QJsonObject& o, const char* key, double current, double lo, double hi)
{
    const QJsonValue v = o.value(QLatin1String(key));
    if (!v.isDouble()) return current;
    const double d = v.toDouble();
    if (!std::isfinite(d)) return current;
    return std::clamp(d, lo, hi);
}

int inum(const QJsonObject& o, const char* key, int current, int lo, int hi)
{
    return static_cast<int>(std::lround(num(o, key, current, lo, hi)));
}

bool flag(const QJsonObject& o, const char* key, bool current)
{
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isBool() ? v.toBool() : current;
}

QJsonObject section(const QJsonObject& o, const char* key)
{
    const QJsonValue v = o.value(QLatin1String(key));
    return v.isObject() ? v.toObject() : QJsonObject();
}

}  // namespace

Settings Settings::applied(const Settings& base, const QJsonObject& json)
{
    Settings s = base;

    if (const auto d = section(json, "display"); !d.isEmpty()) {
        s.display.width = inum(d, "width", s.display.width, 16, 32768);
        s.display.height = inum(d, "height", s.display.height, 16, 32768);
        s.display.widthMm = num(d, "width_mm", s.display.widthMm, 10, 5000);
        s.display.heightMm = num(d, "height_mm", s.display.heightMm, 10, 5000);
        if (d.value("output").isString() && d.value("output").toString().size() <= 128)
            s.display.output = d.value("output").toString();
    }
    if (const auto d = section(json, "display_area"); !d.isEmpty()) {
        s.displayArea.width = num(d, "width", s.displayArea.width, 1, 65536);
        s.displayArea.height = num(d, "height", s.displayArea.height, 1, 65536);
        s.displayArea.x = num(d, "x", s.displayArea.x, -65536, 65536);
        s.displayArea.y = num(d, "y", s.displayArea.y, -65536, 65536);
    }
    if (const auto t = section(json, "tablet"); !t.isEmpty()) {
        s.tablet.width = num(t, "width", s.tablet.width, 1, 2000);
        s.tablet.height = num(t, "height", s.tablet.height, 1, 2000);
    }
    if (const auto t = section(json, "tablet_area"); !t.isEmpty()) {
        s.tabletArea.width = num(t, "width", s.tabletArea.width, 0.5, 5000);
        s.tabletArea.height = num(t, "height", s.tabletArea.height, 0.5, 5000);
        s.tabletArea.x = num(t, "x", s.tabletArea.x, -5000, 5000);
        s.tabletArea.y = num(t, "y", s.tabletArea.y, -5000, 5000);
        double rot = num(t, "rotation", s.tabletArea.rotation, -1e9, 1e9);
        rot = std::fmod(rot, 360.0);
        if (rot < 0) rot += 360.0;
        s.tabletArea.rotation = rot;
    }
    s.clip = flag(json, "clip", s.clip);
    s.limit = flag(json, "limit", s.limit);
    s.lockAspect = flag(json, "lock_aspect", s.lockAspect);
    return s;
}

QJsonObject Settings::toJson() const
{
    return QJsonObject{
        {"version", 1},
        {"display", QJsonObject{{"width", display.width}, {"height", display.height},
                                {"width_mm", display.widthMm}, {"height_mm", display.heightMm},
                                {"output", display.output}}},
        {"display_area", QJsonObject{{"width", displayArea.width}, {"height", displayArea.height},
                                     {"x", displayArea.x}, {"y", displayArea.y}}},
        {"tablet", QJsonObject{{"width", tablet.width}, {"height", tablet.height}}},
        {"tablet_area", QJsonObject{{"width", tabletArea.width}, {"height", tabletArea.height},
                                    {"x", tabletArea.x}, {"y", tabletArea.y},
                                    {"rotation", tabletArea.rotation}}},
        {"clip", clip},
        {"limit", limit},
        {"lock_aspect", lockAspect},
    };
}

}  // namespace t2t
