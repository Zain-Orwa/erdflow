#include "main_window.hpp"
#include "download_dialog.hpp"
#include "ribbon.hpp"
#include "symbol_picker.hpp"
#include "symbols.hpp"

#include <QAction>
#include <QActionGroup>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QBuffer>
#include <QCloseEvent>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QAbstractSpinBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QClipboard>
#include <QFileDialog>
#include <QFileInfo>
#include <QFocusEvent>
#include <QFontMetricsF>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QIcon>
#include <QImageReader>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QStandardItemModel>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStatusBar>
#include <QResizeEvent>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <algorithm>
#include <cmath>
#include <array>

namespace erdflow::desktop {
namespace {
using namespace domain;
QString text(const std::string& value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }
std::string bytes(const QString& value) { return value.toUtf8().toStdString(); }
QString key(ElementRef ref) {
    const auto id = uuid(ref);
    return QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(id.bytes.data()), 16).toHex());
}
QString kind_label(ElementRef ref);
// A plain note is a symbol rather than a note, and the panel has to call it
// what it is: a card and a character drawn bare are not the same thing to
// anyone looking at them, whatever they share underneath.
QString kind_label(const Project& project, ElementRef ref) {
    if (const auto* note_id = std::get_if<NoteId>(&ref)) {
        const auto found = project.notes.find(*note_id);
        if (found != project.notes.end() && found->second.plain) return QStringLiteral("Symbol");
    }
    return kind_label(ref);
}
QString kind_label(ElementRef ref) {
    if (std::holds_alternative<EntityId>(ref)) return QStringLiteral("Entity");
    if (std::holds_alternative<AttributeId>(ref)) return QStringLiteral("Attribute");
    if (std::holds_alternative<SpecializationId>(ref)) return QStringLiteral("Specialization");
    if (std::holds_alternative<PictureId>(ref)) return QStringLiteral("Picture");
    if (std::holds_alternative<NoteId>(ref)) return QStringLiteral("Note");
    return QStringLiteral("Relationship");
}
// The colour an element is actually drawn with on the canvas: the one it was
// given if it has one, and otherwise whatever the theme gives its kind. The
// panel reads this so that it shows the element's own colour rather than only
// the colours a user happened to choose by hand.
QColor surface_of(const Project& project, const Theme& colors, ElementRef ref) {
    // A see-through surface shows the canvas through it, so what the panel
    // has to match is the blend the eye sees rather than the paint on its own.
    const auto faded = project.transparency.find(ref);
    const auto seen = [&](QColor paint) {
        if (faded != project.transparency.end()) paint.setAlphaF(static_cast<float>(1.0 - faded->second / 100.0));
        return over(colors.canvas, paint);
    };
    if (const auto chosen = project.colours.find(ref); chosen != project.colours.end())
        return seen(QColor(chosen->second.red, chosen->second.green, chosen->second.blue));
    if (std::holds_alternative<AttributeId>(ref)) return seen(colors.attribute_fill);
    if (std::holds_alternative<EntityId>(ref)) return seen(colors.entity_fill);
    if (std::holds_alternative<SpecializationId>(ref)) return seen(colors.isa_fill);
    if (std::holds_alternative<PictureId>(ref)) return seen(colors.panel);
    if (std::holds_alternative<NoteId>(ref)) return seen(note_surface(colors));
    // An associative relationship converts to a relation of its own and wears
    // the entity palette on the canvas, so it wears it here too.
    const auto& relationship = project.relationships.at(std::get<RelationshipId>(ref));
    return seen(relationship.associative ? colors.entity_fill : colors.relationship_fill);
}

// The relationships that join one entity to itself: the ones naming it as a
// participant more than once. A recursive entity is one that has any.
std::vector<ElementRef> recursions_of(const Project& project, EntityId id) {
    std::vector<ElementRef> found;
    for (const auto& [relationship_id, relationship] : project.relationships) {
        std::size_t touches = 0;
        for (const auto& participant : relationship.participants)
            if (participant.target == ParticipantTarget{id}) ++touches;
        if (touches > 1) found.emplace_back(relationship_id);
    }
    return found;
}

// The name a background style goes by, for the actions that choose it.
QString background_key(domain::BackgroundStyle style) {
    switch (style) {
    case domain::BackgroundStyle::Theme: return QStringLiteral("Theme");
    case domain::BackgroundStyle::Squares: return QStringLiteral("Squares");
    case domain::BackgroundStyle::Lines: return QStringLiteral("Lines");
    case domain::BackgroundStyle::Dots: return QStringLiteral("Dots");
    case domain::BackgroundStyle::Image: return QStringLiteral("Image");
    }
    return QStringLiteral("Theme");
}

QString display_name(const Project& project, ElementRef ref) {
    const auto value = text(name(project, ref));
    return value.isEmpty() ? QStringLiteral("(unnamed)") : value;
}
// The name at the top of a card. A side means nothing on its own: it is that
// element as it takes part in this one, so the card says both, with an arrow
// between them. Each wears the colour it is drawn in on the diagram, and each
// ink is chosen against its own colour rather than taken from the theme, since
// either colour may be one the user picked.
// How much of a shape's width its outline takes before the name can start. A
// diamond or a triangle narrows away from its middle and needs far more of it
// than a rectangle or an oval does.
double inset_of(bool pointed) { return pointed ? 0.22 : 0.09; }

// The room an element's shape would like: its own proportions on the diagram,
// and no less than its name takes to read in full. It is a wish rather than a
// rule -- the panel gives what it has, and the name is elided when that is
// less -- so nothing ever runs out of the panel whatever its width.
int wanted_width(double aspect, const QString& name, bool pointed, int tall, double points) {
    QFont font;
    font.setPointSizeF(points);
    font.setWeight(QFont::DemiBold);
    const auto text = QFontMetricsF(font).horizontalAdvance(name);
    const auto for_the_name = text / std::max(1.0 - 2 * inset_of(pointed), 0.2) + 14;
    return static_cast<int>(std::lround(std::max<double>(tall * std::max(aspect, 0.2), for_the_name)));
}

// The element's own shape with its name written inside it. The name is the
// thing the user gave it, so it is edited on the shape it belongs to rather
// than in a box beside a picture of one. The shape is redrawn whenever the
// room changes, so it stays crisp at any width.
class ShapedName final : public QWidget {
public:
    ShapedName(std::function<QPixmap(QSize)> draw, double aspect, const QString& name, bool pointed, QWidget* parent)
        : QWidget(parent), draw_(std::move(draw)), pointed_(pointed) {
        setObjectName("elementShape");
        setFixedHeight(56);
        wanted_ = wanted_width(aspect, name, pointed, 56, 13.0);
        // Asked for as much as the name wants, given as much as the panel has.
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }
    [[nodiscard]] QSize sizeHint() const override { return {wanted_, height()}; }
    [[nodiscard]] QSize minimumSizeHint() const override { return {56, height()}; }
    void hold(QLineEdit* field) {
        field_ = field;
        place();
    }
protected:
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        shape_ = draw_(size());
        place();
    }
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.drawPixmap(0, 0, shape_);
    }
private:
    void place() {
        if (!field_ || width() < 2) return;
        // A diamond or a triangle narrows away from its middle, so the name
        // is given only the room its shape actually has there.
        const auto inset = width() * inset_of(pointed_);
        const auto tall = std::min(30, height() - 10);
        field_->setGeometry(static_cast<int>(inset), (height() - tall) / 2,
                            std::max(24, static_cast<int>(width() - inset * 2)), tall);
    }
    std::function<QPixmap(QSize)> draw_;
    bool pointed_;
    int wanted_ = 56;
    QPixmap shape_;
    QLineEdit* field_ = nullptr;
};

// An element's shape with its name written inside it, for the places that only
// show a name rather than letting it be edited. The name is drawn rather than
// laid out, so it can be held inside an outline that narrows away from its
// middle, which no ordinary label would respect.
class ShapedTag final : public QWidget {
public:
    ShapedTag(std::function<QPixmap(QSize)> draw, QString name, QColor ink, double aspect, bool pointed,
              const char* named, QWidget* parent)
        : QWidget(parent), draw_(std::move(draw)), name_(std::move(name)), ink_(std::move(ink)), pointed_(pointed) {
        setObjectName(QString::fromLatin1(named));
        // Read out and hovered as the name it stands for, since the text is
        // painted rather than held by a label of its own.
        setAccessibleName(name_);
        setToolTip(name_);
        font_ = font();
        font_.setPointSizeF(10.5);
        font_.setWeight(QFont::DemiBold);
        setFixedHeight(30);
        wanted_ = wanted_width(aspect, name_, pointed, 30, 10.5);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }
    [[nodiscard]] QSize sizeHint() const override { return {wanted_, height()}; }
    // Never squeezed so far that its name is only a mark: an end that cannot
    // show a few letters says nothing at all.
    [[nodiscard]] QSize minimumSizeHint() const override { return {54, height()}; }
protected:
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        shape_ = draw_(size());
    }
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.drawPixmap(0, 0, shape_);
        painter.setFont(font_);
        painter.setPen(ink_);
        const auto inset = width() * inset_of(pointed_);
        const QRectF room(inset, 0, width() - inset * 2, height());
        painter.drawText(room, Qt::AlignCenter,
                         QFontMetricsF(font_).elidedText(name_, Qt::ElideRight, room.width()));
    }
private:
    std::function<QPixmap(QSize)> draw_;
    QString name_;
    QColor ink_;
    bool pointed_;
    int wanted_ = 44;
    QFont font_;
    QPixmap shape_;
};

// The name at the top of a card. A side means nothing on its own: it is that
// element as it takes part in this one, so the card says both, with an arrow
// between them, each written inside the shape and colour it is drawn with.
QWidget* card_title(QWidget* side, QWidget* toward, QWidget* parent) {
    auto* row = new QWidget(parent);
    row->setObjectName("cardTitle");
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 2, 0, 4);
    layout->setSpacing(7);
    side->setParent(row);
    toward->setParent(row);
    layout->addWidget(side, 0);
    auto* arrow = new QLabel(QStringLiteral("\u2192"), row);
    arrow->setObjectName("cardArrow");
    arrow->setStyleSheet(QStringLiteral("QLabel#cardArrow { font-size: 13px; font-weight: 800; }"));
    layout->addWidget(arrow, 0);
    layout->addWidget(toward, 0);
    layout->addStretch();
    return row;
}
// A field's name inside a card: bold, and in a hue of its own, so the several
// things asked about one side are told apart at a glance rather than read in
// order. The hues come from the theme and change with it.
QLabel* field_label(const QString& text, const QColor& ink, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName("fieldLabel");
    label->setStyleSheet(QStringLiteral("QLabel#fieldLabel { color: %1; font-weight: 700; }").arg(ink.name()));
    return label;
}
// The bytes of an image file, ready to be kept in a project. They are the
// file's own when it is already a PNG or JPEG of modest size, so nothing is
// lost; anything else is re-encoded, and scaled down first when it is large,
// since a project file has room for only so much picture.
std::optional<std::vector<std::uint8_t>> encoded_image(const QString& path, QString& why_not,
                                                      int longest = 1024, qint64 keep_below = 1024 * 1024) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const auto format = reader.format().toLower();
    const auto image = reader.read();
    if (image.isNull()) {
        why_not = reader.errorString().isEmpty() ? QStringLiteral("The file is not an image ERDFlow can read.")
                                                 : reader.errorString();
        return std::nullopt;
    }
    QByteArray encoded;
    QFile file(path);
    if ((format == "png" || format == "jpeg" || format == "jpg") && file.size() <= keep_below
        && file.open(QIODevice::ReadOnly)) {
        encoded = file.readAll();
    } else {
        // The best of the sizes that fits, rather than one size and a refusal:
        // a picture is worth keeping as large as the file has room for.
        for (auto side = longest; side >= 512; side = side * 3 / 4) {
            auto fitted = image;
            if (fitted.width() > side || fitted.height() > side)
                fitted = fitted.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            encoded.clear();
            QBuffer buffer(&encoded);
            buffer.open(QIODevice::WriteOnly);
            // A photograph is far smaller as a JPEG; anything transparent has to stay a PNG.
            if (fitted.hasAlphaChannel()) fitted.save(&buffer, "PNG");
            else fitted.save(&buffer, "JPEG", 90);
            if (static_cast<std::size_t>(encoded.size()) <= domain::max_image_bytes) break;
        }
    }
    if (encoded.isEmpty() || static_cast<std::size_t>(encoded.size()) > domain::max_image_bytes) {
        why_not = QStringLiteral("The picture is too large to keep in a project file.");
        return std::nullopt;
    }
    return std::vector<std::uint8_t>(encoded.begin(), encoded.end());
}

QLabel* hint(const QString& value, QWidget* parent) {
    auto* label = new QLabel(value, parent);
    label->setWordWrap(true);
    label->setObjectName("hint");
    return label;
}
// Descriptions commit through the same command path on focus loss.
// The Explorer's fold marks sit against its right-hand edge rather than in
// front of each row.
//
// The Explorer is on the left of the window and the diagram fills the middle,
// so the hand comes back from the canvas to the panel's near edge. A mark in
// front of a row is the far edge: it means crossing the whole width of the
// panel to open a group and crossing back to carry on. Against the right edge
// it is the first thing reached rather than the last, and every group opens
// from the same column whatever depth it sits at.
//
// The indentation is left exactly as it was, because that is what says what
// belongs to what. Only the mark moves.
class ExplorerTree final : public QTreeView {
public:
    using QTreeView::QTreeView;
    // The width of the strip the marks live in, kept clear of the text so a
    // long name is elided before it reaches the mark rather than under it.
    static constexpr int fold_strip = 20;

    // Where a row's mark is drawn: against the viewport's right edge, so the
    // marks stay in one column as the panel is resized and, when the tree is
    // scrolled sideways, stay where the hand expects rather than sliding away.
    [[nodiscard]] QRect fold_rect(const QRect& row) const {
        constexpr int mark = 14;
        return QRect(viewport()->width() - fold_strip + (fold_strip - mark) / 2,
                     row.top() + (row.height() - mark) / 2, mark, mark);
    }

protected:
    // Nothing is drawn in the branch column, so no mark appears in front of a
    // row. The column is still there and still indents.
    void drawBranches(QPainter*, const QRect&, const QModelIndex&) const override {}

    void drawRow(QPainter* painter, const QStyleOptionViewItem& options, const QModelIndex& index) const override {
        QTreeView::drawRow(painter, options, index);
        if (!model() || !model()->hasChildren(index)) return;
        // Drawn by the style itself rather than by hand, so it is the same
        // mark the rest of the window uses and follows whatever theme is on.
        QStyleOptionViewItem mark = options;
        mark.rect = fold_rect(options.rect);
        mark.state |= QStyle::State_Children;
        if (isExpanded(index)) mark.state |= QStyle::State_Open;
        else mark.state &= ~QStyle::State_Open;
        style()->drawPrimitive(QStyle::PE_IndicatorBranch, &mark, painter, this);
    }

    void mousePressEvent(QMouseEvent* event) override {
        // Pressing the mark opens or closes the group, and does nothing else:
        // it must not also change what is selected, or reaching for a fold
        // would throw away the selection the user was working with.
        const auto index = indexAt(event->pos());
        if (index.isValid() && model() && model()->hasChildren(index)
            && fold_rect(visualRect(index)).contains(event->pos())) {
            setExpanded(index, !isExpanded(index));
            event->accept();
            return;
        }
        QTreeView::mousePressEvent(event);
    }
};

// Keeps every row clear of the strip the fold marks stand in, so the two never
// overlap and a row's highlight ends in the same place whether or not it has
// anything to fold.
class FoldOnTheRight final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override {
        QStyledItemDelegate::initStyleOption(option, index);
        option->rect.adjust(0, 0, -ExplorerTree::fold_strip, 0);
    }
};

class DescriptionEdit final : public QPlainTextEdit {
public:
    using QPlainTextEdit::QPlainTextEdit;
    std::function<void()> commit;
protected:
    void focusOutEvent(QFocusEvent* event) override {
        QPlainTextEdit::focusOutEvent(event);
        if (commit) commit();
    }
};
// A choice or a number must not change because a finger brushed the trackpad
// while the pointer was over it: a value is changed by pressing the control
// and choosing, or by typing, and not by passing across it. The wheel is
// handed to whatever scrolls behind instead, so the panel moves under the
// pointer as it was meant to, and a control with nothing behind it simply
// lets the turn go by.
class WheelGuard final : public QObject {
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() != QEvent::Wheel) return QObject::eventFilter(watched, event);
        auto* widget = qobject_cast<QWidget*>(watched);
        if (!widget) return QObject::eventFilter(watched, event);
        for (auto* ancestor = widget->parentWidget(); ancestor; ancestor = ancestor->parentWidget())
            if (auto* area = qobject_cast<QAbstractScrollArea*>(ancestor)) {
                QApplication::sendEvent(area->viewport(), event);
                break;
            }
        return true;
    }
};

void finish_field_edit() {
    if (auto* widget = QApplication::focusWidget();
        qobject_cast<QLineEdit*>(widget) || qobject_cast<QPlainTextEdit*>(widget))
        widget->clearFocus();
}
}

