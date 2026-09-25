#include "ControlServer.h"

#include "t2t/SettingsStore.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalSocket>

namespace t2t {

ControlServer::ControlServer(Daemon& daemon, Config cfg, QObject* parent)
    : QObject(parent), daemon_(daemon), cfg_(std::move(cfg))
{
    posClock_.start();
    server_.setSocketOptions(QLocalServer::UserAccessOption);
    connect(&server_, &QLocalServer::newConnection, this, &ControlServer::onNewConnection);
    connect(&daemon_, &Daemon::log, this, &ControlServer::addLogLine);
    connect(&daemon_, &Daemon::frameProcessed, this, &ControlServer::onFrame);
    connect(&daemon_, &Daemon::settingsChanged, this, [this](const Settings& s) {
        broadcast(QJsonObject{{"ev", "settings"}, {"settings", s.toJson()}, {"info", info()}});
    });
    connect(&daemon_, &Daemon::deviceChanged, this, [this](bool) {
        broadcast(QJsonObject{{"ev", "device"}, {"info", info()}});
    });
}

ControlServer::~ControlServer()
{
    for (auto& [s, c] : clients_)
        s->disconnect(this);
    server_.close();
    if (server_.isListening() || !cfg_.socketName.isEmpty())
        QLocalServer::removeServer(cfg_.socketName);
}

bool ControlServer::listen(QString* err)
{
#ifndef Q_OS_WIN
    const QDir dir = QFileInfo(cfg_.socketName).dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (err) *err = QStringLiteral("cannot create %1").arg(dir.path());
        return false;
    }
    QLocalServer::removeServer(cfg_.socketName);   // stale file from a crash
#endif
    if (!server_.listen(cfg_.socketName)) {
        if (err) *err = server_.errorString();
        return false;
    }
    return true;
}

// -- logging ------------------------------------------------------------------------------

void ControlServer::addLogLine(const QString& line)
{
    log_.push_back(line);
    while (int(log_.size()) > cfg_.maxLogLines)
        log_.pop_front();
    broadcast(QJsonObject{{"ev", "log"}, {"line", line}});
}

// -- clients ------------------------------------------------------------------------------

void ControlServer::onNewConnection()
{
    while (QLocalSocket* s = server_.nextPendingConnection()) {
        clients_.emplace(s, Client{});
        connect(s, &QLocalSocket::readyRead, this, [this, s] { onReadyRead(s); });
        connect(s, &QLocalSocket::disconnected, this, [this, s] {
            clients_.erase(s);
            s->deleteLater();
        });
    }
}

void ControlServer::onReadyRead(QLocalSocket* s)
{
    auto it = clients_.find(s);
    if (it == clients_.end()) return;
    it->second.buf += s->readAll();
    if (it->second.buf.size() > (1 << 20)) {   // garbage flood: drop the client
        s->disconnectFromServer();
        return;
    }
    int nl;
    while ((nl = it->second.buf.indexOf('\n')) >= 0) {
        const QByteArray line = it->second.buf.left(nl).trimmed();
        it->second.buf.remove(0, nl + 1);
        if (!line.isEmpty())
            handle(s, line);
        it = clients_.find(s);          // handle() may have dropped the client
        if (it == clients_.end()) return;
    }
}

QJsonObject ControlServer::replyBase(const QJsonValue& id) const
{
    QJsonObject r{{"ok", true}};
    if (!id.isUndefined() && !id.isNull())
        r.insert("id", id);
    return r;
}

