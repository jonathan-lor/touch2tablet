#pragma once
#include <Windows.h>
#include <QString>
#include <optional>

namespace t2t::windows {
inline std::optional<RECT> monitorBounds(const QString& name)
{
    struct Search { QString name; std::optional<RECT> rect; } search{name, {}};
    EnumDisplayMonitors(nullptr, nullptr, [](HMONITOR monitor, HDC, LPRECT, LPARAM param) -> BOOL {
        auto& s = *reinterpret_cast<Search*>(param);
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        if (GetMonitorInfoW(monitor, &info) && ((s.name.isEmpty() && (info.dwFlags & MONITORINFOF_PRIMARY))
            || s.name == QString::fromWCharArray(info.szDevice))) s.rect = info.rcMonitor;
        return TRUE;
    }, reinterpret_cast<LPARAM>(&search));
    return search.rect;
}
}