MainWindow::MainWindow(application::Editor& editor, application::ProjectStore& store,
                       application::IdGenerator& ids, QWidget* parent)
    : QMainWindow(parent), ids_(ids), editor_(editor), store_(store) {
    setObjectName("mainWindow");
    wheel_guard_ = new WheelGuard(this);
    resize(1440, 920);
    // Small enough to be useful on a narrow screen. What the window cannot do
    // is stay this size and keep everything at full width, so the toolbar gives
    // up its labels before the window gives up its tools.
    setMinimumSize(560, 460);
    build_shell();
    build_actions();
    // The tabs go on once every action and menu they are built from exists.
    ribbon_ = new Ribbon(*this);
    // Which field a picked character goes into is decided by where the caret
    // was, so the last text field written in is remembered as focus moves.
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget* was, QWidget* now) {
        remember_caret(was);
        remember_text_target(now);
    });
    canvas_->on_edit = [this](const auto& result) { show_result(result); };
    canvas_->on_selection = [this](const auto& selected) { selection_changed(selected); };
    canvas_->on_tool = [this](Tool tool) {
        tool_actions_.at(tool)->setChecked(true);
        refresh_tool_labels();
    };
    canvas_->on_status = [this](const QString& message) { statusBar()->showMessage(message, 7000); };
    canvas_->on_zoom = [this](double factor) {
        zoom_label_->setText(QString::number(qRound(factor * 100)) + "%");
    };
    refresh();
}

MainWindow::~MainWindow() {
    // QWidget owns the projections; retire callbacks while Editor still lives.
    refreshing_ = true;
    canvas_->on_edit = {};
    canvas_->on_selection = {};
    canvas_->on_tool = {};
    canvas_->on_status = {};
    canvas_->on_zoom = {};
    // Some of what the window listens to speaks up while the window is being
    // torn down: the application says focus has left, a dock that was showing
    // says it is no longer visible, and a view says its selection has emptied.
    // All three arrive from QWidget's own destructor, which runs after every
    // member of this class has already been destroyed, because a base class is
    // destroyed last. A slot reached then would read a map that no longer
    // exists, so the window stops listening before any of it can happen.
    // Signals that only a person can cause are left alone: nobody presses a
    // button on a window that is going away.
    qApp->disconnect(this);
    if (validation_dock_) validation_dock_->disconnect(this);
    if (explorer_ && explorer_->selectionModel()) explorer_->selectionModel()->disconnect(this);
    if (issues_) issues_->disconnect(this);
    delete properties_->takeWidget();
    delete takeCentralWidget();
}

void MainWindow::build_shell() {
    auto* workspace = new QWidget(this);
    auto* layout = new QVBoxLayout(workspace);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* header = new QWidget(workspace);
    header->setObjectName("workspaceHeader");
    auto* header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(22, 14, 22, 14);
    auto* badge = new QLabel("CONCEPTUAL", header);
    badge->setObjectName("workspaceBadge");
    header_layout->addWidget(badge);
    document_label_ = new QLabel(header);
    document_label_->setObjectName("documentTitle");
    header_layout->addWidget(document_label_, 1);
    auto* example = new QPushButton("Open example", header);
    example->setObjectName("openExample");
    connect(example, &QPushButton::clicked, this, &MainWindow::load_example);
    header_layout->addWidget(example);
    layout->addWidget(header);
    canvas_ = new DiagramView(editor_, workspace);
    canvas_->setObjectName("diagramCanvas");
    canvas_->setAccessibleName("Conceptual ERD canvas");
    layout->addWidget(canvas_, 1);
    auto* instructions = hint("Choose a shape, then click the canvas. Connect links an attribute to its owner, or a relationship to an entity.", workspace);
    instructions->setContentsMargins(18, 10, 18, 10);
    layout->addWidget(instructions);
    setCentralWidget(workspace);

    auto* explorer_dock = new QDockWidget("Explorer", this);
    explorer_dock->setObjectName("explorerDock");
    explorer_ = new ExplorerTree(explorer_dock);
    explorer_->setObjectName("explorer");
    explorer_->setItemDelegate(new FoldOnTheRight(explorer_));
    explorer_->setAccessibleName("Project elements");
    explorer_->setHeaderHidden(true);
    explorer_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    explorer_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    explorer_->setMinimumWidth(120);
    explorer_->setUniformRowHeights(true);
    explorer_model_ = new QStandardItemModel(this);
    explorer_->setModel(explorer_model_);
    explorer_dock->setWidget(explorer_);
    addDockWidget(Qt::LeftDockWidgetArea, explorer_dock);
    connect(explorer_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
        if (refreshing_) return;
        std::vector<ElementRef> selected;
        for (const auto& index : explorer_->selectionModel()->selectedRows()) {
            const auto found = references_.find(index.data(Qt::UserRole).toString());
            // One attribute may be selected through either of its rows; it is
            // still one attribute.
            if (found != references_.end() && std::find(selected.begin(), selected.end(), found->second) == selected.end())
                selected.push_back(found->second);
        }
        canvas_->select_elements(selected, true);
        selection_changed(selected);
    });
    connect(explorer_, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
        if (index.data(Qt::UserRole).toString() == "project") {
            bool accepted = false;
            const auto value = QInputDialog::getText(this, "Project name", "Name", QLineEdit::Normal,
                                                   text(editor_.project().name), &accepted);
            if (accepted) show_result(editor_.rename_project(bytes(value)));
        } else if (references_.contains(index.data(Qt::UserRole).toString())) rename_selection();
    });

    auto* properties_dock = new QDockWidget("Properties", this);
    properties_dock->setObjectName("propertiesDock");
    properties_ = new QScrollArea(properties_dock);
    properties_->setWidgetResizable(true);
    properties_->setMinimumWidth(180);
    properties_->setFrameShape(QFrame::NoFrame);
    properties_dock->setWidget(properties_);
    addDockWidget(Qt::RightDockWidgetArea, properties_dock);

    validation_dock_ = new QDockWidget("Model checks", this);
    validation_dock_->setObjectName("validationDock");
    issues_ = new QTreeView(validation_dock_);
    issues_->setObjectName("modelIssues");
    issues_->setRootIsDecorated(false);
    issues_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    issue_model_ = new QStandardItemModel(this);
    issues_->setModel(issue_model_);
    validation_dock_->setWidget(issues_);
    addDockWidget(Qt::BottomDockWidgetArea, validation_dock_);
    validation_dock_->hide();
    connect(issues_, &QTreeView::clicked, this, [this](const QModelIndex& index) {
        const auto found = references_.find(index.siblingAtColumn(0).data(Qt::UserRole).toString());
        if (found != references_.end()) canvas_->select_elements({found->second}, true);
    });
    auto* view_menu = menuBar()->addMenu("&View");
    view_menu->setObjectName("viewMenu");
    view_menu->addAction(explorer_dock->toggleViewAction());
    view_menu->addAction(properties_dock->toggleViewAction());
    view_menu->addAction(validation_dock_->toggleViewAction());
    count_label_ = new QLabel(this);
    readiness_label_ = new QLabel(this);
    zoom_label_ = new QLabel("100%", this);
    statusBar()->addWidget(count_label_);
    statusBar()->addPermanentWidget(readiness_label_);
    statusBar()->addPermanentWidget(zoom_label_);
    statusBar()->setSizeGripEnabled(true);
    resizeDocks({explorer_dock, properties_dock}, {230, 310}, Qt::Horizontal);
}

namespace {
// Both pickers show a sample of a line. The sizes live here rather than at each
// call site, which is what keeps the icon a widget asks for the same size as the
// one it is later redrawn at when the theme changes.
constexpr QSize line_style_sample{48, 24};
constexpr QSize notation_sample{58, 22};

QString isa_label(Tool mode) {
    return mode == Tool::Generalization ? QStringLiteral("Generalization") : QStringLiteral("Specialization");
}
// The notations offered, in the order they appear everywhere in the UI.
const std::array<std::pair<Notation, QString>, 4>& notation_styles() {
    static const std::array<std::pair<Notation, QString>, 4> styles{{
        {Notation::Chen, "Chen"}, {Notation::MinMax, "Min–max"},
        {Notation::CrowsFoot, "Crow's foot"}, {Notation::Bachman, "Bachman"}
    }};
    return styles;
}
} // namespace