void ControlServer::handle(QLocalSocket* s, const QByteArray& line)
{
    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(line, &perr);
    if (!doc.isObject()) {
        send(s, QJsonObject{{"ok", false}, {"error", "bad json"}});
        return;
    }
    const QJsonObject req = doc.object();
    const QJsonValue id = req.value("id");
    const QString op = req.value("op").toString();
    QJsonObject rep = replyBase(id);

    if (op == u"get") {
        rep.insert("settings", daemon_.settings().toJson());
        rep.insert("info", info());
    } else if (op == u"apply" || op == u"save") {
        daemon_.applySettings(Settings::applied(daemon_.settings(), req.value("settings").toObject()));
        const Settings s2 = daemon_.settings();
        if (op == u"save") {
            QString err;
            if (SettingsStore::write(cfg_.settingsPath, s2.toJson(), &err)) {
                addLogLine(QStringLiteral("settings saved to %1").arg(cfg_.settingsPath));
            } else {
                rep = replyBase(id);
                rep.insert("ok", false);
                rep.insert("error", QStringLiteral("save failed: %1").arg(err));
            }
        }
        rep.insert("settings", s2.toJson());
    } else if (op == u"shutdown") {
        daemon_.stop();
        QTimer::singleShot(100, this, &ControlServer::shutdownRequested);
    } else if (op == u"start" || op == u"stop") {
        if (op == u"start") daemon_.start(); else daemon_.stop();
        rep.insert("info", info());
        broadcast(QJsonObject{{"ev", "device"}, {"info", info()}});
    } else if (op == u"reset") {
        daemon_.applySettings(Settings::defaults());
        rep.insert("settings", daemon_.settings().toJson());
    } else if (op == u"log") {
        QJsonArray lines;
        for (const auto& l : log_) lines.append(l);
        rep.insert("lines", lines);
    } else if (op == u"subscribe") {
        clients_[s].subscribed = true;
        QJsonArray lines;
        for (const auto& l : log_) lines.append(l);
        rep.insert("proto", 1);
        rep.insert("settings", daemon_.settings().toJson());
        rep.insert("info", info());
        rep.insert("lines", lines);
    } else {
        rep = replyBase(id);
        rep.insert("ok", false);
        rep.insert("error", QStringLiteral("unknown op '%1'").arg(op));
    }
    send(s, rep);
}

void ControlServer::send(QLocalSocket* s, const QJsonObject& obj)
{
    if (s->state() != QLocalSocket::ConnectedState) return;
    s->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n');
    if (s->bytesToWrite() > (4 << 20))    // client not reading; drop it
        s->disconnectFromServer();
}

void ControlServer::broadcast(const QJsonObject& obj)
{
    for (auto& [s, c] : clients_)
        if (c.subscribed) send(s, obj);
}

// -- status -------------------------------------------------------------------------------

QJsonObject ControlServer::info() const
{
    QJsonObject o{
        {"connected", daemon_.connected()},
        {"running", daemon_.running()},
        {"frames_processed", qint64(daemon_.frameCount())},
        {"touch_path", QJsonValue::Null},
        {"raw", QJsonValue::Null},
        {"panel", QJsonValue::Null},
        {"tablet_path", QJsonValue::Null},
        {"settings_path", cfg_.settingsPath},
        {"proto", 1},
    };
    if (const auto si = daemon_.sourceInfo()) {
        o.insert("touch_path", si->path);
        o.insert("raw", QJsonObject{{"xmin", si->range.xmin}, {"xmax", si->range.xmax},
                                    {"ymin", si->range.ymin}, {"ymax", si->range.ymax}});
        o.insert("panel", QJsonObject{{"name", si->panel->name},
                                      {"width_mm", si->panel->widthMm}, {"height_mm", si->panel->heightMm}});
    }
    if (const QString p = daemon_.sinkPath(); !p.isEmpty())
        o.insert("tablet_path", p);
    if (const auto st = daemon_.inputState())
        o.insert("input", QJsonObject{{"fingers", st->fingers}, {"active", st->active},
            {"raw", QJsonArray{st->rawX, st->rawY}}, {"out", QJsonArray{st->mapped.x, st->mapped.y}}});
    return o;
}

void ControlServer::onFrame(const GestureMachine::State& st)
{
    bool any = false;
    for (auto& [s, c] : clients_) any = any || c.subscribed;
    if (!any) return;

    if (st.fingers == 0) {
        if (lastHadFingers_)
            broadcast(QJsonObject{{"ev", "pos"}, {"fingers", 0}});
        lastHadFingers_ = false;
        return;
    }
    lastHadFingers_ = true;
    if (!st.primaryReady) return;
    const qint64 now = posClock_.elapsed();
    if (lastPosMs_ >= 0 && now - lastPosMs_ < 1000 / cfg_.posRateHz) return;
    lastPosMs_ = now;

    broadcast(QJsonObject{
        {"ev", "pos"},
        {"fingers", st.fingers},
        {"raw", QJsonArray{st.rawX, st.rawY}},
        {"mm", QJsonArray{qRound(st.mapped.mmX * 100) / 100.0, qRound(st.mapped.mmY * 100) / 100.0}},
        {"out", QJsonArray{st.mapped.x, st.mapped.y}},
        {"inside", st.mapped.inside},
        {"ignored", st.ignored},
    });
}

}  // namespace t2t
