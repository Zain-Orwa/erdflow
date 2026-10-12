// Read this before changing anything: CLAUDE.md at the repository root.
//
// Measurements for the pre-release milestone (2026-10-12): how the Relational
// Schema's lines are shaped, how long the canvases take to drag, search and
// export, whether an export carries halos, and how a rubber band behaves when
// the view scrolls under it. It changes nothing in a project and is not one of
// the CTest suites; it is run by hand, before and after each change, and its
// numbers are compared.

#include "app/desktop/diagram_view.hpp"
#include "app/desktop/main_window.hpp"
#include "app/desktop/picture_export.hpp"
#include "app/desktop/schema_view.hpp"
#include "app/desktop/theme.hpp"
#include "application/editor.hpp"
#include "infrastructure/project_store.hpp"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QImage>
#include <QMouseEvent>
#include <QScrollBar>
#include <QSettings>
#include <algorithm>
#include <cmath>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <vector>

using namespace erdflow;

namespace {

void settle() {
    QApplication::processEvents();
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}
void settle_ms(int ms) {
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < ms) {
        QApplication::processEvents(QEventLoop::AllEvents, 5);
        settle();
    }
}
double since(const QElapsedTimer& clock) { return static_cast<double>(clock.nsecsElapsed()) / 1e6; }

// The shape of every line, measured against what a clean orthogonal route is:
// no point that is not a corner, no run shorter than a few pixels between two
// turns, no run that doubles back on the one before the last, and nothing
// crossing a table that is not one of its own ends.
struct RouteAudit {
    int lines = 0;
    int segments = 0;
    int corners = 0;
    int not_orthogonal = 0;
    int zero_length = 0;
    int collinear_points = 0;
    int short_inner_runs = 0;   // a run under 10 px between two turns
    int z_jogs = 0;             // ... after which the line carries on the way it was going: a sideways step
    int hooks = 0;              // a run going back the way the run two before it came, after a run under 18 px
    int detours = 0;            // the same with a longer run between: possibly a way round a table
    int end_jogs = 0;           // a short run among the two at either end: a stub
    int reversals = 0;          // hooks and detours together
    int short_end_stubs = 0;    // an end run shorter than the marks drawn on it need
    int table_crossings = 0;
    int lines_with_defect = 0;
};