void MainWindow::build_actions() {
    auto* file = new QMenu("&File", this);
    file->setObjectName("fileMenu");
    menuBar()->insertMenu(menuBar()->actions().front(), file);
    auto* action_new = file->addAction("&New project", QKeySequence::New, this, &MainWindow::new_project);
    action_new->setObjectName("newProject");
    action_glyphs_[action_new] = Glyph::New;
    action_glyphs_[file->addAction("&Open…", QKeySequence::Open, this, &MainWindow::open_dialog)] = Glyph::Open;
    auto* action_save = file->addAction("&Save", QKeySequence::Save, this, [this] { save(); });
    action_save->setObjectName("saveProject");
    action_glyphs_[action_save] = Glyph::Save;
    file->addAction("Save &as…", QKeySequence::SaveAs, this, [this] { save(true); });
    file->addSeparator();

    // Download is how work leaves ERDFlow. Its own menu, so the ribbon can put
    // a row over it the way Insert and Design are put over theirs, and a copy
    // of it under File, which is where a document application keeps it.
    //
    // Documents come first. Someone handing this work on is choosing between a
    // report and a picture before they are choosing between PNG and SVG.
    auto* download_menu = new QMenu("Download", this);
    download_menu->setObjectName("downloadMenu");
    download_menu->addSection("Documents");
    struct DocumentEntry { DocumentFormat format; const char* name; };
    for (const auto& entry : {DocumentEntry{DocumentFormat::Pdf, "downloadPdfDocument"},
                              DocumentEntry{DocumentFormat::Markdown, "downloadMarkdown"},
                              DocumentEntry{DocumentFormat::Html, "downloadHtml"},
                              DocumentEntry{DocumentFormat::Csv, "downloadCsv"}}) {
        const auto& info = document_format(entry.format);
        auto* item = download_menu->addAction(QString::fromUtf8(info.label) + "…", this,
                                              [this, format = entry.format] { download_document(format); });
        item->setObjectName(QString::fromLatin1(entry.name));
        item->setToolTip(QString::fromUtf8(info.caution));
    }

    // Then the pictures a person reaches for without thinking about options.
    // SVG leads because it is the default download: it reads at any size and it
    // is one of the two that carry the project home again.
    download_menu->addSection("Pictures");
    struct PictureEntry { PictureFormat format; const char* name; bool common; };
    QMenu* more_pictures = nullptr;
    for (const auto& entry : {PictureEntry{PictureFormat::Svg, "downloadSvg", true},
                              PictureEntry{PictureFormat::Png, "downloadPng", true},
                              PictureEntry{PictureFormat::Pdf, "downloadPdfPage", true},
                              PictureEntry{PictureFormat::Jpeg, "downloadJpeg", false},
                              PictureEntry{PictureFormat::WebP, "downloadWebp", false},
                              PictureEntry{PictureFormat::Tiff, "downloadTiff", false}}) {
        // A format this build has no writer for is left out rather than offered
        // and then failed.
        if (!picture_format_available(entry.format)) continue;
        const auto& info = picture_format(entry.format);
        // The three anyone wants sit on the menu; the rest are gathered behind
        // one entry, so a common choice is never hunted for among rare ones.
        if (!entry.common && !more_pictures) {
            more_pictures = download_menu->addMenu("Other picture formats");
            more_pictures->setObjectName("downloadMorePictures");
        }
        auto* into = entry.common ? download_menu : more_pictures;
        auto* item = into->addAction(QString::fromUtf8(info.label) + "…", this, [this, format = entry.format] {
            auto options = download_choice_.as_picture;
            options.format = format;
            download_picture(options);
        });
        item->setObjectName(QString::fromLatin1(entry.name));
        if (*info.caution) item->setToolTip(QString::fromUtf8(info.caution));
    }

    download_menu->addSeparator();
    auto* download_options = download_menu->addAction("Download with options…", QKeySequence("Ctrl+Shift+E"),
                                                      this, &MainWindow::download_dialog);
    download_options->setObjectName("downloadWithOptions");
    download_options->setToolTip("Choose the format, and for a picture its size, extent and background.");
    action_glyphs_[download_options] = Glyph::Download;
    auto* copy_action = download_menu->addAction("Copy as picture", QKeySequence("Ctrl+Shift+C"),
                                                 this, [this] { copy_picture(); });
    copy_action->setObjectName("copyAsPicture");
    copy_action->setToolTip("Put a picture of the selection, or of the whole diagram, on the clipboard.");
    // Gathered so they can be turned off together while there is nothing drawn.
    // A submenu's own entries are collected too, since the submenu itself only
    // names them.
    for (auto* action : download_menu->actions()) {
        if (action->isSeparator()) continue;
        if (auto* submenu = action->menu()) {
            for (auto* nested : submenu->actions()) download_actions_.push_back(nested);
            download_actions_.push_back(action);
            continue;
        }
        download_actions_.push_back(action);
    }
    file->addMenu(download_menu);
    file->addSeparator();
    file->addAction("Open example", this, &MainWindow::load_example);
    file->addSeparator();
    file->addAction("&Quit", QKeySequence::Quit, this, &QWidget::close);
    auto* edit = new QMenu("&Edit", this);
    // Named like the window's other menus, so it can be found by name.
    edit->setObjectName("editMenu");
    menuBar()->insertMenu(findChild<QMenu*>("viewMenu")->menuAction(), edit);
    undo_ = edit->addAction("Undo", QKeySequence::Undo, this, [this] {
        finish_field_edit(); canvas_->cancel_interaction(); show_result(editor_.undo());
    });
    undo_->setObjectName("undoCommand");
    redo_ = edit->addAction("Redo", QKeySequence::Redo, this, [this] {
        finish_field_edit(); canvas_->cancel_interaction(); show_result(editor_.redo());
    });
    redo_->setObjectName("redoCommand");
    // The menu entry names the edit that will be reversed, which makes the
    // label change width on every command. A toolbar button must not resize
    // as you work, so it keeps a fixed icon text and explains itself by tooltip.
    undo_->setIconText("Undo");
    redo_->setIconText("Redo");
    action_glyphs_[undo_] = Glyph::Undo;
    action_glyphs_[redo_] = Glyph::Redo;
    edit->addSeparator();
    rename_ = edit->addAction("Rename…", this, &MainWindow::rename_selection);
    action_glyphs_[rename_] = Glyph::Rename;
    duplicate_ = edit->addAction("Duplicate", QKeySequence("Ctrl+D"), this, [this] {
        finish_field_edit(); show_result(editor_.duplicate(selection_));
    });
    duplicate_->setObjectName("duplicateElements");
    action_glyphs_[duplicate_] = Glyph::Duplicate;
    action_glyphs_[edit->addAction("Delete selection", this, [this] { finish_field_edit(); canvas_->delete_selection(); })] = Glyph::Delete;
    edit->addSeparator();
    // A symbol is drawn as its character filling its box, so making the box
    // bigger is what makes the character bigger. The view's own zoom already
    // owns Ctrl+= and Ctrl+-, and it means something else -- how close the
    // whole diagram is looked at, not how big one thing on it is -- so these
    // take the same keys with Shift, the way a document editor's font size does.
    enlarge_ = edit->addAction("Enlarge symbol", QKeySequence("Ctrl+Shift+="), this,
                               [this] { finish_field_edit(); canvas_->resize_symbols(symbol_step); });
    enlarge_->setObjectName("enlargeSymbol");
    enlarge_->setToolTip("Make the selected symbol bigger. Its corners do the same thing by hand.");
    shrink_ = edit->addAction("Shrink symbol", QKeySequence("Ctrl+Shift+-"), this,
                              [this] { finish_field_edit(); canvas_->resize_symbols(1 / symbol_step); });
    shrink_->setObjectName("shrinkSymbol");
    shrink_->setToolTip("Make the selected symbol smaller.");
    // Delete/Backspace and the letter tool shortcuts belong to the canvas, so
    // typing inside property fields never deletes model elements.
    auto* toolbar = addToolBar("Model tools");
    toolbar->setObjectName("modelTools");
    toolbar->setMovable(false);
    // Icon beside text, the way an office application labels its toolbar: the
    // glyph carries recognition, the word removes any doubt.
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    // Big enough that the drawing in an icon can be read rather than guessed
    // at. The toolbar runs out of width before it runs out of room in height,
    // so anything past this pushes buttons into the overflow.
    toolbar->setIconSize(QSize(34, 34));
    toolbar->addAction(action_save);
    toolbar->addSeparator();
    toolbar->addAction(undo_);
    toolbar->addAction(redo_);
    toolbar->addSeparator();
    auto* group = new QActionGroup(this);
    const std::array<std::pair<Tool, QString>, 4> tools{{
        {Tool::Select, "Select"}, {Tool::Entity, "Entity"}, {Tool::Attribute, "Attribute"},
        {Tool::Relationship, "Relationship"}
    }};
    for (const auto& [tool, label] : tools) {
        auto* action = toolbar->addAction(label);
        action->setCheckable(true);
        action->setData(label);
        action->setObjectName("tool" + label);
        action->setActionGroup(group);
        action->setChecked(tool == Tool::Select);
        tool_actions_[tool] = action;
        action_glyphs_[action] = tool == Tool::Select ? Glyph::Select
            : tool == Tool::Entity ? Glyph::Entity
            : tool == Tool::Attribute ? Glyph::Attribute : Glyph::Relationship;
        connect(action, &QAction::triggered, this, [this, tool] { choose_tool(tool, false); });
        // A double click locks the tool. The button itself has to report it,
        // because the click that locks is also an ordinary click that selects.
        if (auto* button = toolbar->widgetForAction(action)) button->installEventFilter(this);
    }
    // ISA covers two directions onto the same structure, so one entry carries
    // both: the arrow chooses the direction, the button uses the chosen one and
    // locks on a double click exactly like every other tool.
    isa_action_ = new QAction(isa_label(isa_mode_), this);
    isa_action_->setCheckable(true);
    isa_action_->setData(isa_label(isa_mode_));
    isa_action_->setObjectName("toolIsa");
    isa_action_->setActionGroup(group);
    action_glyphs_[isa_action_] = Glyph::Isa;
    connect(isa_action_, &QAction::triggered, this, [this] { choose_tool(isa_mode_, false); });
    auto* isa_menu = new QMenu(this);
    for (const auto mode : {Tool::Specialization, Tool::Generalization}) {
        auto* entry = isa_menu->addAction(isa_label(mode));
        entry->setObjectName("isa" + isa_label(mode));
        entry->setToolTip(mode == Tool::Specialization
            ? "Top-down: click the entity to specialise, then connect its subtypes."
            : "Bottom-up: select the subtypes, then click the entity that generalises them.");
        connect(entry, &QAction::triggered, this, [this, mode] {
            isa_mode_ = mode;
            isa_action_->setData(isa_label(mode));
            choose_tool(mode, false);
        });
    }
    auto* isa_button = new QToolButton(toolbar);
    isa_button->setObjectName("isaButton");
    isa_button->setDefaultAction(isa_action_);
    isa_button->setMenu(isa_menu);
    isa_button->setPopupMode(QToolButton::MenuButtonPopup);
    // These two are plain widgets on the toolbar rather than actions, so they
    // inherit none of its presentation and have to be told to match it.
    isa_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    isa_button->setIconSize(toolbar->iconSize());
    toolbar->addWidget(isa_button);
    tool_actions_[Tool::Specialization] = isa_action_;
    tool_actions_[Tool::Generalization] = isa_action_;
    isa_button->installEventFilter(this);

    // How connectors are drawn belongs with the tool that draws them, so the
    // Connect button carries the choice on its own arrow rather than in a
    // separate control the eye has to find.
    auto* connect_action = new QAction("Connect", this);
    connect_action->setCheckable(true);
    connect_action->setData("Connect");
    connect_action->setObjectName("toolConnect");
    connect_action->setActionGroup(group);
    action_glyphs_[connect_action] = Glyph::Connect;
    connect(connect_action, &QAction::triggered, this, [this] { choose_tool(Tool::Connect, false); });
    auto* line_menu = new QMenu(this);
    // Right angles first, since that is how a line between two points chosen
    // by hand is drawn, and what the canvas draws unless told otherwise.
    for (const auto style : {LineStyle::Elbow, LineStyle::Curved, LineStyle::Straight}) {
        const QString label = style == LineStyle::Straight ? "Straight lines"
            : style == LineStyle::Elbow ? "Right-angle lines" : "Curved lines";
        auto* entry = line_menu->addAction(QIcon(canvas_->line_style_preview(style, line_style_sample)), label);
        entry->setCheckable(true);
        entry->setChecked(style == canvas_->line_style());
        entry->setObjectName(style == LineStyle::Straight ? "lineStraight"
            : style == LineStyle::Elbow ? "lineElbow" : "lineCurved");
        line_actions_[style] = entry;
        connect(entry, &QAction::triggered, this, [this, style] { choose_line_style(style); });
    }
    // Where a new line meets each shape is the other thing Connect decides
    // about the lines it draws, so that choice sits on the same arrow. It is
    // remembered between sessions: it is a way of working, not a property of
    // one diagram.
    canvas_->set_join_mode(QSettings().value("joinMode", "clicked").toString() == "automatic"
        ? JoinMode::Automatic : JoinMode::WhereClicked);
    line_menu->addSeparator();
    auto* join_group = new QActionGroup(this);
    for (const auto mode : {JoinMode::WhereClicked, JoinMode::Automatic}) {
        auto* entry = line_menu->addAction(mode == JoinMode::WhereClicked ? "Join where I click" : "Join automatically");
        entry->setCheckable(true);
        entry->setChecked(mode == canvas_->join_mode());
        entry->setActionGroup(join_group);
        entry->setObjectName(mode == JoinMode::WhereClicked ? "joinWhereClicked" : "joinAutomatic");
        entry->setToolTip(mode == JoinMode::WhereClicked
            ? "Each end of a new line is pinned to the point you click on the shape. Drag a selected line's end to move it."
            : "Each end of a new line slides around its shape to face the other end as things move.");
        connect(entry, &QAction::triggered, this, [this, mode] {
            canvas_->set_join_mode(mode);
            QSettings().setValue("joinMode", mode == JoinMode::Automatic ? "automatic" : "clicked");
        });
    }
    auto* connect_button = new QToolButton(toolbar);
    connect_button->setObjectName("connectButton");
    connect_button->setDefaultAction(connect_action);
    connect_button->setMenu(line_menu);
    connect_button->setPopupMode(QToolButton::MenuButtonPopup);
    connect_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect_button->setIconSize(toolbar->iconSize());
    toolbar->addWidget(connect_button);
    tool_actions_[Tool::Connect] = connect_action;
    connect_button->installEventFilter(this);

    // Pictures and notes are the visual aids. A picture comes from a file, so
    // it is an action and lives on Insert; a note is put down by a click, so
    // it is a tool like the elements, sits with them after Connect, and locks
    // like them.
    auto* picture = new QAction("Picture…", this);
    picture->setObjectName("insertPicture");
    picture->setToolTip("Insert a picture from a file.");
    action_glyphs_[picture] = Glyph::Picture;
    connect(picture, &QAction::triggered, this, [this] { insert_picture_dialog(); });
    auto* note_action = new QAction("Note", this);
    note_action->setCheckable(true);
    note_action->setData("Note");
    note_action->setObjectName("toolNote");
    note_action->setActionGroup(group);
    action_glyphs_[note_action] = Glyph::Note;
    tool_actions_[Tool::Note] = note_action;
    connect(note_action, &QAction::triggered, this, [this] { choose_tool(Tool::Note, false); });
    toolbar->addAction(note_action);
    if (auto* button = toolbar->widgetForAction(note_action)) button->installEventFilter(this);
    // The picture in the menu bar too, for the keyboard and for anyone who
    // looks there first. The ribbon's Insert row is built from this menu, and
    // the canvas's right-click menu offers both a picture and a note.
    auto* insert_menu = new QMenu("&Insert", this);
    insert_menu->setObjectName("insertMenu");
    menuBar()->insertMenu(findChild<QMenu*>("viewMenu")->menuAction(), insert_menu);
    insert_menu->addAction(picture);
    // Characters that cannot be typed but are wanted constantly in this of all
    // editors: the relational algebra signs, the set and logic signs, arrows,
    // Greek, and the marks and emoji a note is annotated with. They go into a
    // name, a role or a description, so this is one gallery reached two ways
    // rather than two galleries. The ribbon's Insert row is built from this
    // menu, so the submenu appears there as a button of its own.
    // One entry rather than a submenu: a submenu's button carries a dropdown
    // arrow, and on the ribbon's Insert row the style puts that arrow beneath
    // the label, which makes the row taller than Home and breaks the tabs'
    // one promise, that every row is the same height. The gallery lists its
    // groups down its own side, so emoji are one click in rather than one
    // click out.
    auto* symbols = insert_menu->addAction("Symbols…");
    symbols->setObjectName("insertSymbols");
    symbols->setToolTip("Relational algebra, logic, arrows, Greek, marks, emoji and people, for names and notes.");
    action_glyphs_[symbols] = Glyph::Symbols;
    connect(symbols, &QAction::triggered, this, [this] { show_symbols(QStringLiteral("Relational algebra")); });
    canvas_->on_insert_picture = [this](QPointF at) { insert_picture_dialog(at); };
    canvas_->on_comment = [this](std::vector<domain::CommentTarget> targets) { add_comment(std::move(targets)); };

    // Notation follows Connect: it decides how the lines Connect draws are read.
    // The picker draws each option, so the cardinality symbols can be recognised
    // rather than remembered from a name.
    notation_separator_ = toolbar->addSeparator();
    // Named, so a reader who does not yet know the symbols knows what the
    // picker is for. The label goes with the picker whenever the bar has to
    // give it up.
    auto* notation_label = new QLabel(" Notation ", toolbar);
    notation_label->setObjectName("notationLabel");
    notation_label_action_ = toolbar->addWidget(notation_label);
    notation_box_ = new QComboBox(toolbar);
    notation_box_->setObjectName("notationPicker");
    notation_box_->setIconSize(notation_sample);
    notation_box_->setToolTip("How each participant's minimum and maximum are drawn.");
    // Held to the width of its drawings and a little text. The sample is what
    // the choice is made on; the name is only there to confirm it, and letting
    // it run to full length costs more of the toolbar than it is worth.
    notation_box_->setMaximumWidth(notation_sample.width() + 62);
    notation_box_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    notation_box_->setMinimumContentsLength(4);
    notation_box_->installEventFilter(wheel_guard_);
    for (const auto& [style, label] : notation_styles())
        notation_box_->addItem(QIcon(canvas_->notation_preview(style, notation_sample)), label,
                               QVariant::fromValue(static_cast<int>(style)));
    notation_box_->setCurrentIndex(static_cast<int>(canvas_->notation()));
    connect(notation_box_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (refreshing_ || index < 0) return;
        choose_notation(static_cast<Notation>(index));
    });
    notation_action_ = toolbar->addWidget(notation_box_);


    // Panning and framing are about looking rather than modelling, so they sit
    // on the canvas by what they act on instead of in the row of drawing tools.
    auto* pan_action = new QAction("Pan", this);
    pan_action->setCheckable(true);
    pan_action->setData("Pan");
    pan_action->setObjectName("toolPan");
    pan_action->setActionGroup(group);
    tool_actions_[Tool::Pan] = pan_action;
    action_glyphs_[pan_action] = Glyph::Pan;
    connect(pan_action, &QAction::triggered, this, [this] { choose_tool(Tool::Pan, false); });

    auto* fit = new QAction("Fit", this);
    fit->setObjectName("viewFit");
    fit->setShortcut(QKeySequence("Ctrl+0"));
    fit->setToolTip("Fit the whole diagram in the view.");
    connect(fit, &QAction::triggered, canvas_, &DiagramView::fit_diagram);
    action_glyphs_[fit] = Glyph::Fit;

    // Putting the panels away is about looking rather than modelling, so it
    // joins the controls on the canvas. It is written out in the View menu,
    // and named by its tooltip on the raft, where there is only room for a
    // picture.
    full_view_ = new QAction("Full view", this);
    full_view_->setObjectName("viewFullView");
    full_view_->setCheckable(true);
    action_glyphs_[full_view_] = Glyph::FullView;
    connect(full_view_, &QAction::toggled, this, [this](bool on) { set_full_view(on); });

    // What follows lives in the right-hand corner rather than in the row of
    // tools: checking the model and choosing a theme are about the whole
    // diagram, not about the next thing drawn on it.
    auto* stretch = new QWidget(toolbar);
    stretch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(stretch);
    toolbar->addSeparator();
    // One button for both halves of looking at the findings: it opens them,
    // and once they are open it puts them away again. The button says which
    // it will do by the mark it wears, and follows the panel however the panel
    // was opened or closed.
    check_ = toolbar->addAction("Check model", this, [this] {
        finish_field_edit();
        if (validation_dock_->isVisible()) {
            validation_dock_->hide();
            return;
        }
        refresh_validation();
        validation_dock_->show();
    });
    check_->setObjectName("checkModel");
    check_->setCheckable(true);
    action_glyphs_[check_] = Glyph::Check;
    connect(validation_dock_, &QDockWidget::visibilityChanged, this, [this] { refresh_check_action(); });


    // A small raft of view controls over the bottom-right of the canvas, where
    // a diagram is framed and zoomed rather than across the window from it.
    canvas_controls_ = new QWidget(canvas_);
    canvas_controls_->setObjectName("canvasControls");
    auto* stack = new QVBoxLayout(canvas_controls_);
    stack->setContentsMargins(4, 4, 4, 4);
    stack->setSpacing(2);
    // Every button on the raft is the same size and sits on the same centre
    // line, so the column reads as one control rather than as icons that
    // happen to be near some signs.
    stack->setAlignment(Qt::AlignHCenter);
    const auto raft_button = [&](QAction* action, const char* named) {
        auto* button = new QToolButton(canvas_controls_);
        button->setObjectName(named);
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setAutoRaise(true);
        button->setIconSize(QSize(18, 18));
        button->setFixedSize(26, 24);
        stack->addWidget(button, 0, Qt::AlignHCenter);
        return button;
    };
    raft_button(full_view_, "canvasFullView");
    raft_button(fit, "canvasFit");
    // Pan locks on a double-click, exactly as the tools on the toolbar do, so
    // a long look around the diagram does not need the button pressed again
    // after every drag.
    raft_button(pan_action, "canvasPan")->installEventFilter(this);
    // Zooming has no glyph of its own in either set, and a pair of signs says
    // what it does more plainly than a picture would at this size.
    for (const auto& [text, name, step] : std::initializer_list<std::tuple<const char*, const char*, int>>{
             {"+", "canvasZoomIn", 1}, {"\u2212", "canvasZoomOut", -1}}) {
        auto* button = new QToolButton(canvas_controls_);
        button->setObjectName(name);
        button->setText(QString::fromUtf8(text));
        button->setToolTip(step > 0 ? "Zoom in" : "Zoom out");
        button->setAutoRaise(true);
        button->setFixedSize(26, 24);
        connect(button, &QToolButton::clicked, this,
                [this, step] { if (step > 0) canvas_->zoom_in(); else canvas_->zoom_out(); });
        stack->addWidget(button, 0, Qt::AlignHCenter);
    }
    canvas_->installEventFilter(this);
    place_canvas_controls();

    auto* view = findChild<QMenu*>("viewMenu");
    view->addAction(full_view_);
    view->addSeparator();
    view->addAction(fit);
    view->addAction("Actual size", QKeySequence("Ctrl+1"), canvas_, &DiagramView::actual_size);
    view->addAction("Zoom in", QKeySequence::ZoomIn, canvas_, &DiagramView::zoom_in);
    view->addAction("Zoom out", QKeySequence::ZoomOut, canvas_, &DiagramView::zoom_out);
    // Dragging follows the pointer continuously by default. Aligning to the
    // grid rounds movement to the grid step, which reads as stuttering rather
    // than as help, so it stays available but off until it is asked for.
    for (bool align : {false, true}) {
        auto* action = view->addAction(align ? "Align to grid" : "Show grid");
        action->setCheckable(true);
        action->setChecked(!align);
        action->setObjectName(align ? "viewAlignToGrid" : "viewShowGrid");
        if (align) action->setToolTip("Line elements up on the grid as you move or place them.");
        connect(action, &QAction::toggled, this, [this, align](bool checked) {
            if (align) canvas_->set_align_to_grid(checked); else canvas_->set_grid_visible(checked);
        });
    }
    // Remarks left on the diagram. The switch is how the diagram is being
    // looked at rather than part of it, so it sits with the grid and is not
    // saved with the document: it quiets every remark at once, for reading the
    // model or taking a picture of it. The mark on a commented element stays
    // either way, because a remark nobody can see is a remark nobody can find.
    auto* show_comments = view->addAction("Show comments");
    show_comments->setCheckable(true);
    show_comments->setChecked(true);
    show_comments->setObjectName("viewShowComments");
    show_comments->setShortcut(QKeySequence("Ctrl+Shift+M"));
    show_comments->setToolTip("Show what has been said about the diagram when you point at it. "
                              "The mark on a commented element stays either way.");
    connect(show_comments, &QAction::toggled, this, [this](bool checked) { canvas_->set_comments_visible(checked); });
    canvas_->set_align_to_grid(false);
    canvas_->set_grid_visible(true);
    canvas_->set_comments_visible(true);
    // The paper the diagram is drawn on. It sits with the other choices about
    // how the diagram looks, and unlike them it travels with the document: a
    // diagram drawn on graph paper should open on graph paper.
    auto* backgrounds = view->addMenu("Background");
    backgrounds->setObjectName("backgroundMenu");
    auto* background_group = new QActionGroup(this);
    const std::array<std::pair<domain::BackgroundStyle, const char*>, 4> papers{{
        {domain::BackgroundStyle::Theme, "None"}, {domain::BackgroundStyle::Squares, "Squares"},
        {domain::BackgroundStyle::Lines, "Lines"}, {domain::BackgroundStyle::Dots, "Dots"}}};
    for (const auto& [style, label] : papers) {
        auto* action = backgrounds->addAction(QString::fromLatin1(label));
        action->setCheckable(true);
        action->setActionGroup(background_group);
        action->setObjectName(QString("background") + background_key(style));
        background_actions_[style] = action;
        connect(action, &QAction::triggered, this, [this, style] { choose_background(style); });
    }
    auto* own_picture = backgrounds->addAction("Picture…");
    own_picture->setCheckable(true);
    own_picture->setActionGroup(background_group);
    own_picture->setObjectName("backgroundImage");
    background_actions_[domain::BackgroundStyle::Image] = own_picture;
    connect(own_picture, &QAction::triggered, this, &MainWindow::choose_background_image);
    // How strongly a picture shows through, which is what keeps it behind the
    // diagram rather than in front of it. A ruling needs no such thing: it is
    // drawn as the ruling it is, so the bar is offered only for a picture.
    backgrounds->addSeparator();
    auto* strength_row = new QWidget(backgrounds);
    auto* strength_layout = new QHBoxLayout(strength_row);
    strength_layout->setContentsMargins(22, 4, 12, 4);
    strength_layout->setSpacing(8);
    strength_layout->addWidget(new QLabel("Strength", strength_row));
    auto* strength = new QSlider(Qt::Horizontal, strength_row);
    strength->setObjectName("backgroundStrength");
    strength->setRange(0, 100);
    strength->setMinimumWidth(120);
    strength->installEventFilter(wheel_guard_);
    auto* reading = new QLabel(strength_row);
    reading->setMinimumWidth(36);
    strength_layout->addWidget(strength, 1);
    strength_layout->addWidget(reading);
    auto* strength_action = new QWidgetAction(backgrounds);
    strength_action->setObjectName("backgroundStrengthRow");
    strength_action->setDefaultWidget(strength_row);
    backgrounds->addAction(strength_action);
    connect(strength, &QSlider::valueChanged, this, [this, reading](int value) {
        reading->setText(QString("%1%").arg(value));
        if (refreshing_) return;
        auto paper = editor_.project().background;
        if (paper.strength == value) return;
        paper.strength = static_cast<std::uint8_t>(value);
        show_result(editor_.set_background(std::move(paper)));
    });
    connect(backgrounds, &QMenu::aboutToShow, this, [this] { refresh_background_menu(); });

    // The same participants can be read in several notations. This is a display
    // choice, so it lives with the other view settings rather than in the file.
    auto* themes = view->addMenu("Theme");
    themes->setObjectName("themeMenu");
    auto* theme_group = new QActionGroup(this);
    for (const auto& entry : erdflow::desktop::themes()) {
        auto* action = themes->addAction(entry.label);
        action->setCheckable(true);
        action->setChecked(entry.id == theme_);
        action->setObjectName("theme" + QString(entry.key).remove('-'));
        action->setActionGroup(theme_group);
        theme_actions_[entry.id] = action;
        connect(action, &QAction::triggered, this, [this, id = entry.id] { set_theme(id); });
        // Hovering a name shows the theme on the whole window, which is the only
        // way to judge one: a palette is about how the diagram reads, not about
        // what it is called.
        connect(action, &QAction::hovered, this, [this, id = entry.id] { preview_theme(id); });
    }
    // Leaving the menu without choosing puts back what was chosen before.
    connect(themes, &QMenu::aboutToHide, this, [this] {
        if (theme_ != committed_theme_) apply_appearance(committed_theme_);
    });
    // Appearance is tried repeatedly rather than set once, so the same list is
    // put on the toolbar behind a button. It is the menu itself, not a copy, so
    // the two can never disagree about which theme is the current one.
    theme_button_ = new QToolButton(toolbar);
    theme_button_->setObjectName("themeButton");
    theme_button_->setText("Theme");
    theme_button_->setToolTip("Change the appearance of the window and the diagram.");
    theme_button_->setMenu(themes);
    theme_button_->setPopupMode(QToolButton::InstantPopup);
    theme_button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    theme_button_->setIconSize(toolbar->iconSize());
    // The icon set is a choice about appearance like the theme is, so it sits
    // in the same menu rather than somewhere of its own.
    auto* icon_menu = view->addMenu("Icons");
    auto* icon_group = new QActionGroup(this);
    for (const auto mode : {IconMode::Outline, IconMode::Normal, IconMode::Modern}) {
        auto* action = icon_menu->addAction(mode == IconMode::Modern ? "Modern"
                                          : mode == IconMode::Outline ? "Outline" : "Painted");
        action->setCheckable(true);
        action->setChecked(mode == icon_mode_);
        action->setObjectName("icons" + icon_mode_key(mode));
        action->setActionGroup(icon_group);
        icon_mode_actions_[mode] = action;
        connect(action, &QAction::triggered, this, [this, mode] { set_icon_mode(mode); });
    }

    // Outermost on the right: the corner is where a choice about the whole
    // window is looked for.
    toolbar->addSeparator();
    toolbar->addWidget(theme_button_);
    auto* notations = view->addMenu("Notation");
    auto* notation_group = new QActionGroup(this);
    for (const auto& [style, label] : notation_styles()) {
        auto* action = notations->addAction(label);
        action->setCheckable(true);
        action->setChecked(style == canvas_->notation());
        action->setObjectName("notation" + QString(label).remove(QRegularExpression("[^A-Za-z]")));
        action->setActionGroup(notation_group);
        notation_actions_[style] = action;
        connect(action, &QAction::triggered, this, [this, style] { choose_notation(style); });
    }
    full_view_->setToolTip("Full view — put the panels away and give the whole window to the diagram.");
    // Every action now exists, so give them their first icons. The window must
    // look right on its own, not only once a theme is chosen from outside it.
    refresh_icons();
    auto* help = menuBar()->addMenu("&Help");
    help->setObjectName("helpMenu");
    help->addAction("Quick guide", this, [this] {
        QMessageBox::information(this, "Drawing a conceptual ERD",
            "1. Choose Entity, Attribute, or Relationship and click the canvas.\n"
            "2. Use Connect, then click the two objects to link them.\n"
            "3. Select an object to edit its Properties. Field edits apply on focus loss.\n"
            "4. Select a relationship to set each participant's cardinality, participation, and role.\n"
            "5. Save your work as an .erdx project.\n\n"
            "Drag to move; Shift-click for multiple selection. Scroll to zoom.\n"
            "Escape cancels the current gesture. Delete removes the canvas selection.\n"
            "Unfinished diagrams can be saved; Model checks explain missing information.");
    });
    help->addAction("About ERDFlow", this, [this] {
        QMessageBox::about(this, "ERDFlow", "ERDFlow 0.1 · Conceptual editor foundation\n\n"
                           "Draw once, progressively refine.\nC++20 · Qt 6 · Local project files");
    });
}

