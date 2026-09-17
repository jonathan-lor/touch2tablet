// Atomic read/write of the settings JSON file (QtCore only).
#pragma once

#include <QJsonObject>
#include <QString>
#include <optional>

namespace t2t {

struct SettingsStore {
    /// Parsed JSON object, or nullopt with *err set.  A missing file is not an error: returns
    /// nullopt with an empty *err.
    static std::optional<QJsonObject> read(const QString& path, QString* err);
    /// Write atomically (temp file + rename), creating the directory if needed.
    static bool write(const QString& path, const QJsonObject& json, QString* err);
};

}  // namespace t2t