RouteAudit audit_routes(const std::vector<std::vector<QPointF>>& lines, const std::vector<QRectF>& boxes,
                        double end_reach) {
    RouteAudit audit;
    const auto direction = [](QPointF from, QPointF to) {
        const auto d = to - from;
        if (std::abs(d.x()) < 0.01 && std::abs(d.y()) < 0.01) return 0;
        if (std::abs(d.y()) < 0.01) return d.x() > 0 ? 1 : 2;
        if (std::abs(d.x()) < 0.01) return d.y() > 0 ? 3 : 4;
        return 5;
    };
    const auto opposite = [](int a, int b) { return (a == 1 && b == 2) || (a == 2 && b == 1) || (a == 3 && b == 4) || (a == 4 && b == 3); };
    for (const auto& line : lines) {
        if (line.size() < 2) continue;
        ++audit.lines;
        const auto before = audit;
        audit.segments += static_cast<int>(line.size()) - 1;
        audit.corners += std::max(0, static_cast<int>(line.size()) - 2);
        std::vector<int> dirs;
        std::vector<double> lengths;
        for (std::size_t i = 1; i < line.size(); ++i) {
            const auto d = direction(line[i - 1], line[i]);
            if (d == 0) ++audit.zero_length;
            if (d == 5) ++audit.not_orthogonal;
            dirs.push_back(d);
            lengths.push_back(std::hypot(line[i].x() - line[i - 1].x(), line[i].y() - line[i - 1].y()));
        }
        for (std::size_t i = 1; i < dirs.size(); ++i)
            if (dirs[i] != 0 && dirs[i] == dirs[i - 1]) ++audit.collinear_points;
        for (std::size_t i = 1; i + 1 < lengths.size(); ++i)
            if (lengths[i] < 10.0 && dirs[i] != 0) {
                ++audit.short_inner_runs;
                if (dirs[i - 1] == dirs[i + 1]) ++audit.z_jogs;
                if (i <= 2 || i + 3 >= lengths.size()) ++audit.end_jogs;
            }
        for (std::size_t i = 2; i < dirs.size(); ++i)
            if (opposite(dirs[i], dirs[i - 2])) {
                ++audit.reversals;
                if (lengths[i - 1] < 18.0) ++audit.hooks; else ++audit.detours;
            }
        if (lengths.size() >= 2) {
            if (lengths.front() + 0.5 < end_reach) ++audit.short_end_stubs;
            if (lengths.back() + 0.5 < end_reach) ++audit.short_end_stubs;
        }
        // Tables a line ends on are its own; any other it passes through is crossed.
        std::vector<bool> own(boxes.size(), false);
        for (std::size_t t = 0; t < boxes.size(); ++t) {
            const auto near = [&](QPointF p) { return boxes[t].adjusted(-1.5, -1.5, 1.5, 1.5).contains(p); };
            own[t] = near(line.front()) || near(line.back());
        }
        for (std::size_t i = 1; i < line.size(); ++i) {
            const QRectF run = QRectF(line[i - 1], line[i]).normalized().adjusted(-0.5, -0.5, 0.5, 0.5);
            for (std::size_t t = 0; t < boxes.size(); ++t)
                if (!own[t] && run.intersects(boxes[t].adjusted(2, 2, -2, -2))) ++audit.table_crossings;
        }
        const bool defect = audit.collinear_points > before.collinear_points || audit.short_inner_runs > before.short_inner_runs
                         || audit.hooks > before.hooks || audit.short_end_stubs > before.short_end_stubs
                         || audit.table_crossings > before.table_crossings || audit.not_orthogonal > before.not_orthogonal
                         || audit.zero_length > before.zero_length;
        if (defect) ++audit.lines_with_defect;
    }
    return audit;
}

void print_audit(const std::string& name, const RouteAudit& a) {
    std::cout << "ROUTES " << name << ": lines=" << a.lines << " segments=" << a.segments << " corners=" << a.corners
              << " lines_with_defect=" << a.lines_with_defect << " | collinear=" << a.collinear_points
              << " short_runs=" << a.short_inner_runs << " z_jogs=" << a.z_jogs << " end_jogs=" << a.end_jogs
              << " hooks=" << a.hooks << " detours=" << a.detours
              << " short_end_stubs=" << a.short_end_stubs << " table_crossings=" << a.table_crossings
              << " not_orthogonal=" << a.not_orthogonal << " zero_length=" << a.zero_length << '\n';
}

void send_mouse(QWidget* to, QEvent::Type type, QPointF at, Qt::MouseButton button, Qt::MouseButtons buttons,
                Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    QMouseEvent event(type, at, to->mapToGlobal(at), button, buttons, modifiers);
    QApplication::sendEvent(to, &event);
}

// A table dragged by its heading, and then everything dragged together.
void schema_drags(const std::string& name, desktop::SchemaView& schema) {
    const auto boxes = schema.table_boxes();
    if (boxes.empty()) return;
    const auto drag = [&](const std::string& what) {
        const auto grip = boxes.front().topLeft() + QPointF(24, 10);
        const auto routed_before = schema.routings();
        send_mouse(&schema, QEvent::MouseButtonPress, grip, Qt::LeftButton, Qt::LeftButton);
        settle();
        QElapsedTimer clock;
        clock.start();
        constexpr int moves = 40;
        for (int i = 1; i <= moves; ++i) {
            send_mouse(&schema, QEvent::MouseMove, grip + QPointF(i * 3.0, i * 1.5), Qt::NoButton, Qt::LeftButton);
            settle();
        }
        const auto moving = since(clock);
        send_mouse(&schema, QEvent::MouseButtonRelease, grip + QPointF(moves * 3.0, moves * 1.5), Qt::LeftButton, Qt::NoButton);
        settle();
        std::cout << "SCHEMA_DRAG " << name << " " << what << ": per_move_ms=" << moving / moves
                  << " routings=" << schema.routings() - routed_before << '\n';
    };
    schema.select(std::nullopt);
    drag("one table");
    schema.select_all();
    settle();
    drag("select all");
    schema.select(std::nullopt);
    settle();
}

