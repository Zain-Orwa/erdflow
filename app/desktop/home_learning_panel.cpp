// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/home_learning_panel.hpp"

#include "app/desktop/home_sidebar.hpp"
#include "app/desktop/icons.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

#include <algorithm>

namespace erdflow::desktop {
namespace {
constexpr int panel_width = 306;
constexpr int side_left = 35;
constexpr int side_right = 33;
constexpr int topic_icon = 20;
constexpr int link_icon = 18;
// The words beside an icon start here, as they do in the reference.
constexpr int words_indent = 37;

const QString top_words = QStringLiteral("From ideas\nto real databases.");
const QString heading_words = QStringLiteral("Learn more");
const QString footer_words = QStringLiteral("Design today.\nBuild tomorrow.");

QFont face(const Tokens& t, const QFont& base, double pixels, int weight, bool italic = false) {
    auto font = base;
    font.setFamilies(t.family);
    font.setPixelSize(static_cast<int>(pixels));
    font.setWeight(static_cast<QFont::Weight>(weight));
    font.setItalic(italic);
    return font;
}

// A link: an icon, the words, and an arrow at the far end, in the primary.
class LinkRow final : public QAbstractButton {
public:
    LinkRow(const HomeLearningLinkDefinition& what, QWidget* parent)
        : QAbstractButton(parent), what_(what) {
        setObjectName(QString::fromLatin1(what.object_name));
        setText(QString::fromLatin1(what.label));
        setAccessibleName(text());
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
        setFixedHeight(34);
    }

