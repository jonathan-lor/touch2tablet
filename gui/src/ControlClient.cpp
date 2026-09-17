#include "ControlClient.h"

#include <QJsonDocument>

namespace t2t {

ControlClient::ControlClient(QString socketName, QObject* parent)
    : QObject(parent), name_(std::move(socketName))
{
    retry_.setSingleShot(true);
    connect(&retry_, &QTimer::timeout, this, &ControlClient::tryConnect);
    connect(&sock_, &QLocalSocket::connected, this, &ControlClient::onConnected);
    connect(&sock_, &QLocalSocket::readyRead, this, &ControlClient::onReadyRead);
    connect(&sock_, &QLocalSocket::disconnected, this, [this] { onLost(QStringLiteral("connection to daemon lost")); });
    connect(&sock_, &QLocalSocket::errorOccurred, this, [this](QLocalSocket::LocalSocketError) {
        if (!connected_) {
            emit stateChanged(false, QStringLiteral("daemon not reachable at %1 (%2)").arg(name_, sock_.errorString()));
            retry_.start(retryIntervalMs);
        }
    });
}

ControlClient::~ControlClient()
{
    // Members are destroyed in reverse order (retry_ before sock_); make sure the socket's
    // disconnect/error signals cannot reach onLost() while that is happening.
    sock_.disconnect(this);
    retry_.stop();
    sock_.abort();
}

void ControlClient::start()
{
    tryConnect();
}

void ControlClient::tryConnect()
{
    if (connected_ || sock_.state() == QLocalSocket::ConnectingState) return;
    sock_.connectToServer(name_);
}

void ControlClient::onConnected()
{
    connected_ = true;
    buf_.clear();
    emit stateChanged(true, QStringLiteral("connected"));
    request(QJsonObject{{"op", "subscribe"}}, [this](const QJsonObject& r) { emit hello(r); });
}

void ControlClient::onLost(const QString& why)
{
    const bool was = connected_;
    connected_ = false;
    for (auto& [id, cb] : pending_)
        if (cb) cb(QJsonObject{{"ok", false}, {"error", "connection lost"}});
    pending_.clear();
    if (was) emit stateChanged(false, why);
    retry_.start(retryIntervalMs);
}

void ControlClient::request(QJsonObject req, Callback cb)
{
    if (!connected_) {
        if (cb) cb(QJsonObject{{"ok", false}, {"error", "not connected"}});
        return;
    }
    const int id = nextId_++;
    req.insert("id", id);
    if (cb) pending_[id] = std::move(cb);
    sock_.write(QJsonDocument(req).toJson(QJsonDocument::Compact) + '\n');
    sock_.flush();
}

void ControlClient::onReadyRead()
{
    buf_ += sock_.readAll();
    int nl;
    while ((nl = buf_.indexOf('\n')) >= 0) {
        const QByteArray line = buf_.left(nl);
        buf_.remove(0, nl + 1);
        const QJsonDocument doc = QJsonDocument::fromJson(line);
        if (!doc.isObject()) continue;
        const QJsonObject m = doc.object();
        if (m.contains("id")) {
            auto it = pending_.find(m.value("id").toInt(-1));
            if (it != pending_.end()) {
                Callback cb = std::move(it->second);
                pending_.erase(it);
                if (cb) cb(m);
                continue;
            }
        }
        if (m.contains("ev"))
            emit event(m);
    }
}

}  // namespace t2t
