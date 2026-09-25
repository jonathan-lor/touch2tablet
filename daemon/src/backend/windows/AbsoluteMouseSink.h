#pragma once
#include "IPenSink.h"
#include <Windows.h>
#include <functional>
#include <optional>

namespace t2t::windows {
class AbsoluteMouseSink final : public IPenSink {
public:
    struct Bounds { RECT monitor, desktop; };
    struct Api {
        std::function<bool(const INPUT&)> inject;
        std::function<std::optional<Bounds>(const QString&)> bounds;
    };
    AbsoluteMouseSink();
    explicit AbsoluteMouseSink(Api api);
    ~AbsoluteMouseSink() override { close(); }
    bool open(const Settings&, QString*) override;
    void close() override;
    bool isOpen() const override { return open_; }
    bool needsReopen(const Settings&) const override;
    void handle(const PenEvent&) override;
    QString path() const override { return open_ ? QStringLiteral("windows:absolute-mouse") : QString(); }
    QString lastError() const override { return error_; }
private:
    bool move(int x, int y, DWORD button = 0);
    bool release();
    bool send(const INPUT&);
    Api api_;
    Display display_;
    bool open_ = false, down_ = false, wanted_ = false;
    QString error_;
};
}
