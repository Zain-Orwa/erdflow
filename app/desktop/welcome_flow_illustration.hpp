// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "app/desktop/theme.hpp"

#include <QColor>
#include <QElapsedTimer>
#include <QPixmap>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QSizeF>
#include <QString>
#include <QTimer>
#include <QTransform>
#include <QWidget>

#include <cstddef>
#include <functional>
#include <vector>

class QPainter;

namespace erdflow::desktop {

// The mark drawn on a panel that circles the database. Read before anything
// written on it is: an entity, a diamond and an entity; a little hierarchy of
// boxes; a table; a page of SQL.
enum class HeroIcon { Conceptual, Structure, Relational, Sql, None };

// One of the panels. Everything about it is given rather than built in, so the
// same drawing serves the home screen, a marketing page and anything else that
// wants it: the component is not a picture of this screen, it is a component.
//
// The drawing is authored at 520 x 280 and painted into whatever it is given,
// always at one scale on both axes, so the figures below are in those units.
struct HeroOrbitItem {
    QString id;
    HeroIcon icon = HeroIcon::None;
    QString title;
    QString subtitle;          // optional; written under the title where given
    QColor accent;
    bool visible = true;
    // Where on the orbit the panel starts, as a fraction of a turn: 0 to the
    // database's right, 0.25 in front of it, 0.5 to its left, 0.75 behind it.
    // Every panel then travels round at the same speed, so these are also how
    // far apart the panels stay.
    double orbit_phase = 0.0;
    // The orbit, an ellipse round the database's centre. Left at zero, the
    // panel shares the one orbit every product panel travels.
    double orbit_radius_x = 0.0;
    double orbit_radius_y = 0.0;
    // Upright and a little taller than wide, like a card stood on its edge.
    // Scaled only ever as a whole, never on one axis.
    QSizeF size{78.0, 96.0};
    // Whether the title is written on the panel. Where it is not, the mark
    // says it.
    bool labelled = true;
    // Anything else the panel is to show (a picture, a notice, a feature),
    // drawn by the caller in place of the mark. It is given the panel's face
    // upright, in the authored units above; the painter already carries the
    // panel's place, depth and lean, and is clipped to its rounded shape, so
    // nothing drawn can spill past it. The orbit, the lines and the database
    // never see what is drawn here. Left empty, the mark is drawn.
    std::function<void(QPainter&, const QRectF&)> draw;
    // What pressing the panel does. Left empty, as the product's own panels
    // leave it, the panel is decoration and a press passes through it.
    std::function<void()> on_press;
};

// The ERDFlow illustration: a database on a platform, with the stages of the
// work on panels that travel round it.
//
// Drawn rather than loaded. A picture of the interface would be a picture of
// one moment of one version of it -- wrong the first time a colour changes,
// and unusable at any other size. Every part of this is vector, in the theme's
// own tokens, at whatever size it is given.
//
// Qt Widgets throughout. The specification allows a contained QML island as a
// last resort; it is not needed, and ERDFlow's shell stays what it is.
class WelcomeFlowIllustration final : public QWidget {
public:
    // One whole revolution of the panels round the database, at a constant
    // speed: twenty degrees a second (Zain, 2026-09-24).
    static constexpr double revolution_seconds = 18.0;

    explicit WelcomeFlowIllustration(QWidget* parent = nullptr);

