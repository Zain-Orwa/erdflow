// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/project_details_form.hpp"

#include "app/desktop/home_sidebar.hpp"
#include "app/desktop/icons.hpp"

#include <QAbstractButton>
#include <QCheckBox>
#include <QDir>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpacerItem>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <algorithm>

namespace erdflow::desktop {
namespace {
constexpr int input_height = 42;
constexpr int description_height = 82;

// The folder choice, drawn as the reference draws it: a small rounded box
// filled with the primary and carrying a white tick when it is on. Painted
// here rather than styled, because a style sheet can only put a tick in the
// box by loading a picture file, and a missing image plugin would leave an
// empty box that reads as off when it is on.
class FolderCheck final : public QCheckBox {
public:
    FolderCheck(const QString& words, QWidget* parent) : QCheckBox(words, parent) {
        setCursor(Qt::PointingHandCursor);
        setMinimumHeight(28);
    }
    void wear(ThemeId id) { theme_ = id; update(); }
    [[nodiscard]] QSize sizeHint() const override {
        return {28 + fontMetrics().horizontalAdvance(text()) + 4, 28};
    }

protected:
    void paintEvent(QPaintEvent*) override {
        const auto& t = tokens(theme_);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRectF box(0.5, (height() - 18) / 2.0 + 0.5, 17, 17);
        if (isChecked()) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(t.primary);
            painter.drawRoundedRect(box, 4, 4);
            QPen tick(readable_on(t.primary), 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter.setPen(tick);
            painter.setBrush(Qt::NoBrush);
            QPainterPath mark;
            mark.moveTo(box.left() + 4.5, box.center().y() + 0.5);
            mark.lineTo(box.left() + 7.5, box.center().y() + 3.5);
            mark.lineTo(box.right() - 4, box.top() + 5);
            painter.drawPath(mark);
        } else {
            painter.setPen(QPen(t.border_medium, 1.0));
            painter.setBrush(t.surface);
            painter.drawRoundedRect(box, 4, 4);
        }
        if (hasFocus()) {
            auto ring = t.primary;
            ring.setAlphaF(0.35f);
            painter.setPen(QPen(ring, 3.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(box.adjusted(-2, -2, 2, 2), 5, 5);
        }
        painter.setPen(t.text_primary);
        painter.setFont(font());
        painter.drawText(QRectF(28, 0, width() - 28, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
    }

private:
    ThemeId theme_ = ThemeId::Azure;
};

// More options: a chevron and the words, in the primary. Drawn the way the
// Home screen's links are drawn, underlined under the pointer and ringed when
// the keyboard is on it, so it reads as one of them rather than as a button.
class Disclosure final : public QAbstractButton {
public:
    explicit Disclosure(QWidget* parent) : QAbstractButton(parent) {
        setText("More options");
        setAccessibleName(text());
        setCheckable(true);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);
        setFixedHeight(30);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }
    void wear(ThemeId id) {
        theme_ = id;
        updateGeometry();
        update();
    }
    [[nodiscard]] QSize sizeHint() const override {
        return {chevron + gap + QFontMetrics(words()).horizontalAdvance(text()) + 10, 30};
    }

protected:
    void paintEvent(QPaintEvent*) override {
        const auto& t = tokens(theme_);
        const auto ink = legible_on(t.primary, t.surface);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const auto mark = outline_pixmap(isChecked() ? QStringLiteral("chevron-down")
                                                     : QStringLiteral("chevron-right"),
                                         ink, chevron);
        if (!mark.isNull()) painter.drawPixmap(QPointF(0, (height() - chevron) / 2.0), mark);
        auto font = words();
        font.setUnderline(underMouse());
        painter.setFont(font);
        painter.setPen(isDown() ? ink.darker(115) : ink);
        painter.drawText(QRectF(chevron + gap, 0, width() - chevron - gap, height()),
                         Qt::AlignVCenter | Qt::AlignLeft, text());
        if (hasFocus()) {
            auto ring = t.primary;
            ring.setAlphaF(0.75f);
            painter.setPen(QPen(ring, 1.5));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(QRectF(rect()).adjusted(0.75, 0.75, -0.75, -0.75), 6, 6);
        }
    }

private:
    [[nodiscard]] QFont words() const {
        auto font = this->font();
        font.setFamilies(tokens(theme_).family);
        font.setPixelSize(14);
        font.setWeight(QFont::DemiBold);
        return font;
    }
    static constexpr int chevron = 16;
    static constexpr int gap = 6;
    ThemeId theme_ = ThemeId::Azure;
};

// The line saying where the project will be made. A path is as long as it is,
// and the page is sized from what it holds, so the line never asks for width:
// it takes what the form has and shortens the path in its middle, where the
// folders that matter least are, keeping the start and the project's own name.
class PathLine final : public QLabel {
public:
    explicit PathLine(QWidget* parent) : QLabel(parent) {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    }
    void set_whole(const QString& words, const QString& full) {
        whole_ = words;
        setToolTip(full);
        refit();
    }
    [[nodiscard]] QString whole() const { return whole_; }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QLabel::resizeEvent(event);
        refit();
    }
    void changeEvent(QEvent* event) override {
        QLabel::changeEvent(event);
        if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange) refit();
    }

private:
    void refit() { setText(fontMetrics().elidedText(whole_, Qt::ElideMiddle, std::max(0, width()))); }
    QString whole_;
};

// A path as a person reads it: under their own home folder it starts with ~,
// as a shell says it. Windows has no such habit, so there it is left whole.
QString as_read(QString path) {
#ifndef Q_OS_WIN
    const auto home = QDir::homePath();
    if (path == home || path.startsWith(home + '/')) path = '~' + path.mid(home.size());
#endif
    return QDir::toNativeSeparators(path);
}

QLabel* field_label(const QString& words, QWidget* parent) {
    auto* label = new QLabel(words, parent);
    label->setObjectName("projectFieldLabel");
    return label;
}
} // namespace

ProjectDetailsForm::ProjectDetailsForm(QWidget* parent) : QWidget(parent) {
    setObjectName("projectDetailsForm");
    build();
    wear(theme_);
}

void ProjectDetailsForm::build() {
    // Laid out to be short. The Home screen is one page that is never
    // scrolled, and a name is the only thing a project cannot start without,
    // so it is the only thing asked at first. Where it goes, what it is for and
    // whether it gets a folder all have working defaults, and wait under More
    // options (Zain, 2026-09-24).
    auto* column = new QVBoxLayout(this);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(0);

    auto* rule = new QFrame(this);
    rule->setObjectName("projectDetailsRule");
    rule->setFrameShape(QFrame::HLine);
    rule->setFixedHeight(1);
    column->addWidget(rule);
    rule_gap_ = new QSpacerItem(0, 14, QSizePolicy::Minimum, QSizePolicy::Fixed);
    column->addSpacerItem(rule_gap_);

    // The name, as wide as it was when Location stood beside it, rather than
    // stretched across the page.
    auto* name_row = new QHBoxLayout;
    auto* name_box = new QVBoxLayout;
    name_box->setSpacing(6);
    name_box->addWidget(field_label("Project name", this));
    name_ = new QLineEdit(this);
    name_->setObjectName("projectName");
    name_->setFixedHeight(input_height);
    name_->setPlaceholderText("University Management");
    name_box->addWidget(name_);
    name_row->addLayout(name_box, 5);
    name_row->addStretch(7);
    column->addLayout(name_row);
    column->addSpacing(6);

    // Where it will be made, said all the time and following every change, so
    // that folding the location away never hides where the project goes.
    saved_to_ = new PathLine(this);
    saved_to_->setObjectName("projectSavedTo");
    column->addWidget(saved_to_);
    row_gap_ = new QSpacerItem(0, 8, QSizePolicy::Minimum, QSizePolicy::Fixed);
    column->addSpacerItem(row_gap_);

    more_ = new Disclosure(this);
    more_->setObjectName("projectMoreOptions");
    column->addWidget(more_, 0, Qt::AlignLeft);

    // Side by side rather than stacked, as name and location were before, so
    // that opening them keeps the Home screen one page; the folder choice
    // stands level with the buttons, where it always stood.
    options_ = new QWidget(this);
    options_->setObjectName("projectOptions");
    auto* options = new QHBoxLayout(options_);
    options->setContentsMargins(0, 8, 0, 0);
    options->setSpacing(16);
    auto* where_box = new QVBoxLayout;
    where_box->setSpacing(6);
    where_box->addWidget(field_label("Location", options_));
    auto* where_row = new QHBoxLayout;
    where_row->setSpacing(10);
    where_ = new QLineEdit(options_);
    where_->setObjectName("projectLocation");
    where_->setFixedHeight(input_height);
    // Somewhere real to begin, so the commonest case needs no browsing at all.
    where_->setText(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                    + "/ERDFlow");
    where_row->addWidget(where_, 1);
    browse_ = new QPushButton("Browse...", options_);
    browse_->setObjectName("projectBrowse");
    browse_->setFixedSize(122, input_height);
    where_row->addWidget(browse_);
    where_box->addLayout(where_row);
    where_box->addStretch(1);
    options->addLayout(where_box, 7);

    auto* about_box = new QVBoxLayout;
    about_box->setSpacing(6);
    about_box->addWidget(field_label("Description (optional)", options_));
    about_ = new QPlainTextEdit(options_);
    about_->setObjectName("projectDescription");
    // As tall as the reference's 82 where there is room, and no shorter than
    // one line and a little where there is not.
    about_->setMinimumHeight(input_height + 2);
    about_->setMaximumHeight(description_height);
    about_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    about_->setPlaceholderText(
        "E.g. University database with students, courses and professors...");
    // Tab moves on to the next field rather than typing a tab into the
    // description, or somebody on the keyboard could get in and not out.
    about_->setTabChangesFocus(true);
    about_box->addWidget(about_, 1);
    options->addLayout(about_box, 5);
    options_->setVisible(false);
    column->addWidget(options_, 1);
    // Made here, after the description and before Create, so the keyboard
    // reaches it in the order it is read; placed in the row of buttons below.
    own_folder_ = new FolderCheck("Create project folder", this);
    own_folder_->setObjectName("projectOwnFolder");
    own_folder_->setChecked(true);
    own_folder_->setVisible(false);
    actions_gap_ = new QSpacerItem(0, 12, QSizePolicy::Minimum, QSizePolicy::Fixed);
    column->addSpacerItem(actions_gap_);

    // What is missing, said where it is missing from rather than in a box that
    // has to be dismissed before it can be put right. A line of its own the
    // width of the form, shown only while there is something to say, and not
    // wrapped: the page it sits in is sized from its contents, and a line that
    // grew taller as the page narrowed could keep the page from settling.
    trouble_ = new QLabel(QString(), this);
    trouble_->setObjectName("projectTrouble");
    trouble_->setVisible(false);
    column->addWidget(trouble_);
    column->addSpacing(6);

    // Create names what it will make, in the chosen card's words, so it is
    // wide enough for them rather than a fixed width.
    auto* actions = new QHBoxLayout;
    actions->setSpacing(14);
    actions->addWidget(own_folder_);
    actions->addStretch(1);
    create_ = new QPushButton("Create Project", this);
    create_->setObjectName("projectCreate");
    create_->setFixedHeight(44);
    create_->setMinimumWidth(176);
    create_->setDefault(true);
    actions->addWidget(create_);
    cancel_ = new QPushButton("Cancel", this);
    cancel_->setObjectName("projectCancel");
    cancel_->setFixedSize(116, 44);
    actions->addWidget(cancel_);
    column->addLayout(actions);

    connect(more_, &QAbstractButton::toggled, this, [this](bool on) { show_more(on); });
    connect(browse_, &QPushButton::clicked, this, [this] {
        if (!ask_where) return;
        const auto chosen = ask_where(where_->text());
        if (!chosen.isEmpty()) where_->setText(chosen);
    });
    connect(create_, &QPushButton::clicked, this, [this] {
        // Checked when it is pressed rather than by greying the button out. A
        // dead button says nothing about what is wrong with the form; this
        // says exactly which field is empty. A missing location is under More
        // options, which is opened so the field it is about can be seen.
        if (!complete()) {
            trouble_->setText(what_is_missing());
            trouble_->setVisible(true);
            if (project_name().isEmpty()) {
                name_->setFocus();
            } else {
                show_more(true);
                where_->setFocus();
            }
            return;
        }
        trouble_->setVisible(false);
        if (create) create();
    });
    connect(cancel_, &QPushButton::clicked, this, [this] { if (cancel) cancel(); });
    connect(name_, &QLineEdit::textChanged, this, [this] { follow_the_form(); });
    connect(where_, &QLineEdit::textChanged, this, [this] { follow_the_form(); });
    connect(own_folder_, &QCheckBox::toggled, this, [this] { follow_the_path(); });
    show_more(false);
    follow_the_path();
}

void ProjectDetailsForm::follow_the_form() {
    // A complaint goes as soon as what it was about is put right, rather than
    // sitting there contradicting the form.
    if (trouble_->isVisible() && complete()) trouble_->setVisible(false);
    follow_the_path();
}

void ProjectDetailsForm::follow_the_path() {
    const auto where = saved_to();
    if (where.isEmpty()) {
        static_cast<PathLine*>(saved_to_)->set_whole(
            "No location chosen yet. Choose one under More options.", {});
        return;
    }
    static_cast<PathLine*>(saved_to_)->set_whole("Saved to " + where, where);
}

QString ProjectDetailsForm::saved_to() const {
    if (location().isEmpty()) return {};
    // The folder the project is made in: its own, named as its file will be,
    // where that is ticked, and otherwise the location itself.
    const auto file_name = file_name_for ? file_name_for(project_name()) : project_name();
    auto folder = QDir::cleanPath(location());
    if (wants_own_folder() && !file_name.isEmpty()) folder = QDir(folder).filePath(file_name);
    folder = as_read(folder);
    if (!folder.endsWith('/') && !folder.endsWith('\\')) folder += QDir::separator();
    return folder;
}

void ProjectDetailsForm::set_action(const QString& words) { create_->setText(words); }
QString ProjectDetailsForm::action() const { return create_->text(); }

void ProjectDetailsForm::show_more(bool on) {
    if (more_->isChecked() != on) {
        const QSignalBlocker quiet(more_);
        more_->setChecked(on);
        more_->update();
    }
    if (options_->isHidden() == !on) return;
    options_->setVisible(on);
    own_folder_->setVisible(on);
    if (reshaped) reshaped();
}

bool ProjectDetailsForm::showing_more() const { return !options_->isHidden(); }

QString ProjectDetailsForm::project_name() const { return name_->text().trimmed(); }
QString ProjectDetailsForm::location() const { return where_->text().trimmed(); }
QString ProjectDetailsForm::description() const { return about_->toPlainText().trimmed(); }
bool ProjectDetailsForm::wants_own_folder() const { return own_folder_->isChecked(); }

void ProjectDetailsForm::set_project_name(const QString& value) { name_->setText(value); }
void ProjectDetailsForm::set_location(const QString& value) { where_->setText(value); }
void ProjectDetailsForm::set_description(const QString& value) { about_->setPlainText(value); }
void ProjectDetailsForm::set_wants_own_folder(bool on) { own_folder_->setChecked(on); }

void ProjectDetailsForm::show_error(const QString& message, bool about_location) {
    trouble_->setText(message);
    trouble_->setVisible(!message.isEmpty());
    if (about_location && !message.isEmpty()) show_more(true);
}

void ProjectDetailsForm::clear_error() { show_error({}); }

void ProjectDetailsForm::focus_name() { name_->setFocus(Qt::OtherFocusReason); }

void ProjectDetailsForm::set_compact(bool on) {
    if (compact_ == on) return;
    compact_ = on;
    about_->setMinimumHeight(on ? input_height : input_height + 2);
    // The gaps between the rows close a little too.
    rule_gap_->changeSize(0, on ? 8 : 14, QSizePolicy::Minimum, QSizePolicy::Fixed);
    row_gap_->changeSize(0, on ? 4 : 8, QSizePolicy::Minimum, QSizePolicy::Fixed);
    actions_gap_->changeSize(0, on ? 10 : 12, QSizePolicy::Minimum, QSizePolicy::Fixed);
    if (layout()) layout()->invalidate();
    wear(theme_);
}

bool ProjectDetailsForm::complete() const {
    return !project_name().isEmpty() && !location().isEmpty();
}

QString ProjectDetailsForm::what_is_missing() const {
    if (project_name().isEmpty() && location().isEmpty())
        return "A project needs a name and somewhere to live.";
    if (project_name().isEmpty()) return "Give the project a name.";
    if (location().isEmpty()) return "Say where the project should live.";
    return {};
}

void ProjectDetailsForm::wear(ThemeId id) {
    theme_ = id;
    const auto& t = tokens(id);
    static_cast<FolderCheck*>(own_folder_)->wear(id);
    static_cast<Disclosure*>(more_)->wear(id);
    const auto create_fill = chosen_row_fill(t);
    // One sheet for the whole form, so every field is the same height, the
    // same radius and the same border without any of them being told twice.
    setStyleSheet(QStringLiteral(R"(
        QWidget#projectDetailsForm { background: transparent; }
        QFrame#projectDetailsRule { background: %1; border: none; }
        QLabel#projectSavedTo { color: %3; font-size: 13px; background: transparent; }
        QLabel#projectFieldLabel {
            color: %2; font-size: 14px; font-weight: 500; background: transparent;
        }
        QLabel#projectTrouble { color: %10; font-size: 14px; background: transparent; }
        QLineEdit#projectName, QLineEdit#projectLocation, QPlainTextEdit#projectDescription {
            background: %4; border: 1px solid %5; border-radius: %6px;
            padding: 0 14px; font-size: 14px; color: %2;
        }
        QPlainTextEdit#projectDescription { padding: %15px 14px; }
        QLineEdit#projectName:focus, QLineEdit#projectLocation:focus,
        QPlainTextEdit#projectDescription:focus { border: 2px solid %7; }
        QPushButton#projectBrowse, QPushButton#projectCancel {
            background: %4; border: 1px solid %5; border-radius: %8px;
            font-size: 14px; font-weight: 600; color: %2;
        }
        QPushButton#projectBrowse:hover, QPushButton#projectCancel:hover { background: %9; }
        QPushButton#projectCreate {
            background: %11; border: none; border-radius: %8px; padding: 0 22px;
            font-size: 14px; font-weight: 600; color: %13;
        }
        QPushButton#projectCreate:hover { background: %12; }
        QPushButton#projectCreate:pressed { background: %14; }
        QCheckBox#projectOwnFolder { color: %2; font-size: 14px; background: transparent; }
    )").arg(t.border_soft.name(), t.text_primary.name())
       .arg(t.text_muted.name())
       .arg(t.surface.name(), t.border_medium.name())
       .arg(t.radius_input)
       .arg(t.primary.name())
       .arg(t.radius_button)
       // White on Azure's primary reads at 3.68:1, so Create takes the first
       // of primary, hover and pressed that reaches 4.5:1 -- #1976D2 for
       // Azure -- and deepens from there when pointed at and pressed. An error
       // in Azure's red on white is 3.76:1, so it is darkened just enough.
       .arg(t.hover_surface.name(), legible_on(t.red, t.surface).name(), create_fill.name(),
            create_fill.darker(112).name(), readable_on(create_fill).name(),
            create_fill.darker(124).name())
       // Closer in a short window's shorter box, so one line still fits it
       // without the box growing a scroll bar of its own.
       .arg(compact_ ? 5 : 10));
}

} // namespace erdflow::desktop
