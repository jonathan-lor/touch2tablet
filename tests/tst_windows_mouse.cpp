#include "backend/windows/AbsoluteMouseSink.h"
#include <QtTest>
using namespace t2t;
using namespace t2t::windows;

class WindowsMouseTest : public QObject {
    Q_OBJECT
private slots:
    void mappingButtonsAndResize()
    {
        std::vector<INPUT> events;
        AbsoluteMouseSink::Bounds bounds{{-1920, 0, 0, 1080}, {-1920, 0, 1920, 1080}};
        AbsoluteMouseSink sink({[&](const INPUT& i) { events.push_back(i); return true; },
            [&](const QString&) { return std::optional(bounds); }});
        Settings s;
        s.display.width = 1920; s.display.height = 1080;
        QString error;
        QVERIFY(sink.open(s, &error));
        sink.handle({PenEvent::Type::ProximityIn});
        QVERIFY(events.empty());
        sink.handle({PenEvent::Type::Move, 0, 0});
        auto pixel = [](LONG v, LONG origin, LONG size) { return origin + qint64(v) * size / 65536; };
        QCOMPARE(pixel(events.back().mi.dx, -1920, 3840), -1920);
        sink.handle({PenEvent::Type::Down, 1919, 1079});
        QVERIFY(events.back().mi.dwFlags & MOUSEEVENTF_LEFTDOWN);
        QCOMPARE(pixel(events.back().mi.dx, -1920, 3840), -1);
        QCOMPARE(pixel(events.back().mi.dy, 0, 1080), 1079);
        bounds = {{0, 0, 1280, 720}, {0, 0, 1280, 720}};
        sink.handle({PenEvent::Type::Move, 1919, 1079});
        QCOMPARE(pixel(events.back().mi.dx, 0, 1280), 1279);
        QCOMPARE(pixel(events.back().mi.dy, 0, 720), 719);
        QVERIFY(!(events.back().mi.dwFlags & MOUSEEVENTF_LEFTDOWN));
        sink.close();
        QCOMPARE(events.back().mi.dwFlags, DWORD(MOUSEEVENTF_LEFTUP));
        const auto count = events.size();
        sink.close();
        QCOMPARE(events.size(), count);
    }

    void failureDuringDragReleasesAndCanReopen()
    {
        bool fail = false;
        std::vector<DWORD> flags;
        AbsoluteMouseSink sink({[&](const INPUT& i) {
            flags.push_back(i.mi.dwFlags);
            if (fail) { fail = false; return false; }
            return true;
        }, [](const QString&) { return std::optional(AbsoluteMouseSink::Bounds{{0,0,1920,1080},{0,0,1920,1080}}); }});
        QString error;
        QVERIFY(sink.open(Settings{}, &error));
        sink.handle({PenEvent::Type::ProximityIn});
        sink.handle({PenEvent::Type::Down, 100, 100});
        fail = true;
        sink.handle({PenEvent::Type::Move, 200, 200});
        QVERIFY(!sink.isOpen());
        QCOMPARE(flags.back(), DWORD(MOUSEEVENTF_LEFTUP));
        QVERIFY(!sink.lastError().isEmpty());
        QVERIFY(sink.open(Settings{}, &error));
    }

    void monitorLossStillReleasesButton()
    {
        bool present = true;
        std::vector<DWORD> flags;
        AbsoluteMouseSink sink({[&](const INPUT& i) { flags.push_back(i.mi.dwFlags); return true; },
            [&](const QString&) -> std::optional<AbsoluteMouseSink::Bounds> {
                if (!present) return {};
                return AbsoluteMouseSink::Bounds{{0,0,1920,1080},{0,0,1920,1080}};
            }});
        QString error;
        QVERIFY(sink.open(Settings{}, &error));
        sink.handle({PenEvent::Type::ProximityIn});
        sink.handle({PenEvent::Type::Down, 100, 100});
        present = false;
        sink.handle({PenEvent::Type::Move, 200, 200});
        QVERIFY(!sink.isOpen());
        QCOMPARE(flags.back(), DWORD(MOUSEEVENTF_LEFTUP));
        QVERIFY(!sink.open(Settings{}, &error));
    }
};
QTEST_GUILESS_MAIN(WindowsMouseTest)
#include "tst_windows_mouse.moc"
