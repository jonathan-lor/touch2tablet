#include "UinputPenSink.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <libevdev/libevdev-uinput.h>
#include <libevdev/libevdev.h>

namespace t2t {

UinputPenSink::~UinputPenSink()
{
    close();
}

bool UinputPenSink::open(const Settings& s, QString* err)
{
    close();
    libevdev* dev = libevdev_new();
    libevdev_set_name(dev, kName);
    libevdev_set_id_bustype(dev, BUS_USB);
    libevdev_set_id_vendor(dev, kVendor);
    libevdev_set_id_product(dev, kProduct);
    libevdev_set_id_version(dev, 1);

    libevdev_enable_event_type(dev, EV_KEY);
    libevdev_enable_event_code(dev, EV_KEY, BTN_TOOL_PEN, nullptr);
    libevdev_enable_event_code(dev, EV_KEY, BTN_TOUCH, nullptr);

    // libinput rejects tablets without a resolution; px/mm of the monitor is the honest value.
    const int W = s.display.width, H = s.display.height;
    input_absinfo ax{}; ax.maximum = W - 1; ax.resolution = std::max(1, int(std::lround(W / s.display.widthMm)));
    input_absinfo ay{}; ay.maximum = H - 1; ay.resolution = std::max(1, int(std::lround(H / s.display.heightMm)));
    libevdev_enable_event_type(dev, EV_ABS);
    libevdev_enable_event_code(dev, EV_ABS, ABS_X, &ax);
    libevdev_enable_event_code(dev, EV_ABS, ABS_Y, &ay);

    const int rc = libevdev_uinput_create_from_device(dev, LIBEVDEV_UINPUT_OPEN_MANAGED, &uidev_);
    libevdev_free(dev);
    if (rc < 0) {
        uidev_ = nullptr;
        if (err) *err = QStringLiteral("uinput: %1").arg(QString::fromLocal8Bit(std::strerror(-rc)));
        return false;
    }
    width_ = W; height_ = H;
    pendingProx_ = false;
    return true;
}

void UinputPenSink::close()
{
    if (!uidev_) return;
    write(EV_KEY, BTN_TOUCH, 0);   // leave the device clean before it disappears
    write(EV_KEY, BTN_TOOL_PEN, 0);
    syn();
    libevdev_uinput_destroy(uidev_);
    uidev_ = nullptr;
}

bool UinputPenSink::needsReopen(const Settings& s) const
{
    return !uidev_ || s.display.width != width_ || s.display.height != height_;
}

QString UinputPenSink::path() const
{
    return uidev_ ? QString::fromLocal8Bit(libevdev_uinput_get_devnode(uidev_)) : QString();
}

void UinputPenSink::write(unsigned type, unsigned code, int value)
{
    if (uidev_) libevdev_uinput_write_event(uidev_, type, code, value);
}

void UinputPenSink::syn()
{
    write(EV_SYN, SYN_REPORT, 0);
}

void UinputPenSink::handle(const PenEvent& ev)
{
    if (!uidev_) return;
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
