#include "backend/windows/TouchFrameDecoder.h"
#include "t2t/GestureMachine.h"
#include "pen_signature.h"
#include <QtTest>

using namespace t2t;
using namespace t2t::windows;

class WindowsTouchTest : public QObject {
    Q_OBJECT
private slots:
    void contactSnapshotsPreserveFirstFingerBehavior()
    {
        TouchFrameDecoder decoder;
        std::vector<PenEvent> events;
        GestureMachine machine(Settings{}, RawRange{}, [&](const PenEvent& e) { events.push_back(e); });
        auto feed = [&](std::vector<ContactSample> contacts) {
            for (const auto& e : decoder.decode(contacts).events) {
                switch (e.type) {
                case TouchEvent::Type::Begin: machine.trackingBegin(e.slot); break;
                case TouchEvent::Type::Position: machine.position(e.slot, e.x, e.y); break;
                case TouchEvent::Type::End: machine.trackingEnd(e.slot); break;
                }
            }
            machine.frame();
        };
        feed({{10, 100, 100, true}});
        QCOMPARE(signature(events), QStringLiteral("IMD"));
        feed({{10, 100, 100, true}, {11, 900, 500, true}});
        QCOMPARE(machine.state().rawX, 100);
        // A missing contact is released even if Windows omits its explicit UP.
        feed({{11, 900, 500, true}});
        QVERIFY(machine.state().ignored);
        QCOMPARE(signature(events), QStringLiteral("IMDMUO"));
        const size_t releasedSize = events.size();
        feed({{11, 800, 400, true}});
        QCOMPARE(events.size(), releasedSize);
        feed({{11, 800, 400, false}});
        feed({{12, 300, 300, true}});
        QVERIFY(machine.state().active);
        QCOMPARE(machine.state().rawX, 300);
        // A replacement arriving in the same snapshot also cannot inherit the stroke.
        feed({{13, 400, 400, true}});
        QVERIFY(machine.state().ignored);
        QVERIFY(!machine.state().active);
    }

};

QTEST_GUILESS_MAIN(WindowsTouchTest)
#include "tst_windows_touch.moc"
