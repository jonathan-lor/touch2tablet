// Linux IPenSink: a uinput device whose ABS range is the monitor in pixels, shown to the OS as an
// absolute mouse or, in pen mode, as a pen tablet (Settings::outputMode). Changing the mode
// re-creates the device.
//
// Mouse: the position goes out in one frame and the button in the next, so the pointer is over
// its target when the press lands. Pen: proximity-in and the first position go out in one frame
// and the tip press in the next, because mutter only re-picks the surface under a tool on hover
// motion.
#pragma once

#include "IPenSink.h"

#include <functional>
#include <memory>

struct libevdev;

namespace t2t {

class UinputSink : public IPenSink {
public:
    /// A created device.  Tests substitute a recorder for the real uinput one.
    struct Device {
        virtual ~Device() = default;
        virtual void write(unsigned type, unsigned code, int value) = 0;
        virtual QString path() const = 0;
    };
    /// Create a device as described; nullptr + *err on failure.
    using CreateDevice = std::function<std::unique_ptr<Device>(const libevdev* description, QString* err)>;

    UinputSink();
    explicit UinputSink(CreateDevice create);
    ~UinputSink() override;

    bool open(const Settings& s, QString* err) override;
    void close() override;
    bool isOpen() const override { return device_ != nullptr; }
    bool needsReopen(const Settings& s) const override;
    void handle(const PenEvent& ev) override;
    QString path() const override;
    QString kind() const override;

    static constexpr const char* kMouseName = "touch2tablet Absolute Mouse";
    static constexpr const char* kPenName = "touch2tablet Pen Tablet";
    static constexpr int kVendor = 0x222a, kMouseProduct = 0x1002, kPenProduct = 0x1001;

private:
    void handleMouse(const PenEvent& ev);
    void handlePen(const PenEvent& ev);
    void moveMouse(int x, int y);
    void write(unsigned type, unsigned code, int value);
    void syn();

    CreateDevice create_;
    std::unique_ptr<Device> device_;
    OutputMode mode_ = OutputMode::Mouse;
    int width_ = 0, height_ = 0;
    bool strokeStart_ = false;   // mouse: the next position is the stroke's first (see moveMouse)
    bool buttonDown_ = false;    // mouse
    bool pendingProx_ = false;   // pen: proximity-in written, waiting for the first position to frame it
};

}  // namespace t2t
