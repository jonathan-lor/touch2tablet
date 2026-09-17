#include "pen_signature.h"
#include "t2t/GestureMachine.h"

#include <QJsonDocument>
#include <QtTest>

using namespace t2t;
using T = PenEvent::Type;

namespace {

struct Recorder {
    std::vector<PenEvent> events;
    GestureMachine::Emit sink()
    {
        return [this](const PenEvent& e) { events.push_back(e); };
    }
    QString signature() const { return ::signature(events); }
    QString signatureNoMoves() const { return signature().remove('M'); }
    int count(T t) const
    {
        return int(std::count_if(events.begin(), events.end(), [&](const PenEvent& e) { return e.type == t; }));
    }
    std::optional<PenEvent> first(T t) const
    {
        for (const auto& e : events) if (e.type == t) return e;
        return std::nullopt;
    }
};

Settings withPatch(const char* json)
{
    return Settings::applied(Settings::defaults(), QJsonDocument::fromJson(json).object());
}

}  // namespace

class GestureTest : public QObject {
    Q_OBJECT
private slots:
    void trackingIdBeforeCoordinates()
    {
        // GT911 quirk: tracking id in one frame, coordinates only in a later one.
        Recorder r;
        GestureMachine gm(Settings::defaults(), RawRange{}, r.sink());
        gm.trackingBegin(0); gm.frame();
        QVERIFY(r.events.empty());
        gm.position(0, 512, 300); gm.frame();
        QCOMPARE(r.signature(), QString("IMD"));
        QVERIFY(qAbs(r.first(T::Down)->x - 1280) <= 2);
        gm.trackingEnd(0); gm.frame();
        QCOMPARE(r.signature(), QString("IMDUO"));
    }

    void trackingIdOnlyThenLiftProducesNothing()
    {
        Recorder r;
        GestureMachine gm(Settings::defaults(), RawRange{}, r.sink());
        gm.trackingBegin(0); gm.frame();
        gm.trackingEnd(0); gm.frame();
        QVERIFY(r.events.empty());
        QCOMPARE(gm.state().fingers, 0);
    }

    void extraFingerDoesNotMoveThePen()
    {
        Recorder r;
        GestureMachine gm(Settings::defaults(), RawRange{}, r.sink());
        gm.trackingBegin(0); gm.position(0, 100, 100); gm.frame();
        const int x0 = r.events.back().x;
        gm.trackingBegin(1); gm.position(1, 900, 500); gm.frame();
        gm.position(1, 950, 520); gm.frame();
        for (const auto& e : r.events)
            if (e.type == T::Move || e.type == T::Down) QCOMPARE(e.x, x0);
        QCOMPARE(gm.state().fingers, 2);
        gm.trackingEnd(1); gm.frame();
        gm.trackingEnd(0); gm.frame();
        QCOMPARE(r.signatureNoMoves(), QString("IDUO"));
    }

    void primaryLiftWithOthersEndsStroke()
    {
        Recorder r;
        GestureMachine gm(Settings::defaults(), RawRange{}, r.sink());
        gm.trackingBegin(0); gm.position(0, 100, 100); gm.frame();
        gm.trackingBegin(1); gm.position(1, 900, 500); gm.frame();
        gm.trackingEnd(0); gm.frame();                   // the stroke's finger lifts
        QCOMPARE(r.signatureNoMoves(), QString("IDUO"));
        const int n = int(r.events.size());
        gm.position(1, 950, 520); gm.frame();            // leftover is ignored: nothing emitted
        QCOMPARE(int(r.events.size()), n);
        QVERIFY(gm.state().ignored);
        gm.trackingEnd(1); gm.frame();
        gm.trackingBegin(0); gm.position(0, 100, 100); gm.frame();
        QCOMPARE(r.signatureNoMoves(), QString("IDUOID"));   // next touch works normally
    }

    void interleavedFingersLeaveNothingStuck()
    {
        // Fingers landing and lifting in every order, several in one frame: each tip press is
        // released and the machine ends idle.
        Recorder r;
        GestureMachine gm(Settings::defaults(), RawRange{}, r.sink());
        gm.trackingBegin(0); gm.position(0, 100, 100);
        gm.trackingBegin(1); gm.position(1, 500, 300); gm.frame();     // two land together
        gm.trackingBegin(2); gm.position(2, 900, 500); gm.frame();
        gm.trackingEnd(1); gm.frame();
        gm.trackingEnd(0); gm.trackingEnd(2); gm.frame();              // two lift together
        gm.trackingBegin(0); gm.position(0, 200, 200); gm.frame();     // slot reused
        gm.trackingBegin(1); gm.position(1, 300, 300); gm.frame();
        gm.trackingEnd(0); gm.frame();                                 // primary first this time
        gm.trackingEnd(1); gm.frame();
        QCOMPARE(r.signatureNoMoves(), QString("IDUOIDUO"));
        QCOMPARE(r.count(T::Down), r.count(T::Up));
        QCOMPARE(gm.state().fingers, 0);
        QVERIFY(!gm.state().active);
        QVERIFY(!gm.state().ignored);
    }

    void limitIgnoresTouchOutsideArea()
    {
        // left half of the glass is the area; a touch on the right half must produce nothing
        const Settings s = withPatch(R"({"limit":true,"tablet_area":{"width":82.5,"height":100,"x":41.25,"y":50}})");
        Recorder r;
        GestureMachine gm(s, RawRange{}, r.sink());
        gm.trackingBegin(0); gm.position(0, 900, 300); gm.frame();
        gm.position(0, 100, 310); gm.frame();            // even after moving inside
        gm.trackingEnd(0); gm.frame();
        QVERIFY(r.events.empty());
        gm.trackingBegin(0); gm.position(0, 100, 300); gm.frame();
        QCOMPARE(r.signature(), QString("IMD"));
    }

    void resetReleasesAndLeavesProximity()
    {
        Recorder r;
        GestureMachine gm(Settings::defaults(), RawRange{}, r.sink());
        gm.trackingBegin(0); gm.position(0, 100, 100); gm.frame();
        gm.reset();
        QCOMPARE(r.signature(), QString("IMDUO"));
        QCOMPARE(gm.state().fingers, 0);
        QVERIFY(!gm.state().active);
    }
};

QTEST_APPLESS_MAIN(GestureTest)
#include "tst_gesture.moc"
