// ControlServer over a real QLocalSocket, against a Daemon with fake backends: every op of the
// protocol, subscription events (pos/log/settings/device), persistence via save, bad input.
#include "ControlServer.h"
#include "Daemon.h"
#include "fakes.h"
#include "t2t/SettingsStore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QtTest>

using namespace t2t;

namespace {

/// Blocking line client for tests.
struct Client {
    QLocalSocket sock;
    QByteArray buf;
    bool connect(const QString& name)
    {
        sock.connectToServer(name);
        if (!sock.waitForConnected(1000)) return false;
        QTest::qWait(1);                     // let the server accept
        return true;
    }
    void sendLine(const QByteArray& line) { sock.write(line + '\n'); sock.flush(); }
    void send(const QJsonObject& o) { sendLine(QJsonDocument(o).toJson(QJsonDocument::Compact)); }
    std::optional<QJsonObject> readLine(int timeoutMs = 1000)
    {
        QElapsedTimer t; t.start();
        for (;;) {
            const int nl = buf.indexOf('\n');
            if (nl >= 0) {
                const QByteArray line = buf.left(nl);
                buf.remove(0, nl + 1);
                return QJsonDocument::fromJson(line).object();
            }
            if (t.elapsed() > timeoutMs) return std::nullopt;
            QTest::qWait(1);                 // server lives on this thread: let its event loop run
            buf += sock.readAll();
        }
    }
    QJsonObject rpc(const QJsonObject& o) { send(o); auto r = readLine(); return r ? *r : QJsonObject{{"_timeout", true}}; }
    /// Read events until one with ev == name shows up (others are returned in `skipped`).
    std::optional<QJsonObject> waitEvent(const QString& name, int timeoutMs = 1500)
    {
        QElapsedTimer t; t.start();
        while (t.elapsed() < timeoutMs) {
            auto m = readLine(std::max(1, int(timeoutMs - t.elapsed())));
            if (!m) return std::nullopt;
            if (m->value("ev").toString() == name) return m;
        }
        return std::nullopt;
    }
};

}  // namespace

class ControlTest : public QObject {
    Q_OBJECT
    QTemporaryDir tmp_;
    QString sockName_, settingsPath_;
    bool present_ = true;
    FakeSource* src_ = nullptr;
    std::unique_ptr<Daemon> daemon_;
    std::unique_ptr<ControlServer> server_;

    void startServer(const Settings& s = Settings::defaults())
    {
        daemon_ = std::make_unique<Daemon>(
            s,
            [this]() -> std::unique_ptr<ITouchSource> {
                if (!present_) return nullptr;
                auto p = std::make_unique<FakeSource>(); src_ = p.get(); return p;
            },
            [] { return std::unique_ptr<IPenSink>(std::make_unique<FakeSink>()); });
        daemon_->scanIntervalMs = 20;
        server_ = std::make_unique<ControlServer>(*daemon_, ControlServer::Config{sockName_, settingsPath_});
        QString err;
        QVERIFY2(server_->listen(&err), qPrintable(err));
        daemon_->start();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(tmp_.isValid());
        // Qt uses a Unix socket path or a Windows named pipe with this name.
        sockName_ = tmp_.filePath(QStringLiteral("ctl.sock"));
        settingsPath_ = tmp_.filePath(QStringLiteral("cfg/settings.json"));
    }
    void init() { present_ = true; src_ = nullptr; QFile::remove(settingsPath_); }
    void cleanup() { server_.reset(); daemon_.reset(); }

    void getReturnsSettingsAndInfo()
    {
        startServer();
        Client c; QVERIFY(c.connect(sockName_));
        const QJsonObject r = c.rpc({{"op", "get"}, {"id", 7}});
        QVERIFY(r.value("ok").toBool());
        QCOMPARE(r.value("id").toInt(), 7);
        QCOMPARE(r.value("settings").toObject().value("display").toObject().value("width").toInt(), 2560);
        const QJsonObject info = r.value("info").toObject();
        QVERIFY(info.value("connected").toBool());
        QCOMPARE(info.value("touch_path").toString(), QStringLiteral("/dev/fake"));
        QCOMPARE(info.value("tablet_path").toString(), QStringLiteral("/dev/fakepen"));
        QCOMPARE(info.value("raw").toObject().value("xmax").toInt(), 1024);
        QCOMPARE(info.value("panel").toObject().value("width_mm").toDouble(), 165.0);
        QCOMPARE(info.value("settings_path").toString(), settingsPath_);
        QCOMPARE(info.value("proto").toInt(), 1);
    }

