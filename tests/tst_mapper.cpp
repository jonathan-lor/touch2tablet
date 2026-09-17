#include "t2t/Mapper.h"

#include <QJsonDocument>
#include <QtTest>

using namespace t2t;

namespace {
const RawRange kRaw{0, 1919, 0, 1079};   // an arbitrary raw range, unlike the display's

Settings withPatch(const char* json, const Settings& base = Settings::defaults())
{
    return Settings::applied(base, QJsonDocument::fromJson(json).object());
}
}  // namespace

class MapperTest : public QObject {
    Q_OBJECT
private slots:
    void fullAreaIsIdentityScaled()
    {
        const Mapper m(Settings::defaults(), kRaw);
        MapResult r = m.map(0, 0);
        QCOMPARE(r.x, 0); QCOMPARE(r.y, 0); QVERIFY(r.inside);
        r = m.map(1919, 1079);
        QCOMPARE(r.x, 2559); QCOMPARE(r.y, 1439); QVERIFY(r.inside);
        r = m.map(960, 540);
        QVERIFY(qAbs(r.x - 1280) <= 1);
        QVERIFY(qAbs(r.y - 720) <= 1);
    }

    void millimeters()
    {
        const Mapper m(Settings::defaults(), kRaw);
        double mx, my;
        m.toMm(1919, 1079, mx, my);
        QCOMPARE(mx, 165.0); QCOMPARE(my, 100.0);
        m.toMm(0, 0, mx, my);
        QCOMPARE(mx, 0.0); QCOMPARE(my, 0.0);
    }

    void quadrantAreaFillsDisplay()
    {
        // top-left quarter of the glass -> whole display
        const Settings s = withPatch(R"({"tablet_area":{"width":82.5,"height":50,"x":41.25,"y":25}})");
        const Mapper m(s, kRaw);
        const MapResult r = m.map(955, 535);   // 82.1 mm, 49.5 mm: just inside
        QVERIFY(r.inside);
        QVERIFY(r.x >= 2540); QVERIFY(r.y >= 1425);
        const MapResult far = m.map(1919, 1079);   // outside, clipped to the corner
        QVERIFY(!far.inside);
        QCOMPARE(far.x, 2559); QCOMPARE(far.y, 1439);
        const MapResult origin = m.map(0, 0);
        QCOMPARE(origin.x, 0); QCOMPARE(origin.y, 0); QVERIFY(origin.inside);
    }

    void clipOffStillClampsToDisplay()
    {
        const Settings s = withPatch(R"({"tablet_area":{"width":82.5,"height":50,"x":41.25,"y":25},"clip":false})");
        const MapResult r = Mapper(s, kRaw).map(1919, 1079);
        QVERIFY(!r.inside);
        QCOMPARE(r.x, 2559); QCOMPARE(r.y, 1439);
    }

    void clipOffAllowsOvershootInsideDisplay()
    {
        // small display area in the middle, clip off: touches past the area edge go beyond it
        const Settings s = withPatch(R"({"tablet_area":{"width":82.5,"height":50,"x":82.5,"y":50},
                                         "display_area":{"width":640,"height":360,"x":1280,"y":720},"clip":false})");
        const Mapper m(s, kRaw);
        const MapResult edge = m.map(1919, 540);    // right edge of glass: u = 1.5
        QVERIFY(!edge.inside);
        QCOMPARE(edge.x, 1280 + 320 + 320);          // 640 * 1.5 - 320 past center
        const Settings sc = withPatch(R"({"clip":true})", s);
        QCOMPARE(Mapper(sc, kRaw).map(1919, 540).x, 1280 + 320);
    }

    void displaySubArea()
    {
        const Settings s = withPatch(R"({"display_area":{"width":1280,"height":720,"x":640,"y":360}})");
        const Mapper m(s, kRaw);
        const MapResult r = m.map(1919, 1079);
        QCOMPARE(r.x, 1280); QCOMPARE(r.y, 720);
        QCOMPARE(m.map(0, 0).x, 0);
    }

    void rotation90()
    {
        // 100x100 area centered on the glass, rotated 90 deg clockwise.
        const Settings s = withPatch(R"({"tablet_area":{"width":100,"height":100,"rotation":90}})");
        const Mapper m(s, kRaw);
        // Glass right-edge, mid height (165, 50) mm: in the rotated frame that is "up" -> top of display.
        const MapResult right = m.map(1919, 540);
        QCOMPARE(right.y, 0);
        QVERIFY(qAbs(right.x - 1280) <= 2);
        QVERIFY(!right.inside);   // 82.5 mm from center > 50
        // Glass top-middle (82.5, 0) mm -> left edge of display, vertically centered.
        const MapResult top = m.map(960, 0);
        QCOMPARE(top.x, 0);
        QVERIFY(qAbs(top.y - 720) <= 2);
        QVERIFY(top.inside);
        // Center stays center.
        const MapResult c = m.map(960, 540);
        QVERIFY(qAbs(c.x - 1280) <= 2); QVERIFY(qAbs(c.y - 720) <= 2);
    }

    void rotation180MirrorsBoth()
    {
        const Settings s = withPatch(R"({"tablet_area":{"rotation":180}})");
        const Mapper m(s, kRaw);
        const MapResult r = m.map(0, 0);
        QCOMPARE(r.x, 2559); QCOMPARE(r.y, 1439);
        const MapResult r2 = m.map(1919, 1079);
        QCOMPARE(r2.x, 0); QCOMPARE(r2.y, 0);
    }

    void differentRawRange()
    {
        const Mapper m(Settings::defaults(), RawRange{100, 4195, 50, 2097});
        QCOMPARE(m.map(100, 50).x, 0);
        QCOMPARE(m.map(4195, 2097).x, 2559);
        QCOMPARE(m.map(4195, 2097).y, 1439);
    }
};

QTEST_APPLESS_MAIN(MapperTest)
#include "tst_mapper.moc"
