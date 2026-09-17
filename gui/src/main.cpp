// touch2tablet GUI entry point.
//   touch2tablet [--socket PATH]     (default: $XDG_RUNTIME_DIR/touch2tablet/control.sock)
#include "ControlClient.h"
#include "MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QStandardPaths>

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("touch2tablet"));   // QSettings -> ~/.config/touch2tablet/touch2tablet.conf
    QApplication::setApplicationName(QStringLiteral("touch2tablet"));
    QApplication::setApplicationVersion(QStringLiteral("0.1"));
    // No setDesktopFileName(): the executable name already matches touch2tablet.desktop, and
    // setting it makes Qt >= 6.9 register with the desktop portal, which logs a harmless
    // "Could not register app ID: Connection already associated" warning on GNOME.

    const QString defaultSocket = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation)
                                  + QStringLiteral("/touch2tablet/control.sock");
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Tablet area and display area for a touch panel used as a pen tablet"));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption optSocket({"k", "socket"}, QStringLiteral("Daemon control socket"), QStringLiteral("path"), defaultSocket);
    parser.addOption(optSocket);
    parser.process(app);

    t2t::ControlClient client(parser.value(optSocket));
    t2t::MainWindow win(client);
    win.show();
    client.start();
    return app.exec();
}