    void pauseResumeAndShutdown()
    {
        startServer();
        Client c; QVERIFY(c.connect(sockName_));
        QVERIFY(c.rpc({{"op", "stop"}}).value("ok").toBool());
        QVERIFY(!daemon_->running());
        QVERIFY(!daemon_->connected());
        QTest::qWait(60);
        QVERIFY(!daemon_->connected());
        const auto started = c.rpc({{"op", "start"}});
        QVERIFY(started.value("info").toObject().value("running").toBool());
        QVERIFY(daemon_->connected());
        QSignalSpy shutdown(server_.get(), &ControlServer::shutdownRequested);
        QVERIFY(c.rpc({{"op", "shutdown"}}).value("ok").toBool());
        QVERIFY(!daemon_->connected());
        QTRY_COMPARE(shutdown.count(), 1);
    }

    void applyMergesAndIsLive()
    {
        startServer();
        Client c; QVERIFY(c.connect(sockName_));
        const QJsonObject r = c.rpc({{"op", "apply"}, {"settings", QJsonObject{
            {"tablet_area", QJsonObject{{"width", 80}}}, {"clip", false}}}});
        QVERIFY(r.value("ok").toBool());
        const QJsonObject s = r.value("settings").toObject();
        QCOMPARE(s.value("tablet_area").toObject().value("width").toDouble(), 80.0);
        QCOMPARE(s.value("tablet_area").toObject().value("height").toDouble(), 100.0);   // merged, not replaced
        QVERIFY(!s.value("clip").toBool());
        QCOMPARE(daemon_->settings().tabletArea.width, 80.0);   // live in the daemon
        QVERIFY(!QFile::exists(settingsPath_));                  // not persisted
    }

    void saveWritesFileThatReloads()
    {
        startServer();
        Client c; QVERIFY(c.connect(sockName_));
        const QJsonObject r = c.rpc({{"op", "save"}, {"settings", QJsonObject{{"limit", true}, {"tablet_area", QJsonObject{{"rotation", 90}}}}}});
        QVERIFY2(r.value("ok").toBool(), qPrintable(r.value("error").toString()));
        QVERIFY(QFile::exists(settingsPath_));
        QString err;
        const auto json = SettingsStore::read(settingsPath_, &err);
        QVERIFY2(json.has_value(), qPrintable(err));
        const Settings back = Settings::fromJson(*json);
        QVERIFY(back.limit);
        QCOMPARE(back.tabletArea.rotation, 90.0);
        QCOMPARE(back, daemon_->settings());
        const QJsonObject log = c.rpc({{"op", "log"}});
        bool saw = false;
        for (const auto& l : log.value("lines").toArray()) saw = saw || l.toString().contains("settings saved");
        QVERIFY(saw);
    }

    void saveFailureIsReported()
    {
        // A regular file cannot contain a child, on either Unix or Windows.
        QFile blocker(tmp_.filePath(QStringLiteral("not-a-directory")));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        settingsPath_ = blocker.fileName() + QStringLiteral("/settings.json");
        startServer();
        Client c; QVERIFY(c.connect(sockName_));
        const QJsonObject r = c.rpc({{"op", "save"}, {"settings", QJsonObject{}}});
        settingsPath_ = tmp_.filePath(QStringLiteral("cfg/settings.json"));
        QVERIFY(!r.value("ok").toBool());
        QVERIFY(r.value("error").toString().startsWith("save failed"));
        QVERIFY(r.contains("settings"));   // still tells the client what is live
    }

    void resetRestoresDefaults()
    {
        startServer(Settings::applied(Settings::defaults(), QJsonDocument::fromJson(R"({"tablet_area":{"width":50}})").object()));
        Client c; QVERIFY(c.connect(sockName_));
        const QJsonObject r = c.rpc({{"op", "reset"}});
        QVERIFY(r.value("ok").toBool());
        QCOMPARE(r.value("settings").toObject().value("tablet_area").toObject().value("width").toDouble(), 165.0);
        QCOMPARE(daemon_->settings(), Settings::defaults());
    }

