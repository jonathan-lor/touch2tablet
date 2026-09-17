// Linux ITouchSource: libevdev over an evdev node with an exclusive grab (EVIOCGRAB), so the
// compositor stops seeing the panel as a touchscreen while we translate it.
#pragma once

#include "ITouchSource.h"

#include <QSocketNotifier>
#include <memory>

struct libevdev;

namespace t2t {

class EvdevTouchSource : public ITouchSource {
    Q_OBJECT
public:
    /// Scan /dev/input/event* for a listed panel (Panels.h) with ABS_MT_POSITION_X/Y; open + grab
    /// it.  Returns nullptr if absent or not openable (the reason is written to *err if given).
    static std::unique_ptr<EvdevTouchSource> probe(QString* err = nullptr);

    /// For adding a panel: every slotted multitouch node as a ready-to-paste panels.json entry.
    static QString identify();

    ~EvdevTouchSource() override;
    Info info() const override { return info_; }

private:
    EvdevTouchSource(int fd, libevdev* dev, Info info);
    void readAll();

    int fd_ = -1;
    libevdev* dev_ = nullptr;
    Info info_;
    std::unique_ptr<QSocketNotifier> notifier_;
    TouchFrame pending_;
    int slot_ = 0;
    std::optional<int> px_, py_;   // position accumulating for slot_ within the frame
    void flushPos();
};

}  // namespace t2t