    void wear(ThemeId id);
    // The light the platform, the lines and the database are lit with, and
    // the tube that runs to them across the page. White on a light page; on a
    // dark one a pale tint of the theme's own accent, so they glow in the
    // theme's colour rather than burning white against it (Zain, 2026-10-06).
    [[nodiscard]] static QColor hero_light(ThemeId id);
    // The colours the whole drawing is made of, worked out once for a theme
    // (Zain, 2026-10-06). On a light page they are the drawing's own whites
    // and blues, exactly as they always were; on a dark one its whites become
    // the accent's own light, and the panels the theme's surface raised
    // towards a tone of their own, so nothing in it stands on the page as
    // white paper from some other palette.
    struct Palette {
        bool dark = false;
        QColor accent;  // the drawing's blue
        QColor light;   // what it is lit with: white, or the accent's pale tint
        QColor raised;  // a panel's surface before its tone, on a dark page
        QColor word;    // SQL, lettered on its panel
        // The database and the platform it stands on, part by part (Zain,
        // 2026-10-07), so its tiers, seams and lids read apart in every
        // theme: on a light page the drawing's own blues and whites; on a dark
        // one fixed steps of lightness in the accent's hue, its saturation
        // capped, so a neon, white or grey accent cannot melt it into one mass.
        struct Database {
            QColor side[3][4];      // each tier's wall, left to right
            QColor lid_top, lid_foot, lid_edge;
            QColor seam;            // the foot of each tier, and its outline
            QColor shadow;          // its shadow on the platform
            QColor plate_under, plate_top, plate_foot, plate_rim, plate_inner;
            QColor glow;            // the light the platform casts beneath it
        } database;
    };
    [[nodiscard]] static Palette palette_for(ThemeId id);

    // What the panels say. Passing none puts back the four the product uses.
    void show_items(std::vector<HeroOrbitItem> items);
    [[nodiscard]] const std::vector<HeroOrbitItem>& items() const { return items_; }

    // The ready-made sets. The first is what the home screen shows: four
    // panels, read by their marks. The second exists to prove the component
    // is not built for one caller, and to be there when a second wants it.
    [[nodiscard]] static std::vector<HeroOrbitItem> product_cards(const Tokens& t);
    [[nodiscard]] static std::vector<HeroOrbitItem> marketing_cards(const Tokens& t);

    // Whether the drawing moves.
    //
    // Motion is a courtesy, never a carrier of meaning: everything the
    // illustration says, it says standing still. So it can always be stopped,
    // and stopping it loses nothing. Nothing else stops it: pointing at a
    // panel highlights it and the orbit carries on.
    void set_moving(bool on);
    [[nodiscard]] bool moving() const { return moving_; }
    // What the machine asks for, where it can be asked. Honoured when the
    // illustration is first made; an explicit `set_moving` afterwards wins,
    // because somebody who says so outranks a guess about them.
    [[nodiscard]] static bool platform_prefers_stillness();

    // How long the orbit has been running, in seconds. Set it to look at any
    // moment of the revolution without waiting for it -- which is what the
    // tests do, and what a still picture of a given moment would do.
    void set_clock(double seconds);
    [[nodiscard]] double clock() const { return seconds_; }
    // Where a panel is round the orbit now, in degrees from nought to 360,
    // measured as `orbit_phase` is.
    [[nodiscard]] double angle_of_card(std::size_t which) const;
    // How far round the orbit a panel is now, from nothing to one.
    [[nodiscard]] double turn_of(std::size_t which) const { return angle_of_card(which) / 360.0; }

    // Which panel the pointer currently rests on, or none. For tests, which
    // cannot point a real mouse and read the answer off the drawing.
    [[nodiscard]] int hovered_card() const { return under_pointer_; }