void MainWindow::refresh() {
    if (refreshing_) return;
    refreshing_ = true;
    canvas_->synchronize();
    selection_ = canvas_->selected_elements();
    refresh_explorer();
    refresh_properties();
    refresh_validation();
    undo_->setEnabled(editor_.can_undo());
    redo_->setEnabled(editor_.can_redo());
    undo_->setText(editor_.can_undo() ? "Undo " + text(editor_.undo_label()) : "Undo");
    redo_->setText(editor_.can_redo() ? "Redo " + text(editor_.redo_label()) : "Redo");
    // An explicitly set icon text survives setText, so the toolbar keeps its
    // fixed wording and only the tooltip follows the named edit.
    undo_->setToolTip(undo_->text() + "\t" + undo_->shortcut().toString(QKeySequence::NativeText));
    redo_->setToolTip(redo_->text() + "\t" + redo_->shortcut().toString(QKeySequence::NativeText));
    refresh_selection_commands();
    refresh_download_actions();
    auto title = text(editor_.project().name);
    document_label_->setText(title + (editor_.dirty() ? " · Unsaved" : ""));
    setWindowTitle(title + "[*] — ERDFlow");
    setWindowModified(editor_.dirty());
    const auto& project = editor_.project();
    count_label_->setText(QString("  %1 entities  ·  %2 attributes  ·  %3 relationships  ")
        .arg(project.entities.size()).arg(project.attributes.size()).arg(project.relationships.size()));
    refreshing_ = false;
}

void MainWindow::refresh_explorer() {
    // The tree is rebuilt on every change, so what the user had opened is
    // noted first and reopened after, or each edit would fold it all shut.
    std::set<QString> opened;
    const auto identity = [](const QModelIndex& index) {
        const auto id = index.data(Qt::UserRole).toString();
        return id.isEmpty() ? index.data(Qt::DisplayRole).toString() : id;
    };
    const auto remember = [&](auto&& self, const QModelIndex& parent) -> void {
        for (int row = 0; row < explorer_model_->rowCount(parent); ++row) {
            const auto index = explorer_model_->index(row, 0, parent);
            if (explorer_->isExpanded(index)) opened.insert(identity(index));
            self(self, index);
        }
    };
    const bool first_build = explorer_model_->rowCount() == 0;
    remember(remember, QModelIndex());

    explorer_model_->clear();
    references_.clear();
    auto* project = new QStandardItem(text(editor_.project().name));
    project->setData("project", Qt::UserRole);
    project->setEditable(false);
    explorer_model_->appendRow(project);
    // Each group and every row under it wears the same glyph the toolbar uses to
    // place that kind of element, so the tree reads as the diagram does. They
    // are built from the active theme and icon set, which is why the tree is
    // rebuilt when either changes.
    const auto& colors = theme(theme_);
    const auto attribute_badge = glyph_icon(Glyph::Attribute, colors, 22, icon_mode_);
    const auto& attributes = editor_.project().attributes;
    const auto owned_by = [&](const ElementRef& owner) {
        std::vector<AttributeId> owned;
        for (const auto& [id, attribute] : attributes)
            if (attribute.owner && *attribute.owner == owner) owned.push_back(id);
        return owned;
    };
    // An element's own attributes are listed beneath it, one level in, so an
    // entity can be opened to see what belongs to it. A composite attribute
    // lists its parts the same way. These are the same attributes the group
    // below counts; here they are shown by what they belong to.
    const auto nest = [&](auto&& self, QStandardItem* under, const ElementRef& owner) -> void {
        for (const auto id : owned_by(owner)) {
            const ElementRef ref{id};
            auto* row = new QStandardItem(attribute_badge, display_name(editor_.project(), ref));
            row->setData(key(ref), Qt::UserRole);
            row->setToolTip(kind_label(ref) + " · " + key(ref));
            under->appendRow(row);
            references_.emplace(key(ref), ref);
            self(self, row, ref);
        }
    };
    const auto append = [&](const QString& label, Glyph glyph, const auto& collection, bool with_owned) {
        const auto badge = glyph_icon(glyph, colors, 22, icon_mode_);
        auto* group = new QStandardItem(badge, label + QString(" (%1)").arg(collection.size()));
        group->setSelectable(false);
        // The group is known by a key of its own rather than by its text, whose
        // count changes with every element added: an open group that changed
        // its number must still be the same open group.
        group->setData("group:" + label, Qt::UserRole);
        project->appendRow(group);
        for (const auto& [id, item] : collection) {
            (void)item;
            const ElementRef ref{id};
            auto* row = new QStandardItem(badge, display_name(editor_.project(), ref));
            row->setData(key(ref), Qt::UserRole);
            row->setToolTip(kind_label(ref) + " · " + key(ref));
            group->appendRow(row);
            references_.emplace(key(ref), ref);
            if (with_owned) nest(nest, row, ref);
        }
    };
    append("Entities", Glyph::Entity, editor_.project().entities, true);
    append("Attributes", Glyph::Attribute, attributes, false);
    append("Relationships", Glyph::Relationship, editor_.project().relationships, true);
    // Visual aids are listed only when there are any, so a diagram without
    // them is not shown two empty groups.
    if (!editor_.project().pictures.empty()) append("Pictures", Glyph::Picture, editor_.project().pictures, false);
    if (!editor_.project().notes.empty()) append("Notes", Glyph::Note, editor_.project().notes, false);

    // The groups start open and the elements under them folded, so the tree
    // shows what there is without spilling every attribute twice. After that
    // it keeps whatever the user has opened.
    if (first_build) {
        explorer_->expandToDepth(1);
    } else {
        const auto reopen = [&](auto&& self, const QModelIndex& parent) -> void {
            for (int row = 0; row < explorer_model_->rowCount(parent); ++row) {
                const auto index = explorer_model_->index(row, 0, parent);
                if (opened.contains(identity(index))) explorer_->expand(index);
                self(self, index);
            }
        };
        reopen(reopen, QModelIndex());
    }
    highlight_explorer();
}

