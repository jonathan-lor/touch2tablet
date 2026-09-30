#include "UinputSink.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <libevdev/libevdev-uinput.h>
#include <libevdev/libevdev.h>
#include <utility>

namespace t2t {

namespace {

class UinputDevice : public UinputSink::Device {
public:
    explicit UinputDevice(libevdev_uinput* dev) : dev_(dev) {}
    ~UinputDevice() override { libevdev_uinput_destroy(dev_); }
    void write(unsigned type, unsigned code, int value) override { libevdev_uinput_write_event(dev_, type, code, value); }
    QString path() const override { return QString::fromLocal8Bit(libevdev_uinput_get_devnode(dev_)); }

private:
    libevdev_uinput* dev_;
};

std::unique_ptr<UinputSink::Device> createUinputDevice(const libevdev* description, QString* err)
{
    libevdev_uinput* dev = nullptr;
    const int rc = libevdev_uinput_create_from_device(description, LIBEVDEV_UINPUT_OPEN_MANAGED, &dev);
    if (rc < 0) {
        if (err) *err = QStringLiteral("uinput: %1").arg(QString::fromLocal8Bit(std::strerror(-rc)));
        return nullptr;
    }
    return std::make_unique<UinputDevice>(dev);
}

}  // namespace

UinputSink::UinputSink() : UinputSink(createUinputDevice) {}

UinputSink::UinputSink(CreateDevice create) : create_(std::move(create)) {}

UinputSink::~UinputSink()
{
    close();
}

bool UinputSink::open(const Settings& s, QString* err)
{
    close();
    const bool pen = s.outputMode == OutputMode::Pen;
    libevdev* dev = libevdev_new();
    libevdev_set_name(dev, pen ? kPenName : kMouseName);
    // libinput counts a USB mouse as external, which can switch a laptop's touchpad off.
    libevdev_set_id_bustype(dev, pen ? BUS_USB : BUS_VIRTUAL);
    libevdev_set_id_vendor(dev, kVendor);
    libevdev_set_id_product(dev, pen ? kPenProduct : kMouseProduct);
    libevdev_set_id_version(dev, 1);

    libevdev_enable_event_type(dev, EV_KEY);
    if (pen) {
        libevdev_enable_event_code(dev, EV_KEY, BTN_TOOL_PEN, nullptr);
        libevdev_enable_event_code(dev, EV_KEY, BTN_TOUCH, nullptr);
    } else {
        libevdev_enable_event_code(dev, EV_KEY, BTN_LEFT, nullptr);   // absolute axes, no pen bits: udev tags a mouse
    }

    // libinput rejects tablets without a resolution; px/mm of the monitor is the honest value.
    const int W = s.display.width, H = s.display.height;
    input_absinfo ax{}; ax.maximum = W - 1; ax.resolution = std::max(1, int(std::lround(W / s.display.widthMm)));
    input_absinfo ay{}; ay.maximum = H - 1; ay.resolution = std::max(1, int(std::lround(H / s.display.heightMm)));
    libevdev_enable_event_type(dev, EV_ABS);
    libevdev_enable_event_code(dev, EV_ABS, ABS_X, &ax);
    libevdev_enable_event_code(dev, EV_ABS, ABS_Y, &ay);

    device_ = create_(dev, err);
    libevdev_free(dev);
    if (!device_) return false;
    mode_ = s.outputMode;
    width_ = W; height_ = H;
    strokeStart_ = buttonDown_ = pendingProx_ = false;
    return true;
}

void UinputSink::close()
{
    if (!device_) return;
    // Leave the device clean before it disappears.
    if (mode_ == OutputMode::Pen) {
        write(EV_KEY, BTN_TOUCH, 0);
        write(EV_KEY, BTN_TOOL_PEN, 0);
        syn();
    } else if (std::exchange(buttonDown_, false)) {
        write(EV_KEY, BTN_LEFT, 0);
        syn();
    }
    device_.reset();
}

bool UinputSink::needsReopen(const Settings& s) const
{
    return !device_ || s.outputMode != mode_ || s.display.width != width_ || s.display.height != height_;
}

QString UinputSink::path() const
{
    return device_ ? device_->path() : QString();
}

QString UinputSink::kind() const
{
    return mode_ == OutputMode::Pen ? QStringLiteral("pen tablet") : QStringLiteral("absolute mouse");
}

void UinputSink::write(unsigned type, unsigned code, int value)
{
    if (device_) device_->write(type, code, value);
}

void UinputSink::syn()
{
    write(EV_SYN, SYN_REPORT, 0);
}

void UinputSink::handle(const PenEvent& ev)
{
    if (!device_) return;
    if (mode_ == OutputMode::Pen)
        handlePen(ev);
    else
        handleMouse(ev);
}

void UinputSink::handleMouse(const PenEvent& ev)
{
    switch (ev.type) {
    case PenEvent::Type::ProximityIn:
        strokeStart_ = true;
        break;
    case PenEvent::Type::Move:
        moveMouse(ev.x, ev.y);
        break;
    case PenEvent::Type::Down:
        if (strokeStart_) moveMouse(ev.x, ev.y);   // no position came first; still press in a frame of its own
        write(EV_KEY, BTN_LEFT, 1);
        syn();
        buttonDown_ = true;
        break;
    case PenEvent::Type::Up:
    case PenEvent::Type::ProximityOut:
        if (std::exchange(buttonDown_, false)) {
            write(EV_KEY, BTN_LEFT, 0);
            syn();
        }
        break;
    }
}

void UinputSink::moveMouse(int x, int y)
{
    // The kernel drops a value equal to the device's last one, so a stroke starting where the
    // previous one ended would leave the pointer wherever another mouse has since moved it.
    // Nudging X first gets the position through; libinput reports only the frame's final value.
    if (std::exchange(strokeStart_, false))
        write(EV_ABS, ABS_X, x > 0 ? x - 1 : x + 1);
    write(EV_ABS, ABS_X, x);
    write(EV_ABS, ABS_Y, y);
    syn();
}

void UinputSink::handlePen(const PenEvent& ev)
{
    switch (ev.type) {
    case PenEvent::Type::ProximityIn:
        write(EV_KEY, BTN_TOOL_PEN, 1);
        pendingProx_ = true;   // framed together with the first position
        break;
    case PenEvent::Type::Move:
        write(EV_ABS, ABS_X, ev.x);
        write(EV_ABS, ABS_Y, ev.y);
        syn();
        pendingProx_ = false;
        break;
    case PenEvent::Type::Down:
        if (pendingProx_) syn();   // no position came first; still keep the tip in its own frame
        pendingProx_ = false;
        write(EV_KEY, BTN_TOUCH, 1);
        syn();
        break;
    case PenEvent::Type::Up:
        write(EV_KEY, BTN_TOUCH, 0);
        syn();
        break;
    case PenEvent::Type::ProximityOut:
        write(EV_KEY, BTN_TOOL_PEN, 0);
        syn();
        pendingProx_ = false;
        break;
    }
}

}  // namespace t2t
