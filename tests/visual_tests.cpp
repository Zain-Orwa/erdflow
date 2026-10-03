// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
//
// The Azure programme's quality layer (ADR-022 section 9.12, UI-8): the Home
// screen and both workspaces photographed at the 1440 x 1080 reference and held
// to pictures kept beside this file, and an audit of what a person on the
// keyboard, or reading the screen, meets on Home.
//
// Pictures depend on the fonts a machine has, so they are kept per platform
// and release, under tests/visual/<system>-<major version>. A platform with no pictures of its own is
// told so and not compared, rather than compared against another's lettering.
// ERDFLOW_UPDATE_BASELINES=1 writes the pictures afresh; what was drawn, and
// where it differed, is always left in the build's visual-output folder.
#include "application/editor.hpp"
#include "app/desktop/home_demo_scenes.hpp"
#include "app/desktop/home_page.hpp"
#include "app/desktop/home_sidebar.hpp"
#include "app/desktop/main_window.hpp"
#include "app/desktop/start_route_card.hpp"
#include "app/desktop/welcome_flow_illustration.hpp"
#include "infrastructure/project_store.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QElapsedTimer>
#include <QEnterEvent>
#include <QImage>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSysInfo>
#include <QToolButton>

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace erdflow;

namespace {
int failures = 0;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void settle_for(int milliseconds) {
    QElapsedTimer clock;
    clock.start();
    do {
        QApplication::processEvents(QEventLoop::AllEvents, 20);
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    } while (clock.elapsed() < milliseconds);
}

template <typename T>
T* child(QWidget& root, const char* name) {
    auto* found = root.findChild<T*>(QString::fromLatin1(name));
    require(found != nullptr, std::string("Missing ") + name);
    return found;
}

// The system and its major version, such as macos-26: fonts change between
// releases as well as between systems, so a picture is only compared on the
// release it was taken on.
QString platform() {
    return QSysInfo::productType() + QLatin1Char('-')
         + QSysInfo::productVersion().section(QLatin1Char('.'), 0, 0);
}

// Two pictures are the same when almost every pixel is. Anti-aliasing and
// sub-pixel rounding move a few edge pixels a little between runs and between
// point releases of the same fonts; a changed layout, colour or word moves a
// great many a lot. The limits sit between the two.
struct Difference {
    double share = 1.0;
    QImage map;
};

Difference compare(const QImage& was, const QImage& now) {
    Difference result;
    if (was.size() != now.size()) return result;
    const auto a = was.convertToFormat(QImage::Format_ARGB32);
    const auto b = now.convertToFormat(QImage::Format_ARGB32);
    result.map = QImage(a.size(), QImage::Format_ARGB32);
    result.map.fill(Qt::white);
    long long changed = 0;
    for (int y = 0; y < a.height(); ++y) {
        const auto* one = reinterpret_cast<const QRgb*>(a.constScanLine(y));
        const auto* other = reinterpret_cast<const QRgb*>(b.constScanLine(y));
        auto* mark = reinterpret_cast<QRgb*>(result.map.scanLine(y));
        for (int x = 0; x < a.width(); ++x) {
            const auto delta = std::max({std::abs(qRed(one[x]) - qRed(other[x])),
                                         std::abs(qGreen(one[x]) - qGreen(other[x])),
                                         std::abs(qBlue(one[x]) - qBlue(other[x]))});
            if (delta > 40) {
                ++changed;
                mark[x] = qRgb(220, 30, 30);
            } else {
                const auto grey = qGray(one[x]) / 3 + 170;
                mark[x] = qRgb(grey, grey, grey);
            }
        }
    }
    result.share = static_cast<double>(changed) / (static_cast<double>(a.width()) * a.height());
    return result;
}

void check_picture(const QString& name, const QImage& drawn) {
    const QDir output(QStringLiteral(ERDFLOW_VISUAL_OUTPUT));
    QDir().mkpath(output.path());
    drawn.save(output.filePath(name + ".png"));
    const QDir kept(QStringLiteral(ERDFLOW_VISUAL_BASELINES "/") + platform());
    const auto baseline = kept.filePath(name + ".png");
    if (qEnvironmentVariableIntValue("ERDFLOW_UPDATE_BASELINES") == 1) {
        QDir().mkpath(kept.path());
        require(drawn.save(baseline), "Could not write " + baseline.toStdString());
        std::cout << "WROTE " << baseline.toStdString() << '\n';
        return;
    }
    QImage was(baseline);
    if (was.isNull()) {
        std::cout << "NO BASELINE " << name.toStdString() << " for " << platform().toStdString()
                  << " -- not compared. Run with ERDFLOW_UPDATE_BASELINES=1 on this platform to keep one.\n";
        return;
    }
    const auto difference = compare(was, drawn);
    if (!difference.map.isNull()) difference.map.save(output.filePath(name + "-difference.png"));
    if (was.size() != drawn.size()) {
        std::cout << "FAIL " << name.toStdString() << ": drawn at a different size from its baseline\n";
        ++failures;
        return;
    }
    if (difference.share > 0.004) {
        std::cout << "FAIL " << name.toStdString() << ": " << difference.share * 100.0
                  << "% of pixels differ from the baseline; see "
                  << output.filePath(name + "-difference.png").toStdString() << '\n';
        ++failures;
        return;
    }
    std::cout << "PASS picture " << name.toStdString() << '\n';
}

// Everything that moves or depends on the machine is fixed first, so that the
// same build draws the same picture twice: the illustration and the live
// demos held still, the demos finished, and the caret not blinking.
void make_still(desktop::MainWindow& window) {
    auto* home = static_cast<desktop::HomePage*>(window.findChild<QWidget*>("homePage"));
    home->hero()->set_moving(false);
    home->set_demos_moving(false);
}

QImage photograph(QWidget& widget) { return widget.grab().toImage(); }

void audit_home(desktop::MainWindow& window) {
    auto* home = static_cast<desktop::HomePage*>(window.findChild<QWidget*>("homePage"));
    require(home && home->isVisible(), "Home is in front");

    // Tab reaches every control, in the order they are read: the bar, the rail
    // top to bottom, the card that can be taken and its own + Create, then the
    // panel. Nothing is asked under the cards (ADR-022 9.19).
    const QStringList read_in_order{
        "appTopBarTheme",
        "homeNavHome", "homeNavOpenProject", "homeNavRecent",
        "homeNavExamples", "homeNavTemplates", "homeNavImport", "homeNavSettings", "homeNavHelp",
        "startRouteConceptual", "startRouteConceptualCreate",
        "homeLinkTutorials"};
    QStringList reached;
    auto* start = child<QWidget>(*home, "appTopBarTheme");
    auto* at = start;
    for (int step = 0; step < 200; ++step) {
        if (at->isVisible() && at->isEnabled() && (at->focusPolicy() & Qt::TabFocus)
            && home->isAncestorOf(at) && read_in_order.contains(at->objectName())
            && !reached.contains(at->objectName()))
            reached << at->objectName();
        at = at->nextInFocusChain();
        if (at == start) break;
    }
    require(reached == read_in_order,
            "Tab reaches every Home control in reading order; it reached: "
                + reached.join(", ").toStdString());

    // A focused control shows it. Drawn with and without focus, the pictures
    // must differ -- the ring is the only thing that changed.
    for (const char* name : {"startRouteConceptual", "startRouteConceptualCreate", "homeNavOpenProject",
                             "homeLinkTutorials", "appTopBarTheme"}) {
        auto* control = child<QWidget>(*home, name);
        window.setFocus();
        settle_for(30);
        const auto without = photograph(*control);
        control->setFocus(Qt::TabFocusReason);
        settle_for(30);
        require(control->hasFocus(), std::string(name) + " takes focus");
        const auto with = photograph(*control);
        require(compare(without, with).share > 0.002,
                std::string(name) + " shows where the keyboard is");
    }

    // Every card and row says what it is to whatever reads the screen, and a
    // card that cannot be taken says why without needing a pointer.
    for (auto* card : home->cards()) {
        require(!card->accessibleName().isEmpty() && !card->accessibleDescription().isEmpty(),
                "Every card has a name and a description");
        if (!card->isEnabled())
            require(!card->toolTip().isEmpty(), "A card that cannot be taken says why");
    }
    for (const auto& row : desktop::home_navigation())
        require(!home->sidebar()->button(row.section)->accessibleName().isEmpty(),
                "Every sidebar row has an accessible name");

    // Ordinary text reads at 4.5:1 or better on what it sits on, in Azure.
    const auto& t = desktop::tokens(desktop::ThemeId::Azure);
    struct Pair { const char* what; QColor ink, surface; };
    const std::vector<Pair> pairs{
        {"body text on a surface", t.text_primary, t.surface},
        {"secondary text on a surface", t.text_secondary, t.surface},
        {"muted text on a surface", t.text_muted, t.surface},
        {"muted text on the learning panel", t.text_muted, t.learning_surface},
        {"the subtitle", QColor("#526981"), t.surface},
        {"a card's body on a chosen card", t.text_secondary, t.selected_card_surface},
        {"a chosen sidebar row", Qt::white, desktop::chosen_row_fill(t)},
        {"a card's + Create", desktop::readable_on(desktop::chosen_row_fill(t)), desktop::chosen_row_fill(t)},
        {"a learning link", desktop::legible_on(t.primary, t.learning_surface), t.learning_surface},
        {"the form's complaint", desktop::legible_on(t.red, t.surface), t.surface},
        {"Coming soon", QColor("#8A5B00"), t.gold_soft},
        {"a sidebar row", t.text_primary, t.sidebar_surface},
        {"a hovered sidebar row", t.text_primary, t.hover_surface},
    };
    for (const auto& pair : pairs)
        require(desktop::contrast_ratio(pair.ink, pair.surface) >= 4.5,
                std::string("Contrast of ") + pair.what + " is at least 4.5:1, it is "
                    + std::to_string(desktop::contrast_ratio(pair.ink, pair.surface)));
    // And the brand primary itself is untouched where it is not lettering.
    require(t.primary == QColor("#1E88E5"), "Azure's primary stays #1E88E5");
    std::cout << "PASS accessibility audit of Home\n";
}
} // namespace

