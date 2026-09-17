// Linux IPenSink: a uinput pen tablet whose ABS range is the monitor in pixels.  Proximity-in
// and the first position go out in one frame and the tip press in the next, because mutter
// only re-picks the surface under a tool on hover motion.
#pragma once

#include "IPenSink.h"

struct libevdev_uinput;

namespace t2t {

class UinputPenSink : public IPenSink {
public:
    UinputPenSink() = default;
    ~UinputPenSink() override;

    bool open(const Settings& s, QString* err) override;
    void close() override;
    bool isOpen() const override { return uidev_ != nullptr; }
    bool needsReopen(const Settings& s) const override;
    void handle(const PenEvent& ev) override;
    QString path() const override;

    static constexpr const char* kName = "touch2tablet Pen Tablet";
    static constexpr int kVendor = 0x222a, kProduct = 0x1001;

private:
    void write(unsigned type, unsigned code, int value);
    void syn();

    libevdev_uinput* uidev_ = nullptr;
    int width_ = 0, height_ = 0;
    bool pendingProx_ = false;   // proximity-in written, waiting for the first position to frame it
};

}  // namespace t2t
