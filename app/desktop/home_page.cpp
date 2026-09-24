// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/home_page.hpp"

#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpacerItem>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>

namespace erdflow::desktop {
namespace {
// The reference geometry the canonical drawing is authored at. These are the
// proportions the design was decided in; the page is laid out responsively
// around them rather than scaled to them.
constexpr int sidebar_width = 206;
constexpr int learning_width = 306;
constexpr int centre_side_padding = 36;

// Where the page stops trying to hold three columns. The learning panel is the
// first to go, being the one nothing depends on; the sidebar follows. Both are
// deliberate points rather than a gradual squeeze, because a column that
// shrinks until its words break is worse than one that is not there.
constexpr int learning_survives_above = 1180;
constexpr int sidebar_survives_above = 880;

// The illustration beside the welcome. It stands to the right of the words
// with its foot level with "Create a new project" and rises into the room
// above the title, as the reference has it, rather than taking a band of the
// page for itself. As large as that room allows, up to the tallest here; below
// the shortest it is decoration squeezed past reading, and steps aside.
constexpr int hero_tallest = 230;
constexpr int hero_shortest = 128;
constexpr double hero_aspect = 520.0 / 280.0;
// The room kept above the welcome for the illustration to rise into, and how
// much of it a short window gives up.
constexpr int header_room = 64;
constexpr int header_room_compact = 4;

// The cards share one line and never wrap (Zain, 2026-09-23). Each keeps a
// door's shape -- its height 1.10 times its width -- and its width stays
// between these: on a wide window the cards stop growing and the room left
// over becomes wider gaps and then a margin either side of the group, never
// wider cards (Zain, 2026-09-24).
constexpr int card_gap = 16;
constexpr int widest_gap = 28;
constexpr int widest_card = 315;
constexpr int narrowest_card = 150;

// The row of start cards: one line, the cards dividing it between them.
class CardLine final : public QWidget {
public:
    explicit CardLine(QWidget* parent) : QWidget(parent) {
        setObjectName("startRouteGrid");
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    std::vector<StartRouteCard*> cards;

    [[nodiscard]] int count() const { return std::max<int>(1, static_cast<int>(cards.size())); }
    // The width the cards are sized from: the line's, less the room a scroll
    // bar would take whether or not one is showing. A card's height follows
    // from its width, so if a scroll bar appearing narrowed the cards, the
    // shorter cards could make it go again -- and the page would never settle.
    void set_steady_width(int wide) {
        if (steady_ == wide) return;
        steady_ = wide;
        place();
    }
    [[nodiscard]] int sizing_width() const { return steady_ > 0 ? std::min(width(), steady_) : width(); }
    // As wide as the line allows, up to the widest a card is.
    [[nodiscard]] int row_width(int line) const {
        return std::clamp((line - card_gap * (count() - 1)) / count(), 1, widest_card);
    }
    [[nodiscard]] int height_at(int each, bool compact) const {
        auto tallest = 0;
        for (auto* card : cards) tallest = std::max(tallest, card->height_for(each, compact));
        return tallest;
    }
    // How tall the line is with nothing but its width to decide, in either
    // mode. What fit() weighs against the room there is.
    [[nodiscard]] int natural_height(bool compact) const {
        return height_at(row_width(sizing_width()), compact);
    }
    // A height the line must not pass, on a window too short for the cards at
    // the width the line would give them. They are narrowed to fit it, keeping
    // their shape -- but no narrower than the point where their words would
    // need more than their shape, since a narrower card is then a taller one.
    // Nothing at all is the same as no limit.
    void set_limit(int tallest) {
        if (limit_ == tallest) return;
        limit_ = tallest;
        place();
    }
    [[nodiscard]] int chosen_width() const {
        auto each = row_width(sizing_width());
        if (limit_ <= 0) return each;
        const bool compact = !cards.empty() && cards.front()->compact();
        while (each > narrowest_card && height_at(each, compact) > limit_) {
            auto content = 0;
            for (auto* card : cards)
                content = std::max(content, card->content_height_for(each - 2, compact));
            if (content > height_at(each, compact)) break;
            each -= 2;
        }
        return each;
    }
    [[nodiscard]] QSize minimumSizeHint() const override {
        const auto narrowest = narrowest_card * count() + card_gap * (count() - 1);
        return {narrowest, height()};
    }
    [[nodiscard]] QSize sizeHint() const override { return {width(), height()}; }
    void place() {
        // Setting the height below resizes the line, which asks for this
        // again; once is enough.
        if (placing_) return;
        placing_ = true;
        const auto each = chosen_width();
        const auto compact = !cards.empty() && cards.front()->compact();
        const auto tall = height_at(each, compact);
        // Cards that fill the line start at its left, under the heading. Cards
        // held narrower -- at their widest on a wide window, or narrowed to a
        // short one -- leave room over: it widens the gaps first, a little,
        // and then sits either side of the group so the group stays centred.
        const bool filling = each == (sizing_width() - card_gap * (count() - 1)) / count();
        const auto spare = std::max(0, sizing_width() - each * count() - card_gap * (count() - 1));
        // Filling, the width held back for a scroll bar that is not showing
        // goes into the gaps, so the row still ends where the form does.
        const auto slack = std::max(0, width() - sizing_width());
        const auto gap = count() <= 1 ? 0
            : filling ? std::min(widest_gap, card_gap + slack / (count() - 1))
                      : std::min(widest_gap, card_gap + spare / (count() - 1));
        const auto group = each * count() + gap * (count() - 1);
        auto x = filling ? 0 : std::max(0, (width() - group) / 2);
        for (auto* card : cards) {
            card->setGeometry(x, 0, each, tall);
            x += each + gap;
        }
        if (height() != tall) setFixedHeight(tall);
        placing_ = false;
    }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        place();
    }

private:
    int limit_ = 0;
    int steady_ = 0;
    bool placing_ = false;
};

// A label given the size and weight a token names, in the theme's own face.
void dress(QLabel* label, const TextStyle& style, const QColor& ink, const QStringList& family) {
    auto font = label->font();
    font.setFamilies(family);
    font.setPixelSize(static_cast<int>(style.size));
    font.setWeight(static_cast<QFont::Weight>(style.weight));
    label->setFont(font);
    label->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(ink.name()));
}
} // namespace

