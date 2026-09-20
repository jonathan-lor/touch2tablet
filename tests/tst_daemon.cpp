// Daemon orchestration with fake backends: hotplug scan, frame -> machine -> sink,
// detach/reattach, settings hot-swap re-creating the sink.
#include "Daemon.h"
#include "fakes.h"
#include "pen_signature.h"

#include <QJsonDocument>
#include <QSignalSpy>
#include <QtTest>

using namespace t2t;

class DaemonTest : public QObject {
    Q_OBJECT

    // The probe hands out this source when `present` is true.  Ownership goes to the daemon;
    // we keep a raw pointer to drive it.
    bool present_ = false;
    FakeSource* src_ = nullptr;
    std::vector<PenEvent> events_;

    std::unique_ptr<Daemon> make(const Settings& s = Settings::defaults())
    {
        events_.clear();
        FakeSink::events = &events_;
        FakeSink::opens = FakeSink::closes = 0;
        auto d = std::make_unique<Daemon>(
            s,
            [this]() -> std::unique_ptr<ITouchSource> {
                if (!present_) return nullptr;
                auto p = std::make_unique<FakeSource>();
                src_ = p.get();
                return p;
            },
            [] { return std::unique_ptr<IPenSink>(std::make_unique<FakeSink>()); });
        d->scanIntervalMs = 20;
        return d;
    }

private slots:
    void init() { present_ = false; src_ = nullptr; }

    void scansUntilDeviceAppears()
    {
        auto d = make();
        QSignalSpy dev(d.get(), &Daemon::deviceChanged);
        d->start();
        QTest::qWait(50);
        QVERIFY(!d->connected());
        present_ = true;
        QTRY_VERIFY_WITH_TIMEOUT(d->connected(), 500);
        QCOMPARE(dev.count(), 1);
        QCOMPARE(dev.last().at(0).toBool(), true);
        QCOMPARE(FakeSink::opens, 1);
        QCOMPARE(d->sinkPath(), QStringLiteral("/dev/fakepen"));
        QCOMPARE(d->sourceInfo()->path, QStringLiteral("/dev/fake"));
    }

    void tapDragLift()
    {
        present_ = true;
        auto d = make();
        d->start();
        QVERIFY(d->connected());
        QSignalSpy spy(d.get(), &Daemon::frameProcessed);
        src_->begin(0, 512, 300);
        QCOMPARE(signature(events_), QString("IMD"));
        QVERIFY(qAbs(events_.back().x - 1280) <= 2);
        QCOMPARE(spy.count(), 1);
        const auto st = spy.last().at(0).value<GestureMachine::State>();
        QCOMPARE(st.fingers, 1);
        QVERIFY(st.active);
        src_->move(0, 600, 300);
        QCOMPARE(signature(events_), QString("IMDM"));
        src_->end(0);
        QCOMPARE(signature(events_), QString("IMDMUO"));
    }

    void deviceVanishesMidTouchAndReturns()
    {
        present_ = true;
        auto d = make();
        d->start();
        QSignalSpy dev(d.get(), &Daemon::deviceChanged);
        src_->begin(0, 100, 100);
        QCOMPARE(signature(events_), QString("IMD"));
        src_->vanish();
        QVERIFY(!d->connected());
        QCOMPARE(signature(events_), QString("IMDUO"));   // released + left proximity before closing
        QCOMPARE(FakeSink::closes, 1);
        QCOMPARE(dev.last().at(0).toBool(), false);
        // still present -> re-attached by the scan timer
        QTRY_VERIFY_WITH_TIMEOUT(d->connected(), 500);
        QCOMPARE(FakeSink::opens, 2);
        events_.clear();
        src_->begin(0, 100, 100);
        src_->end(0);
        QCOMPARE(signature(events_), QString("IMDUO"));
    }

    void lostSyncReleasesAndRecovers()
    {
        present_ = true;
        auto d = make();
        d->start();
        src_->begin(0, 100, 100);
        src_->overrun();
        QCOMPARE(signature(events_), QString("IMDUO"));
        QVERIFY(d->connected());                 // the device stays attached
        events_.clear();
        src_->begin(0, 100, 100);
        QCOMPARE(signature(events_), QString("IMD"));
    }

    void applySettingsRemapsLive()
    {
        present_ = true;
        auto d = make();
        d->start();
        src_->begin(0, 512, 300);
        d->applySettings(Settings::applied(d->settings(), QJsonDocument::fromJson(
            R"({"display_area":{"width":1280,"height":720,"x":640,"y":360}})").object()));
        QCOMPARE(FakeSink::opens, 1);   // no reopen needed
        src_->move(0, 512, 300);
        QVERIFY(qAbs(events_.back().x - 640) <= 2);
    }

    void applySettingsReopensSinkOnDisplayChange()
    {
        present_ = true;
        auto d = make();
        d->start();
        QSignalSpy sc(d.get(), &Daemon::settingsChanged);
        src_->begin(0, 512, 300);
        d->applySettings(Settings::applied(d->settings(), QJsonDocument::fromJson(
            R"({"display":{"width":1920,"height":1080}})").object()));
        QCOMPARE(FakeSink::closes, 1);
        QCOMPARE(FakeSink::opens, 2);
        QCOMPARE(signature(events_), QString("IMDUO"));   // machine reset before the swap
        QCOMPARE(sc.count(), 1);
        events_.clear();
        src_->end(0);
        src_->begin(0, 1024, 600);
        src_->end(0);
        QCOMPARE(signature(events_), QString("IMDUO"));
        QCOMPARE(events_[2].x, 1919);
        QCOMPARE(events_[2].y, 1079);
    }

    void glassSizeFollowsThePanel()
    {
        const auto restore = qScopeGuard([usual = FakeSource::panel] { FakeSource::panel = usual; });
        FakeSource::panel.widthMm = 300.0;
        FakeSource::panel.heightMm = 200.0;
        present_ = true;
        auto d = make(Settings::applied(Settings::defaults(), QJsonDocument::fromJson(
            R"({"tablet_area":{"width":50,"height":30,"x":40,"y":40}})").object()));
        d->start();
        // An area laid out on another glass does not carry over: full glass of the new panel.
        QCOMPARE(d->settings().tablet, (Tablet{300.0, 200.0}));
        QCOMPARE(d->settings().tabletArea, (TabletArea{300.0, 200.0, 150.0, 100.0, 0.0}));
        d->applySettings(Settings::applied(d->settings(), QJsonDocument::fromJson(
            R"({"tablet_area":{"width":150,"height":100}})").object()));
        QCOMPARE(d->settings().tabletArea.width, 150.0);
        // The glass is not a preference: settings made for another one (say a preset) are refitted.
        d->applySettings(Settings::applied(d->settings(), QJsonDocument::fromJson(
            R"({"tablet":{"width":10,"height":10},"tablet_area":{"width":5,"height":5}})").object()));
        QCOMPARE(d->settings().tablet, (Tablet{300.0, 200.0}));
        QCOMPARE(d->settings().tabletArea.width, 300.0);
    }

    void stopReleasesEverything()
    {
        present_ = true;
        auto d = make();
        d->start();
        src_->begin(0, 100, 100);
        d->stop();
        QCOMPARE(signature(events_), QString("IMDUO"));
        QVERIFY(!d->connected());
        QCOMPARE(FakeSink::closes, 1);
        QTest::qWait(60);                        // device still present, but scanning has stopped
        QVERIFY(!d->connected());
        QCOMPARE(FakeSink::opens, 1);
    }
};

QTEST_GUILESS_MAIN(DaemonTest)
#include "tst_daemon.moc"
