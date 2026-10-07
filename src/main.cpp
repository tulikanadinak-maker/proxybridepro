#include <QApplication>
#include <QStyleFactory>
#include "core/application.h"
#include "gui/main_window.h"

int main(int argc, char* argv[]) {
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName("Witeck Proxy Unlimited");
    app.setApplicationVersion("2.0.0");
    app.setOrganizationName("WiteckProxyUnlimited");
    app.setStyle(QStyleFactory::create("Fusion"));

    auto& core = ProxyBridge::Application::instance();
    if (!core.initialize()) return -1;

    ProxyBridge::MainWindow window;
    window.show();

    int result = app.exec();
    core.shutdown();
    return result;
}
