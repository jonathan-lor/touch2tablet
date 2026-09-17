// touch2tabletd: present a supported touch panel (Panels.h) as a virtual pen tablet (Linux).
//
//   touch2tabletd [--settings PATH] [--socket PATH] [--no-device]
//   touch2tabletd --identify | --print-udev-rules | --print-panel-table
//
// Settings: JSON as written by the GUI (see README).  Missing file = defaults.
// Control socket: see ControlServer.h.  --no-device serves the socket without touching hardware.
#include "ControlServer.h"
#include "Daemon.h"
#include "UnixSignals.h"
#include "backend/linux/EvdevTouchSource.h"
#include "backend/linux/UinputPenSink.h"
#include "t2t/SettingsStore.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QTextStream>
#include <cstdio>

using namespace t2t;

namespace {

Settings loadSettings(const QString& path, QString* note)
{
    QString err;
    const auto json = SettingsStore::read(path, &err);
    if (!json) {
        *note = err.isEmpty() ? QStringLiteral("no settings file at %1; using defaults").arg(path)
                              : QStringLiteral("%1; using defaults").arg(err);
        return Settings::defaults();
    }
    *note = QStringLiteral("settings loaded from %1").arg(path);
    return Settings::fromJson(*json);
}

}  // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("touch2tabletd"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Present a supported touch panel as a virtual pen tablet"));
    parser.addHelpOption();
    parser.addVersionOption();
    // Default: the invoking user's config dir (~/.config/touch2tablet/settings.json); the systemd
    // units pass the path explicitly so behavior does not depend on who runs the daemon.
    const QString defaultSettings = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
                                    + QStringLiteral("/touch2tablet/settings.json");
    QCommandLineOption optSettings({"s", "settings"}, QStringLiteral("Settings JSON file"), QStringLiteral("path"),
                                   defaultSettings);
    const QString defaultSocket = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)
                                  + QStringLiteral("/touch2tablet/control.sock");
    QCommandLineOption optSocket({"k", "socket"}, QStringLiteral("Control socket path"), QStringLiteral("path"), defaultSocket);
    QCommandLineOption optNoDevice(QStringLiteral("no-device"), QStringLiteral("Serve the control socket only; never open input devices"));
    parser.addOption(optSettings);
    parser.addOption(optSocket);
    QCommandLineOption optIdentify(QStringLiteral("identify"), QStringLiteral("Print a panels.json entry for every multitouch device, then exit"));
    QCommandLineOption optUdev(QStringLiteral("print-udev-rules"), QStringLiteral("Print the udev rule for the supported panels, then exit"));
    QCommandLineOption optTable(QStringLiteral("print-panel-table"), QStringLiteral("Print the table of docs/supported-panels.md, then exit"));
    parser.addOption(optNoDevice);
    parser.addOption(optIdentify);
    parser.addOption(optUdev);
    parser.addOption(optTable);
    parser.process(app);
    if (parser.isSet(optIdentify) || parser.isSet(optUdev) || parser.isSet(optTable)) {
        QTextStream(stdout) << (parser.isSet(optIdentify) ? EvdevTouchSource::identify()
                                : parser.isSet(optUdev)   ? udevRules(panels())
                                                          : markdownTable(panels()));
        return 0;
    }
    const bool noDevice = parser.isSet(optNoDevice);

    QTextStream out(stdout);
    const bool underJournal = qEnvironmentVariableIsSet("JOURNAL_STREAM");   // journald stamps lines itself
    auto logLine = [&out, underJournal](const QString& line) {
        if (!underJournal)
            out << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss")) << ' ';
        out << line << '\n';
        out.flush();
    };

    QString note;
    const Settings settings = loadSettings(parser.value(optSettings), &note);
    logLine(note);

    Daemon daemon(
        settings,
        [&logLine, noDevice]() -> std::unique_ptr<ITouchSource> {
            if (noDevice) return nullptr;
            QString err;
            auto src = EvdevTouchSource::probe(&err);
            static QString lastErr;
            if (!src && !err.isEmpty() && err != lastErr) {   // log a changed failure reason once
                logLine(QStringLiteral("touch panel not usable yet: %1").arg(err));
                lastErr = err;
            }
            return std::unique_ptr<ITouchSource>(std::move(src));
        },
        [] { return std::unique_ptr<IPenSink>(std::make_unique<UinputPenSink>()); });
    QObject::connect(&daemon, &Daemon::log, &app, logLine);

    ControlServer control(daemon, ControlServer::Config{parser.value(optSocket), parser.value(optSettings)});
    control.addLogLine(note);

    UnixSignals unixSignals;
    QObject::connect(&unixSignals, &UnixSignals::terminate, &app, [&](int signo) {
        logLine(QStringLiteral("signal %1: shutting down").arg(signo));
        daemon.stop();
        app.quit();
    });
    QObject::connect(&unixSignals, &UnixSignals::hangup, &app, [&] {
        QString n;
        daemon.applySettings(loadSettings(parser.value(optSettings), &n));
        logLine(QStringLiteral("SIGHUP: %1").arg(n)); control.addLogLine(QStringLiteral("SIGHUP: %1").arg(n));
    });

    {
        QString err;
        if (control.listen(&err)) {
            const QString l = QStringLiteral("control socket at %1").arg(control.socketName());
            logLine(l); control.addLogLine(l);
        } else {
            const QString l = QStringLiteral("control socket unavailable (%1); running without it").arg(err);
            logLine(l); control.addLogLine(l);
        }
    }
    const QString l = QStringLiteral("touch2tabletd %1 starting; looking for a supported panel%2")
                          .arg(QCoreApplication::applicationVersion(), noDevice ? QStringLiteral(" (--no-device)") : QString());
    logLine(l); control.addLogLine(l);
    daemon.start();
    return app.exec();
}
