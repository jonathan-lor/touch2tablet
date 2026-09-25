#include "MainWindow.h"
#include "ControlServer.h"
#include "Daemon.h"
#include "backend/windows/DisplayMapping.h"
#include <QTemporaryDir>
#include <QStandardPaths>
#include <QUuid>
#include <QtTest>

using namespace t2t;

class WindowsGuiTest : public QObject {
    Q_OBJECT
private slots:
    void monitorDetectionUsesOutputDeviceIdentity()
    {
        QStandardPaths::setTestModeEnabled(true);
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        Daemon daemon(Settings{}, {}, {});
        const auto endpoint = QStringLiteral("t2t-gui-test-") + QUuid::createUuid().toString(QUuid::Id128);
        ControlServer server(daemon, {endpoint, temp.filePath("settings.json")});
        QString error;
        QVERIFY2(server.listen(&error), qPrintable(error));
        ControlClient client(endpoint);
        MainWindow window(client);
        QSignalSpy hello(&client, &ControlClient::hello);
        client.start();
        QTRY_COMPARE(hello.count(), 1);

        MONITORINFOEXW native{};
        native.cbSize = sizeof(native);
        const auto monitor = MonitorFromWindow(reinterpret_cast<HWND>(window.winId()), MONITOR_DEFAULTTONEAREST);
        QVERIFY(GetMonitorInfoW(monitor, &native));
        QPushButton* detect = nullptr;
        QPushButton* apply = nullptr;
        for (auto* button : window.findChildren<QPushButton*>()) {
            if (button->text() == "Detect from the monitor this window is on") detect = button;
            if (button->text() == "Apply") apply = button;
        }
        QVERIFY(detect);
        QVERIFY(apply);
        detect->click();
        QVERIFY(apply->isEnabled());
        apply->click();
        // Exercise the GUI -> protocol -> daemon contract, not just Win32 calls.
        QTRY_COMPARE(daemon.settings().display.output, QString::fromWCharArray(native.szDevice));
        const auto bounds = windows::monitorBounds(daemon.settings().display.output);
        QVERIFY(bounds);
        QCOMPARE(daemon.settings().display.width, bounds->right - bounds->left);
        QCOMPARE(daemon.settings().display.height, bounds->bottom - bounds->top);
    }
};

QTEST_MAIN(WindowsGuiTest)
#include "tst_windows_gui.moc"
