#include "AbsoluteMouseSink.h"
#include "DisplayMapping.h"
#include "PointerDevices.h"
#include <algorithm>
#include <utility>

namespace t2t::windows {
AbsoluteMouseSink::AbsoluteMouseSink() : AbsoluteMouseSink(Api{
    [](const INPUT& input) { return SendInput(1, const_cast<INPUT*>(&input), sizeof(INPUT)) == 1; },
    [](const QString& name) -> std::optional<Bounds> {
        const auto monitor = monitorBounds(name);
        if (!monitor) return {};
        const LONG x = GetSystemMetrics(SM_XVIRTUALSCREEN), y = GetSystemMetrics(SM_YVIRTUALSCREEN);
        return Bounds{*monitor, {x, y, x + GetSystemMetrics(SM_CXVIRTUALSCREEN), y + GetSystemMetrics(SM_CYVIRTUALSCREEN)}};
    }}) {}

AbsoluteMouseSink::AbsoluteMouseSink(Api api) : api_(std::move(api)) {}

bool AbsoluteMouseSink::open(const Settings& s, QString* error)
{
    close();
    if (down_) { if (error) *error = error_; return false; }
    error_.clear();
    display_ = s.display;
    const auto b = api_.bounds(display_.output);
    if (display_.width <= 0 || display_.height <= 0 || !b
        || b->monitor.right <= b->monitor.left || b->monitor.bottom <= b->monitor.top
        || b->desktop.right <= b->desktop.left || b->desktop.bottom <= b->desktop.top) {
        error_ = QStringLiteral("Selected monitor or virtual desktop is unavailable");
        if (error) *error = error_;
        return false;
    }
    open_ = true;
    if (error) error->clear();
    return true;
}

bool AbsoluteMouseSink::needsReopen(const Settings& s) const
{
    return !open_ || display_ != s.display;
}

bool AbsoluteMouseSink::send(const INPUT& input)
{
    if (api_.inject(input)) return true;
    error_ = winError(QStringLiteral("SendInput"));
    open_ = false;
    return false;
}

bool AbsoluteMouseSink::release()
{
    if (!down_) return true;
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    if (!send(input)) return false;
    down_ = false;
    return true;
}

void AbsoluteMouseSink::close()
{
    // Release without a coordinate: this also works after the monitor disappears.
    release();
    open_ = wanted_ = false;
}

bool AbsoluteMouseSink::move(int x, int y, DWORD button)
{
    const auto b = api_.bounds(display_.output);
    if (!b || b->desktop.right <= b->desktop.left || b->desktop.bottom <= b->desktop.top
        || b->monitor.right <= b->monitor.left || b->monitor.bottom <= b->monitor.top) {
        error_ = QStringLiteral("Selected monitor disconnected");
        open_ = false;
        release();
        return false;
    }
    const LONG px = b->monitor.left + MulDiv(std::clamp(x, 0, display_.width - 1),
        b->monitor.right - b->monitor.left - 1, std::max(1, display_.width - 1));
    const LONG py = b->monitor.top + MulDiv(std::clamp(y, 0, display_.height - 1),
        b->monitor.bottom - b->monitor.top - 1, std::max(1, display_.height - 1));
    // Target the pixel centre in SendInput's 0..65535 virtual-desktop space.
    const auto normalise = [](LONG pixel, LONG origin, LONG size) -> LONG {
        return LONG(std::clamp<qint64>(((qint64(pixel) - origin) * 65536 + 32768) / size, 0, 65535));
    };
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK | button;
    input.mi.dx = normalise(px, b->desktop.left, b->desktop.right - b->desktop.left);
    input.mi.dy = normalise(py, b->desktop.top, b->desktop.bottom - b->desktop.top);
    if (send(input)) return true;
    release();
    return false;
}

void AbsoluteMouseSink::handle(const PenEvent& e)
{
    if (!open_) return;
    switch (e.type) {
    case PenEvent::Type::ProximityIn: wanted_ = true; break;
    case PenEvent::Type::Move:
        if (wanted_) move(e.x, e.y);
        break;
    case PenEvent::Type::Down:
        if (wanted_ && !down_ && move(e.x, e.y, MOUSEEVENTF_LEFTDOWN)) down_ = true;
        break;
    case PenEvent::Type::Up: release(); break;
    case PenEvent::Type::ProximityOut: release(); wanted_ = false; break;
    }
}
}
