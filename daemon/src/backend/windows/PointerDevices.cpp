#include "PointerDevices.h"

#include <QJsonArray>

namespace t2t::windows {

QString winError(const QString& operation, DWORD code)
{
    wchar_t* message = nullptr;
    FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                  nullptr, code, 0, reinterpret_cast<wchar_t*>(&message), 0, nullptr);
    const QString detail = message ? QString::fromWCharArray(message).trimmed() : QStringLiteral("unknown error");
    if (message) LocalFree(message);
    return QStringLiteral("%1: %2 (Windows error %3)").arg(operation, detail).arg(code);
}

bool hasUiAccess(QString* error)
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        if (error) *error = winError(QStringLiteral("OpenProcessToken"));
        return false;
    }
    DWORD access = 0, size = 0;
    const BOOL ok = GetTokenInformation(token, TokenUIAccess, &access, sizeof(access), &size);
    const DWORD code = GetLastError();
    CloseHandle(token);
    if (!ok && error) *error = winError(QStringLiteral("TokenUIAccess"), code);
    return ok && access != 0;
}

std::vector<PointerDevice> pointerDevices(QString* error)
{
    if (error) error->clear();
    std::vector<POINTER_DEVICE_INFO> infos;
    bool enumerated = false;
    for (int attempt = 0; attempt < 3; ++attempt) {
        UINT32 count = 0;
        if (!GetPointerDevices(&count, nullptr)) break;
        if (!count) return {};
        infos.resize(count);
        if (GetPointerDevices(&count, infos.data())) {
            infos.resize(count);
            enumerated = true;
            break;
        }
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) break;
    }
    if (!enumerated) {
        if (error) *error = winError(QStringLiteral("GetPointerDevices"));
        return {};
    }

    std::vector<PointerDevice> devices;
    for (const auto& info : infos) {
        PointerDevice d;
        d.native = info;
        UINT size = 0;
        if (GetRawInputDeviceInfoW(info.device, RIDI_DEVICENAME, nullptr, &size) != UINT(-1) && size) {
            std::vector<wchar_t> name(size + 1, 0);
            if (GetRawInputDeviceInfoW(info.device, RIDI_DEVICENAME, name.data(), &size) != UINT(-1))
                d.path = QString::fromWCharArray(name.data());
        }
        if (d.path.isEmpty()) d.errors.append(winError(QStringLiteral("RIDI_DEVICENAME")));
        RID_DEVICE_INFO rid{};
        rid.cbSize = sizeof(rid);
        size = sizeof(rid);
        if (GetRawInputDeviceInfoW(info.device, RIDI_DEVICEINFO, &rid, &size) != UINT(-1) && rid.dwType == RIM_TYPEHID) {
            d.vendor = rid.hid.dwVendorId;
            d.product = rid.hid.dwProductId;
            d.usagePage = rid.hid.usUsagePage;
            d.usage = rid.hid.usUsage;
        } else {
            d.errors.append(winError(QStringLiteral("RIDI_DEVICEINFO")));
        }
        UINT32 count = 0;
        if (GetPointerDeviceProperties(info.device, &count, nullptr)) {
            d.properties.resize(count);
            if (count && !GetPointerDeviceProperties(info.device, &count, d.properties.data())) {
                d.errors.append(winError(QStringLiteral("GetPointerDeviceProperties")));
                d.properties.clear();
            } else {
                d.properties.resize(count);
            }
        } else {
            d.errors.append(winError(QStringLiteral("GetPointerDeviceProperties")));
        }
        // Diagnostic candidate only: an exact firmware-name check is still required
        // before a production backend may take over a panel sharing these USB IDs.
        if (info.pointerDeviceType == POINTER_DEVICE_TYPE_TOUCH && d.usagePage == 0x0d && d.usage == 0x04) {
            for (const Panel& p : panels()) {
                if (p.bus == 3 && UINT(p.vendor) == d.vendor && UINT(p.product) == d.product) {
                    d.panel = &p;
                    break;
                }
            }
        }
        devices.push_back(std::move(d));
    }
    return devices;
}

QJsonObject PointerDevice::json() const
{
    QJsonArray props, problems;
    for (const auto& p : properties)
        props.append(QJsonObject{{"usage_page", p.usagePageId}, {"usage", p.usageId},
                                 {"logical_min", p.logicalMin}, {"logical_max", p.logicalMax},
                                 {"physical_min", p.physicalMin}, {"physical_max", p.physicalMax},
                                 {"unit", qint64(p.unit)}, {"unit_exponent", qint64(p.unitExponent)}});
    for (const auto& e : errors) problems.append(e);
    return {{"product_string", QString::fromWCharArray(native.productString)}, {"path", path},
            {"device_type", int(native.pointerDeviceType)}, {"max_contacts", native.maxActiveContacts},
            {"vendor", QStringLiteral("%1").arg(vendor, 4, 16, QLatin1Char('0'))},
            {"product", QStringLiteral("%1").arg(product, 4, 16, QLatin1Char('0'))},
            {"usage_page", int(usagePage)}, {"usage", int(usage)},
            {"panel_candidate", panel ? QJsonValue(panel->name) : QJsonValue(QJsonValue::Null)},
            {"properties", props}, {"errors", problems}};
}

} // namespace t2t::windows
