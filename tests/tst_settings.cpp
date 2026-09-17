#include "t2t/Settings.h"

#include <QJsonDocument>
#include <QtTest>

using namespace t2t;

class SettingsTest : public QObject {
    Q_OBJECT
private slots:
    void jsonRoundTrip()
    {
        Settings s = Settings::defaults();
        s.tabletArea = {80.0, 45.0, 60.0, 40.0, 15.0};
        s.displayArea = {1280.0, 720.0, 640.0, 360.0};
        s.limit = true;
        s.lockAspect = true;
        const QJsonObject j = s.toJson();
        QCOMPARE(j.value("version").toInt(), 1);
        QCOMPARE(j.value("tablet_area").toObject().value("rotation").toDouble(), 15.0);
        QCOMPARE(Settings::fromJson(j), s);
    }

    void partialPatchKeepsRest()
    {
        Settings base = Settings::defaults();
        base.tabletArea.width = 120.0;
        const auto patch = QJsonDocument::fromJson(R"({"tablet_area":{"height":30},"clip":false})").object();
        const Settings s = Settings::applied(base, patch);
        QCOMPARE(s.tabletArea.width, 120.0);   // untouched
        QCOMPARE(s.tabletArea.height, 30.0);
        QVERIFY(!s.clip);
        QVERIFY(!s.limit);
    }

    void garbageIsRejected()
    {
        const auto patch = QJsonDocument::fromJson(
            R"({"display":{"width":"x","height":1e12},"tablet_area":{"rotation":370,"width":-5},"clip":"yes"})").object();
        const Settings s = Settings::fromJson(patch);
        QCOMPARE(s.display.width, 2560);          // wrong type -> kept
        QCOMPARE(s.display.height, 32768);        // clamped
        QVERIFY(qAbs(s.tabletArea.rotation - 10.0) < 1e-9);
        QCOMPARE(s.tabletArea.width, 0.5);        // clamped to minimum
        QVERIFY(s.clip);                          // string is not a bool
        const auto neg = QJsonDocument::fromJson(R"({"tablet_area":{"rotation":-90}})").object();
        QCOMPARE(Settings::fromJson(neg).tabletArea.rotation, 270.0);
    }
};

QTEST_APPLESS_MAIN(SettingsTest)
#include "tst_settings.moc"
