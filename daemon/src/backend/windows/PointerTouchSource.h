#pragma once

#include "ITouchSource.h"
#include "PointerDevices.h"
#include "TouchFrameDecoder.h"
#include <QTimer>
#include <memory>

namespace t2t::windows {

// UIAccess capture belongs to the interactive user session, never a Windows service.
class PointerTouchSource final : public ITouchSource {
public:
    static std::unique_ptr<PointerTouchSource> probe(QString* error);
    ~PointerTouchSource() override;
    Info info() const override;

private:
    explicit PointerTouchSource(PointerDevice device);
    static LRESULT CALLBACK windowProc(HWND, UINT, WPARAM, LPARAM);
    void readFrame(UINT32 id);
    void checkDevice();
    void fail(const QString& reason);
    void release();

    PointerDevice device_;
    std::vector<POINTER_DEVICE_PROPERTY> axes_;
    TouchFrameDecoder decoder_;
    QTimer watchdog_;
    HWND window_ = nullptr;
    bool capturing_ = false, failed_ = false, haveFrame_ = false;
    UINT32 lastFrame_ = 0;
};
}
