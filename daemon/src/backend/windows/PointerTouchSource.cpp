#include "PointerTouchSource.h"
#include <tpcshrd.h>
#include <algorithm>

namespace t2t::windows {
namespace {
constexpr wchar_t windowClass[] = L"touch2tablet.PointerCapture";
bool supported(const PointerDevice& d)
{
    return d.panel && d.panel->evdevName == QStringLiteral("UsbHID ")
        + QString::fromWCharArray(d.native.productString);
}
}

PointerTouchSource::PointerTouchSource(PointerDevice device) : device_(std::move(device))
{
    connect(&watchdog_, &QTimer::timeout, this, &PointerTouchSource::checkDevice);
}

std::unique_ptr<PointerTouchSource> PointerTouchSource::probe(QString* error)
{
    error->clear();
    if (!hasUiAccess(error)) {
        if (error->isEmpty()) *error = QStringLiteral("Run the signed daemon installed in Program Files (UIAccess required)");
        return {};
    }
    auto devices = pointerDevices(error);
    if (!error->isEmpty()) return {};
    const PointerDevice* selected = nullptr;
    int touchCount = 0;
    for (const auto& d : devices) {
        if (d.native.pointerDeviceType != POINTER_DEVICE_TYPE_TOUCH) continue;
        ++touchCount;
        if (supported(d)) selected = &d;
    }
    if (!selected) return {};
    // RegisterPointerInputTarget redirects ALL touchscreens. Do not swallow input
    // from a second screen: leave normal touch operational until it is removed.
    if (touchCount != 1) {
        *error = QStringLiteral("Windows capture currently requires one touchscreen; disconnect the other touchscreen to enable tablet mode");
        return {};
    }
    auto source = std::unique_ptr<PointerTouchSource>(new PointerTouchSource(*selected));
    for (USHORT usage : {USHORT(0x30), USHORT(0x31)}) {
        auto p = std::find_if(selected->properties.begin(), selected->properties.end(), [usage](const auto& v) {
            return v.usagePageId == 1 && v.usageId == usage;
        });
        if (p == selected->properties.end() || p->logicalMax <= p->logicalMin) {
            *error = QStringLiteral("Panel has no usable raw X/Y range");
            return {};
        }
        source->axes_.push_back(*p);
    }
    WNDCLASSW cls{};
    cls.lpfnWndProc = windowProc;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = windowClass;
    if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        *error = winError("RegisterClass"); return {};
    }
    source->window_ = CreateWindowExW(0, windowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE,
                                     nullptr, cls.hInstance, source.get());
    if (!source->window_) { *error = winError("CreateWindowEx"); return {}; }
    // Redirection alone does not opt out of Windows' touch rings/gesture UI.
    // Scope the opt-out to our capture window so ordinary touch is unchanged
    // when tablet mode is stopped (the window is then destroyed).
    const BOOL feedback = FALSE;
    for (const auto type : {FEEDBACK_TOUCH_CONTACTVISUALIZATION, FEEDBACK_TOUCH_TAP,
            FEEDBACK_TOUCH_DOUBLETAP, FEEDBACK_TOUCH_PRESSANDHOLD,
            FEEDBACK_TOUCH_RIGHTTAP}) {
        if (!SetWindowFeedbackSetting(source->window_, type, 0, sizeof(feedback), &feedback)) {
            *error = winError("SetWindowFeedbackSetting"); return {};
        }
    }
    if (!RegisterPointerInputTarget(source->window_, PT_TOUCH)) {
        *error = winError("RegisterPointerInputTarget"); return {};
    }
    // A null class cursor does not clear the input thread's existing cursor.
    // This invisible capture window must not supply a second cursor at the
    // original touch location. The destination app owns the injected mouse's
    // cursor; only clear the capture thread's cursor, not the system theme.
    SetCursor(nullptr);
    source->capturing_ = true;
    source->watchdog_.start(1000);
    return source;
}

PointerTouchSource::~PointerTouchSource()
{
    release();
    if (window_) {
        SetWindowLongPtrW(window_, GWLP_USERDATA, 0);
        DestroyWindow(window_);
    }
}

ITouchSource::Info PointerTouchSource::info() const
{
    return {device_.panel, QString::fromWCharArray(device_.native.productString), device_.path,
            {axes_[0].logicalMin, axes_[0].logicalMax, axes_[1].logicalMin, axes_[1].logicalMax}};
}

void PointerTouchSource::release()
{
    watchdog_.stop();
    const bool registered = capturing_;
    capturing_ = false;
    if (registered) UnregisterPointerInputTarget(window_, PT_TOUCH);
}