    void wear(ThemeId id) {
        theme_ = id;
        const auto& t = tokens(id);
        mark_ = outline_pixmap(QString::fromLatin1(what_.icon), t.primary, link_icon);
        arrow_ = outline_pixmap(QStringLiteral("chevron-right"), t.primary, 16);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        const auto& t = tokens(theme_);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        if (underMouse() || isDown()) {
            QPainterPath shape;
            shape.addRoundedRect(QRectF(rect()).adjusted(-6, 0, 0, 0), 8, 8);
            painter.fillPath(shape, t.hover_surface);
        }
        if (!mark_.isNull()) painter.drawPixmap(QPointF(0, (height() - link_icon) / 2.0), mark_);
        if (!arrow_.isNull()) painter.drawPixmap(QPointF(width() - 18, (height() - 16) / 2.0), arrow_);
        auto font = face(t, this->font(), 14, 500);
        font.setUnderline(underMouse());
        painter.setFont(font);
        // The primary as lettering on the panel's pale surface is under 4.5:1
        // for Azure, so it is deepened just enough to be read.
        const auto ink = legible_on(t.primary, t.learning_surface);
        painter.setPen(isDown() ? ink.darker(115) : ink);
        painter.drawText(QRectF(words_indent, 0, width() - words_indent - 22, height()),
                         Qt::AlignVCenter | Qt::AlignLeft, text());
        if (hasFocus()) {
            auto ring = t.primary;
            ring.setAlphaF(0.75f);
            painter.setPen(QPen(ring, 1.5));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(QRectF(rect()).adjusted(-5, 1.5, -1.5, -1.5), 7, 7);
        }
    }

private:
    HomeLearningLinkDefinition what_;
    ThemeId theme_ = ThemeId::Azure;
    QPixmap mark_;
    QPixmap arrow_;
};
} // namespace

const std::array<HomeLearningTopic, home_learning_topic_count>& home_learning_topics() {
    // The specification's copy, verbatim.
    static const std::array<HomeLearningTopic, home_learning_topic_count> topics{{
        {"Conceptual Modeling", "Design the big picture with entities and relationships.",
         "conceptual"},
        {"Relational Modeling", "Define tables, columns, keys and constraints.", "relational"},
        {"SQL Generation", "Create and export SQL scripts for your database.", "sql"},
        {"End-to-End Flow", "From ERD to SQL and real data.", "flow"},
    }};
    return topics;
}

const std::array<HomeLearningLinkDefinition, home_learning_link_count>& home_learning_links() {
    static const std::array<HomeLearningLinkDefinition, home_learning_link_count> links{{
        {HomeLearningLink::ViewTutorials, "homeLinkTutorials", "homeActionTutorials",
         "View tutorials", "tutorials"},
    }};
    return links;
}

std::size_t HomeLearningPanel::index_of(HomeLearningLink link) {
    return static_cast<std::size_t>(link);
}

HomeLearningPanel::HomeLearningPanel(QWidget* parent) : QWidget(parent) {
    setObjectName("homeLearningPanel");
    setAccessibleName("Learn more");

    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(side_left, 0, side_right, 0);
    column->setSpacing(0);
    // Every gap here is a stretch with a floor, so the panel breathes on a tall
    // window and closes up on a short one instead of running off the bottom.
    column->addStretch(3);
    phrase_ = new QLabel(top_words, this);
    phrase_->setObjectName("homeLearningPhrase");
    column->addWidget(phrase_);
    column->addSpacing(14);
    column->addStretch(1);
    rule_ = new QWidget(this);
    rule_->setObjectName("homeAccentRule");
    rule_->setFixedSize(51, 3);
    column->addWidget(rule_);
    column->addSpacing(18);
    column->addStretch(2);
    heading_ = new QLabel(heading_words, this);
    heading_->setObjectName("homeLearningHeading");
    column->addWidget(heading_);
    column->addSpacing(12);
    column->addStretch(1);

    const auto& topics = home_learning_topics();
    for (std::size_t i = 0; i < topics.size(); ++i) {
        auto* row = new QHBoxLayout;
        row->setSpacing(0);
        row->setContentsMargins(0, 0, 0, 0);
        auto* icon = new QLabel(this);
        icon->setFixedSize(words_indent, topic_icon + 2);
        icon->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        topic_icons_[i] = icon;
        row->addWidget(icon, 0, Qt::AlignTop);
        auto* words = new QVBoxLayout;
        words->setSpacing(3);
        auto* title = new QLabel(QString::fromLatin1(topics[i].title), this);
        title->setObjectName("homeLearningTopicTitle");
        auto* body = new QLabel(QString::fromLatin1(topics[i].description), this);
        body->setObjectName("homeLearningTopicBody");
        body->setWordWrap(true);
        topic_titles_[i] = title;
        topic_bodies_[i] = body;
        words->addWidget(title);
        words->addWidget(body);
        row->addLayout(words, 1);
        column->addLayout(row);
        column->addSpacing(12);
        column->addStretch(1);
    }

    auto* divider = new QWidget(this);
    divider->setObjectName("homeLearningDivider");
    divider->setFixedHeight(1);
    column->addWidget(divider);
    column->addSpacing(10);
    column->addStretch(1);

    for (const auto& link : home_learning_links()) {
        const auto at = index_of(link.link);
        auto* action = new QAction(QString::fromLatin1(link.label), this);
        action->setObjectName(QString::fromLatin1(link.action_object_name));
        actions_[at] = action;
        auto* button = new LinkRow(link, this);
        buttons_[at] = button;
        connect(button, &QAbstractButton::clicked, action, &QAction::trigger);
        connect(action, &QAction::triggered, this, [this, which = link.link] {
            if (auto& callback = callbacks_[index_of(which)]) callback();
            if (activated) activated(which);
        });
        column->addWidget(button);
        column->addSpacing(6);
    }
    // Room for the wave and its two lines at the foot, which are painted.
    column->addSpacing(8);
    column->addStretch(2);
    column->addSpacing(96);
    wear(theme_);
}

void HomeLearningPanel::wear(ThemeId id) {
    theme_ = id;
    const auto& t = tokens(id);
    const auto quiet = id == ThemeId::Azure ? QColor("#526981") : t.text_secondary;
    const auto ink = [](QLabel* label, const QColor& colour, const QFont& font) {
        label->setFont(font);
        label->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(colour.name()));
    };
    ink(phrase_, quiet, face(t, font(), 16, 400));
    ink(heading_, t.text_primary, face(t, font(), 18, 700));
    rule_->setStyleSheet(QStringLiteral("background: %1; border-radius: 1px;").arg(t.primary.name()));
    if (auto* divider = findChild<QWidget*>("homeLearningDivider"))
        divider->setStyleSheet(QStringLiteral("background: %1;").arg(t.border_soft.name()));
    const auto& topics = home_learning_topics();
    for (std::size_t i = 0; i < topics.size(); ++i) {
        topic_icons_[i]->setPixmap(outline_pixmap(QString::fromLatin1(topics[i].icon), t.primary,
                                                  topic_icon));
        ink(topic_titles_[i], t.text_primary, face(t, font(), 15, 700));
        ink(topic_bodies_[i], t.text_muted, face(t, font(), 13, 400));
    }
    for (auto* button : buttons_) static_cast<LinkRow*>(button)->wear(id);
    update();
}

