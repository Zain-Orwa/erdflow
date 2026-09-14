#include "main_window.hpp"
#include "theme.hpp"
#include "infrastructure/project_store.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QSettings>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("ERDFlow");
    QCoreApplication::setApplicationVersion("0.1.0");
    QCoreApplication::setOrganizationName("ERDFlow");
    // The theme owns both the application palette and the diagram colours, so a
    // remembered choice is applied before any window is built.
    QApplication::setStyle("Fusion");
    const QSettings settings;
    const auto chosen = erdflow::desktop::theme_from_key(settings.value("theme", "office-light").toString());
    erdflow::desktop::apply_theme(app, chosen);
    QCommandLineParser parser;
    parser.setApplicationDescription("ERDFlow conceptual ERD editor");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"example", "Open the bundled university example."});
    parser.addOption({"smoke-test", "Exit after initializing the window (for build verification)."});
    parser.addOption({"screenshot", "Save a window screenshot after initialization.", "path"});
    parser.addPositionalArgument("project", "An .erdx project to open.", "[project]");
    parser.process(app);
    erdflow::infrastructure::QtIdGenerator ids;
    erdflow::application::Editor editor(ids);
    erdflow::infrastructure::ErdxProjectStore store;
    erdflow::desktop::MainWindow window(editor, store, ids);
    window.restore_tool_palette(settings.value("toolPaletteFolded", false).toBool());
    window.set_icon_mode(erdflow::desktop::icon_mode_from_key(
        settings.value("iconMode", "normal").toString()));
    window.set_theme(chosen);
    window.show();
    QTimer::singleShot(0, &window, [&] {
        if (!parser.positionalArguments().isEmpty()) window.open_path(parser.positionalArguments().front());
        else if (parser.isSet("example")) window.load_example();
        QTimer::singleShot(500, &window, [&] {
            if (parser.isSet("screenshot") && !window.grab().save(parser.value("screenshot"))) {
                app.exit(1);
                return;
            }
            if (parser.isSet("smoke-test")) app.quit();
        });
    });
    return app.exec();
}
