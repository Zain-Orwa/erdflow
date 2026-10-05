// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/home_sidebar.hpp"

#include "app/desktop/icons.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QKeyEvent>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace erdflow::desktop {
namespace {
// The reference geometry. A row is 44 tall and sits 14 in from either side of
// a 206 rail; its icon is 20 across, 14 in from the row's own edge, with 12
// between it and the words.
constexpr int rail_width = 206;
constexpr int row_height = 44;
constexpr int outer_margin = 14;
constexpr int top_margin = 17;
constexpr int foot_margin = 20;
constexpr int rule_room = 17;
constexpr int icon_size = 20;
constexpr int icon_inset = 14;
constexpr int icon_gap = 12;

double luminance(const QColor& colour) {
    const auto channel = [](double value) {
        value /= 255.0;
        return value <= 0.03928 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(colour.red()) + 0.7152 * channel(colour.green())
         + 0.0722 * channel(colour.blue());
}

// One row: a button, drawn in the rail's three states.
class NavRow final : public QAbstractButton {
public:
    NavRow(const HomeNavigationDefinition& what, QWidget* parent)
        : QAbstractButton(parent), what_(what) {
        setObjectName(QString::fromLatin1(what.object_name));
        setText(QString::fromLatin1(what.label));
        setAccessibleName(text());
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
    }

    void wear(ThemeId id) {
        theme_ = id;
        const auto& t = tokens(id);
        // Drawn once for each state it can be in, when the theme is worn, so
        // the paint does no rendering of its own.
        resting_ = outline_pixmap(QString::fromLatin1(what_.icon), t.text_secondary, icon_size);
        hovered_ = outline_pixmap(QString::fromLatin1(what_.icon), t.primary, icon_size);
        chosen_ = outline_pixmap(QString::fromLatin1(what_.icon),
                                 readable_on(chosen_row_fill(t)), icon_size);
        update();
    }

    void set_chosen(bool on) {
        if (chosen_state_ == on) return;
        chosen_state_ = on;
        setAccessibleDescription(on ? QStringLiteral("Current page") : QString());
        update();
    }
    [[nodiscard]] bool chosen() const { return chosen_state_; }

protected:
    void paintEvent(QPaintEvent*) override {
        const auto& t = tokens(theme_);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRectF box(0.5, 0.5, width() - 1.0, height() - 1.0);
        const auto radius = t.radius_nav_item;
        const bool over = underMouse() && !chosen_state_;

        QColor ink = t.text_primary;
        const QPixmap* mark = &resting_;
        if (chosen_state_) {
            const auto fill = chosen_row_fill(t);
            // The subtle lift a chosen row has, drawn as a few widening washes
            // rather than a blur, which Qt Widgets cannot do cheaply.
            const auto& shade = t.shadow_selected_nav;
            for (int step = 5; step >= 1; --step) {
                auto wash = shade.ink;
                wash.setAlphaF(static_cast<float>(wash.alphaF() / (step * 3.0)));
                QPainterPath under;
                under.addRoundedRect(box.adjusted(-step * 0.6, step * 0.5, step * 0.6,
                                                  step * 0.6 + shade.dy / 4.0),
                                     radius + step, radius + step);
                painter.fillPath(under, wash);
            }
            QPainterPath shape;
            shape.addRoundedRect(box, radius, radius);
            painter.fillPath(shape, fill);
            ink = readable_on(fill);
            mark = &chosen_;
        } else if (over || isDown()) {
            QPainterPath shape;
            shape.addRoundedRect(box, radius, radius);
            painter.fillPath(shape, t.hover_surface);
            mark = &hovered_;
        }

        if (!mark->isNull())
            painter.drawPixmap(QPointF(icon_inset, (height() - icon_size) / 2.0), *mark);

        auto face = font();
        face.setFamilies(t.family);
        face.setPixelSize(static_cast<int>(t.nav.size));
        face.setWeight(static_cast<QFont::Weight>(t.nav.weight));
        painter.setFont(face);
        painter.setPen(ink);
        const auto words_left = icon_inset + icon_size + icon_gap;
        painter.drawText(QRectF(words_left, 0, width() - words_left - 8, height()),
                         Qt::AlignVCenter | Qt::AlignLeft, text());

        // Where the keyboard is, drawn inside the row so it never reads as the
        // chosen state and never runs into the row below.
        if (hasFocus()) {
            auto ring = chosen_state_ ? readable_on(chosen_row_fill(t)) : t.primary;
            ring.setAlphaF(0.75f);
            painter.setPen(QPen(ring, 1.5));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(box.adjusted(2, 2, -2, -2), radius - 2, radius - 2);
        }
    }

private:
    HomeNavigationDefinition what_;
    ThemeId theme_ = ThemeId::Azure;
    bool chosen_state_ = false;
    QPixmap resting_;
    QPixmap hovered_;
    QPixmap chosen_;
};
} // namespace

double contrast_ratio(const QColor& a, const QColor& b) {
    const auto one = luminance(a) + 0.05;
    const auto other = luminance(b) + 0.05;
    return std::max(one, other) / std::min(one, other);
}

QColor legible_on(const QColor& ink, const QColor& surface) {
    auto colour = ink;
    const bool dark_surface = luminance(surface) < 0.18;
    for (int step = 0; step < 40 && contrast_ratio(colour, surface) < 4.5; ++step)
        colour = dark_surface ? colour.lighter(106) : colour.darker(106);
    return colour;
}

QColor chosen_row_fill(const Tokens& t) {
    for (const auto& candidate : {t.primary, t.primary_hover, t.primary_pressed})
        if (contrast_ratio(candidate, readable_on(candidate)) >= 4.5) return candidate;
    return t.primary_pressed;
}

const std::array<HomeNavigationDefinition, home_section_count>& home_navigation() {
    static const std::array<HomeNavigationDefinition, home_section_count> rows{{
        {HomeSection::Home, HomeGroup::Start, "homeNavHome", "homeActionHome", "Home", "house"},
        {HomeSection::OpenProject, HomeGroup::Start, "homeNavOpenProject", "homeActionOpenProject",
         "Open Project", "open"},
        {HomeSection::Recent, HomeGroup::Start, "homeNavRecent", "homeActionRecent", "Recent", "recent"},
        {HomeSection::Settings, HomeGroup::Foot, "homeNavSettings", "homeActionSettings",
         "Settings", "settings"},
        {HomeSection::Help, HomeGroup::Foot, "homeNavHelp", "homeActionHelp", "Help", "help"},
    }};
    return rows;
}

std::size_t HomeSidebar::index_of(HomeSection section) {
    return static_cast<std::size_t>(section);
}

HomeSidebar::HomeSidebar(QWidget* parent) : QWidget(parent) {
    setObjectName("homeSidebar");
    setAccessibleName("Home navigation");
    for (const auto& row : home_navigation()) {
        const auto at = index_of(row.section);
        auto* action = new QAction(QString::fromLatin1(row.label), this);
        action->setObjectName(QString::fromLatin1(row.action_object_name));
        actions_[at] = action;
        auto* button = new NavRow(row, this);
        buttons_[at] = button;
        button->installEventFilter(this);
        // A press is the action, so whatever is listening to the action --
        // a callback here, or anything connected in ordinary Qt style --
        // hears exactly one activation for one press.
        connect(button, &QAbstractButton::clicked, action, &QAction::trigger);
        connect(action, &QAction::triggered, this, [this, section = row.section] {
            if (auto& callback = callbacks_[index_of(section)]) callback();
            if (activated) activated(section);
        });
    }
    set_selected(HomeSection::Home);
    wear(theme_);
}

void HomeSidebar::wear(ThemeId id) {
    theme_ = id;
    for (auto* button : buttons_) static_cast<NavRow*>(button)->wear(id);
    update();
}

void HomeSidebar::set_selected(HomeSection section) {
    selected_ = section;
    for (std::size_t i = 0; i < buttons_.size(); ++i)
        static_cast<NavRow*>(buttons_[i])->set_chosen(i == index_of(section));
}

QString HomeSidebar::selected_label() const { return label(selected_); }
QAction* HomeSidebar::action(HomeSection section) const { return actions_[index_of(section)]; }
QAbstractButton* HomeSidebar::button(HomeSection section) const { return buttons_[index_of(section)]; }

void HomeSidebar::set_callback(HomeSection section, Callback callback) {
    callbacks_[index_of(section)] = std::move(callback);
}

QString HomeSidebar::label(HomeSection section) const {
    return QString::fromLatin1(home_navigation()[index_of(section)].label);
}

QStringList HomeSidebar::labels() const {
    QStringList all;
    for (const auto& row : home_navigation()) all << QString::fromLatin1(row.label);
    return all;
}

QSize HomeSidebar::sizeHint() const { return minimumSizeHint(); }

QSize HomeSidebar::minimumSizeHint() const {
    return {rail_width, top_margin + static_cast<int>(home_section_count) * row_height
                            + rule_room * 3 + foot_margin};
}

void HomeSidebar::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    lay_out();
}

