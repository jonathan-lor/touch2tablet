// UinputSink with a recording device: what each output mode announces and writes to the kernel.
#include "backend/linux/UinputSink.h"

#include <QtTest>
#include <libevdev/libevdev.h>

using namespace t2t;

namespace {

// Writes as text: "X100 Y200 ." is ABS_X 100, ABS_Y 200, SYN_REPORT; L = BTN_LEFT, T = BTN_TOUCH,
// P = BTN_TOOL_PEN.
struct Recorder : UinputSink::Device {
    explicit Recorder(QStringList& log) : log(log) {}
    void write(unsigned type, unsigned code, int value) override
    {
        if (type == EV_SYN) { log << QStringLiteral("."); return; }
        const char* name = type == EV_ABS ? (code == ABS_X ? "X" : "Y")
                         : code == BTN_LEFT ? "L" : code == BTN_TOUCH ? "T" : code == BTN_TOOL_PEN ? "P" : "?";
        log << QString::fromLatin1(name) + QString::number(value);
    }
    QString path() const override { return QStringLiteral("/dev/input/event99"); }
    QStringList& log;
};

// What a created device announced.
struct Created {
    QString name;
    int bus = 0;
    bool left = false, pen = false, touch = false;
    int maxX = 0, maxY = 0;
};

const PenEvent kStroke[] = {
    {PenEvent::Type::ProximityIn}, {PenEvent::Type::Move, 100, 200}, {PenEvent::Type::Down, 100, 200},
    {PenEvent::Type::Move, 110, 200}, {PenEvent::Type::Up}, {PenEvent::Type::ProximityOut}};

}  // namespace

class LinuxUinputTest : public QObject {
    Q_OBJECT

    QStringList log_;
    std::vector<Created> created_;

    UinputSink::CreateDevice recorder()
    {
        return [this](const libevdev* d, QString*) {
            created_.push_back({QString::fromUtf8(libevdev_get_name(d)), libevdev_get_id_bustype(d),
                                libevdev_has_event_code(d, EV_KEY, BTN_LEFT) != 0,
                                libevdev_has_event_code(d, EV_KEY, BTN_TOOL_PEN) != 0,
                                libevdev_has_event_code(d, EV_KEY, BTN_TOUCH) != 0,
                                libevdev_get_abs_maximum(d, ABS_X), libevdev_get_abs_maximum(d, ABS_Y)});
            return std::make_unique<Recorder>(log_);
        };
    }

    QString take()
    {
        const QString s = log_.join(QLatin1Char(' '));
        log_.clear();
        return s;
    }

private slots:
    void init() { log_.clear(); created_.clear(); }

    void mouseStroke()
    {
        UinputSink sink(recorder());
        QVERIFY(sink.open(Settings{}, nullptr));   // 2560 x 1440, absolute mouse
        const Created c = created_.back();
        QCOMPARE(c.name, QString::fromLatin1(UinputSink::kMouseName));
        QCOMPARE(c.bus, BUS_VIRTUAL);
        QVERIFY(c.left && !c.pen && !c.touch);   // a mouse to udev, not a tablet
        QCOMPARE(c.maxX, 2559);
        QCOMPARE(c.maxY, 1439);

        for (const PenEvent& e : kStroke) sink.handle(e);
        // The stroke's first position is nudged through, and the press follows in a frame of its own.
        QCOMPARE(take(), QStringLiteral("X99 X100 Y200 . L1 . X110 Y200 . L0 ."));

        sink.handle({PenEvent::Type::ProximityIn});
        sink.handle({PenEvent::Type::Move, 0, 5});
        sink.handle({PenEvent::Type::Down, 0, 5});
        sink.close();   // releases the held button
        QCOMPARE(take(), QStringLiteral("X1 X0 Y5 . L1 . L0 ."));
    }

    void penStroke()
    {
        UinputSink sink(recorder());
        Settings s;
        s.outputMode = OutputMode::Pen;
        QVERIFY(sink.open(s, nullptr));
        const Created c = created_.back();
        QCOMPARE(c.name, QString::fromLatin1(UinputSink::kPenName));
        QVERIFY(c.pen && c.touch && !c.left);

        for (const PenEvent& e : kStroke) sink.handle(e);
        // Proximity goes out with the first position and the tip in the next frame.
        QCOMPARE(take(), QStringLiteral("P1 X100 Y200 . T1 . X110 Y200 . T0 . P0 ."));
    }

    void modeOrDisplayChangeRecreatesTheDevice()
    {
        UinputSink sink(recorder());
        const Settings s;
        QVERIFY(sink.open(s, nullptr));
        QCOMPARE(sink.kind(), QStringLiteral("absolute mouse"));
        Settings t = s;
        t.displayArea.width = 100;
        QVERIFY(!sink.needsReopen(t));   // remapping keeps the device
        t.outputMode = OutputMode::Pen;
        QVERIFY(sink.needsReopen(t));
        t = s;
        t.display.width = 1920;
        QVERIFY(sink.needsReopen(t));

        t.outputMode = OutputMode::Pen;
        sink.close();                    // what Daemon::applySettings does
        QVERIFY(sink.open(t, nullptr));
        QCOMPARE(created_.size(), size_t(2));
        QVERIFY(created_.back().pen);
        QCOMPARE(created_.back().maxX, 1919);
        QCOMPARE(sink.kind(), QStringLiteral("pen tablet"));
        QVERIFY(!sink.needsReopen(t));
    }
};

QTEST_APPLESS_MAIN(LinuxUinputTest)
#include "tst_linux_uinput.moc"