HomePage::HomePage(QWidget* parent) : QWidget(parent) {
    setObjectName("homePage");
    setAutoFillBackground(false);
    // Made in the order they are read -- the bar, the rail, the centre, the
    // panel -- which is the order Tab moves through them.
    top_bar_ = new AppTopBar(this);
    sidebar_ = new HomeSidebar(this);
    build_centre();
    learning_ = new HomeLearningPanel(this);
    wear(theme_);
}

void HomePage::build_centre() {
    centre_scroll_ = new QScrollArea(this);
    centre_scroll_->setObjectName("homeCentreScroll");
    centre_scroll_->setFrameShape(QFrame::NoFrame);
    centre_scroll_->setWidgetResizable(true);
    centre_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    centre_ = new QWidget(centre_scroll_);
    centre_->setObjectName("homeCentre");
    centre_scroll_->setWidget(centre_);

    auto* column = new QVBoxLayout(centre_);
    column->setContentsMargins(centre_side_padding, 0, centre_side_padding, 0);
    column->setSpacing(0);
    // Every gap is a floor and a stretch: the page opens out on a tall window
    // and closes up on a short one, which is how it stays one page. Most of
    // what a tall window has to spare goes under the form rather than over
    // the welcome, so the page reads from the top rather than floating.
    top_space_ = new QSpacerItem(0, header_room, QSizePolicy::Minimum, QSizePolicy::Fixed);
    column->addSpacerItem(top_space_);
    column->addStretch(1);

    // The welcome on the left. The illustration is not in this row: it is
    // stood beside it by place_hero, so that it can be larger than the words
    // are tall without pushing the cards down.
    auto* top = new QHBoxLayout;
    top->setSpacing(24);
    auto* greeting = new QVBoxLayout;
    greeting->setSpacing(0);
    title_ = new QLabel("Welcome to ERDFlow", centre_);
    title_->setObjectName("homeTitle");
    subtitle_ = new QLabel("Design. Model. Convert. Generate.", centre_);
    subtitle_->setObjectName("homeSubtitle");
    section_ = new QLabel("Create a new project", centre_);
    section_->setObjectName("homeSectionTitle");
    greeting->addWidget(title_);
    greeting->addSpacing(4);
    greeting->addWidget(subtitle_);
    greeting->addStretch(1);
    greeting_space_ = new QSpacerItem(0, 24, QSizePolicy::Minimum, QSizePolicy::Fixed);
    greeting->addSpacerItem(greeting_space_);
    greeting->addWidget(section_);
    top->addLayout(greeting, 1);
    // The drawing sits in a holder of the size it is shown at, rather than
    // being fixed to it, so it stays a component that draws at whatever size
    // any other caller gives it.
    hero_holder_ = new QWidget(centre_);
    hero_holder_->setObjectName("homeHeroHolder");
    auto* holding = new QVBoxLayout(hero_holder_);
    holding->setContentsMargins(0, 0, 0, 0);
    hero_ = new WelcomeFlowIllustration(hero_holder_);
    // Told to fill the holder whatever it would ask for on its own, or its own
    // idea of a minimum would push it past the holder's edge and clip it.
    hero_->setMinimumSize(0, 0);
    hero_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    holding->addWidget(hero_);
    column->addLayout(top);
    header_space_ = new QSpacerItem(0, 16, QSizePolicy::Minimum, QSizePolicy::Fixed);
    column->addSpacerItem(header_space_);

    // The routes, on one line. Remembered in order so the choice can be moved
    // between them by keyboard as well as by pointer.
    auto* line = new CardLine(centre_);
    card_row_ = line;
    for (const auto& what : start_routes()) {
        auto* card = new StartRouteCard(what, line);
        card->setChecked(what.route == chosen_);
        const auto choose = [this, route = what.route] {
            chosen_ = route;
            for (auto* one : cards_) one->setChecked(one->route() == route);
            if (route_selected) route_selected(route);
        };
        connect(card, &QAbstractButton::clicked, this, choose);
        // The card's own + Create is what starts; it chooses its card first,
        // so what is lit is always what was made.
        connect(card->create_button(), &QPushButton::clicked, this, [this, choose, route = what.route] {
            choose();
            if (route_chosen) route_chosen(route);
        });
        cards_.push_back(card);
    }
    line->cards = cards_;
    column->addWidget(card_row_);
    cards_space_ = new QSpacerItem(0, 22, QSizePolicy::Minimum, QSizePolicy::Fixed);
    column->addSpacerItem(cards_space_);

    // A live demo inside each card, in place of its drawing and description
    // (Zain, 2026-09-25; ADR-022 section 9.21). One clock plays all three,
    // each starting a second after the one before it, so their busiest
    // moments never fall together.
    demo_clock_ = new HomeDemoClock(this);
    // Which demos move. Zain asked on 2026-09-25 for all three to stand as
    // still pictures of their finished scenes, keeping the motion so that
    // one, two or all of them can be switched back on: set a card's `moves`
    // to true and that demo plays again, a second after the one before it.
    struct Played {
        HomeDemoKind kind;
        double delay;
        bool moves;
    };
    const std::array<Played, 3> played{{
        {HomeDemoKind::Conceptual, 0.0, false},
        {HomeDemoKind::Relational, 1.0, false},
        {HomeDemoKind::Sql, 2.0, false},
    }};
    for (std::size_t i = 0; i < played.size() && i < cards_.size(); ++i) {
        demos_.push_back(new HomeLiveDemo(played[i].kind));
        cards_[i]->set_live_demo(demos_.back());
        demo_clock_->add(demos_.back(), played[i].delay, played[i].moves);
        // Standing still, the Conceptual card shows the Conceptual canvas
        // itself, drawn small (Zain, 2026-09-25); moving, its painted scene.
        demos_.back()->set_real_canvas(!played[i].moves);
    }
    // A demo switched on plays only where motion is welcome, through the one
    // seam that says so (ADR-022 section 9.9), and only while the cards can be
    // seen. With none switched on, nothing ticks at all.
    demo_clock_->follow(card_row_);
    demo_clock_->set_moving(!WelcomeFlowIllustration::platform_prefers_stillness());

    // Nothing is asked under the cards: the page ends with them (Zain,
    // 2026-09-24, ADR-022 section 9.19). Where a project's name and place are
    // asked is decided separately.
    column->addStretch(3);
}

