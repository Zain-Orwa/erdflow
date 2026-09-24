// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "main_window.hpp"
#include "ribbon.hpp"
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
    // A profile that has chosen keeps its choice; only one that never has is
    // given the new default. Somebody who settled on a theme must not find it
    // swapped out from under them because the shipped default moved.
    const auto chosen = erdflow::desktop::theme_from_key(
        settings.value("theme", erdflow::desktop::default_theme_key()).toString());
    erdflow::desktop::apply_theme(app, chosen);
    QCommandLineParser parser;
    parser.setApplicationDescription("ERDFlow conceptual ERD editor");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"example", "Open the bundled university example."});
    parser.addOption({"smoke-test", "Exit after initializing the window (for build verification)."});
    parser.addOption({"screenshot", "Save a window screenshot after initialization.", "path"});
    // Which ribbon row the screenshot should show. Development tooling beside
    // the screenshot option, so a row other than Home can be looked at without
    // a person having to click the tab first.
    parser.addOption({"tab", "Show a ribbon tab before the screenshot, such as tabExport.", "name"});
    // Opens the search bar already looking for something, so the search can be
    // seen without anyone having to find the shortcut first. Development
    // tooling beside the two above, and the quickest way to see what a search
    // does to a diagram.
    parser.addOption({"search", "Open the search bar looking for this text.", "text"});
    // Raises the schema panel, which is otherwise closed until somebody asks
    // for it. Development tooling beside the three above: it is the only way
    // to photograph the schema without a person clicking Preview first.
    parser.addOption({"schema", "Raise the Relational Design panel."});
    // Which of the four notations the ends are drawn in, so each can be
    // photographed without a person working the picker. Development tooling
    // beside the options above.
    parser.addOption({"notation", "Draw ends as chen, minmax, crowsfoot or bachman.", "name"});
    // Which appearance to wear, so a screenshot can be taken of any of them
    // without disturbing the remembered choice. Development tooling like the
    // options above it.
    parser.addOption({"theme", "Wear this theme, such as midnight, for this run only.", "key"});
    parser.addOption({"schema-full", "Raise Relational Design at full height."});
    // The window's size for a screenshot, such as 1440x1080 -- the reference
    // viewport the visual comparisons are made at (ADR-022 section 9.6).
    // Development tooling like the options above it.
    parser.addOption({"size", "Resize the window to WIDTHxHEIGHT, such as 1440x1080.", "size"});
    parser.addPositionalArgument("project", "An .erdx project to open.", "[project]");
    parser.process(app);
    const auto wearing = parser.isSet("theme")
        ? erdflow::desktop::theme_from_key(parser.value("theme"))
        : chosen;
    if (wearing != chosen) erdflow::desktop::apply_theme(app, wearing);
    erdflow::infrastructure::QtIdGenerator ids;
    erdflow::application::Editor editor(ids);
    erdflow::infrastructure::ErdxProjectStore store;
    erdflow::desktop::MainWindow window(editor, store, ids);
    window.set_icon_mode(erdflow::desktop::icon_mode_from_key(
        settings.value("iconMode", "outline").toString()));
    window.set_theme(wearing);
    if (parser.isSet("size")) {
        const auto parts = parser.value("size").toLower().split('x');
        if (parts.size() == 2) window.resize(parts[0].toInt(), parts[1].toInt());
    }
    window.show();
    QTimer::singleShot(0, &window, [&] {
        if (!parser.positionalArguments().isEmpty()) window.open_path(parser.positionalArguments().front());
        else if (parser.isSet("example")) window.load_example();
        if (parser.isSet("tab") && window.ribbon()) window.ribbon()->show_tab(parser.value("tab"));
        if (parser.isSet("search")) window.open_search(parser.value("search"));
        if (parser.isSet("notation")) {
            const auto wanted = parser.value("notation").toLower();
            if (wanted == "chen") window.set_notation(erdflow::desktop::Notation::Chen);
            else if (wanted == "minmax") window.set_notation(erdflow::desktop::Notation::MinMax);
            else if (wanted == "crowsfoot") window.set_notation(erdflow::desktop::Notation::CrowsFoot);
            else if (wanted == "bachman") window.set_notation(erdflow::desktop::Notation::Bachman);
        }
        if (parser.isSet("schema") || parser.isSet("schema-full"))
            window.open_schema(parser.isSet("schema-full"));
        QTimer::singleShot(900, &window, [&] {
            if (parser.isSet("screenshot") && !window.grab().save(parser.value("screenshot"))) {
                app.exit(1);
                return;
            }
            if (parser.isSet("smoke-test")) {
                // A smoke test never means to keep anything, and some of the
                // options above are real edits -- opening the example is one --
                // which would leave the document unsaved and the window asking
                // whether to save it on the way out. Nobody is there to answer,
                // so the run would hang rather than end.
                editor.mark_saved(editor.revision());
                app.quit();
            }
        });
    });
    return app.exec();
}
