// touch2tablet core: settings model.  The JSON keys are documented in the README.
#pragma once

#include <QJsonObject>
#include <QString>

namespace t2t {

struct Display {
    int width = 2560;
    int height = 1440;
    double widthMm = 597.0;
    double heightMm = 336.0;
    QString output; // Windows display device name; empty follows the primary monitor.
    bool operator==(const Display&) const = default;
};

struct DisplayArea {
    double width = 2560.0;
    double height = 1440.0;
    double x = 1280.0;
    double y = 720.0;
    bool operator==(const DisplayArea&) const = default;
};

struct Tablet {
    double width = 165.0;
    double height = 100.0;
    bool operator==(const Tablet&) const = default;
};

/// What the OS sees.
/// Linux only. Windows always presents an absolute mouse.
enum class OutputMode {
    Mouse,   // absolute mouse: works wherever a mouse does
    Pen,     // pen tablet, for applications that read pen input
};

struct TabletArea {
    double width = 165.0;
    double height = 100.0;
    double x = 82.5;
    double y = 50.0;
    double rotation = 0.0;   // degrees, clockwise, normalized to [0, 360)
    bool operator==(const TabletArea&) const = default;
};

struct Settings {
    Display display;
    DisplayArea displayArea;
    Tablet tablet;
    TabletArea tabletArea;
    bool clip = true;          // clamp touches outside the tablet area to its edge
    bool limit = false;        // ignore touch sequences that start outside the tablet area
    bool lockAspect = false;   // GUI hint only
    OutputMode outputMode = OutputMode::Mouse;

    bool operator==(const Settings&) const = default;

    static Settings defaults() { return Settings{}; }

    /// Apply a (possibly partial, possibly garbage) JSON patch onto `base`.  Every field present
    /// and valid in `json` replaces the base value, clamped to sane ranges; anything missing or
    /// invalid keeps the base value.
    static Settings applied(const Settings& base, const QJsonObject& json);
    static Settings fromJson(const QJsonObject& json) { return applied(defaults(), json); }

    QJsonObject toJson() const;
};

}  // namespace t2t