void schema_search(const std::string& name, desktop::SchemaView& schema) {
    for (const auto* typed : {"S", "St", "Stu", "x", ""}) {
        QElapsedTimer clock;
        clock.start();
        schema.set_looking_for(QString::fromLatin1(typed));
        schema.repaint();
        std::cout << "SCHEMA_SEARCH " << name << " \"" << typed << "\": ms=" << since(clock) << '\n';
    }
}

// What a selected table does to the pixels around its lines, outside every
// table: the colour that appears is the colour the selection rings it with.
void selection_ring(const std::string& name, desktop::MainWindow& window, desktop::SchemaView& schema) {
    const auto boxes = schema.table_boxes();
    if (boxes.empty()) return;
    schema.select(std::nullopt);
    settle();
    const auto plain = schema.grab().toImage().convertToFormat(QImage::Format_ARGB32);
    const auto table = schema.preview().tables.front().origin;
    if (!table) return;
    schema.select(*table);
    settle();
    const auto selected = schema.grab().toImage().convertToFormat(QImage::Format_ARGB32);
    schema.select(std::nullopt);
    settle();
    std::map<QRgb, int> appeared;
    const auto ratio = plain.width() / std::max(1.0, static_cast<double>(schema.width()));
    for (int y = 0; y < std::min(plain.height(), selected.height()); ++y)
        for (int x = 0; x < std::min(plain.width(), selected.width()); ++x) {
            const QPointF at(x / ratio, y / ratio);
            if (std::any_of(boxes.begin(), boxes.end(), [&](const QRectF& b) { return b.adjusted(-16, -16, 16, 16).contains(at); })) continue;
            if (plain.pixel(x, y) != selected.pixel(x, y)) ++appeared[selected.pixel(x, y) | 0xff000000u];
        }
    std::vector<std::pair<int, QRgb>> ranked;
    for (const auto& [colour, count] : appeared) ranked.emplace_back(count, colour);
    std::sort(ranked.rbegin(), ranked.rend());
    std::cout << "SELECTION_RING " << name << ": text_colour=" << window.palette().color(QPalette::Text).name().toStdString()
              << " changed_pixels_outside_tables=";
    int total = 0;
    for (const auto& [count, colour] : ranked) total += count;
    std::cout << total << " top_colours=";
    for (std::size_t i = 0; i < std::min<std::size_t>(ranked.size(), 3); ++i)
        std::cout << QColor(ranked[i].second).name().toStdString() << "x" << ranked[i].first << ' ';
    std::cout << '\n';
}