void MainWindow::highlight_explorer() {
    const QSignalBlocker blocker(explorer_->selectionModel());
    explorer_->selectionModel()->clearSelection();
    for (const auto ref : selection_) {
        // An attribute is listed both under its owner and in the group of all
        // attributes, and both rows should light up for it.
        const auto matches = explorer_model_->match(explorer_model_->index(0, 0), Qt::UserRole,
                                                    key(ref), -1, Qt::MatchExactly | Qt::MatchRecursive);
        for (const auto& match : matches)
            explorer_->selectionModel()->select(match, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
}

void MainWindow::refresh_properties() {
    if (auto* old = properties_->takeWidget()) { old->hide(); old->deleteLater(); }
    auto* panel = new QWidget(properties_);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    if (selection_.size() != 1) {
        layout->addWidget(hint(selection_.empty()
            ? "Select an object to edit its name, description, and conceptual properties."
            : QString("%1 objects selected. Drag to move together, or use Edit to duplicate or delete.").arg(selection_.size()), panel));
        layout->addStretch();
        properties_->setWidget(panel);
        return;
    }
    const auto ref = selection_.front();
    const auto& project = editor_.project();
    const auto& colors = theme(theme_);
    // Every field label is written the same way: bold, in the theme's own ink.
    // The rows are already told apart by their words and by the controls
    // beside them, so colouring each one would be decoration, and it would
    // compete with the one colour here that carries meaning -- the colour an
    // element is drawn in on the diagram, worn by its heading and its name.
    const auto label_tone = colors.text;
    // A choice whose words are long must not force the panel wider than the
    // window can spare: the closed box shortens to what it is given and the
    // list still reads in full when it is opened.
    const auto narrowable = [this](QComboBox* box) {
        box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        box->setMinimumContentsLength(8);
        box->installEventFilter(wheel_guard_);
        return box;
    };
    // One element written inside its own shape, for a card.
    // The proportions an element is drawn with on the diagram.
    const auto aspect_of = [&project](const ElementRef& element) {
        const auto found = project.layout.find(element);
        if (found == project.layout.end() || found->second.height <= 0) return 2.0;
        return found->second.width / found->second.height;
    };
    const auto shaped_tag = [this, &project, &colors, aspect_of](const ElementRef& element, const char* named, QWidget* parent) {
        const auto pointed = std::holds_alternative<RelationshipId>(element)
                          || std::holds_alternative<SpecializationId>(element);
        return static_cast<QWidget*>(new ShapedTag(
            [this, element](QSize size) { return canvas_->element_preview(element, size, true); },
            display_name(project, element), readable_on(surface_of(project, colors, element)),
            aspect_of(element), pointed, named, parent));
    };
    auto* heading = new QLabel(kind_label(project, ref), panel);
    heading->setObjectName("propertyHeading");
    // The heading says what kind of thing this is, so it stays a title in the
    // theme's own accent. The element's colour is carried by the name field
    // below it and by the cards; wearing it here as well would say the same
    // thing twice and leave the panel shouting.
    const auto surface = surface_of(project, colors, ref);
    const auto ink = readable_on(surface);
    layout->addWidget(heading);
    auto* form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    // The name is written inside the shape the element is drawn as, in the ink
    // that reads on its colour, so what is being named is plain.
    const bool pointed = std::holds_alternative<RelationshipId>(ref)
                      || std::holds_alternative<SpecializationId>(ref);
    auto* shaped = new ShapedName([this, ref](QSize size) { return canvas_->element_preview(ref, size, true); },
                                  aspect_of(ref), text(name(project, ref)), pointed, panel);
    auto* name_edit = new QLineEdit(text(name(project, ref)), shaped);
    name_edit->setObjectName("elementName");
    name_edit->setAlignment(Qt::AlignCenter);
    shaped->hold(name_edit);
    // The name field carries the same colour. Its border is darkened from the
    // fill rather than taken from the theme, which would otherwise draw a line
    // the element's colour knows nothing about around it.
    // The focus ring has to be restated: a style set on the widget wins over the
    // application's, so the theme's focus rule no longer reaches this field, and
    // it is drawn in the ink rather than the accent, which is the one colour
    // already known to contrast with whatever fill the element carries.
    name_edit->setStyleSheet(QStringLiteral(
        "QLineEdit#elementName { background: transparent; color: %1; border: 1px solid transparent;"
        " border-radius: 2px; font-size: 13px; font-weight: 700; }"
        "QLineEdit#elementName:focus { border: 1px solid %1; }")
        .arg(ink.name()));
    name_edit->setMaxLength(512);
    name_edit->setToolTip(text(name(project, ref)));
    // A name longer than its shape is read from its beginning.
    name_edit->setCursorPosition(0);
    // Right-clicking a chosen word offers to leave a remark on that word alone,
    // beneath the cut-and-paste entries the field already has: a comment on a
    // name is often about one part of it rather than the whole of it.
    offer_text_comment(name_edit);
    form->addRow(field_label("Name", label_tone, panel), shaped);
    connect(name_edit, &QLineEdit::editingFinished, this, [this, ref, name_edit] {
        if (refreshing_ || !exists(editor_.project(), ref)) return;
        const auto value = bytes(name_edit->text());
        if (value != name(editor_.project(), ref)) show_result(editor_.rename(ref, value));
    });
    layout->addLayout(form);
    if (const auto* entity_id = std::get_if<EntityId>(&ref)) {
        // Regular or weak. A weak entity is identified through an identifying
        // relationship rather than by a key of its own.
        const auto& entity = project.entities.at(*entity_id);
        auto* kind = narrowable(new QComboBox(panel));
        kind->setObjectName("entityKind");
        kind->addItem("Regular — identified by its own key");
        kind->addItem("Weak — identified through a relationship");
        kind->setCurrentIndex(entity.weak ? 1 : 0);
        connect(kind, &QComboBox::activated, this, [this, id = *entity_id](int index) {
            if (refreshing_) return;
            show_result(editor_.set_entity_weak(id, index == 1));
        });
        auto* kind_form = new QFormLayout;
        kind_form->setRowWrapPolicy(QFormLayout::WrapAllRows);
        kind_form->addRow(field_label("Kind", label_tone, panel), kind);
        layout->addLayout(kind_form);
        if (entity.weak)
            layout->addWidget(hint("Drawn with a double border. Connect it to an identifying relationship, usually with total "
                                   "participation; its key attribute is a partial key, underlined in dashes.", panel));
        // An entity that relates to itself, as an employee who manages
        // employees does. Ticking it draws the relationship that says so;
        // clearing it takes those relationships away again.
        auto* recursive = new QCheckBox("Recursive — relates to itself", panel);
        recursive->setObjectName("entityRecursive");
        recursive->setChecked(!recursions_of(project, *entity_id).empty());
        recursive->setToolTip("A relationship from this entity back to itself. Give each side a role to say which is which.");
        connect(recursive, &QCheckBox::clicked, this, [this, id = *entity_id](bool on) {
            if (refreshing_) return;
            const auto& current = editor_.project();
            const auto existing = recursions_of(current, id);
            if (!on) {
                if (!existing.empty()) show_result(editor_.erase(existing));
                return;
            }
            if (!existing.empty()) return;
            // Placed off the entity's side, which is where the loop is drawn
            // from, so the shape is right the moment it appears.
            const auto found = current.layout.find(ElementRef{id});
            const auto box = found == current.layout.end() ? domain::Rect{} : found->second;
            const domain::Rect body{box.x + box.width + 130,
                                    box.y + box.height / 2 - relationship_body.height / 2,
                                    relationship_body.width, relationship_body.height};
            const auto result = editor_.relate(id, id, body, "Relationship");
            show_result(result);
            if (result && result.created) canvas_->select_elements({*result.created}, true);
        });
        layout->addWidget(recursive);
        if (!recursions_of(project, *entity_id).empty())
            layout->addWidget(hint("The second side loops back around the entity. Drag the line to shape it by hand, "
                                   "or double-click it to hand it back.", panel));
    }
    if (const auto* picture_id = std::get_if<PictureId>(&ref)) {
        // The picture itself, small, so the panel says which picture this is.
        const auto& picture = project.pictures.at(*picture_id);
        QPixmap pixmap;
        pixmap.loadFromData(picture.image.data(), static_cast<uint>(picture.image.size()));
        auto* preview = new QLabel(panel);
        preview->setObjectName("picturePreview");
        preview->setAlignment(Qt::AlignCenter);
        if (!pixmap.isNull()) preview->setPixmap(pixmap.scaled(240, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        layout->addWidget(preview);
        layout->addWidget(hint(QString("%1 × %2 pixels · %3 KB").arg(pixmap.width()).arg(pixmap.height())
                                   .arg((picture.image.size() + 1023) / 1024), panel));
        layout->addWidget(hint("A picture is a visual aid: it is not part of the model and nothing connects to it. "
                               "Set its size below; it keeps its proportions.", panel));
    }
    if (const auto* note_id = std::get_if<NoteId>(&ref)) {
        const bool symbol = project.notes.at(*note_id).plain;
        layout->addWidget(hint(symbol
                                   ? "The name is the character that is drawn. It is drawn on its own, with nothing "
                                     "behind it, and fills whatever size it is given."
                                   : "The name is the note's title, drawn bold; the text below is drawn beneath it.",
                               panel));
        if (symbol) {
            // One number rather than the width and height below, because a
            // symbol is drawn to the smaller of the two and so is square in
            // practice. The two fields are still there for anyone who wants a
            // box that is not; this row keeps the common case to one figure.
            const auto box = project.layout.at(ref);
            auto* size_form = new QFormLayout;
            size_form->setRowWrapPolicy(QFormLayout::WrapAllRows);
            auto* size = new QSpinBox(panel);
            size->setObjectName("symbolSize");
            size->setRange(static_cast<int>(min_symbol_size), static_cast<int>(max_symbol_size));
            size->setValue(static_cast<int>(std::round(std::min(box.width, box.height))));
            size->setSingleStep(8);
            size->setSuffix(" units");
            size->setKeyboardTracking(false);
            size->setToolTip("How big the character is drawn. Its corners on the diagram do the same thing by hand.");
            size->installEventFilter(wheel_guard_);
            connect(size, &QSpinBox::valueChanged, this, [this, ref](int value) {
                if (refreshing_ || !exists(editor_.project(), ref)) return;
                const auto current = editor_.project().layout.at(ref);
                const double side = value;
                // Grown about its centre, so a symbol stays where it was put
                // instead of walking down and to the right as it is enlarged.
                show_result(editor_.resize_symbols({{ref, {current.x + (current.width - side) / 2,
                                                           current.y + (current.height - side) / 2, side, side}}}),
                            false);
            });
            size_form->addRow(field_label("Size", label_tone, panel), size);
            layout->addLayout(size_form);
        }
    }
    if (const auto* id = std::get_if<AttributeId>(&ref)) {
        const auto attribute_id = *id;
        const auto attribute = project.attributes.at(attribute_id);
        auto* attr_form = new QFormLayout;
        attr_form->setRowWrapPolicy(QFormLayout::WrapAllRows);
        auto* kind = narrowable(new QComboBox(panel));
        kind->setObjectName("attributeKind");
        kind->addItems({"Normal", "Key", "Composite", "Multivalued", "Derived"});
        kind->setCurrentIndex(static_cast<int>(attribute.kind));
        attr_form->addRow(field_label("Attribute kind", label_tone, panel), kind);
        connect(kind, &QComboBox::activated, this, [this, attribute_id](int index) {
            show_result(editor_.set_attribute_kind(attribute_id, static_cast<AttributeKind>(index)));
        });
        auto* owner = narrowable(new QComboBox(panel));
        owner->setObjectName("attributeOwner");
        owner->addItem("Unassigned");
        std::vector<std::optional<AttributeOwner>> owners{{}};
        for (const auto& [candidate_key, candidate] : references_) {
            (void)candidate_key;
            if (candidate == ref || is_figure(candidate)) continue;
            if (const auto* attr = std::get_if<AttributeId>(&candidate);
                attr && project.attributes.at(*attr).kind != AttributeKind::Composite) continue;
            if (attribute.kind == AttributeKind::Key && std::holds_alternative<RelationshipId>(candidate)) continue;
            owners.push_back(candidate);
            owner->addItem(kind_label(candidate) + ": " + display_name(project, candidate));
            if (attribute.owner == owners.back()) owner->setCurrentIndex(owner->count() - 1);
        }
        attr_form->addRow(field_label("Owner", label_tone, panel), owner);
        connect(owner, &QComboBox::activated, this, [this, attribute_id, owners](int index) {
            show_result(editor_.set_attribute_owner(attribute_id, owners.at(static_cast<std::size_t>(index))));
        });
        layout->addLayout(attr_form);
        layout->addWidget(hint("Composite attributes can own other attributes. Key attributes belong to entities.", panel));
    }
    if (const auto* specialization_id = std::get_if<SpecializationId>(&ref)) {
        const auto& specialization = project.specializations.at(*specialization_id);
        layout->addWidget(hint("An ISA triangle. Connect the entity it generalises first; every entity connected "
                               "after that becomes a subtype. The two rules below decide how it converts to relations.", panel));
        // The triangle points the way the hierarchy is read, so the direction is
        // an editable property rather than only a choice made at creation.
        auto* direction = narrowable(new QComboBox(panel));
        direction->setObjectName("specializationDirection");
        direction->addItem("Specialization — points down at the subtypes");
        direction->addItem("Generalization — points up at the supertype");
        direction->setCurrentIndex(specialization.direction == Inheritance::Generalization ? 1 : 0);
        connect(direction, &QComboBox::activated, this, [this, id = *specialization_id, direction](int index) {
            if (refreshing_) return;
            (void)direction;
            show_result(editor_.set_inheritance_direction(id,
                index == 1 ? Inheritance::Generalization : Inheritance::Specialization));
        });
        auto* direction_form = new QFormLayout;
        direction_form->setRowWrapPolicy(QFormLayout::WrapAllRows);
        direction_form->addRow(field_label("Direction", label_tone, panel), direction);
        layout->addLayout(direction_form);
        auto* super = new QLabel(specialization.supertype
            ? "Supertype: " + display_name(project, ElementRef{*specialization.supertype})
            : QStringLiteral("Supertype: not connected yet"), panel);
        super->setObjectName("specializationSupertype");
        super->setStyleSheet(QStringLiteral("QLabel#specializationSupertype { color: %1; font-weight: 700; }")
                                 .arg(label_tone.name()));
        layout->addWidget(super);
        if (specialization.supertype) {
            auto* detach_super = new QPushButton("Detach supertype", panel);
            detach_super->setObjectName("detachSupertype");
            connect(detach_super, &QPushButton::clicked, this, [this, id = *specialization_id] {
                show_result(editor_.set_supertype(id, {}));
            });
            layout->addWidget(detach_super);
        }
        // Disjoint or overlapping, and total or partial, are exactly the inputs
        // a later Conceptual to Relational conversion needs to choose a mapping.
        auto* constraint = narrowable(new QComboBox(panel));
        constraint->setObjectName("specializationConstraint");
        constraint->addItem("Disjoint — at most one subtype");
        constraint->addItem("Overlapping — may be several subtypes");
        constraint->setCurrentIndex(specialization.constraint == Disjointness::Overlapping ? 1 : 0);
        auto* completeness = narrowable(new QComboBox(panel));
        completeness->setObjectName("specializationCompleteness");
        completeness->addItem("Partial — need not be any subtype");
        completeness->addItem("Total — must be some subtype");
        completeness->setCurrentIndex(specialization.completeness == Completeness::Total ? 1 : 0);
        const auto apply_rules = [this, id = *specialization_id, constraint, completeness] {
            if (refreshing_) return;
            show_result(editor_.set_specialization_rules(id,
                constraint->currentIndex() == 1 ? Disjointness::Overlapping : Disjointness::Disjoint,
                completeness->currentIndex() == 1 ? Completeness::Total : Completeness::Partial));
        };
        connect(constraint, &QComboBox::activated, this, [apply_rules](int) { apply_rules(); });
        connect(completeness, &QComboBox::activated, this, [apply_rules](int) { apply_rules(); });
        auto* rules = new QFormLayout;
        rules->setRowWrapPolicy(QFormLayout::WrapAllRows);
        rules->addRow(field_label("Constraint", label_tone, panel), constraint);
        rules->addRow(field_label("Completeness", label_tone, panel), completeness);
        layout->addLayout(rules);
        for (const auto& subtype : specialization.subtypes) {
            auto* card = new QWidget(panel);
            card->setObjectName("participantCard");
            auto* row = new QFormLayout(card);
            row->setRowWrapPolicy(QFormLayout::WrapAllRows);
            row->addRow(card_title(shaped_tag(ElementRef{subtype}, "cardShape", card),
                                   shaped_tag(ref, "cardTowardShape", card), card));
            auto* detach = new QPushButton("Detach subtype", card);
            connect(detach, &QPushButton::clicked, this, [this, id = *specialization_id, subtype] {
                show_result(editor_.detach_subtype(id, subtype));
            });
            row->addRow(detach);
            layout->addWidget(card);
        }
    }
    if (const auto* id = std::get_if<RelationshipId>(&ref)) {
        const auto relationship_id = *id;
        const auto& relationship = project.relationships.at(*id);
        // Regular, identifying or associative. An identifying relationship is
        // the one a weak entity is identified through; an associative one
        // keeps its own identity, so it can take part in further relationships
        // exactly as an entity does.
        auto* kind = narrowable(new QComboBox(panel));
        kind->setObjectName("relationshipKind");
        kind->addItem("Regular");
        kind->addItem("Identifying — identifies a weak entity");
        kind->addItem("Associative — has an identity of its own");
        kind->setCurrentIndex(static_cast<int>(relationship_kind(relationship)));
        kind->setToolTip("An identifying relationship is drawn as a double diamond; an associative one as a diamond in a box.");
        connect(kind, &QComboBox::activated, this, [this, id = relationship.id](int index) {
            if (refreshing_) return;
            const auto found_relationship = editor_.project().relationships.find(id);
            if (found_relationship == editor_.project().relationships.end()) return;
            const auto chosen = static_cast<RelationshipKind>(index);
            // An associative entity takes the entity body size, since that is
            // what it behaves as. Keep it centred so the diagram does not shift.
            std::optional<domain::Rect> body;
            const bool was = found_relationship->second.associative;
            const bool becomes = chosen == RelationshipKind::Associative;
            const auto found = editor_.project().layout.find(domain::ElementRef{id});
            if (was != becomes && found != editor_.project().layout.end()) {
                const auto& size = becomes ? entity_body : relationship_body;
                const auto& current = found->second;
                body = domain::Rect{current.x + current.width / 2 - size.width / 2,
                                    current.y + current.height / 2 - size.height / 2,
                                    size.width, size.height};
            }
            show_result(editor_.set_relationship_kind(id, chosen, body));
        });
        auto* kind_form = new QFormLayout;
        kind_form->setRowWrapPolicy(QFormLayout::WrapAllRows);
        kind_form->addRow(field_label("Kind", label_tone, panel), kind);
        layout->addLayout(kind_form);
        // The ratio is the two maximums read together. Setting it here writes
        // both participants at once, so every notation redraws consistently.
        auto* ratio = narrowable(new QComboBox(panel));
        ratio->setObjectName("relationshipRatio");
        const std::array<std::pair<Cardinality, Cardinality>, 4> ratios{{
            {Cardinality::One, Cardinality::One}, {Cardinality::One, Cardinality::Many},
            {Cardinality::Many, Cardinality::One}, {Cardinality::Many, Cardinality::Many}
        }};
        for (const auto& [first, second] : ratios)
            ratio->addItem(QString("%1:%2").arg(first == Cardinality::One ? "1" : "M",
                                                second == Cardinality::One ? "1" : "M"));
        const bool binary = relationship.participants.size() == 2;
        ratio->setEnabled(binary);
        if (binary) {
            const auto current = std::make_pair(relationship.participants[0].maximum,
                                                relationship.participants[1].maximum);
            ratio->setCurrentIndex(static_cast<int>(
                std::find(ratios.begin(), ratios.end(), current) - ratios.begin()));
        }
        connect(ratio, &QComboBox::activated, this, [this, id = relationship.id, ratios](int index) {
            if (refreshing_ || index < 0) return;
            show_result(editor_.set_ratio(id, ratios[static_cast<std::size_t>(index)].first,
                                              ratios[static_cast<std::size_t>(index)].second));
        });
        auto* reverse = new QPushButton("Reverse sides", panel);
        reverse->setObjectName("reverseRelationship");
        reverse->setEnabled(binary);
        reverse->setToolTip("Swap the two sides' constraints, turning 1:M into M:1.");
        connect(reverse, &QPushButton::clicked, this, [this, id = relationship.id] {
            show_result(editor_.reverse_participants(id));
        });
        auto* ratio_form = new QFormLayout;
        ratio_form->setRowWrapPolicy(QFormLayout::WrapAllRows);
        ratio_form->addRow(field_label("Ratio", label_tone, panel), ratio);
        layout->addLayout(ratio_form);
        layout->addWidget(reverse);
        layout->addWidget(hint(binary
            ? "The ratio reads left to right in the order the sides are listed below, and matches whichever notation the toolbar is showing."
            : "A ratio describes exactly two sides. Set each side's own constraints below.", panel));
        layout->addWidget(hint("Connect this relationship to entities, or to another relationship when this one is associative. Each connection has its own role and constraints.", panel));
        for (const auto& participant : relationship.participants) {
            auto* card = new QWidget(panel);
            card->setObjectName("participantCard");
            auto* participant_form = new QFormLayout(card);
            participant_form->setRowWrapPolicy(QFormLayout::WrapAllRows);
            const auto side = target_ref(participant.target);
            participant_form->addRow(card_title(shaped_tag(side, "cardShape", card),
                                                shaped_tag(ref, "cardTowardShape", card), card));
            auto* maximum = narrowable(new QComboBox(card));
            maximum->addItems({"1 — One", "M — Many"});
            maximum->setCurrentIndex(participant.maximum == Cardinality::One ? 0 : 1);
            participant_form->addRow(field_label("Maximum cardinality", label_tone, card), maximum);
            auto* participation = narrowable(new QComboBox(card));
            participation->addItems({"Partial — optional", "Total — required"});
            participation->setCurrentIndex(participant.participation == Participation::Total ? 1 : 0);
            participant_form->addRow(field_label("Participation", label_tone, card), participation);
            auto* role = new QLineEdit(text(participant.role), card);
            role->setPlaceholderText("Role (especially for recursive links)");
            role->setMaxLength(512);
            participant_form->addRow(field_label("Role", label_tone, card), role);
            const auto apply = [this, relationship_id, participant, maximum, participation, role] {
                if (refreshing_ || !editor_.project().relationships.contains(relationship_id)) return;
                const auto max = maximum->currentIndex() == 0 ? Cardinality::One : Cardinality::Many;
                const auto part = participation->currentIndex() == 0 ? Participation::Partial : Participation::Total;
                const auto role_value = bytes(role->text());
                if (max != participant.maximum || part != participant.participation || role_value != participant.role)
                    show_result(editor_.update_participant(relationship_id, participant.id, max, part, role_value));
            };
            connect(maximum, &QComboBox::activated, this, [apply](int) { apply(); });
            connect(participation, &QComboBox::activated, this, [apply](int) { apply(); });
            connect(role, &QLineEdit::editingFinished, this, apply);
            auto* disconnect = new QPushButton("Disconnect", card);
            connect(disconnect, &QPushButton::clicked, this, [this, relationship_id, participant] {
                show_result(editor_.disconnect(relationship_id, participant.id));
            });
            participant_form->addRow(disconnect);
            layout->addWidget(card);
        }
    }
    // A card note draws its text beneath its title; a symbol draws neither, so
    // for a symbol the field is a description like any other element's.
    bool note = false;
    if (const auto* note_id = std::get_if<NoteId>(&ref)) note = !project.notes.at(*note_id).plain;
    layout->addWidget(field_label(note ? "Text" : "Description", label_tone, panel));
    auto* description_edit = new DescriptionEdit(panel);
    description_edit->setObjectName("elementDescription");
    description_edit->setPlainText(text(description(project, ref)));
    description_edit->setPlaceholderText(note ? "Write the note’s text…" : "Explain this object’s meaning…");
    description_edit->setFixedHeight(100);
    offer_text_comment(description_edit);
    description_edit->commit = [this, ref, description_edit] {
        if (refreshing_ || !exists(editor_.project(), ref)) return;
        const auto value = bytes(description_edit->toPlainText());
        if (value != description(editor_.project(), ref)) show_result(editor_.describe(ref, value));
    };
    layout->addWidget(description_edit);
    layout->addWidget(hint("Text changes apply when you leave the field. Every applied change can be undone.", panel));
    auto* geometry = new QFormLayout;
    const auto rect = project.layout.at(ref);
    std::array<QDoubleSpinBox*, 4> fields{};
    const std::array<double, 4> values{rect.x, rect.y, rect.width, rect.height};
    const std::array<QString, 4> names{"X", "Y", "Width", "Height"};
    for (std::size_t i = 0; i < fields.size(); ++i) {
        fields[i] = new QDoubleSpinBox(panel);
        fields[i]->setDecimals(0);
        fields[i]->setRange(i < 2 ? -max_coordinate : 24, max_coordinate);
        fields[i]->setValue(values[i]);
        fields[i]->setKeyboardTracking(false);
        fields[i]->setObjectName("geometry" + names[i]);
        fields[i]->installEventFilter(wheel_guard_);
        geometry->addRow(field_label(names[i], label_tone, panel), fields[i]);
    }
    layout->addLayout(geometry);
    auto* apply_geometry = new QPushButton("Apply position and size", panel);
    apply_geometry->setObjectName("applyGeometry");
    connect(apply_geometry, &QPushButton::clicked, this, [this, ref, fields] {
        show_result(editor_.move({{ref, {fields[0]->value(), fields[1]->value(), fields[2]->value(), fields[3]->value()}}}));
    });
    layout->addWidget(apply_geometry);
    build_comment_section(panel, layout);
    layout->addStretch();
    properties_->setWidget(panel);
}

void MainWindow::build_comment_section(QWidget* panel, QVBoxLayout* layout) {
    if (selection_.size() != 1) return;
    const auto ref = selection_.front();
    const auto& project = editor_.project();
    const auto pinned = comments_on(project, ref);
    if (pinned.empty()) return;
    const auto& colors = theme(theme_);

    auto* heading = new QLabel(pinned.size() == 1 ? "Comment" : QString("Comments (%1)").arg(pinned.size()), panel);
    heading->setObjectName("commentHeading");
    heading->setStyleSheet(QString("font-weight: 700; color: %1;").arg(colors.text.name()));
    layout->addWidget(heading);

    for (const auto& id : pinned) {
        const auto& comment = project.comments.at(id);
        auto* row = new QWidget(panel);
        row->setObjectName("commentRow");
        auto* rows = new QVBoxLayout(row);
        rows->setContentsMargins(10, 8, 10, 8);
        rows->setSpacing(6);
        // A remark stands on the theme's warning colour, faintly, so it reads
        // as something said about the diagram rather than as part of it -- the
        // same hue the mark on the canvas wears, so the two are plainly one
        // thing seen in two places.
        row->setStyleSheet(QString("QWidget#commentRow { background: %1; border: 1px solid %2;"
                                   " border-radius: 4px; }")
                               .arg(over(colors.panel, QColor(colors.warning.red(), colors.warning.green(),
                                                              colors.warning.blue(), 28)).name(),
                                    colors.border.name()));

        auto* said = new QLabel(text(comment.text), row);
        said->setObjectName("commentSaid");
        said->setWordWrap(true);
        said->setTextInteractionFlags(Qt::TextSelectableByMouse);
        // A remark that has been put away is still readable here, faded, so it
        // can be brought back; putting away quiets the diagram, not the panel.
        if (comment.hidden) said->setStyleSheet(QString("color: %1; font-style: italic;").arg(colors.muted.name()));
        rows->addWidget(said);

        // Where else this one remark is pinned, since a remark covering several
        // things is read differently from one about this alone.
        if (comment.targets.size() > 1) {
            const auto others = comment.targets.size() - 1;
            auto* elsewhere = new QLabel(others == 1 ? QString("Also on 1 other thing.")
                                                     : QString("Also on %1 other things.").arg(others), row);
            elsewhere->setObjectName("commentElsewhere");
            elsewhere->setStyleSheet(QString("color: %1; font-size: 11px;").arg(colors.muted.name()));
            rows->addWidget(elsewhere);
        }

        auto* buttons = new QHBoxLayout;
        buttons->setSpacing(6);
        auto* edit = new QPushButton("Edit…", row);
        edit->setObjectName("commentEdit");
        connect(edit, &QPushButton::clicked, this, [this, id] {
            const auto found = editor_.project().comments.find(id);
            if (found == editor_.project().comments.end()) return;
            const auto words = ask_for_comment(text(found->second.text), {});
            if (!words.isEmpty()) show_result(editor_.set_comment_text(id, bytes(words)), false);
        });
        auto* put_away = new QPushButton(comment.hidden ? "Show" : "Hide", row);
        put_away->setObjectName("commentHide");
        put_away->setToolTip(comment.hidden
            ? "Bring this remark back, so pointing at what it is pinned to shows it again."
            : "Put this remark away without deleting it. Its mark stays on the diagram.");
        connect(put_away, &QPushButton::clicked, this, [this, id, was = comment.hidden] {
            show_result(editor_.set_comment_hidden(id, !was), false);
        });
        auto* remove = new QPushButton("Delete", row);
        remove->setObjectName("commentDelete");
        connect(remove, &QPushButton::clicked, this, [this, id] {
            show_result(editor_.erase_comment(id), false);
        });
        buttons->addWidget(edit);
        buttons->addWidget(put_away);
        buttons->addWidget(remove);
        buttons->addStretch();
        rows->addLayout(buttons);
        layout->addWidget(row);
    }
}

void MainWindow::refresh_validation() {
    issue_model_->clear();
    issue_model_->setHorizontalHeaderLabels({"Level", "Object", "Message"});
    const auto findings = validate(editor_.project());
    for (const auto& issue : findings) {
        const auto level = issue.severity == Severity::Error ? "Error" : issue.severity == Severity::Warning ? "Warning" : "Info";
        auto* severity = new QStandardItem(level);
        if (issue.element) severity->setData(key(*issue.element), Qt::UserRole);
        issue_model_->appendRow({severity,
            new QStandardItem(issue.element ? display_name(editor_.project(), *issue.element) : "Project"),
            new QStandardItem(text(issue.message))});
    }
    if (findings.empty()) issue_model_->appendRow({new QStandardItem("OK"), new QStandardItem("Project"),
        new QStandardItem("No issues in the supported conceptual checks.")});
    issues_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    issues_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    issues_->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    readiness_label_->setText(findings.empty() ? "No model issues   " : QString("%1 model checks   ").arg(findings.size()));
}

void MainWindow::refresh_selection_commands() {
    duplicate_->setEnabled(!selection_.empty());
    rename_->setEnabled(selection_.size() == 1);
    // Every one of them, not merely one: a command that acted on the
    // symbols in a mixed selection and silently left the rest alone would
    // be doing something other than what its name says.
    const auto sizeable = !selection_.empty() && canvas_->selected_symbols().size() == selection_.size();
    enlarge_->setEnabled(sizeable);
    shrink_->setEnabled(sizeable);
}

void MainWindow::selection_changed(const std::vector<ElementRef>& selection) {
    if (refreshing_ || selection_ == selection) return;
    refreshing_ = true;
    selection_ = selection;
    highlight_explorer();
    refresh_properties();
    refresh_selection_commands();
    refreshing_ = false;
}

void MainWindow::show_result(const application::EditResult& result, bool choose_what_was_made) {
    refresh();
    if (!result) {
        statusBar()->showMessage(text(result.error), 12000);
        QMessageBox::warning(this, "Change could not be applied", text(result.error));
    } else if (result.created && choose_what_was_made) canvas_->select_elements({*result.created});
}

// One entry point, so the menu and the toolbar picker cannot disagree about
// which notation is in use.
void MainWindow::choose_notation(Notation notation) {
    canvas_->set_notation(notation);
    const auto previous = refreshing_;
    refreshing_ = true;
    if (const auto found = notation_actions_.find(notation); found != notation_actions_.end())
        found->second->setChecked(true);
    if (notation_box_) notation_box_->setCurrentIndex(static_cast<int>(notation));
    refreshing_ = previous;
}

// Choosing a tool and reporting the choice happen in one place, so the button
// label can never disagree with what the canvas will actually do.
void MainWindow::choose_line_style(LineStyle style) {
    canvas_->set_line_style(style);
    for (const auto& [candidate, action] : line_actions_) action->setChecked(candidate == style);
}

void MainWindow::choose_tool(Tool tool, bool locked) {
    finish_field_edit();
    canvas_->set_tool(tool, locked);
    canvas_->setFocus();
    refresh_tool_labels();
}

void MainWindow::place_canvas_controls() {
    if (!canvas_controls_) return;
    canvas_controls_->adjustSize();
    // Measured from the view's own edge and inset by a scrollbar's thickness
    // whether or not one is showing, so fitting the diagram — which brings
    // scrollbars in or takes them out — never moves the raft.
    const auto bar = canvas_->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, canvas_);
    canvas_controls_->move(canvas_->width() - canvas_controls_->width() - bar - 12,
                           canvas_->height() - canvas_controls_->height() - bar - 12);
    canvas_controls_->raise();
}

int MainWindow::icon_pixels() const {
    auto* toolbar = findChild<QToolBar*>("modelTools");
    return toolbar ? toolbar->iconSize().width() : 34;
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    fit_toolbar();
}

// A tool that has fallen off the end of the toolbar may as well not exist, so
// the toolbar sheds what it can spare before it sheds a tool, and it sheds
// the cheapest thing first: some of the icons' size, then the words on the
// corner controls, whose check mark and half disc are read at a glance, then
// the notation picker, which the View menu also offers, and the names only
// when the window has been made genuinely small.
//
// Which of those is needed is measured rather than guessed from the window's
// width. What fits depends on how many tools there are and how long their names
// read, and a threshold picked by hand goes wrong the moment either changes.
void MainWindow::fit_toolbar() {
    auto* toolbar = findChild<QToolBar*>("modelTools");
    if (!toolbar || fitting_) return;
    fitting_ = true;
    struct Step {
        Qt::ToolButtonStyle style;
        int icon;
        bool notation;
        bool notation_named;
        bool corner_named;
    };
    // Names stay for as long as they possibly can: a tool's lock mark hangs on
    // its name, and a bar of bare icons is the state for a window that has
    // been made small, not for one at an ordinary size. The icons give up
    // size first, then the picker its word, then the corner controls theirs,
    // then the picker, and only then the names.
    static constexpr std::array<Step, 9> steps{{
        {Qt::ToolButtonTextBesideIcon, 34, true, true, true},
        {Qt::ToolButtonTextBesideIcon, 28, true, true, true},
        {Qt::ToolButtonTextBesideIcon, 24, true, true, true},
        {Qt::ToolButtonTextBesideIcon, 24, true, false, true},
        {Qt::ToolButtonTextBesideIcon, 24, true, false, false},
        {Qt::ToolButtonTextBesideIcon, 24, false, false, false},
        {Qt::ToolButtonIconOnly, 28, true, false, false},
        {Qt::ToolButtonIconOnly, 24, false, false, false},
        {Qt::ToolButtonIconOnly, 20, false, false, false},
    }};
    for (std::size_t index = 0; index < steps.size(); ++index) {
        const auto& step = steps[index];
        toolbar->setToolButtonStyle(step.style);
        toolbar->setIconSize(QSize(step.icon, step.icon));
        for (const char* named : {"isaButton", "connectButton"})
            if (auto* button = findChild<QToolButton*>(named)) {
                button->setToolButtonStyle(step.style);
                button->setIconSize(toolbar->iconSize());
            }
        // The corner controls are set after the bar, since the bar hands its
        // own style to the buttons it made and the corner's may differ.
        const auto corner_style = step.corner_named ? step.style : Qt::ToolButtonIconOnly;
        if (auto* check = findChild<QAction*>("checkModel"))
            if (auto* button = qobject_cast<QToolButton*>(toolbar->widgetForAction(check)))
                button->setToolButtonStyle(corner_style);
        if (theme_button_) {
            theme_button_->setToolButtonStyle(corner_style);
            theme_button_->setIconSize(toolbar->iconSize());
        }
        // Hiding the widget would leave its room behind in the toolbar's layout;
        // it is the action holding it that has to go.
        for (auto* hidden : {notation_separator_, notation_action_})
            if (hidden) hidden->setVisible(step.notation);
        if (notation_label_action_) notation_label_action_->setVisible(step.notation && step.notation_named);
        toolbar->adjustSize();
        if (toolbar->sizeHint().width() <= width() || index + 1 == steps.size()) break;
    }
    fitting_ = false;
    refresh_icons();
}

void MainWindow::set_icon_mode(IconMode mode) {
    icon_mode_ = mode;
    QSettings().setValue("iconMode", icon_mode_key(mode));
    for (const auto& [candidate, action] : icon_mode_actions_) action->setChecked(candidate == mode);
    refresh_icons();
    refresh_explorer();
}

// Everything that has to change for the window to be wearing a theme. Choosing
// one and merely looking at one do the same work; only what is remembered and
// what is ticked differ between them.
void MainWindow::apply_appearance(ThemeId id) {
    theme_ = id;
    if (auto* application = qobject_cast<QApplication*>(QCoreApplication::instance()))
        apply_theme(*application, id);
    canvas_->set_theme(id);
    refresh_icons();
    refresh_explorer();
    // The panel's labels are written in the theme's own hues, so they are
    // rebuilt with it rather than keeping the colours of the theme just left.
    const auto was_refreshing = refreshing_;
    refreshing_ = true;
    refresh_properties();
    refreshing_ = was_refreshing;
}

void MainWindow::preview_theme(ThemeId id) { apply_appearance(id); }

void MainWindow::set_theme(ThemeId id) {
    committed_theme_ = id;
    apply_appearance(id);
    QSettings().setValue("theme", theme(id).key);
    for (const auto& [candidate, action] : theme_actions_) action->setChecked(candidate == id);
}

// Icons are drawn from the theme, so they are rebuilt whenever it changes.
void MainWindow::refresh_icons() {
    const auto& colors = theme(theme_);
    for (const auto& [action, glyph] : action_glyphs_)
        action->setIcon(glyph_icon(glyph, colors, icon_pixels(), icon_mode_));
    if (theme_button_) theme_button_->setIcon(glyph_icon(Glyph::Theme, colors, icon_pixels(), icon_mode_));
    if (notation_box_) {
        for (int index = 0; index < notation_box_->count(); ++index)
            notation_box_->setItemIcon(index, QIcon(canvas_->notation_preview(
                static_cast<Notation>(index), notation_sample)));
    }
    for (const auto& [style, action] : line_actions_)
        action->setIcon(QIcon(canvas_->line_style_preview(style, line_style_sample)));
    // The raft's hand carries a lock mark the action's own icon does not, so
    // redrawing the icons has to redraw that too.
    refresh_tool_labels();
    // And Check model wears whichever of its two marks the panel calls for.
    refresh_check_action();
}

// A small padlock in the corner of an icon, for a button that has no name to
// hang the lock mark on.
QIcon with_lock_badge(const QIcon& base, const Theme& colors, int size) {
    QPixmap pixmap = base.pixmap(QSize(size, size) * 2);
    pixmap.setDevicePixelRatio(2);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal s = size;
    const QRectF body(s * 0.58, s * 0.66, s * 0.36, s * 0.28);
    const QRectF shackle(body.left() + body.width() * 0.2, body.top() - body.height() * 0.6,
                         body.width() * 0.6, body.height() * 0.9);
    painter.setPen(QPen(colors.accent, s * 0.08));
    painter.setBrush(Qt::NoBrush);
    painter.drawArc(shackle, 0, 180 * 16);
    painter.setPen(QPen(colors.selected_text, s * 0.04));
    painter.setBrush(colors.accent);
    painter.drawRoundedRect(body, s * 0.05, s * 0.05);
    return QIcon(pixmap);
}

void MainWindow::refresh_tool_labels() {
    const auto active = canvas_->tool();
    const auto locked = canvas_->tool_locked();
    if (auto* hand = findChild<QToolButton*>("canvasPan")) {
        const auto plain = glyph_icon(Glyph::Pan, theme(theme_), 18, icon_mode_);
        hand->setIcon(active == Tool::Pan && locked ? with_lock_badge(plain, theme(theme_), 18) : plain);
        hand->setToolTip(active == Tool::Pan && locked
            ? "Pan is locked. Drag as much as you like; choose another tool or press Escape to stop."
            : "Pan. Double-click to lock it for a longer look around.");
    }
    for (const auto& [tool, action] : tool_actions_) {
        // Generalization and specialization share one action, so only the mode
        // its button is actually set to should drive the label.
        if (action == isa_action_ && tool != isa_mode_) continue;
        const auto plain = action->data().toString();
        // A locked tool is marked on the button, since nothing else on screen
        // would explain why placing does not stop after the first element.
        action->setText(tool == active && locked ? plain + " 🔒" : plain);
        action->setToolTip(tool == active && locked
            ? plain + " is locked. Keep placing; choose another tool or press Escape to stop."
            : "Double-click to lock " + plain.toLower() + " for placing several.");
    }
}

QWidget* MainWindow::toolbar_widget(QAction* action) const {
    auto* toolbar = findChild<QToolBar*>("modelTools");
    return toolbar ? toolbar->widgetForAction(action) : nullptr;
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    // The controls float over the view rather than in a layout, so they are put
    // back in the corner whenever the view changes size under them.
    if (canvas_ && watched == canvas_
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show
            || event->type() == QEvent::LayoutRequest)) {
        place_canvas_controls();
        return false;
    }
    if (event->type() == QEvent::MouseButtonDblClick) {
        if (watched == static_cast<QObject*>(findChild<QToolButton*>("canvasPan"))) {
            choose_tool(Tool::Pan, true);
            return true;
        }
        if (watched == static_cast<QObject*>(findChild<QToolButton*>("isaButton"))) {
            choose_tool(isa_mode_, true);
            return true;
        }
        if (watched == static_cast<QObject*>(findChild<QToolButton*>("connectButton"))) {
            choose_tool(Tool::Connect, true);
            return true;
        }
        for (const auto& [tool, action] : tool_actions_)
            if (action != isa_action_ && watched == static_cast<QObject*>(toolbar_widget(action))) {
                choose_tool(tool, true);
                return true;
            }
        // A button on another row that carries one of the tool actions, as the
        // ribbon's Insert row does, locks the tool exactly as Home's does. ISA
        // is one action for two tools, so it locks the direction it is set to.
        if (auto* button = qobject_cast<QToolButton*>(watched); button && button->defaultAction())
            for (const auto& [tool, action] : tool_actions_)
                if (button->defaultAction() == action && (action != isa_action_ || tool == isa_mode_)) {
                    choose_tool(tool, true);
                    return true;
                }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::rename_selection() {
    finish_field_edit();
    if (selection_.size() != 1) return;
    const auto ref = selection_.front();
    bool accepted = false;
    const auto value = QInputDialog::getText(this, "Rename " + kind_label(ref).toLower(), "Name",
                                           QLineEdit::Normal, text(name(editor_.project(), ref)), &accepted);
    if (accepted) show_result(editor_.rename(ref, bytes(value)));
}

bool MainWindow::confirm_discard() {
    finish_field_edit();
    canvas_->cancel_interaction();
    if (!editor_.dirty()) return true;
    const auto answer = QMessageBox::warning(this, "Save your changes?",
        "The project has unsaved changes.", QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (answer == QMessageBox::Save) return save();
    return answer == QMessageBox::Discard;
}

bool MainWindow::save(bool choose_path) {
    finish_field_edit();
    canvas_->cancel_interaction();
    auto location = path_;
    if (choose_path || location.isEmpty()) {
        location = QFileDialog::getSaveFileName(this, "Save ERDFlow project", location.isEmpty() ? "Untitled.erdx" : location,
                                               "ERDFlow project (*.erdx)");
        if (location.isEmpty()) return false;
        if (!location.endsWith(".erdx", Qt::CaseInsensitive)) {
            location += ".erdx";
            if (QFileInfo::exists(location) && QMessageBox::question(this, "Replace existing project?",
                "A file already exists at " + location + ". Replace it?", QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No) != QMessageBox::Yes) return false;
        }
    }
    const auto result = application::save_project(editor_, store_, bytes(location));
    if (!result) {
        QMessageBox::warning(this, "Project could not be saved", text(result.error));
        return false;
    }
    path_ = location;
    refresh();
    statusBar()->showMessage("Saved " + QFileInfo(path_).fileName(), 7000);
    return true;
}

void MainWindow::new_project() {
    if (!confirm_discard()) return;
    editor_.new_project();
    path_.clear();
    refresh();
    canvas_->actual_size();
    canvas_->centerOn(0, 0);
}

namespace {
// A spin box is a number field with a line edit inside it, and that line edit
// answers to everything a name field answers to. A picked character must never
// land in one: the panel's size, position and transparency fields take numbers,
// and a symbol typed into one would be silently thrown away. They are refused
// here rather than each of them being named, so a field added later is covered
// by being the kind of field it is.
[[nodiscard]] bool number_field(const QWidget* widget) {
    return qobject_cast<const QAbstractSpinBox*>(widget) != nullptr
        || qobject_cast<const QAbstractSpinBox*>(widget->parentWidget()) != nullptr;
}
} // namespace

void MainWindow::remember_text_target(QWidget* widget) {
    // Only somewhere a character could actually go, and never the picker's own
    // search box: searching for a symbol must not make the search box the
    // place the symbol lands.
    if (!widget) return;
    if (symbols_ && symbols_->isAncestorOf(widget)) return;
    if (!qobject_cast<QLineEdit*>(widget) && !qobject_cast<QPlainTextEdit*>(widget)) return;
    if (number_field(widget)) return;
    if (!isAncestorOf(widget)) return;
    text_target_ = widget;
    text_target_name_ = widget->objectName();
    refresh_symbol_destination();
}

void MainWindow::remember_caret(QWidget* widget) {
    if (!widget || widget != text_target_) return;
    if (auto* line = qobject_cast<QLineEdit*>(widget)) text_target_caret_ = line->cursorPosition();
    else if (auto* text = qobject_cast<QPlainTextEdit*>(widget)) text_target_caret_ = text->textCursor().position();
    // Which field that caret belongs to, so a rebuilt one can be told from the
    // one the writing actually stopped in.
    caret_owner_ = widget;
}

QWidget* MainWindow::text_target() {
    // Where the keyboard is, asked of this window rather than of the desktop,
    // so it is still the answer while the gallery is the window the desktop
    // calls active. A character belongs wherever the next typed letter would
    // go; if that is nowhere, it belongs on the diagram.
    auto* focused = focusWidget();
    if (!focused || !focused->isVisible()) return nullptr;
    if (symbols_ && symbols_->isAncestorOf(focused)) return nullptr;
    if (!qobject_cast<QLineEdit*>(focused) && !qobject_cast<QPlainTextEdit*>(focused)) return nullptr;
    if (number_field(focused)) return nullptr;
    // Committing a name rebuilds the properties panel and hands the keyboard
    // to the field that replaced the one being written in. The replacement
    // reads from its beginning, so the caret goes back to where the writing
    // stopped; otherwise a character would land in front of the name.
    // The field the caret was remembered in, not merely the last field focused:
    // committing an edit rebuilds the panel and destroys that field, and the
    // keyboard can reach its replacement before the character does. Asking
    // whether this is the same widget the caret came from answers that in both
    // orders, because a destroyed field leaves nothing to be the same as.
    if (focused != caret_owner_ && focused->objectName() == text_target_name_ && text_target_caret_ >= 0) {
        if (auto* line = qobject_cast<QLineEdit*>(focused))
            line->setCursorPosition(std::min(text_target_caret_, static_cast<int>(line->text().size())));
        else if (auto* text = qobject_cast<QPlainTextEdit*>(focused)) {
            auto cursor = text->textCursor();
            cursor.setPosition(std::min(text_target_caret_, static_cast<int>(text->toPlainText().size())));
            text->setTextCursor(cursor);
        }
    }
    text_target_ = focused;
    return focused;
}

bool MainWindow::place_symbol(const QString& character) {
    // Nothing is being written in, so the character goes on the diagram
    // itself, drawn bare: no card, no border, no title, the way an emoji sits
    // in a line of chat. It is a note underneath, so it is moved, coloured,
    // copied, deleted and undone like anything else on the diagram.
    // Square, because the character is drawn to fill the room it is given and
    // the characters worth placing are about as wide as they are tall. It is a
    // starting size rather than the size: a placed symbol is enlarged and
    // shrunk by its corners, by Edit's two commands, or in the panel.
    constexpr double width = symbol_body.width, height = symbol_body.height;
    // Put down where the user was working, which is where the pointer last
    // was over the diagram. The pointer is over the gallery at the moment a
    // character is picked, so its place on the canvas is the one remembered
    // from before that. With the pointer never yet over the canvas, the middle
    // of the view is the only sensible answer.
    auto centre = canvas_->pointer_place().value_or(canvas_->mapToScene(canvas_->viewport()->rect().center()));
    // Several characters picked one after another would otherwise land on the
    // same spot and hide one another, so each takes a step down and across
    // until it finds room, the way a duplicated element does.
    const auto& layout = editor_.project().layout;
    const auto taken = [&](const QPointF& at) {
        return std::any_of(layout.begin(), layout.end(), [&](const auto& entry) {
            return std::abs(entry.second.x - (at.x() - width / 2)) < 2
                && std::abs(entry.second.y - (at.y() - height / 2)) < 2;
        });
    };
    for (int step = 0; step < 64 && taken(centre); ++step) centre += QPointF(24, 24);
    const domain::Rect rect{centre.x() - width / 2, centre.y() - height / 2, width, height};
    // Left unchosen on purpose. Picking a character is putting one down, not
    // choosing something to work on, and choosing it would swap the properties
    // panel over to it every time one was placed.
    const auto result = editor_.create_symbol(bytes(character), rect);
    show_result(result, false);
    return bool(result);
}

void MainWindow::refresh_symbol_destination() {
    if (!symbols_) return;
    auto* target = text_target();
    if (!target) { symbols_->set_destination({}); return; }
    // The field's own label is what the user sees next to it, so that is what
    // the picker calls the destination rather than an internal name.
    QString field = target->accessibleName();
    if (field.isEmpty()) {
        if (target->objectName() == QStringLiteral("elementName")) field = QStringLiteral("Name");
        else if (target->objectName() == QStringLiteral("elementDescription")) field = QStringLiteral("Description");
        else if (target->objectName() == QStringLiteral("projectName")) field = QStringLiteral("Project name");
        else field = QStringLiteral("the field being written in");
    }
    symbols_->set_destination(field);
}

void MainWindow::show_symbols(const QString& group) {
    if (!symbols_) {
        symbols_ = new SymbolPicker(this);
        symbols_->on_chosen = [this](const QString& character) { insert_symbol(character); };
    }
    symbols_->set_theme(theme(theme_));
    if (!group.isEmpty()) symbols_->show_group(group);
    refresh_symbol_destination();
    symbols_->show();
    symbols_->raise();
}

bool MainWindow::insert_symbol(const QString& character) {
    auto* target = text_target();
    if (auto* line = qobject_cast<QLineEdit*>(target)) {
        line->insert(character);
        // The caret goes back where the character landed, so the next one is
        // typed or picked in the same place rather than at the start again.
        line->setFocus(Qt::OtherFocusReason);
        return true;
    }
    if (auto* text = qobject_cast<QPlainTextEdit*>(target)) {
        text->insertPlainText(character);
        text->setFocus(Qt::OtherFocusReason);
        return true;
    }
    const auto placed = place_symbol(character);
    if (placed)
        statusBar()->showMessage("Put on the diagram where the pointer last was. To put one in a name instead, "
                                 "click into the name first, then pick the character.", 7000);
    refresh_symbol_destination();
    return placed;
}

void MainWindow::insert_picture_dialog(std::optional<QPointF> at) {
    const auto location = QFileDialog::getOpenFileName(this, "Insert picture", {},
        "Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tif *.tiff);;All files (*)");
    if (!location.isEmpty()) insert_picture(location, at);
}

bool MainWindow::insert_picture(const QString& path, std::optional<QPointF> at) {
    finish_field_edit();
    QString why_not;
    const auto bytes_of_image = encoded_image(path, why_not);
    if (!bytes_of_image) {
        QMessageBox::warning(this, "Picture could not be inserted", why_not);
        return false;
    }
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const auto image = reader.read();
    // Placed where it was asked for, or else in the middle of what is on
    // screen, at a size that shows it without filling the view; a small image
    // keeps its own size.
    const auto centre = at.value_or(canvas_->mapToScene(canvas_->viewport()->rect().center()));
    auto size = QSizeF(image.size());
    if (size.width() > 320 || size.height() > 320) size = size.scaled(QSizeF(320, 320), Qt::KeepAspectRatio);
    size = size.expandedTo(QSizeF(24, 24));
    const domain::Rect rect{centre.x() - size.width() / 2, centre.y() - size.height() / 2, size.width(), size.height()};
    const auto result = editor_.create_picture(bytes(QFileInfo(path).completeBaseName()), rect, *bytes_of_image);
    show_result(result);
    if (result && result.created) canvas_->select_elements({*result.created}, true);
    return bool(result);
}

void MainWindow::refresh_check_action() {
    if (!check_ || !validation_dock_) return;
    const bool open = validation_dock_->isVisible();
    check_->setChecked(open);
    action_glyphs_[check_] = open ? Glyph::Dismiss : Glyph::Check;
    check_->setIcon(glyph_icon(action_glyphs_[check_], theme(theme_), icon_pixels(), icon_mode_));
    check_->setToolTip(open ? "Put the model checks away."
                            : "Look the model over and list what is missing.");
}

void MainWindow::refresh_background_menu() {
    const auto& paper = editor_.project().background;
    for (const auto& [style, action] : background_actions_) action->setChecked(style == paper.style);
    if (auto* strength = findChild<QSlider*>("backgroundStrength")) {
        const auto was = refreshing_;
        refreshing_ = true;
        strength->setValue(paper.strength);
        refreshing_ = was;
    }
    if (auto* row = findChild<QWidgetAction*>("backgroundStrengthRow"))
        row->setVisible(paper.style == domain::BackgroundStyle::Image);
}

void MainWindow::choose_background(domain::BackgroundStyle style) {
    auto paper = editor_.project().background;
    paper.style = style;
    show_result(editor_.set_background(std::move(paper)));
    refresh_background_menu();
}

void MainWindow::choose_background_image() {
    const auto location = QFileDialog::getOpenFileName(this, "Background picture", {},
        "Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tif *.tiff);;All files (*)");
    if (location.isEmpty()) { refresh_background_menu(); return; }
    QString why_not;
    // A background is looked at rather than glanced at, so it is kept as large
    // as a project file will carry.
    const auto picture = encoded_image(location, why_not, 2560, domain::max_image_bytes);
    if (!picture) {
        QMessageBox::warning(this, "Background could not be set", why_not);
        refresh_background_menu();
        return;
    }
    auto paper = editor_.project().background;
    paper.style = domain::BackgroundStyle::Image;
    paper.image = *picture;
    show_result(editor_.set_background(std::move(paper)));
    refresh_background_menu();
}

void MainWindow::set_full_view(bool on) {
    if (on) {
        hidden_panels_.clear();
        for (auto* dock : findChildren<QDockWidget*>())
            if (dock->isVisible()) {
                hidden_panels_.push_back(dock);
                dock->hide();
            }
    } else {
        for (auto* dock : hidden_panels_) dock->show();
        hidden_panels_.clear();
    }
    full_view_->setToolTip(on ? "Full view. Press again to bring the panels back."
                              : "Full view — put the panels away and give the whole window to the diagram.");
    statusBar()->showMessage(on ? "Full view. Press it again to bring the panels back." : "Panels restored.", 5000);
    place_canvas_controls();
}

QByteArray MainWindow::project_payload(QString& note) {
    const auto encoded = store_.project_bytes(editor_.project());
    if (encoded) return QByteArray(encoded.bytes.data(), static_cast<qsizetype>(encoded.bytes.size()));
    // Too large to travel inside a picture. The picture is still worth having,
    // so it is written without the project and the reason is said plainly,
    // rather than the export failing over something the picture does not need.
    note = "The project was too large to travel inside it: " + text(encoded.error);
    return {};
}

application::LoadResult MainWindow::read_project(const QString& path) {
    // A picture ERDFlow wrote is a project as much as a .erdx is, so the two
    // are opened by the same command and differ only in where the bytes were
    // found. A picture from anywhere else is an ordinary picture, and saying
    // so is more useful than reporting it as a damaged project.
    if (!may_carry_project(path)) return store_.load(bytes(path));
    const auto payload = payload_of_picture_file(path);
    if (payload.isEmpty())
        return {{}, "This picture does not carry an ERDFlow project inside it. "
                    "Use Insert → Picture to place it on the diagram instead."};
    return store_.project_from_bytes(std::string(payload.constData(), static_cast<std::size_t>(payload.size())));
}

void MainWindow::open_dialog() {
    const auto location = QFileDialog::getOpenFileName(this, "Open ERDFlow project", path_,
        "ERDFlow project or picture (*.erdx *.svg *.png);;ERDFlow project (*.erdx);;"
        "Picture carrying a project (*.svg *.png)");
    if (!location.isEmpty()) open_path(location);
}

bool MainWindow::open_path(const QString& path) {
    // Validate the complete candidate before asking to replace the open work.
    auto candidate = read_project(path);
    if (!candidate) {
        QMessageBox::warning(this, "Project could not be opened", text(candidate.error));
        return false;
    }
    if (!confirm_discard()) return false;
    // Save in the discard prompt may have updated this very file. Re-read it
    // before installing so the pre-prompt candidate cannot restore old data.
    candidate = read_project(path);
    if (!candidate) {
        QMessageBox::warning(this, "Project could not be opened", text(candidate.error));
        return false;
    }
    const auto result = editor_.replace_project(std::move(*candidate.project));
    if (!result) { show_result(result); return false; }
    // A project opened out of a picture has no project file of its own yet.
    // Leaving the picture as the save location would overwrite it with project
    // bytes and destroy the picture, so the next save asks where it should go.
    path_ = may_carry_project(path) ? QString() : path;
    refresh();
    canvas_->fit_diagram();
    if (path_.isEmpty())
        statusBar()->showMessage("Opened the project carried inside " + QFileInfo(path).fileName()
                                 + ". Save it to give it a project file of its own.", 9000);
    return true;
}

void MainWindow::download_dialog() {
    finish_field_edit();
    canvas_->cancel_interaction();
    DownloadDialog dialog(*canvas_, editor_.project(), this);
    dialog.set_choice(download_choice_);
    if (dialog.exec() != QDialog::Accepted) return;
    download_choice_ = dialog.choice();
    if (download_choice_.document) download_document(download_choice_.as_document);
    else download_picture(download_choice_.as_picture);
}

// Where a downloaded file is suggested to go: named after the project, beside
// it when it has a file of its own, so what leaves lands where the work lives.
QString MainWindow::download_location(const QString& suffix, const QString& label) {
    const auto stem = path_.isEmpty() ? QString("Untitled") : QFileInfo(path_).completeBaseName();
    const auto suggested = path_.isEmpty() ? stem + "." + suffix
                                           : QFileInfo(path_).dir().filePath(stem + "." + suffix);
    auto location = QFileDialog::getSaveFileName(this, "Download", suggested,
                                                 QString("%1 (*.%2)").arg(label, suffix));
    if (location.isEmpty()) return {};
    if (!location.endsWith("." + suffix, Qt::CaseInsensitive)) {
        location += "." + suffix;
        if (QFileInfo::exists(location) && QMessageBox::question(this, "Replace existing file?",
            "A file already exists at " + location + ". Replace it?", QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No) != QMessageBox::Yes) return {};
    }
    return location;
}

bool MainWindow::download_document(DocumentFormat format, const QString& location_given) {
    finish_field_edit();
    canvas_->cancel_interaction();
    const auto& info = document_format(format);
    auto location = location_given;
    if (location.isEmpty())
        location = download_location(QString::fromLatin1(info.suffix), QString::fromUtf8(info.label));
    if (location.isEmpty()) return false;
    const auto result = write_document(*canvas_, editor_.project(), format, location);
    if (!result) {
        QMessageBox::warning(this, "Document could not be written", result.error);
        return false;
    }
    // A listing is not the project, and someone who downloads one and expects
    // to reopen it should be told so once rather than discover it later.
    statusBar()->showMessage("Downloaded " + QFileInfo(location).fileName()
                             + ". It is a listing of the model, not the project itself.", 9000);
    return true;
}

bool MainWindow::download_picture(const PictureOptions& options, const QString& location_given) {
    finish_field_edit();
    canvas_->cancel_interaction();
    const auto& info = picture_format(options.format);
    const auto suffix = QString::fromLatin1(info.suffix);
    auto location = location_given;
    if (location.isEmpty()) location = download_location(suffix, QString::fromUtf8(info.label));
    if (location.isEmpty()) return false;
    QString note;
    const auto payload = options.carry_project && info.carries_project ? project_payload(note) : QByteArray();
    const auto result = write_picture(*canvas_, options, payload, location);
    if (!result) {
        QMessageBox::warning(this, "Picture could not be written", result.error);
        return false;
    }
    // What was written, and where the project ended up, since a recipient who
    // expects to reopen the picture needs to know whether it can be.
    auto said = "Downloaded " + QFileInfo(location).fileName();
    if (result.carried_project) said += ", with the project inside it";
    said += ".";
    if (!note.isEmpty()) said += " " + note;
    else if (!result.carried_note.isEmpty() && options.carry_project && info.carries_project)
        said += " " + result.carried_note;
    statusBar()->showMessage(said, 9000);
    return true;
}

bool MainWindow::copy_picture() {
    finish_field_edit();
    canvas_->cancel_interaction();
    // A copy is of what is selected, and of the whole diagram when nothing is,
    // which is what every drawing application does with the same command.
    auto options = download_choice_.as_picture;
    options.extent = canvas_->selection_bounds().isEmpty() ? PictureExtent::WholeDiagram : PictureExtent::Selection;
    QString note;
    const auto payload = options.carry_project ? project_payload(note) : QByteArray();

    options.format = PictureFormat::Png;
    QByteArray png;
    const auto raster = draw_picture(*canvas_, options, payload, png);
    if (!raster) {
        statusBar()->showMessage(raster.error, 9000);
        return false;
    }
    options.format = PictureFormat::Svg;
    QByteArray svg;
    const auto vector = draw_picture(*canvas_, options, payload, svg);

    // Both pictures go on at once and the destination takes whichever it
    // prefers: a word processor usually takes the vector, a chat window the
    // raster, and neither has to be chosen in advance.
    auto* data = new QMimeData;
    QImage image;
    if (image.loadFromData(png, "png")) data->setImageData(image);
    data->setData("image/png", png);
    if (vector) data->setData("image/svg+xml", svg);
    QApplication::clipboard()->setMimeData(data);
    statusBar()->showMessage(options.extent == PictureExtent::Selection
                                 ? "Copied the selection as a picture."
                                 : "Copied the diagram as a picture.", 7000);
    return true;
}

QString MainWindow::ask_for_comment(const QString& said, const QString& about) {
    // A remark is prose, often a sentence or two, so it is written in a box
    // that takes several lines rather than in a single-line field.
    QDialog dialog(this);
    dialog.setObjectName("commentDialog");
    dialog.setWindowTitle(said.isEmpty() ? "Add comment" : "Edit comment");
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(8);
    auto* about_label = new QLabel(about, &dialog);
    about_label->setObjectName("commentAbout");
    about_label->setWordWrap(true);
    layout->addWidget(about_label);
    auto* editor = new QPlainTextEdit(said, &dialog);
    editor->setObjectName("commentText");
    editor->setPlaceholderText("What should whoever reads this diagram know?");
    editor->setMinimumSize(360, 120);
    layout->addWidget(editor);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setObjectName("commentAccept");
    buttons->button(QDialogButtonBox::Cancel)->setObjectName("commentCancel");
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    editor->setFocus();
    if (dialog.exec() != QDialog::Accepted) return {};
    return editor->toPlainText().trimmed();
}

namespace {
// What a remark is being left on, said plainly, so the box asking for the words
// says what they will be pinned to.
QString about_targets(const domain::Project& project, const std::vector<domain::CommentTarget>& targets) {
    if (targets.size() > 1) return QString("On %1 things at once.").arg(targets.size());
    if (targets.empty()) return {};
    if (const auto* element = std::get_if<domain::ElementRef>(&targets.front()))
        return "On " + kind_label(project, *element).toLower() + " " + display_name(project, *element) + ".";
    if (std::holds_alternative<domain::ConnectorRef>(targets.front())) return "On this line.";
    const auto& anchor = std::get<domain::TextAnchor>(targets.front());
    return QString("On the %1 of %2.")
        .arg(anchor.field == domain::TextField::Name ? "name" : "description",
             display_name(project, anchor.owner));
}
} // namespace

bool MainWindow::add_comment(std::vector<domain::CommentTarget> targets, const QString& said) {
    if (targets.empty()) return false;
    finish_field_edit();
    auto words = said;
    if (words.isEmpty()) words = ask_for_comment({}, about_targets(editor_.project(), targets));
    if (words.isEmpty()) return false;
    const auto result = editor_.create_comment(bytes(words), std::move(targets));
    show_result(result, false);
    return bool(result);
}

void MainWindow::offer_text_comment(QWidget* field) {
    field->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(field, &QWidget::customContextMenuRequested, this, [this, field](const QPoint& at) {
        // The standard entries stay: taking cut, copy and paste away from a
        // text field to add one entry of our own would be a poor trade.
        auto* line = qobject_cast<QLineEdit*>(field);
        auto* block = qobject_cast<QPlainTextEdit*>(field);
        std::unique_ptr<QMenu> menu(line ? line->createStandardContextMenu()
                                         : block ? block->createStandardContextMenu() : nullptr);
        if (!menu) return;
        const bool chosen = line ? line->hasSelectedText() : block->textCursor().hasSelection();
        menu->addSeparator();
        auto* comment = menu->addAction("Comment on selection…");
        comment->setObjectName("fieldComment");
        comment->setEnabled(chosen);
        comment->setToolTip(chosen ? "Pin a remark to the words you have chosen."
                                   : "Choose some words first, and the remark is pinned to those.");
        const auto name = field->objectName();
        connect(comment, &QAction::triggered, this, [this, name] { comment_on_selected_text(name); });
        menu->exec(field->mapToGlobal(at));
    });
}

namespace {
// How many characters stand before a position that Qt counts in UTF-16 code
// units. A comment's range is counted in characters so that it means the same
// thing in the file, in the domain and in the panel, none of which index text
// the way Qt does.
std::uint32_t characters_before(const QString& text, int units) {
    const auto clamped = std::clamp(units, 0, static_cast<int>(text.size()));
    return static_cast<std::uint32_t>(text.left(clamped).toUcs4().size());
}
} // namespace

bool MainWindow::comment_on_selected_text(const QString& field_name, const QString& said) {
    if (selection_.size() != 1) return false;
    const auto ref = selection_.front();
    domain::TextAnchor anchor;
    anchor.owner = ref;
    QString whole;
    int begin_units = 0;
    int end_units = 0;
    if (auto* line = properties_->findChild<QLineEdit*>(field_name)) {
        if (!line->hasSelectedText()) return false;
        anchor.field = domain::TextField::Name;
        whole = line->text();
        begin_units = line->selectionStart();
        end_units = begin_units + static_cast<int>(line->selectedText().size());
    } else if (auto* block = properties_->findChild<QPlainTextEdit*>(field_name)) {
        const auto cursor = block->textCursor();
        if (!cursor.hasSelection()) return false;
        anchor.field = domain::TextField::Description;
        whole = block->toPlainText();
        begin_units = cursor.selectionStart();
        end_units = cursor.selectionEnd();
    } else {
        return false;
    }
    // The field may hold text that has not been committed yet. A remark is
    // pinned into what the model actually holds, so the writing is committed
    // first and the range is measured against the result.
    finish_field_edit();
    const auto stored = text(anchor.field == domain::TextField::Name ? name(editor_.project(), ref)
                                                                     : description(editor_.project(), ref));
    if (stored != whole) whole = stored;
    anchor.begin = characters_before(whole, begin_units);
    const auto end = characters_before(whole, end_units);
    if (end <= anchor.begin) return false;
    anchor.length = end - anchor.begin;
    return add_comment({domain::CommentTarget{anchor}}, said);
}

void MainWindow::refresh_download_actions() {
    // Nothing drawn is nothing to hand on. The entries stay where they are and
    // go quiet, rather than the row appearing and disappearing as work starts.
    const auto anything = !canvas_->diagram_bounds().isEmpty();
    for (auto* action : download_actions_) action->setEnabled(anything);
}

void MainWindow::load_example() {
    if (!confirm_discard()) return;
    application::Editor example(ids_);
    example.rename_project("University · Students, courses and professors");
    // The diagram an introductory course draws: three entities, the three ways
    // they relate, and one of every kind of attribute -- a key, a composite
    // with parts of its own, one that is worked out rather than stored, and one
    // that may be held more than once. Between them those cover everything the
    // conceptual editor has to draw, which is why this is the example.
    //
    // The coordinates are the diagram's own, laid out as it is drawn on paper:
    // the view is fitted to them at the end rather than the other way round.
    auto add_entity = [&](const char* name, Rect body) {
        return std::get<EntityId>(*example.create_entity(name, body).created);
    };
    auto add_relationship = [&](const char* name, Rect body) {
        return std::get<RelationshipId>(*example.create_relationship(name, body).created);
    };
    auto add_attribute = [&](const char* name, Rect body, AttributeOwner owner,
                             AttributeKind kind = AttributeKind::Normal) {
        const auto id = std::get<AttributeId>(*example.create_attribute(name, body, owner).created);
        if (kind != AttributeKind::Normal) example.set_attribute_kind(id, kind);
        return id;
    };
    // One side of a relationship, carrying the pair the diagram labels it with:
    // total participation is the 1 of (1,1) and partial the 0, while the
    // maximum is the M or the 1 that follows it.
    auto join = [&](RelationshipId relationship, ParticipantTarget target,
                    Cardinality maximum, Participation participation) {
        const auto joined = example.connect(relationship, target);
        example.update_participant(relationship, *joined.participant, maximum, participation, "");
    };

    const auto student = add_entity("Student", {-441, -195, 160, 80});
    const auto course = add_entity("Course", {-441, 319, 160, 80});
    const auto professor = add_entity("Professor", {433, 319, 160, 80});

    // A student may enroll in any number of courses and a course may hold any
    // number of students, so the pair that resolves into its own table later.
    const auto enrolled = add_relationship("Enrolled", {-456, 33, 190, 110});
    join(enrolled, student, Cardinality::Many, Participation::Partial);
    join(enrolled, course, Cardinality::Many, Participation::Partial);
    // A course is taught by at most one professor, and a professor may be
    // between courses, so neither side is obliged to take part.
    const auto teaches = add_relationship("Teaches", {-52, 304, 190, 110});
    join(teaches, course, Cardinality::One, Participation::Partial);
    join(teaches, professor, Cardinality::One, Participation::Partial);
    // Mentoring is the one side that is compulsory: every professor mentors,
    // while a student need not be mentored at all.
    const auto mentor = add_relationship("Mentor", {415, -208, 190, 110});
    join(mentor, student, Cardinality::Many, Participation::Partial);
    join(mentor, professor, Cardinality::One, Participation::Total);

    // A student is identified by an ID, named by a composite whose three parts
    // hang off it, has an age nobody stores, and may be reached on more than
    // one telephone.
    add_attribute("ID", {-727, -180, 150, 60}, student, AttributeKind::Key);
    const auto student_name = add_attribute("Name", {-544, -400, 150, 60}, student, AttributeKind::Composite);
    add_attribute("First", {-805, -515, 150, 60}, student_name);
    add_attribute("Mid", {-617, -544, 150, 60}, student_name);
    add_attribute("Last", {-418, -542, 150, 60}, student_name);
    add_attribute("Gender", {-715, -300, 150, 60}, student);
    add_attribute("Birth Date", {-367, -356, 150, 60}, student);
    add_attribute("Age", {-170, -363, 150, 60}, student, AttributeKind::Derived);
    add_attribute("Phone", {27, -360, 150, 60}, student, AttributeKind::Multivalued);

    add_attribute("ID", {-576, 506, 150, 60}, course, AttributeKind::Key);
    add_attribute("Name", {-728, 387, 150, 60}, course);
    add_attribute("Credit Hours", {-707, 263, 150, 60}, course);

    add_attribute("ID", {277, 506, 150, 60}, professor, AttributeKind::Key);
    add_attribute("Name", {489, 506, 150, 60}, professor);
    add_attribute("Salary", {661, 414, 150, 60}, professor);

    // The date belongs to the enrollment rather than to the student or to the
    // course, which is the reason a relationship may carry attributes at all.
    // Its ellipse is the one that is drawn wider than the rest, because the
    // name is longer than the others and an example should not open on a
    // label that has been cut short.
    add_attribute("Enrollment Date", {-175, 17, 200, 60}, enrolled);

    show_result(editor_.replace_project(example.project()));
    path_.clear();
    canvas_->select_elements({enrolled});
    canvas_->fit_diagram();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (confirm_discard()) event->accept(); else event->ignore();
}

} // namespace erdflow::desktop