void PointerTouchSource::fail(const QString& reason)
{
    if (failed_) return;
    failed_ = true;
    release();
    // Never destroy this source inside its native window procedure. The context
    // also cancels this notification if shutdown has already deleted the source.
    QTimer::singleShot(0, this, [this, reason] { emit gone(reason); });
}

void PointerTouchSource::checkDevice()
{
    QString error;
    const auto devices = pointerDevices(&error);
    int count = 0;
    bool found = false;
    for (const auto& d : devices) {
        if (d.native.pointerDeviceType != POINTER_DEVICE_TYPE_TOUCH) continue;
        ++count;
        found |= d.native.device == device_.native.device && d.path == device_.path;
    }
    if (!error.isEmpty() || !found || count != 1)
        fail(QStringLiteral("Touchscreen configuration changed; rescanning"));
}

void PointerTouchSource::readFrame(UINT32 id)
{
    if (!capturing_) return;
    POINTER_INFO info{};
    if (!GetPointerInfo(id, &info)) { fail(winError("GetPointerInfo")); return; }
    if (info.pointerType != PT_TOUCH) return;
    if (info.sourceDevice != device_.native.device) { fail(QStringLiteral("Unexpected touchscreen; rescanning")); return; }
    if (haveFrame_ && lastFrame_ == info.frameId) return;
    UINT32 count = 0;
    if (!GetPointerFrameTouchInfo(id, &count, nullptr)) { fail(winError("GetPointerFrameTouchInfo")); return; }
    if (count == 0 || count > 256) { fail(QStringLiteral("Invalid touch frame size")); return; }
    std::vector<POINTER_TOUCH_INFO> pointers(count);
    if (!GetPointerFrameTouchInfo(id, &count, pointers.data())) { fail(winError("GetPointerFrameTouchInfo")); return; }
    pointers.resize(count);
    std::sort(pointers.begin(), pointers.end(), [](const auto& a, const auto& b) {
        return a.pointerInfo.pointerId < b.pointerInfo.pointerId;
    });
    std::vector<ContactSample> contacts;
    for (const auto& p : pointers) {
        const auto& pi = p.pointerInfo;
        if (pi.sourceDevice != device_.native.device) { fail(QStringLiteral("Mixed-device touch frame")); return; }
        LONG values[2]{};
        const bool touching = (pi.pointerFlags & POINTER_FLAG_INCONTACT) && !(pi.pointerFlags & POINTER_FLAG_CANCELED);
        if (touching && !GetRawPointerDeviceData(pi.pointerId, 1, 2, axes_.data(), values)) {
            fail(winError("GetRawPointerDeviceData")); return;
        }
        contacts.push_back({int(pi.pointerId), int(values[0]), int(values[1]), touching});
    }
    lastFrame_ = info.frameId;
    haveFrame_ = true;
    emit frame(decoder_.decode(contacts));
}

LRESULT CALLBACK PointerTouchSource::windowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    auto* self = reinterpret_cast<PointerTouchSource*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        self = static_cast<PointerTouchSource*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
    switch (msg) {
    case WM_SETCURSOR:
        SetCursor(nullptr);
        return TRUE;
    case WM_TOUCH:
        CloseTouchInputHandle(reinterpret_cast<HTOUCHINPUT>(lp));
        return 0;
    // Consume the entire pointer sequence. Passing ENTER to DefWindowProc while
    // consuming DOWN/UPDATE/UP mixes default processing with our own handling.
    case WM_POINTERENTER:
        SetCursor(nullptr);
        return 0;
    case WM_POINTERWHEEL: case WM_POINTERHWHEEL:
        return 0;
    case WM_POINTERACTIVATE:
        return PA_NOACTIVATE;
    case WM_TABLET_QUERYSYSTEMGESTURESTATUS:
        return TABLET_DISABLE_PRESSANDHOLD | TABLET_DISABLE_PENTAPFEEDBACK
            | TABLET_DISABLE_PENBARRELFEEDBACK | TABLET_DISABLE_FLICKS
            | TABLET_DISABLE_TOUCHUIFORCEOFF;
    case WM_POINTERDOWN: case WM_POINTERUPDATE: case WM_POINTERUP: case WM_POINTERLEAVE:
    case WM_NCPOINTERDOWN: case WM_NCPOINTERUPDATE: case WM_NCPOINTERUP:
        SetCursor(nullptr);
        self->readFrame(GET_POINTERID_WPARAM(wp)); return 0;
    case WM_POINTERCAPTURECHANGED:
        if (self->capturing_) self->fail(QStringLiteral("Touch capture lost; reconnecting"));
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
}