// Opaque pixels of the canvas colour standing outside every table in an export
// on a transparent background: nothing but a line's knockout puts them there.
void export_checks(const std::string& name, desktop::MainWindow& window, desktop::ThemeId theme,
                   const QColor& canvas, const QString& folder, bool schema_in_front) {
    window.set_theme(theme);
    settle();
    const auto tag = QString::fromStdString(name) + (theme == desktop::ThemeId::OfficeLight ? "-light" : "-dark");
    for (const auto background : {desktop::PictureBackground::Transparent, desktop::PictureBackground::ThemeColour}) {
        desktop::PictureOptions png;
        png.format = desktop::PictureFormat::Png;
        png.background = background;
        png.carry_project = false;
        const auto path = folder + "/" + tag + (background == desktop::PictureBackground::Transparent ? "-transparent.png" : "-theme.png");
        QElapsedTimer clock;
        clock.start();
        const bool ok = window.export_picture(png, path);
        const auto took = since(clock);
        long halo = 0;
        if (ok && background == desktop::PictureBackground::Transparent && schema_in_front) {
            const QImage image(path);
            const auto margin = png.margin;
            const auto source = window.schema()->export_view().bounds(desktop::PictureExtent::WholeDiagram)
                                    .adjusted(-margin, -margin, margin, margin);
            const auto boxes = window.schema()->table_boxes();
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x) {
                    const auto pixel = image.pixelColor(x, y);
                    if (pixel.alpha() < 200) continue;
                    if (std::abs(pixel.red() - canvas.red()) > 6 || std::abs(pixel.green() - canvas.green()) > 6
                        || std::abs(pixel.blue() - canvas.blue()) > 6) continue;
                    const QPointF at(source.left() + x, source.top() + y);
                    if (std::any_of(boxes.begin(), boxes.end(), [&](const QRectF& b) { return b.adjusted(-30, -30, 30, 30).contains(at); })) continue;
                    ++halo;
                }
        }
        std::cout << "EXPORT_PNG " << tag.toStdString() << (background == desktop::PictureBackground::Transparent ? " transparent" : " theme")
                  << ": ok=" << ok << " ms=" << took;
        if (background == desktop::PictureBackground::Transparent && schema_in_front)
            std::cout << " canvas_coloured_pixels_outside_tables=" << halo;
        std::cout << '\n';
    }
    desktop::PictureOptions pdf;
    pdf.format = desktop::PictureFormat::Pdf;
    pdf.background = desktop::PictureBackground::ThemeColour;
    pdf.carry_project = false;
    const auto path = folder + "/" + tag + ".pdf";
    QElapsedTimer clock;
    clock.start();
    const bool ok = window.export_picture(pdf, path);
    std::cout << "EXPORT_PDF " << tag.toStdString() << ": ok=" << ok << " ms=" << since(clock)
              << " bytes=" << QFileInfo(path).size() << " file=" << path.toStdString() << '\n';
}

// Whether a rubber band keeps its first corner where it was on the diagram
// when the view scrolls under the pointer, or where it was in the window.
void rubber_band_anchor(desktop::DiagramView& canvas) {
    auto* viewport = canvas.viewport();
    QPoint empty;
    bool found = false;
    for (int y = 20; y < viewport->height() - 20 && !found; y += 15)
        for (int x = 20; x < viewport->width() / 2 && !found; x += 15)
            if (!canvas.itemAt(QPoint(x, y))) { empty = QPoint(x, y); found = true; }
    if (!found || !canvas.verticalScrollBar() || canvas.verticalScrollBar()->maximum() == 0) {
        std::cout << "RUBBER_BAND: could not set up (no empty spot or nothing to scroll)\n";
        return;
    }
    const auto origin_scene = canvas.mapToScene(empty);
    send_mouse(viewport, QEvent::MouseButtonPress, empty, Qt::LeftButton, Qt::LeftButton);
    const QPoint pointer = empty + QPoint(160, 120);
    send_mouse(viewport, QEvent::MouseMove, empty + QPoint(80, 60), Qt::NoButton, Qt::LeftButton);
    send_mouse(viewport, QEvent::MouseMove, pointer, Qt::NoButton, Qt::LeftButton);
    settle();
    const auto before = canvas.rubberBandRect();
    auto* bar = canvas.verticalScrollBar();
    const auto value = bar->value();
    bar->setValue(std::min(bar->maximum(), value + 150));
    const auto scrolled = bar->value() - value;
    send_mouse(viewport, QEvent::MouseMove, pointer, Qt::NoButton, Qt::LeftButton);
    settle();
    const auto after = canvas.rubberBandRect();
    // The band's corner away from the pointer is where it began.
    const auto corner = [&](const QRect& band) { return QPoint(band.left() == pointer.x() ? band.right() : band.left(),
                                                               band.top() == pointer.y() ? band.bottom() : band.top()); };
    // Mapped with the view as it is after the scroll: the band now starts at
    // the scene point the press was made on if it is anchored in the diagram.
    const auto began_after = canvas.mapToScene(corner(after));
    send_mouse(viewport, QEvent::MouseButtonRelease, pointer, Qt::LeftButton, Qt::NoButton);
    settle();
    std::cout << "RUBBER_BAND: scrolled_px=" << scrolled << " band_before=" << before.x() << ',' << before.y() << ' '
              << before.width() << 'x' << before.height() << " band_after=" << after.x() << ',' << after.y() << ' '
              << after.width() << 'x' << after.height() << " origin_scene=" << origin_scene.x() << ',' << origin_scene.y()
              << " band_start_scene_after_scroll=" << began_after.x() << ',' << began_after.y()
              << (std::hypot(began_after.x() - origin_scene.x(), began_after.y() - origin_scene.y()) < 2.0
                      ? "  => anchored in the diagram" : "  => moves with the window") << '\n';
}