void HomeSidebar::lay_out() {
    // The groups above the foot run down from the top with a rule between each;
    // the foot group is stood on the bottom edge, and whatever room is left
    // over lies between, which is the flexible space the specification asks
    // for.
    const auto wide = width() - outer_margin * 2;
    auto y = top_margin;
    auto previous = HomeGroup::Start;
    const auto& rows = home_navigation();
    rules_.clear();
    for (const auto& row : rows) {
        if (row.group == HomeGroup::Foot) continue;
        if (row.group != previous) {
            rules_.push_back(y + rule_room / 2);
            y += rule_room;
            previous = row.group;
        }
        buttons_[index_of(row.section)]->setGeometry(outer_margin, y, wide, row_height);
        y += row_height;
    }
    const auto foot = std::count_if(rows.begin(), rows.end(),
                                    [](const auto& row) { return row.group == HomeGroup::Foot; });
    auto bottom = std::max(y + rule_room, height() - foot_margin - static_cast<int>(foot) * row_height);
    for (const auto& row : rows) {
        if (row.group != HomeGroup::Foot) continue;
        buttons_[index_of(row.section)]->setGeometry(outer_margin, bottom, wide, row_height);
        bottom += row_height;
    }
    update();
}

void HomeSidebar::paintEvent(QPaintEvent*) {
    const auto& t = tokens(theme_);
    QPainter painter(this);
    painter.fillRect(rect(), t.sidebar_surface);
    painter.setPen(QPen(t.border_soft, 1.0));
    painter.drawLine(QPointF(width() - 0.5, 0), QPointF(width() - 0.5, height()));
    for (const auto rule : rules_)
        painter.drawLine(QPointF(28, rule + 0.5), QPointF(width() - 28, rule + 0.5));
}

void HomeSidebar::move_focus(int from, int step) {
    const auto count = static_cast<int>(buttons_.size());
    const auto to = std::clamp(from + step, 0, count - 1);
    buttons_[static_cast<std::size_t>(to)]->setFocus(Qt::TabFocusReason);
}

bool HomeSidebar::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        const auto at = std::find(buttons_.begin(), buttons_.end(), watched);
        if (at != buttons_.end()) {
            const auto from = static_cast<int>(at - buttons_.begin());
            const auto count = static_cast<int>(buttons_.size());
            switch (static_cast<QKeyEvent*>(event)->key()) {
            case Qt::Key_Down: move_focus(from, 1); return true;
            case Qt::Key_Up: move_focus(from, -1); return true;
            case Qt::Key_Home: move_focus(0, 0); return true;
            case Qt::Key_End: move_focus(count - 1, 0); return true;
            default: break;
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace erdflow::desktop
