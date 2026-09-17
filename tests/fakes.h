// Fake backends shared by the daemon and control-socket tests.
#pragma once

#include "IPenSink.h"
#include "ITouchSource.h"

#include <vector>

class FakeSource : public t2t::ITouchSource {
    Q_OBJECT
public:
    static inline t2t::Panel panel{0x03, 0xfa4e, 0x0001, QStringLiteral("fake"), QStringLiteral("Fake panel"), 165.0, 100.0, {}};
    Info info() const override { return Info{&panel, QStringLiteral("fake"), QStringLiteral("/dev/fake"), t2t::RawRange{}}; }
    void begin(int slot, int x, int y)
    {
        using E = t2t::TouchEvent;
        emit frame(t2t::TouchFrame{{E{E::Type::Begin, slot, {}, {}}, E{E::Type::Position, slot, x, y}}});
    }
    void move(int slot, int x, int y) { emit frame(t2t::TouchFrame{{t2t::TouchEvent{t2t::TouchEvent::Type::Position, slot, x, y}}}); }
    void end(int slot) { emit frame(t2t::TouchFrame{{t2t::TouchEvent{t2t::TouchEvent::Type::End, slot, {}, {}}}}); }
    void overrun() { emit dropped(); }
    void vanish() { emit gone(QStringLiteral("unplugged")); }
};

struct FakeSink : t2t::IPenSink {
    static inline std::vector<t2t::PenEvent>* events = nullptr;   // optional shared recorder
    static inline int opens = 0, closes = 0;
    bool open_ = false;
    t2t::Settings opened;
    bool open(const t2t::Settings& s, QString*) override { open_ = true; opened = s; ++opens; return true; }
    void close() override { if (open_) ++closes; open_ = false; }
    bool isOpen() const override { return open_; }
    bool needsReopen(const t2t::Settings& s) const override { return !open_ || s.display != opened.display; }
    void handle(const t2t::PenEvent& e) override { if (events) events->push_back(e); }
    QString path() const override { return QStringLiteral("/dev/fakepen"); }
};