void canvas_timings(const std::string& name, desktop::MainWindow& window) {
    auto* canvas = window.canvas();
    const auto elements = [&] {
        std::vector<domain::ElementRef> all;
        for (const auto& [id, entity] : window.editor().project().entities) { (void)entity; all.push_back(id); }
        return all;
    }();
    if (elements.empty()) return;
    const auto drag = [&](const std::string& what, const std::vector<domain::ElementRef>& chosen) {
        canvas->select_elements(chosen, true);
        settle();
        const auto& rect = window.editor().project().layout.at(chosen.front());
        const auto at = canvas->mapFromScene(QPointF(rect.x + rect.width / 2, rect.y + rect.height / 2));
        auto* viewport = canvas->viewport();
        send_mouse(viewport, QEvent::MouseButtonPress, at, Qt::LeftButton, Qt::LeftButton);
        settle();
        QElapsedTimer clock;
        clock.start();
        constexpr int moves = 40;
        for (int i = 1; i <= moves; ++i) {
            send_mouse(viewport, QEvent::MouseMove, at + QPoint(i * 3, i), Qt::NoButton, Qt::LeftButton);
            settle();
        }
        const auto moving = since(clock);
        send_mouse(viewport, QEvent::MouseButtonRelease, at + QPoint(moves * 3, moves), Qt::LeftButton, Qt::NoButton);
        settle();
        std::cout << "CANVAS_DRAG " << name << " " << what << ": per_move_ms=" << moving / moves << '\n';
    };
    drag("one entity", {elements.front()});
    std::vector<domain::ElementRef> everything;
    for (const auto& [ref, rect] : window.editor().project().layout) { (void)rect; everything.push_back(ref); }
    drag("select all (" + std::to_string(everything.size()) + ")", everything);
    canvas->select_elements({});
    settle();
    for (const auto* typed : {"E", "En", "Ent", "zz"}) {
        QElapsedTimer clock;
        clock.start();
        window.open_search(QString::fromLatin1(typed));
        settle();
        std::cout << "CANVAS_SEARCH " << name << " \"" << typed << "\": ms=" << since(clock) << '\n';
    }
    window.close_search();
    settle();
}