int main(int argc, char** argv) {
    // The illustration reads this when it is made, so it is set before anything is.
    qputenv("ERDFLOW_REDUCED_MOTION", "1");
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("ERDFlowVisualTests");
    QCoreApplication::setApplicationName("ERDFlowVisualTests");
    QSettings().clear();
    // Started as the application starts: the Fusion style and the theme a
    // fresh profile is given, applied before any window is built.
    QApplication::setStyle("Fusion");
    desktop::apply_theme(app, desktop::theme_from_key(desktop::default_theme_key()));
    // A caret that blinks is in some pictures and not in others.
    QApplication::setCursorFlashTime(0);
    try {
        infrastructure::QtIdGenerator ids;
        application::Editor editor(ids);
        infrastructure::ErdxProjectStore store;
        desktop::MainWindow window(editor, store, ids);
        window.set_icon_mode(desktop::IconMode::Outline);
        window.set_theme(desktop::ThemeId::Azure);
        // The reference viewport the comparisons are made at (ADR-022 9.6).
        window.resize(1440, 1080);
        window.show();
        window.activateWindow();
        settle_for(300);
        make_still(window);
        settle_for(100);

        audit_home(window);

        auto* home = static_cast<desktop::HomePage*>(window.findChild<QWidget*>("homePage"));
        home->cards().front()->create_button()->setFocus(Qt::OtherFocusReason);
        settle_for(100);
        check_picture("home", photograph(window));

        // The illustration on its own at the size it is authored at, still:
        // the database, its platform, the four panels and their lines.
        {
            desktop::WelcomeFlowIllustration flow(nullptr);
            flow.set_moving(false);
            flow.resize(520, 280);
            check_picture("hero", photograph(flow));
        }

        // The card's states, each drawn on its own at the width the line gives
        // it: at rest, under the pointer, chosen, and not yet available.
        const auto card_size = home->cards().front()->size();
        const auto card_picture = [&](const desktop::StartRouteDefinition& what, bool chosen,
                                      bool pointed_at) {
            desktop::StartRouteCard card(what, nullptr);
            // A card that holds a live demo on Home is drawn holding it here,
            // finished, as the Home screen stands still for its picture.
            const auto* on_home = [&]() -> const desktop::StartRouteCard* {
                for (auto* one : home->cards())
                    if (one->route() == what.route) return one;
                return nullptr;
            }();
            desktop::HomeLiveDemo* demo = nullptr;
            for (auto* one : home->demos())
                if (on_home && one->parentWidget() == on_home) {
                    demo = new desktop::HomeLiveDemo(one->kind());
                    demo->show_step(desktop::demo_finished_step(one->kind()), 0.0);
                    demo->set_real_canvas(one->real_canvas());
                    card.set_live_demo(demo);
                }
            card.resize(card_size);
            card.setChecked(chosen);
            if (pointed_at) {
                QEnterEvent into(QPointF(20, 20), QPointF(20, 20), QPointF(20, 20));
                QApplication::sendEvent(&card, &into);
            }
            return photograph(card);
        };
        const auto& routes = desktop::start_routes();
        check_picture("card-rest", card_picture(routes[0], false, false));
        check_picture("card-hover", card_picture(routes[0], false, true));
        check_picture("card-selected", card_picture(routes[0], true, false));
        // SQL Project, a card still coming.
        check_picture("card-coming-soon", card_picture(routes[2], false, false));

        // A sidebar row under the pointer.
        auto* row = home->sidebar()->button(desktop::HomeSection::OpenProject);
        row->setAttribute(Qt::WA_UnderMouse, true);
        row->update();
        settle_for(30);
        check_picture("sidebar-hover", photograph(*row));
        row->setAttribute(Qt::WA_UnderMouse, false);

        // The two workspaces under Azure, on the bundled example.
        window.load_example();
        settle_for(400);
        check_picture("conceptual", photograph(window));
        window.open_schema(true);
        settle_for(900);
        check_picture("relational-design", photograph(window));

        editor.mark_saved(editor.revision());
    } catch (const std::exception& error) {
        std::cerr << "Visual test failed: " << error.what() << '\n';
        return 1;
    }
    if (failures > 0) {
        std::cerr << failures << " picture(s) differ from their baselines\n";
        return 1;
    }
    std::cout << "Visual tests passed\n";
    return 0;
}