QAction* HomeLearningPanel::action(HomeLearningLink link) const { return actions_[index_of(link)]; }
QAbstractButton* HomeLearningPanel::button(HomeLearningLink link) const {
    return buttons_[index_of(link)];
}
void HomeLearningPanel::set_callback(HomeLearningLink link, Callback callback) {
    callbacks_[index_of(link)] = std::move(callback);
}

QString HomeLearningPanel::top_phrase() const { return phrase_->text(); }
QString HomeLearningPanel::learn_more_heading() const { return heading_->text(); }
QString HomeLearningPanel::footer_phrase() const { return footer_words; }

QStringList HomeLearningPanel::topic_titles() const {
    QStringList all;
    for (const auto* label : topic_titles_) all << label->text();
    return all;
}

QStringList HomeLearningPanel::topic_descriptions() const {
    QStringList all;
    for (const auto* label : topic_bodies_) all << label->text();
    return all;
}

QStringList HomeLearningPanel::link_labels() const {
    QStringList all;
    for (const auto* button : buttons_) all << button->text();
    return all;
}

QSize HomeLearningPanel::sizeHint() const { return {panel_width, 900}; }
QSize HomeLearningPanel::minimumSizeHint() const {
    return {panel_width, QWidget::minimumSizeHint().height()};
}

int HomeLearningPanel::wave_height() const {
    return std::clamp(height() / 6, 96, 170);
}

void HomeLearningPanel::paintEvent(QPaintEvent*) {
    const auto& t = tokens(theme_);
    QPainter painter(this);
    painter.fillRect(rect(), t.learning_surface);
    painter.setRenderHint(QPainter::Antialiasing, true);
    // A soft abstract wave at the foot, in the theme's own pale blues.
    // Decoration, and the only thing here that is.
    const double w = width();
    const double bottom = height();
    const double tall = wave_height();
    QPainterPath first;
    first.moveTo(0, bottom - tall * 0.72);
    first.cubicTo(w * 0.3, bottom - tall * 1.05, w * 0.6, bottom - tall * 0.3, w, bottom - tall);
    first.lineTo(w, bottom);
    first.lineTo(0, bottom);
    painter.fillPath(first, t.primary_soft);
    QPainterPath second;
    second.moveTo(0, bottom - tall * 0.42);
    second.cubicTo(w * 0.4, bottom - tall * 0.82, w * 0.64, bottom - tall * 0.08, w, bottom - tall * 0.61);
    second.lineTo(w, bottom);
    second.lineTo(0, bottom);
    auto deeper = t.primary_soft;
    deeper = QColor(std::max(0, deeper.red() - 14), std::max(0, deeper.green() - 6), deeper.blue());
    // Deepening towards blue gives a grey a blue cast, which a theme with no
    // colour does not have.
    if (colourless(theme_)) deeper = greyed(deeper);
    painter.fillPath(second, deeper);

    // The two decorative lines, in the application's own face set light and
    // italic rather than a script face ERDFlow would have to ship.
    painter.setFont(face(t, font(), 15, 400, true));
    painter.setPen(theme_ == ThemeId::Azure ? QColor("#526981") : t.text_secondary);
    painter.drawText(QRectF(w * 0.41, bottom - tall * 0.62, w * 0.55, 46),
                     Qt::AlignLeft | Qt::AlignTop, footer_words);

    painter.setPen(QPen(t.border_soft, 1.0));
    painter.drawLine(QPointF(0.5, 0), QPointF(0.5, height()));
}

} // namespace erdflow::desktop
