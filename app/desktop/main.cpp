#include "main_window.hpp"
#include "infrastructure/project_store.hpp"

#include <QApplication>
#include <QCommandLineParser>
#include <QPalette>
#include <QTimer>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName("ERDFlow");
    QCoreApplication::setApplicationVersion("0.1.0");
    QCoreApplication::setOrganizationName("ERDFlow");
    QApplication::setStyle("Fusion");
    QPalette palette;
    palette.setColor(QPalette::Window, QColor(27, 34, 45));
    palette.setColor(QPalette::WindowText, QColor(228, 235, 244));
    palette.setColor(QPalette::Base, QColor(22, 29, 39));
    palette.setColor(QPalette::AlternateBase, QColor(32, 41, 54));
    palette.setColor(QPalette::Text, QColor(228, 235, 244));
    palette.setColor(QPalette::Button, QColor(38, 48, 62));
    palette.setColor(QPalette::ButtonText, QColor(228, 235, 244));
    palette.setColor(QPalette::Highlight, QColor(40, 105, 120));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::ToolTipBase, QColor(38, 48, 62));
    palette.setColor(QPalette::ToolTipText, QColor(228, 235, 244));
    palette.setColor(QPalette::PlaceholderText, QColor(146, 161, 179));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(119, 133, 150));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(119, 133, 150));
    app.setPalette(palette);
    app.setStyleSheet(
        "QToolBar { spacing: 6px; padding: 8px; border: none; border-bottom: 1px solid #354252; }"
        "QToolButton { padding: 7px 11px; border-radius: 4px; }"
        "QToolButton:checked { background: #286978; color: white; }"
        "QDockWidget::title { padding: 10px; font-weight: 600; }"
        "QTreeView { border: none; padding: 6px; }"
        "QTreeView::item { padding: 6px 2px; }"
        "QLineEdit, QComboBox, QDoubleSpinBox { padding: 5px; }"
        "QPushButton { padding: 7px 12px; }"
        "QLabel#hint { color: #9eafc3; font-size: 12px; }"
        "QLabel#workspaceBadge { color: #68d6d0; font-size: 11px; font-weight: 700; padding-right: 16px; }"
        "QLabel#documentTitle { font-size: 16px; font-weight: 600; }"
        "QLabel#propertyHeading { color: #68d6d0; font-size: 15px; font-weight: 600; }"
        "QWidget#participantCard { background: #222d3a; border-radius: 5px; }"
        "QStatusBar { border-top: 1px solid #354252; color: #acb9ca; }"
    );
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
