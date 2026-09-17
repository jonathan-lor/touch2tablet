#include "Panels.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStringList>
#include <cmath>
#include <utility>

namespace t2t {

namespace {

constexpr std::pair<const char*, int> kBuses[] = {
    {"usb", 0x03}, {"bluetooth", 0x05}, {"i2c", 0x18}, {"spi", 0x1c},
};

std::optional<int> hex4(const QString& s)
{
    static const QRegularExpression re(QStringLiteral("^[0-9a-fA-F]{4}$"));
    if (!re.match(s).hasMatch()) return std::nullopt;
    return s.toInt(nullptr, 16);
}

QString hex4(int v)
{
    return QStringLiteral("%1").arg(v, 4, 16, QLatin1Char('0'));
}

QString cell(QString s)
{
    return s.replace(QLatin1Char('|'), QLatin1String("\\|")).replace(QLatin1Char('\n'), QLatin1Char(' '));
}

QString code(const QString& s)
{
    return s.isEmpty() ? s : QLatin1Char('`') + s + QLatin1Char('`');
}

}  // namespace

std::optional<int> busFromString(const QString& s)
{
    for (const auto& [name, bus] : kBuses)
        if (s == QLatin1String(name)) return bus;
    return hex4(s);
}

QString busToString(int bus)
{
    for (const auto& [name, b] : kBuses)
        if (b == bus) return QLatin1String(name);
    return hex4(bus);
}

std::optional<std::vector<Panel>> parsePanels(const QByteArray& json, QString* err)
{
    auto fail = [err](const QString& why) {
        if (err) *err = QStringLiteral("panels.json: %1").arg(why);
        return std::nullopt;
    };
    QJsonParseError perr{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &perr);
    if (perr.error != QJsonParseError::NoError)
        return fail(QStringLiteral("%1 at byte %2").arg(perr.errorString()).arg(perr.offset));
    const QJsonValue arr = doc.object().value("panels");
    if (!arr.isArray() || arr.toArray().isEmpty())
        return fail(QStringLiteral("expected a non-empty \"panels\" array"));

    static const QStringList known{"bus", "vendor", "product", "name", "width_mm", "height_mm", "evdev_name", "notes"};
    std::vector<Panel> list;
    int i = 0;
    for (const QJsonValue& v : arr.toArray()) {
        const QJsonObject o = v.toObject();
        const QString nameStr = o.value("name").toString();
        const QString where = nameStr.isEmpty() ? QStringLiteral("panel %1").arg(i) : QStringLiteral("panel %1 (%2)").arg(i).arg(nameStr);
        ++i;
        if (!v.isObject())
            return fail(QStringLiteral("%1: not an object").arg(where));
        for (const QString& key : o.keys())
            if (!known.contains(key))
                return fail(QStringLiteral("%1: unknown key \"%2\"").arg(where, key));

        Panel p;
        const auto bus = busFromString(o.value("bus").toString());
        if (!bus) {
            QStringList names;
            for (const auto& [name, b] : kBuses) names << QLatin1String(name);
            return fail(QStringLiteral("%1: \"bus\" must be one of %2, or 4 hex digits").arg(where, names.join(QStringLiteral(", "))));
        }
        p.bus = *bus;
        for (const auto& [key, out] : {std::pair{"vendor", &p.vendor}, std::pair{"product", &p.product}}) {
            const auto id = hex4(o.value(QLatin1String(key)).toString());
            if (!id)
                return fail(QStringLiteral("%1: \"%2\" must be a string of 4 hex digits").arg(where, QLatin1String(key)));
            *out = *id;
        }
        p.name = nameStr.trimmed();
        if (p.name.isEmpty() || p.name.startsWith(u"TODO"))
            return fail(QStringLiteral("%1: \"name\" is missing").arg(where));
        for (const auto& [key, out] : {std::pair{"width_mm", &p.widthMm}, std::pair{"height_mm", &p.heightMm}}) {
            const QJsonValue mm = o.value(QLatin1String(key));
            if (!mm.isDouble() || !std::isfinite(mm.toDouble()) || mm.toDouble() <= 0)
                return fail(QStringLiteral("%1: \"%2\" must be a positive number (the active glass area)").arg(where, QLatin1String(key)));
            *out = mm.toDouble();
        }
        // Matched verbatim by the daemon and quoted in the udev rule.
        p.evdevName = o.value("evdev_name").toString();
        if (p.evdevName.isEmpty() || p.evdevName != p.evdevName.trimmed() || p.evdevName.contains(QLatin1Char('"'))
            || p.evdevName.contains(QLatin1Char('\\')) || p.evdevName.contains(QLatin1Char('\n')))
            return fail(QStringLiteral("%1: \"evdev_name\" must be the device name exactly as `touch2tabletd --identify` "
                                       "prints it (no quotes, backslashes or surrounding space)").arg(where));
        const QJsonValue notes = o.value("notes");
        if (!notes.isUndefined() && !notes.isString())
            return fail(QStringLiteral("%1: \"notes\" must be a string").arg(where));
        p.notes = notes.toString();
        for (const Panel& other : list)
            if (other.bus == p.bus && other.vendor == p.vendor && other.product == p.product && other.evdevName == p.evdevName)
                return fail(QStringLiteral("%1: same id and evdev_name as \"%2\"").arg(where, other.name));
        list.push_back(std::move(p));
    }
    return list;
}

const std::vector<Panel>& panels()
{
    static const std::vector<Panel> list = [] {
        QFile f(QStringLiteral(":/panels.json"));
        QString err = QStringLiteral("panels.json: not embedded");
        auto parsed = f.open(QIODevice::ReadOnly) ? parsePanels(f.readAll(), &err) : std::nullopt;
        if (!parsed)
            qFatal("%s", qPrintable(err));
        return *std::move(parsed);
    }();
    return list;
}

const Panel* findPanel(int bus, int vendor, int product, const QString& evdevName)
{
    for (const Panel& p : panels())
        if (p.bus == bus && p.vendor == vendor && p.product == product && p.evdevName == evdevName) return &p;
    return nullptr;
}

QString udevRules(const std::vector<Panel>& list)
{
    QString out = QStringLiteral(
        "# touch2tablet: let the user at the active seat grab the supported touch panels and /dev/uinput,\n"
        "# so the daemon can run as a user service.  Installed by scripts/install-user.sh from\n"
        "# `touch2tabletd --print-udev-rules`; do not edit.\n");
    for (const Panel& p : list) {
        QString name = p.name;
        out += QStringLiteral("# %1\n").arg(name.replace(QLatin1Char('\n'), QLatin1Char(' ')));
        out += QStringLiteral("SUBSYSTEM==\"input\", KERNEL==\"event*\", ATTRS{id/bustype}==\"%1\", ATTRS{id/vendor}==\"%2\", "
                              "ATTRS{id/product}==\"%3\", ATTRS{name}==\"%4\", TAG+=\"uaccess\"\n")
                   .arg(hex4(p.bus), hex4(p.vendor), hex4(p.product), p.evdevName);
    }
    out += QStringLiteral("KERNEL==\"uinput\", SUBSYSTEM==\"misc\", TAG+=\"uaccess\", OPTIONS+=\"static_node=uinput\"\n");
    return out;
}

QString markdownTable(const std::vector<Panel>& list)
{
    QString out = QStringLiteral("| Panel | Bus | ID | evdev name | Glass | Notes |\n|---|---|---|---|---|---|\n");
    for (const Panel& p : list) {
        const QStringList cells{cell(p.name), busToString(p.bus), code(hex4(p.vendor) + QLatin1Char(':') + hex4(p.product)),
                                code(cell(p.evdevName)), QStringLiteral("%1 x %2 mm").arg(p.widthMm).arg(p.heightMm),
                                cell(p.notes)};
        out += QStringLiteral("| ") + cells.join(QStringLiteral(" | ")) + QStringLiteral(" |\n");
    }
    return out;
}

}  // namespace t2t
