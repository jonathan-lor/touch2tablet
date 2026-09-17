// Supported touch panels.  The daemon only ever grabs a device from this list, matched on the
// evdev device's bus:vendor:product (sysfs id/{bustype,vendor,product}) and its name: cheap panels
// share generic ids, and the name is what the firmware itself reports.
//
// The list is panels.json at the repository root, embedded as a Qt resource.  The udev rule and
// the table in docs/supported-panels.md are rendered from it; see that page for adding a panel.
#pragma once

#include <QByteArray>
#include <QString>
#include <optional>
#include <vector>

namespace t2t {

struct Panel {
    int bus = 0;   // BUS_* from linux/input.h
    int vendor = 0;
    int product = 0;
    QString evdevName;   // device name as the kernel reports it
    QString name;        // what the GUI and logs call it
    double widthMm = 0;  // active glass area
    double heightMm = 0;
    QString notes;       // optional
};

/// "usb", "i2c", ... or 4 hex digits for a bus without a name here; and back.
std::optional<int> busFromString(const QString& s);
QString busToString(int bus);

/// Strict: unknown keys, malformed ids, non-positive sizes and duplicate identities are errors.
std::optional<std::vector<Panel>> parsePanels(const QByteArray& json, QString* err);

/// The embedded panels.json.  Aborts if it is invalid (tst_panels keeps that from shipping).
const std::vector<Panel>& panels();
const Panel* findPanel(int bus, int vendor, int product, const QString& evdevName);

QString udevRules(const std::vector<Panel>& list);
QString markdownTable(const std::vector<Panel>& list);

}  // namespace t2t