void HomePage::set_compact(bool on) {
    if (compact_ == on) return;
    compact_ = on;
    // What gives way, in order: the gaps close to a few pixels, the
    // illustration steps aside and the card drawings shrink. The words on the
    // cards and their buttons keep their size.
    top_space_->changeSize(0, on ? header_room_compact : header_room, QSizePolicy::Minimum,
                           QSizePolicy::Fixed);
    header_space_->changeSize(0, on ? 6 : 16, QSizePolicy::Minimum, QSizePolicy::Fixed);
    greeting_space_->changeSize(0, on ? 10 : 24, QSizePolicy::Minimum, QSizePolicy::Fixed);
    cards_space_->changeSize(0, on ? 8 : 22, QSizePolicy::Minimum, QSizePolicy::Fixed);
    for (auto* card : cards_) card->set_compact(on);
    static_cast<CardLine*>(card_row_)->place();
    centre_->layout()->invalidate();
    // Once more after the page has been laid out in its new spacing, which is
    // when the room the cards have can be measured rather than estimated.
    QTimer::singleShot(0, this, [this] { fit(); });
}

// How much taller the page is with its full spacing than without.
int HomePage::spacing_saving() const {
    return (header_room - header_room_compact) + (16 - 6) + (24 - 10) + (22 - 8);
}

