#include "t2t/SettingsStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace t2t {

std::optional<QJsonObject> SettingsStore::read(const QString& path, QString* err)
{
    if (err) err->clear();
    QFile f(path);
    if (!f.exists())
        return std::nullopt;
    if (!f.open(QIODevice::ReadOnly)) {
        if (err) *err = QStringLiteral("cannot read %1: %2").arg(path, f.errorString());
        return std::nullopt;
    }
    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &perr);
    if (!doc.isObject()) {
        if (err) *err = QStringLiteral("%1: %2").arg(path, perr.error == QJsonParseError::NoError
                                                             ? QStringLiteral("not a JSON object") : perr.errorString());
        return std::nullopt;
    }
    return doc.object();
}

bool SettingsStore::write(const QString& path, const QJsonObject& json, QString* err)
{
    const QDir dir = QFileInfo(path).dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (err) *err = QStringLiteral("cannot create %1").arg(dir.path());
        return false;
    }
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) {
        if (err) *err = QStringLiteral("cannot write %1: %2").arg(path, f.errorString());
        return false;
    }
    f.write(QJsonDocument(json).toJson(QJsonDocument::Indented));
    if (!f.commit()) {
        if (err) *err = QStringLiteral("cannot write %1: %2").arg(path, f.errorString());
        return false;
    }
    return true;
}

}  // namespace t2t