    // Where things are at this moment, in the widget's own pixels. For tests,
    // and for anything that wants to point at a panel.
    //
    // The centre every panel travels round, which is the database's own and
    // does not move.
    [[nodiscard]] QPointF orbit_centre() const;
    // How many pixels one authored unit is at the present size, the same
    // across as down.
    [[nodiscard]] double drawing_scale() const;
    // The ellipse a panel travels, in authored units: its own if it was given
    // one, otherwise the one the product's panels share.
    [[nodiscard]] QSizeF orbit_radii(std::size_t which) const;
    [[nodiscard]] QPointF card_centre(std::size_t which) const;
    // The panel's outline as drawn, leaning towards the database as it does.
    [[nodiscard]] QPolygonF card_outline(std::size_t which) const;
    // The line to a panel, from its end on the panel to its end on the
    // platform: four points and so three straight segments with two bends. The
    // first and last segments point the same way and are the same length.
    [[nodiscard]] QPolygonF connector(std::size_t which) const;
    // On the platform, or, where the line passes behind the database, at the
    // point where it disappears behind it.
    [[nodiscard]] QPointF connector_start(std::size_t which) const;
    [[nodiscard]] QPointF connector_end(std::size_t which) const;    // on the panel
    // How near the front of the orbit a panel is, from nothing (behind the
    // database) to one (in front of it).
    [[nodiscard]] double card_depth(std::size_t which) const;
    // Whether a panel is behind the database now, and so drawn before it.
    [[nodiscard]] bool card_behind(std::size_t which) const;

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    // The clock runs only while the drawing is on screen. A home screen behind
    // a workspace must not be repainting thirty times a second for nobody.
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    // A panel given something to do on a press does it when pressed and let
    // go over that same panel. Any other press passes through.
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    // The glow and the platform, which do not move, drawn once for each size
    // and theme and kept, so a frame costs only what moves.
    void draw_backdrop(QPainter& painter) const;
    void draw_connectors(QPainter& painter) const;
    void draw_database(QPainter& painter) const;
    void draw_card(QPainter& painter, std::size_t which) const;
    void draw_mark(QPainter& painter, const HeroOrbitItem& item, const QRectF& card) const;
    [[nodiscard]] const QPixmap& backdrop() const;
    // The shown panel at a point in the widget, front-most first, or -1.
    [[nodiscard]] int card_at(QPointF at) const;

    // Everything below is in authored units unless it says otherwise.
    [[nodiscard]] double radians_of(std::size_t which) const;
    [[nodiscard]] double radius_x(std::size_t which) const;
    [[nodiscard]] double radius_y(std::size_t which) const;
    [[nodiscard]] QPointF authored_centre(std::size_t which) const;
    [[nodiscard]] double lean_of(std::size_t which) const;
    [[nodiscard]] double scale_of(std::size_t which) const;
    [[nodiscard]] QTransform card_transform(std::size_t which) const; // panel space -> widget
    [[nodiscard]] QTransform stage_transform() const;                 // authored -> widget
    [[nodiscard]] QPointF platform_point_towards(std::size_t which) const;
    // How much of a line's last run, which ends at `to` and runs along
    // `unit`, is hidden behind the database, up to `limit`. In widget pixels.
    [[nodiscard]] double hidden_length(QPointF to, QPointF unit, double limit) const;
    // Where on the panel's edge the line leaves, in panel space, and how much
    // of that edge faces sideways rather than up or down (one for a side
    // edge, nought for the top or foot), which decides where the line bends.
    struct Anchor { QPointF at; double sideways; };
    [[nodiscard]] Anchor anchor_of(std::size_t which) const;
    [[nodiscard]] QColor hero_blue() const;

    ThemeId theme_ = ThemeId::Azure;
    Palette palette_ = palette_for(ThemeId::Azure);
    // A panel's own tone: the accent on a light page; on a dark one the
    // accent turned a little one way or the other, so the panels stay told
    // apart by colour as well as by mark.
    [[nodiscard]] QColor tone_of(const HeroOrbitItem& item) const;
    // Whether a panel asked for the theme's gold. Not where the gold is the
    // accent itself, as in High Contrast, or every panel would read as one.
    [[nodiscard]] bool warm(const HeroOrbitItem& item) const;
    std::vector<HeroOrbitItem> items_;
    bool moving_ = true;
    // How long the orbit has run. Advanced from a monotonic clock rather than
    // by counting ticks, so a late tick never slows the revolution down.
    double seconds_ = 0.0;
    QElapsedTimer since_;
    double seconds_at_start_ = 0.0;
    QTimer* clock_ = nullptr;
    int under_pointer_ = -1;
    int pressed_ = -1;
    // The kept drawing of what does not move, and what it was drawn for.
    mutable QPixmap backdrop_;
    mutable QSize backdrop_size_;
    mutable ThemeId backdrop_theme_ = ThemeId::Azure;
    mutable qreal backdrop_ratio_ = 0;
};

} // namespace erdflow::desktop
