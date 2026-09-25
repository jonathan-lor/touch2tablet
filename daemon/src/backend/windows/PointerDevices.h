#pragma once

#include "Panels.h"
#include <Windows.h>
#include <QJsonObject>
#include <QStringList>
#include <vector>

namespace t2t::windows {

struct PointerDevice {
    POINTER_DEVICE_INFO native{};
    QString path;
    UINT vendor = 0, product = 0, usagePage = 0, usage = 0;
    std::vector<POINTER_DEVICE_PROPERTY> properties;
    QStringList errors;
    const Panel* panel = nullptr;

    QJsonObject json() const;
};

QString winError(const QString& operation, DWORD code = GetLastError());
std::vector<PointerDevice> pointerDevices(QString* error);
bool hasUiAccess(QString* error);

} // namespace t2t::windows
