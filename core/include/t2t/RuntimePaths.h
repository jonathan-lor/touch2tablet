#pragma once
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QString>

namespace t2t {
inline QString defaultControlEndpoint()
{
#ifdef Q_OS_WIN
    // A per-user named pipe, independent of GUI/daemon application names.
    const auto user = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation).toUtf8();
    return QStringLiteral("touch2tablet-") + QString::fromLatin1(QCryptographicHash::hash(user, QCryptographicHash::Sha256).toHex().left(24));
#else
    return QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + QStringLiteral("/touch2tablet/control.sock");
#endif
}
}