// A large diagram made the way a person makes one: entities with keys and
// attributes, joined one-to-many and many-to-many, so its schema has foreign
// keys and bridge tables to route.
void build_large(application::Editor& editor, int entities_wanted) {
    std::vector<domain::EntityId> entities;
    for (int i = 0; i < entities_wanted; ++i) {
        const double x = (i % 10) * 420.0;
        const double y = (i / 10) * 360.0;
        const auto made = editor.create_entity("Entity " + std::to_string(i), {x, y, 150, 80});
        const auto id = std::get<domain::EntityId>(*made.created);
        entities.push_back(id);
        for (int a = 0; a < 3; ++a) {
            const auto attribute = editor.create_attribute((a == 0 ? "Entity" + std::to_string(i) + " ID" : "Field " + std::to_string(a)),
                                                           {x - 40 + a * 90.0, y + 110, 120, 50}, domain::ElementRef{id});
            if (a == 0 && attribute.created)
                editor.set_attribute_kind(std::get<domain::AttributeId>(*attribute.created), domain::AttributeKind::Key);
        }
    }
    for (int i = 0; i + 1 < entities_wanted; ++i) {
        const auto first = entities[static_cast<std::size_t>(i)];
        const auto second = entities[static_cast<std::size_t>((i * 7 + 3) % entities_wanted)];
        if (first == second) continue;
        const double x = (i % 10) * 420.0 + 210;
        const double y = (i / 10) * 360.0 + 200;
        editor.relate(first, second, {x, y, 140, 80}, "Relates " + std::to_string(i));
    }
}

} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName("ERDFlowTests");
    QCoreApplication::setApplicationName("ERDFlowTests-PrereleaseBaseline");
    QSettings().clear();
    const auto folder = QDir::temp().filePath("erdflow-prerelease-baseline");
    QDir().mkpath(folder);
    std::cout << "QT_VERSION " << qVersion() << " platform=" << QGuiApplication::platformName().toStdString()
              << " output=" << folder.toStdString() << '\n';
    constexpr double end_reach = 28.0; // schema_view.cpp: minimum_at (21) + 7
    infrastructure::QtIdGenerator ids;

    struct Scenario {
        std::string name;
        std::function<void(desktop::MainWindow&)> load;
        std::function<void(application::Editor&)> build;
    };
    const std::vector<Scenario> scenarios{
        {"university", [](desktop::MainWindow& w) { w.load_example(); }, {}},
        {"company", [](desktop::MainWindow& w) { w.load_company_database(); }, {}},
        {"company-relational", [](desktop::MainWindow& w) { w.load_company_database_relational(); }, {}},
        {"large-80", {}, [](application::Editor& e) { build_large(e, 80); }},
    };
    for (const auto& scenario : scenarios) {
        application::Editor editor(ids);
        if (scenario.build) scenario.build(editor);
        infrastructure::ErdxProjectStore store;
        desktop::MainWindow window(editor, store, ids);
        window.resize(1440, 920);
        window.show();
        settle();
        window.show_home(false);
        if (scenario.load) scenario.load(window);
        settle_ms(600);
        const bool has_diagram = !window.editor().project().entities.empty();
        if (has_diagram && !window.editor().project().schema.standalone) {
            canvas_timings(scenario.name, window);
            if (scenario.name == "large-80") rubber_band_anchor(*window.canvas());
            export_checks(scenario.name + "-conceptual", window, desktop::ThemeId::OfficeLight,
                          desktop::theme(desktop::ThemeId::OfficeLight).canvas, folder, false);
            window.set_theme(desktop::ThemeId::OfficeLight);
        }
        window.open_schema(true);
        settle_ms(900);
        auto* schema = window.schema();
        QElapsedTimer clock;
        clock.start();
        const auto routed_before = schema->routings();
        schema->refresh();
        settle();
        std::cout << "SCHEMA_REFRESH " << scenario.name << ": ms=" << since(clock) << " tables=" << schema->table_boxes().size()
                  << " routings=" << schema->routings() - routed_before << '\n';
        print_audit(scenario.name, audit_routes(schema->line_shapes(), schema->table_boxes(), end_reach));
        schema_drags(scenario.name, *schema);
        print_audit(scenario.name + " after drags", audit_routes(schema->line_shapes(), schema->table_boxes(), end_reach));
        schema_search(scenario.name, *schema);
        schema->set_looking_for({});
        for (const auto theme : {desktop::ThemeId::OfficeLight, desktop::ThemeId::Dracula}) {
            window.set_theme(theme);
            settle();
            selection_ring(scenario.name + (theme == desktop::ThemeId::OfficeLight ? "-light" : "-dark"), window, *schema);
            export_checks(scenario.name + "-schema", window, theme, desktop::theme(theme).canvas, folder, true);
        }
        editor.mark_saved(editor.revision());
        window.close();
        settle();
    }
    std::cout << "BASELINE DONE\n";
    return 0;
}
