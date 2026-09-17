// The supported-panel list: the parser is strict, the shipped panels.json is valid, and the
// table in docs/supported-panels.md is the one rendered from it.
#include "Panels.h"

#include <QFile>
#include <QtTest>

using namespace t2t;

class PanelsTest : public QObject {
    Q_OBJECT

    static QByteArray entry(const QByteArray& fields)
    {
        return "{\"panels\":[{" + fields + "}]}";
    }
    static constexpr const char* kGood =
        "\"bus\":\"usb\",\"vendor\":\"222A\",\"product\":\"0001\",\"evdev_name\":\"Acme CTP\",\"name\":\"P | 7\\\"\",\"width_mm\":165,\"height_mm\":99.5";

private slots:
    void parsesAnEntry()
    {
        QString err;
        const auto list = parsePanels(entry(kGood), &err);
        QVERIFY2(list, qPrintable(err));
        QCOMPARE(list->size(), size_t(1));
        const Panel& p = list->front();
        QCOMPARE(p.bus, 0x03);
        QCOMPARE(p.vendor, 0x222a);
        QCOMPARE(p.product, 0x0001);
        QCOMPARE(p.heightMm, 99.5);
        QVERIFY(markdownTable(*list).contains("| P \\| 7\" | usb | `222a:0001` | `Acme CTP` | 165 x 99.5 mm |  |"));
        QVERIFY(udevRules(*list).contains(
            "KERNEL==\"event*\", ATTRS{id/bustype}==\"0003\", ATTRS{id/vendor}==\"222a\", ATTRS{id/product}==\"0001\", ATTRS{name}==\"Acme CTP\", TAG+=\"uaccess\""));
    }

    void rejects_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::addColumn<QString>("complaint");
        const QByteArray good = kGood;
        auto with = [&](const char* from, const char* to) { return entry(QByteArray(good).replace(from, to)); };
        QTest::newRow("not json") << QByteArray("{") << "byte";
        QTest::newRow("empty list") << QByteArray("{\"panels\":[]}") << "non-empty";
        QTest::newRow("bus") << with("\"usb\"", "\"pci\"") << "\"bus\"";
        QTest::newRow("short id") << with("\"222A\"", "\"22a\"") << "\"vendor\"";
        QTest::newRow("numeric id") << with("\"0001\"", "1") << "\"product\"";
        QTest::newRow("unfilled name") << with("P | 7\\\"", "TODO what the GUI and logs should call it") << "\"name\"";
        QTest::newRow("no evdev name") << with(",\"evdev_name\":\"Acme CTP\"", "") << "\"evdev_name\"";
        QTest::newRow("quoted evdev name") << with("Acme CTP", "Acme \\\"CTP\\\"") << "\"evdev_name\"";
        QTest::newRow("zero size") << with("165", "0") << "\"width_mm\"";
        QTest::newRow("missing size") << with(",\"height_mm\":99.5", "") << "\"height_mm\"";
        QTest::newRow("unknown key") << entry(good + ",\"widht_mm\":1") << "widht_mm";
        QTest::newRow("duplicate") << QByteArray("{\"panels\":[{" + good + "},{" + good + "}]}") << "same id";
    }
    void rejects()
    {
        QFETCH(QByteArray, json);
        QFETCH(QString, complaint);
        QString err;
        QVERIFY(!parsePanels(json, &err));
        QVERIFY2(err.contains(complaint), qPrintable(err));
    }

    void sameIdIsToldApartByName()
    {
        const QByteArray good = kGood;
        QString err;
        const auto list = parsePanels("{\"panels\":[{" + good + "},{" + QByteArray(good).replace("Acme CTP", "Acme CTP 10in") + "}]}", &err);
        QVERIFY2(list, qPrintable(err));
        QCOMPARE(list->size(), size_t(2));
    }

    void shippedListIsValid()
    {
        QFile f(QStringLiteral(T2T_SOURCE_DIR "/panels.json"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        QString err;
        QVERIFY2(parsePanels(f.readAll(), &err), qPrintable(err));
        QVERIFY(!panels().empty());   // the embedded copy
        const Panel& p = panels().front();
        QCOMPARE(findPanel(p.bus, p.vendor, p.product, p.evdevName), &p);
        QVERIFY(!findPanel(p.bus, p.vendor, p.product, p.evdevName + QStringLiteral(" v2")));
    }

    void docsTableIsCurrent()
    {
        QFile f(QStringLiteral(T2T_SOURCE_DIR "/docs/supported-panels.md"));
        QVERIFY(f.open(QIODevice::ReadOnly));
        const QString doc = QString::fromUtf8(f.readAll());
        const QString begin = QStringLiteral("<!-- panels:begin -->\n"), end = QStringLiteral("<!-- panels:end -->");
        const qsizetype a = doc.indexOf(begin), b = doc.indexOf(end);
        QVERIFY2(a >= 0 && b > a, "docs/supported-panels.md lost its panels:begin / panels:end markers");
        QVERIFY2(doc.mid(a + begin.size(), b - a - begin.size()) == markdownTable(panels()),
                 "docs/supported-panels.md is out of date with panels.json: run scripts/update-docs.sh");
    }
};

QTEST_GUILESS_MAIN(PanelsTest)
#include "tst_panels.moc"