    void badInputIsTolerated()
    {
        startServer();
        Client c; QVERIFY(c.connect(sockName_));
        c.sendLine("not json");
        auto r = c.readLine(); QVERIFY(r); QVERIFY(!r->value("ok").toBool()); QCOMPARE(r->value("error").toString(), QStringLiteral("bad json"));
        r = c.rpc({{"op", "bogus"}, {"id", "x"}});
        QVERIFY(!r->value("ok").toBool()); QCOMPARE(r->value("id").toString(), QStringLiteral("x"));
        c.sendLine("[1,2,3]");
        r = c.readLine(); QVERIFY(r); QVERIFY(!r->value("ok").toBool());
        // two requests in one write
        c.sendLine("{\"op\":\"get\",\"id\":1}\n{\"op\":\"get\",\"id\":2}");
        r = c.readLine(); QCOMPARE(r->value("id").toInt(), 1);
        r = c.readLine(); QCOMPARE(r->value("id").toInt(), 2);
        QCOMPARE(server_->clientCount(), 1);
    }

    void subscribeStreamsPosLogSettingsDevice()
    {
        startServer();
        Client sub; QVERIFY(sub.connect(sockName_));
        QJsonObject hello = sub.rpc({{"op", "subscribe"}});
        QVERIFY(hello.value("ok").toBool());
        QCOMPARE(hello.value("proto").toInt(), 1);
        QVERIFY(hello.contains("settings") && hello.contains("info") && hello.contains("lines"));

        // pos: touch -> event with all fields; release -> fingers 0
        src_->begin(0, 512, 300);
        auto pos = sub.waitEvent("pos");
        QVERIFY(pos);
        QCOMPARE(pos->value("fingers").toInt(), 1);
        QCOMPARE(pos->value("raw").toArray().at(0).toInt(), 512);
        QVERIFY(qAbs(pos->value("mm").toArray().at(0).toDouble() - 82.5) < 0.1);
        QVERIFY(qAbs(pos->value("out").toArray().at(0).toInt() - 1280) <= 2);
        QVERIFY(pos->value("inside").toBool());
        QVERIFY(!pos->value("ignored").toBool());
        src_->end(0);
        pos = sub.waitEvent("pos");
        QVERIFY(pos);
        QCOMPARE(pos->value("fingers").toInt(), 0);

        // settings change from another client is broadcast
        Client other; QVERIFY(other.connect(sockName_));
        QVERIFY(other.rpc({{"op", "apply"}, {"settings", QJsonObject{{"limit", true}}}}).value("ok").toBool());
        auto logEv = sub.waitEvent("log");            // daemon logs "settings applied ..." first
        QVERIFY(logEv);
        QVERIFY(logEv->value("line").toString().contains("settings applied"));
        auto ev = sub.waitEvent("settings");          // then the settings broadcast
        QVERIFY(ev);
        QVERIFY(ev->value("settings").toObject().value("limit").toBool());
        QVERIFY(ev->contains("info"));

        // device disconnect / reconnect
        src_->vanish();
        auto dev = sub.waitEvent("device");
        QVERIFY(dev);
        QVERIFY(!dev->value("info").toObject().value("connected").toBool());
        dev = sub.waitEvent("device");
        QVERIFY(dev);
        QVERIFY(dev->value("info").toObject().value("connected").toBool());

        // non-subscribed client gets no events
        other.send({{"op", "get"}});
        auto r = other.readLine(); QVERIFY(r); QVERIFY(r->value("ok").toBool());
        QVERIFY(!other.readLine(100));
    }

    void staleSocketFileIsReplaced()
    {
#ifdef Q_OS_WIN
        QSKIP("Windows named pipes do not leave stale socket files");
#else
        QFile f(sockName_); QVERIFY(f.open(QIODevice::WriteOnly)); f.write("junk"); f.close();
        startServer();
        Client c; QVERIFY(c.connect(sockName_));
        QVERIFY(c.rpc({{"op", "get"}}).value("ok").toBool());
#endif
    }

    void endpointClosedOnShutdown()
    {
        startServer();
#ifndef Q_OS_WIN
        QVERIFY(QFile::exists(sockName_));
#endif
        server_.reset();
#ifndef Q_OS_WIN
        QVERIFY(!QFile::exists(sockName_));
#endif
        Client c;
        QVERIFY(!c.connect(sockName_));
    }
};

QTEST_GUILESS_MAIN(ControlTest)
#include "tst_control.moc"
