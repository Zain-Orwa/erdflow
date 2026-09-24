// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/app_top_bar.hpp"

#include "app/desktop/icons.hpp"

#include <QHBoxLayout>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QToolButton>

namespace erdflow::desktop {
namespace {
constexpr int bar_height = 52;
constexpr int outer_margin = 22;

QFont bar_font(const Tokens& t, const QFont& base, int pixels, int weight) {
    auto font = base;
    font.setFamilies(t.family);
    font.setPixelSize(pixels);
    font.setWeight(static_cast<QFont::Weight>(weight));
    return font;
}

void draw_focus(QPainter& painter, const QRectF& box, const QColor& colour) {
    auto ring = colour;
    ring.setAlphaF(0.75f);
    painter.setPen(QPen(ring, 1.5));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(box, 7, 7);
}

// The mark and the name. Drawn rather than loaded, so it is sharp at any
// scale and takes the theme's own blues.
class Brand final : public QToolButton {
public:
    explicit Brand(QWidget* parent) : QToolButton(parent) {
        setObjectName("appTopBarBrand");
        setText("ERDFlow");
        setAccessibleName("ERDFlow");
        setFocusPolicy(Qt::NoFocus);
        setFixedSize(124, bar_height);
    }
    void wear(ThemeId id) { theme_ = id; update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        const auto& t = tokens(theme_);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        // A database: a lid, a body and a darker base, 18 by 23.
        const auto top = (height() - 23) / 2.0;
        painter.setPen(Qt::NoPen);
        painter.setBrush(t.primary);
        painter.drawRoundedRect(QRectF(1, top + 4, 18, 15), 3, 3);
        painter.drawEllipse(QRectF(1, top, 18, 8));
        painter.setBrush(t.primary_pressed);
        painter.drawEllipse(QRectF(1, top + 15, 18, 8));
        painter.setFont(bar_font(t, font(), 18, 700));
        painter.setPen(t.text_primary);
        painter.drawText(QRectF(30, 0, width() - 30, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
    }

private:
    ThemeId theme_ = ThemeId::Azure;
};

// Settings and Theme: an outline icon, and for Theme its word and an arrow.
class Control final : public QToolButton {
public:
    Control(const char* object_name, const QString& words, const char* icon, bool with_words,
            QWidget* parent)
        : QToolButton(parent), icon_(QString::fromLatin1(icon)), with_words_(with_words) {
        setObjectName(QString::fromLatin1(object_name));
        setText(words);
        setAccessibleName(words);
        setToolTip(words);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
        setPopupMode(QToolButton::InstantPopup);
        // The arrow is drawn here, level with the word, rather than left to Qt,
        // which drops it into the bottom-right corner.
        setStyleSheet(QStringLiteral("QToolButton::menu-indicator { image: none; width: 0; }"));
        setFixedSize(with_words ? 112 : 40, bar_height);
    }

    void wear(ThemeId id) {
        theme_ = id;
        const auto& t = tokens(id);
        const auto rest = id == ThemeId::Azure ? QColor("#475569") : t.text_secondary;
        resting_ = outline_pixmap(icon_, rest, 20);
        lit_ = outline_pixmap(icon_, t.primary, 20);
        arrow_ = outline_pixmap(QStringLiteral("chevron-down"), rest, 14);
        arrow_lit_ = outline_pixmap(QStringLiteral("chevron-down"), t.primary, 14);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        const auto& t = tokens(theme_);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const bool over = underMouse() || isDown();
        if (over) {
            QPainterPath chip;
            chip.addRoundedRect(QRectF(1, 9, width() - 2, height() - 18), 8, 8);
            painter.fillPath(chip, t.primary_faint);
        }
        const auto& mark = over ? lit_ : resting_;
        const auto icon_left = with_words_ ? 8.0 : (width() - 20) / 2.0;
        if (!mark.isNull()) painter.drawPixmap(QPointF(icon_left, (height() - 20) / 2.0), mark);
        if (with_words_) {
            painter.setFont(bar_font(t, font(), 14, 500));
            painter.setPen(theme_ == ThemeId::Azure ? QColor("#1E293B") : t.text_primary);
            painter.drawText(QRectF(36, 0, 56, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
            const auto& arrow = over ? arrow_lit_ : arrow_;
            if (!arrow.isNull()) painter.drawPixmap(QPointF(width() - 22, (height() - 14) / 2.0), arrow);
        }
        if (hasFocus()) draw_focus(painter, QRectF(1.5, 9.5, width() - 3, height() - 19), t.primary);
    }

private:
    QString icon_;
    bool with_words_;
    ThemeId theme_ = ThemeId::Azure;
    QPixmap resting_, lit_, arrow_, arrow_lit_;
};

class Divider final : public QWidget {
public:
    explicit Divider(QWidget* parent) : QWidget(parent) {
        setObjectName("appTopBarSeparator");
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
        setFixedSize(21, bar_height);
    }
    void wear(ThemeId id) { theme_ = id; update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setPen(QPen(tokens(theme_).border_soft, 1.0));
        painter.drawLine(QPointF(width() / 2.0, 14), QPointF(width() / 2.0, height() - 14));
    }

private:
    ThemeId theme_ = ThemeId::Azure;
};
} // namespace

AppTopBar::AppTopBar(QWidget* parent) : QWidget(parent) {
    setObjectName("appTopBar");
    setAccessibleName("ERDFlow");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setFixedHeight(bar_height);
    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(outer_margin, 0, outer_margin - 6, 0);
    row->setSpacing(0);
    brand_ = new Brand(this);
    row->addWidget(brand_);
    row->addStretch(1);
    settings_ = new Control("appTopBarSettings", "Settings", "settings", false, this);
    separator_ = new Divider(this);
    theme_button_ = new Control("appTopBarTheme", "Theme", "appearance", true, this);
    row->addWidget(settings_);
    row->addWidget(separator_);
    row->addWidget(theme_button_);
    wear(theme_);
}

void AppTopBar::wear(ThemeId id) {
    theme_ = id;
    static_cast<Brand*>(brand_)->wear(id);
    static_cast<Control*>(settings_)->wear(id);
    static_cast<Divider*>(separator_)->wear(id);
    static_cast<Control*>(theme_button_)->wear(id);
    update();
}

void AppTopBar::attach_settings_menu(QMenu* menu) { settings_->setMenu(menu); }
void AppTopBar::attach_theme_menu(QMenu* menu) { theme_button_->setMenu(menu); }

QSize AppTopBar::sizeHint() const { return {960, bar_height}; }
QSize AppTopBar::minimumSizeHint() const { return {320, bar_height}; }

void AppTopBar::paintEvent(QPaintEvent*) {
    const auto& t = tokens(theme_);
    QPainter painter(this);
    // The reference's near-white is Azure's own; any other theme uses its
    // surface rather than inheriting Azure's cast.
    painter.fillRect(rect(), theme_ == ThemeId::Azure ? QColor("#FDFEFF") : t.surface);
    painter.setPen(QPen(t.border_soft, 1.0));
    painter.drawLine(QPointF(0, height() - 0.5), QPointF(width(), height() - 0.5));
}

} // namespace erdflow::desktop