// Stands the illustration beside the welcome: its right edge on the centre's
// right margin, its foot level with the foot of "Create a new project", and as
// tall as the room above that allows. Where the words leave too little room
// across, or the page is being kept short, it steps aside.
void HomePage::place_hero() {
    if (compact_ || !section_->isVisible()) {
        hero_holder_->hide();
        return;
    }
    const auto words_right = centre_side_padding
        + std::max({title_->sizeHint().width(), subtitle_->sizeHint().width(),
                    section_->sizeHint().width()});
    const auto right = centre_->width() - centre_side_padding;
    const auto across = right - words_right - 32;
    const auto foot = section_->geometry().bottom() + 6;
    const auto tall = std::min({hero_tallest, foot - 6,
                                static_cast<int>(across / hero_aspect)});
    if (tall < hero_shortest) {
        hero_holder_->hide();
        return;
    }
    const auto wide = static_cast<int>(tall * hero_aspect);
    hero_holder_->setGeometry(right - wide, foot - tall, wide, tall);
    hero_holder_->show();
    hero_holder_->raise();
}

// Whether the page is on one screen, and if not, what gives way. Asked once
// the centre has been given its room, which Qt does after the page itself is
// resized -- so it is asked a moment later rather than in the middle of that.
//
// Worked out rather than tried: what the page needs besides the cards is
// measured, what it would need in full is that plus the saving, and the cards'
// own height at the line's width is asked of them. So the answer is the same
// however often it is asked, and the page never flickers between the two.
void HomePage::fit() {
    auto* line = static_cast<CardLine*>(card_row_);
    const auto room = centre_scroll_->viewport()->height();
    const auto besides = centre_->layout()->minimumSize().height() - line->height();
    const auto besides_full = compact_ ? besides + spacing_saving() : besides;
    if (besides_full + line->natural_height(false) <= room) {
        set_compact(false);
        line->set_limit(0);
    } else {
        set_compact(true);
        // The cards may be as tall as the room the rest leaves them, and are
        // narrowed, keeping their shape, to be no taller. The rest is measured
        // once the page stands in its closer spacing, not estimated.
        centre_->layout()->activate();
        const auto rest = centre_->layout()->minimumSize().height() - line->height();
        line->set_limit(std::max(1, room - rest));
    }
    place_hero();
}

void HomePage::wear(ThemeId id) {
    theme_ = id;
    const auto& t = tokens(id);
    top_bar_->wear(id);
    sidebar_->wear(id);
    learning_->wear(id);
    dress(title_, t.hero_title, t.text_heading, t.family);
    dress(subtitle_, t.hero_subtitle, id == ThemeId::Azure ? QColor("#526981") : t.text_secondary,
          t.family);
    dress(section_, t.section_title, t.text_primary, t.family);
    for (auto* card : cards_) card->wear(id);
    for (auto* demo : demos_) demo->wear(id);
    hero_->wear(id);
    centre_scroll_->setStyleSheet(QStringLiteral(
        "QScrollArea#homeCentreScroll, QWidget#homeCentre { background: %1; }").arg(t.surface.name()));
    lay_out();
    update();
}

void HomePage::lay_out() {
    const bool with_learning = width() >= learning_survives_above;
    const bool with_sidebar = width() >= sidebar_survives_above;
    const auto bar = top_bar_->sizeHint().height();
    top_bar_->setGeometry(0, 0, width(), bar);
    const auto tall = std::max(0, height() - bar);

    auto left = 0;
    sidebar_->setVisible(with_sidebar);
    if (with_sidebar) {
        sidebar_->setGeometry(0, bar, sidebar_width, tall);
        left = sidebar_width;
    }
    const auto right = with_learning ? learning_width : 0;
    learning_->setVisible(with_learning);
    if (with_learning) learning_->setGeometry(width() - learning_width, bar, learning_width, tall);
    const auto centre_width = std::max(0, width() - left - right);
    centre_scroll_->setGeometry(left, bar, centre_width, tall);
    static_cast<CardLine*>(card_row_)->set_steady_width(
        centre_width - centre_side_padding * 2
        - centre_scroll_->style()->pixelMetric(QStyle::PM_ScrollBarExtent) - 2);
    QTimer::singleShot(0, this, [this] { fit(); });
}

bool HomePage::centre_needs_scrolling() const {
    return centre_->layout()->minimumSize().height() > centre_scroll_->viewport()->height();
}

void HomePage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // The keyboard starts on the chosen card's + Create, so Return starts it.
    // Left to Qt it lands on whatever was made first, which is the Settings
    // button in the bar.
    QTimer::singleShot(0, this, [this] {
        if (!isVisible()) return;
        const auto* holder = QApplication::focusWidget();
        if (holder && isAncestorOf(holder) && holder != top_bar_->settings_button()) return;
        for (auto* card : cards_)
            if (card->route() == chosen_) card->create_button()->setFocus(Qt::OtherFocusReason);
    });
}

void HomePage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    lay_out();
}

void HomePage::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), tokens(theme_).window_background);
}

int HomePage::chosen_row() const {
    return static_cast<int>(sidebar_->selected());
}

QStringList HomePage::sidebar_labels() const { return sidebar_->labels(); }

} // namespace erdflow::desktop
