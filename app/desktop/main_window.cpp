#include "main_window.hpp"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFocusEvent>
#include <QFormLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>
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
QString kind_label(ElementRef ref) {
    if (std::holds_alternative<EntityId>(ref)) return QStringLiteral("Entity");
    if (std::holds_alternative<AttributeId>(ref)) return QStringLiteral("Attribute");
    if (std::holds_alternative<SpecializationId>(ref)) return QStringLiteral("Specialization");
    return QStringLiteral("Relationship");
}
// The colour an element is actually drawn with on the canvas: the one it was
// given if it has one, and otherwise whatever the theme gives its kind. The
// panel reads this so that it shows the element's own colour rather than only
// the colours a user happened to choose by hand.
QColor surface_of(const Project& project, const Theme& colors, ElementRef ref) {
    if (const auto chosen = project.colours.find(ref); chosen != project.colours.end())
        return QColor(chosen->second.red, chosen->second.green, chosen->second.blue);
    if (std::holds_alternative<AttributeId>(ref)) return colors.attribute_fill;
    if (std::holds_alternative<EntityId>(ref)) return colors.entity_fill;
    if (std::holds_alternative<SpecializationId>(ref)) return colors.isa_fill;
    // An associative relationship converts to a relation of its own and wears
    // the entity palette on the canvas, so it wears it here too.
    const auto& relationship = project.relationships.at(std::get<RelationshipId>(ref));
    return relationship.associative ? colors.entity_fill : colors.relationship_fill;
}

QString display_name(const Project& project, ElementRef ref) {
    const auto value = text(name(project, ref));
    return value.isEmpty() ? QStringLiteral("(unnamed)") : value;
}
QLabel* hint(const QString& value, QWidget* parent) {
    auto* label = new QLabel(value, parent);
    label->setWordWrap(true);
    label->setObjectName("hint");
    return label;
}
// Descriptions commit through the same command path on focus loss.
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
    resize(1440, 920);
    setMinimumSize(960, 620);
    build_shell();
    build_actions();
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
    explorer_ = new QTreeView(explorer_dock);
    explorer_->setObjectName("explorer");
    explorer_->setAccessibleName("Project elements");
    explorer_->setHeaderHidden(true);
    explorer_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    explorer_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    explorer_->setMinimumWidth(200);
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
            if (found != references_.end()) selected.push_back(found->second);
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
    properties_->setMinimumWidth(290);
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
constexpr QSize notation_sample{72, 24};

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
    file->addAction("Open example", this, &MainWindow::load_example);
    file->addSeparator();
    file->addAction("&Quit", QKeySequence::Quit, this, &QWidget::close);
    auto* edit = new QMenu("&Edit", this);
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
    // Delete/Backspace and the letter tool shortcuts belong to the canvas, so
    // typing inside property fields never deletes model elements.
    auto* toolbar = addToolBar("Model tools");
    toolbar->setObjectName("modelTools");
    toolbar->setMovable(false);
    // Icon beside text, the way an office application labels its toolbar: the
    // glyph carries recognition, the word removes any doubt.
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->setIconSize(QSize(22, 22));
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
    for (const auto style : {LineStyle::Curved, LineStyle::Straight}) {
        const QString label = style == LineStyle::Straight ? "Straight lines" : "Curved lines";
        auto* entry = line_menu->addAction(QIcon(canvas_->line_style_preview(style, line_style_sample)), label);
        entry->setCheckable(true);
        entry->setChecked(style == canvas_->line_style());
        entry->setObjectName(style == LineStyle::Straight ? "lineStraight" : "lineCurved");
        line_actions_[style] = entry;
        connect(entry, &QAction::triggered, this, [this, style] { choose_line_style(style); });
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

    auto* pan_action = toolbar->addAction("Pan");
    pan_action->setCheckable(true);
    pan_action->setData("Pan");
    pan_action->setObjectName("toolPan");
    pan_action->setActionGroup(group);
    tool_actions_[Tool::Pan] = pan_action;
    action_glyphs_[pan_action] = Glyph::Pan;
    connect(pan_action, &QAction::triggered, this, [this] { choose_tool(Tool::Pan, false); });
    if (auto* button = toolbar->widgetForAction(pan_action)) button->installEventFilter(this);

    toolbar->addSeparator();
    auto* fit = toolbar->addAction("Fit", canvas_, &DiagramView::fit_diagram);
    fit->setShortcut(QKeySequence("Ctrl+0"));
    action_glyphs_[fit] = Glyph::Fit;
    auto* check = toolbar->addAction("Check model", this, [this] {
        finish_field_edit(); refresh_validation(); validation_dock_->show();
    });
    check->setObjectName("checkModel");
    action_glyphs_[check] = Glyph::Check;
    // Notation is a reading choice people change often, and a submenu hides it.
    // The picker sits in the toolbar and draws each option, so the cardinality
    // symbols can be recognised rather than remembered from a name.
    auto* notation_separator = toolbar->addSeparator();
    auto* notation_label = new QLabel("  Notation ", toolbar);
    notation_label->setObjectName("hint");
    toolbar->addWidget(notation_label);
    notation_box_ = new QComboBox(toolbar);
    notation_box_->setObjectName("notationPicker");
    notation_box_->setIconSize(notation_sample);
    notation_box_->setToolTip("How each participant's minimum and maximum are drawn.");
    for (const auto& [style, label] : notation_styles())
        notation_box_->addItem(QIcon(canvas_->notation_preview(style, notation_sample)), label,
                               QVariant::fromValue(static_cast<int>(style)));
    notation_box_->setCurrentIndex(static_cast<int>(canvas_->notation()));
    connect(notation_box_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (refreshing_ || index < 0) return;
        choose_notation(static_cast<Notation>(index));
    });
    toolbar->addWidget(notation_box_);

    auto* view = findChild<QMenu*>("viewMenu");
    view->addSeparator();
    view->addAction(fit);
    view->addAction("Actual size", QKeySequence("Ctrl+1"), canvas_, &DiagramView::actual_size);
    view->addAction("Zoom in", QKeySequence::ZoomIn, canvas_, &DiagramView::zoom_in);
    view->addAction("Zoom out", QKeySequence::ZoomOut, canvas_, &DiagramView::zoom_out);
    // Dragging follows the pointer continuously by default. Snapping quantises
    // movement to the grid step, which reads as stuttering rather than as
    // alignment help, so it stays available but off until it is asked for.
    for (bool snap : {false, true}) {
        auto* action = view->addAction(snap ? "Snap to grid" : "Show grid");
        action->setCheckable(true);
        action->setChecked(!snap);
        connect(action, &QAction::toggled, this, [this, snap](bool checked) {
            if (snap) canvas_->set_snap_enabled(checked); else canvas_->set_grid_visible(checked);
        });
    }
    canvas_->set_snap_enabled(false);
    canvas_->set_grid_visible(true);
    // The same participants can be read in several notations. This is a display
    // choice, so it lives with the other view settings rather than in the file.
    auto* themes = view->addMenu("Theme");
    auto* theme_group = new QActionGroup(this);
    for (const auto& entry : erdflow::desktop::themes()) {
        auto* action = themes->addAction(entry.label);
        action->setCheckable(true);
        action->setChecked(entry.id == theme_);
        action->setObjectName("theme" + QString(entry.key).remove('-'));
        action->setActionGroup(theme_group);
        theme_actions_[entry.id] = action;
        connect(action, &QAction::triggered, this, [this, id = entry.id] { set_theme(id); });
    }
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
    for (const auto mode : {IconMode::Normal, IconMode::Modern}) {
        auto* action = icon_menu->addAction(mode == IconMode::Modern ? "Modern — 3D artwork"
                                                                     : "Normal — drawn from the theme");
        action->setCheckable(true);
        action->setChecked(mode == icon_mode_);
        action->setObjectName("icons" + icon_mode_key(mode));
        action->setActionGroup(icon_group);
        icon_mode_actions_[mode] = action;
        connect(action, &QAction::triggered, this, [this, mode] { set_icon_mode(mode); });
    }

    // It goes before the notation picker rather than at the end: the toolbar
    // runs out of width on a narrow window, and anything added last is the
    // first thing to disappear into the overflow.
    toolbar->insertSeparator(notation_separator);
    toolbar->insertWidget(notation_separator, theme_button_);
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
    // Every action now exists, so give them their first icons. The window must
    // look right on its own, not only once a theme is chosen from outside it.
    refresh_icons();
    auto* help = menuBar()->addMenu("&Help");
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
    duplicate_->setEnabled(!selection_.empty());
    rename_->setEnabled(selection_.size() == 1);
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
    const auto append = [&](const QString& label, Glyph glyph, const auto& collection) {
        const auto badge = glyph_icon(glyph, colors, 18, icon_mode_);
        auto* group = new QStandardItem(badge, label + QString(" (%1)").arg(collection.size()));
        group->setSelectable(false);
        project->appendRow(group);
        for (const auto& [id, item] : collection) {
            (void)item;
            const ElementRef ref{id};
            auto* row = new QStandardItem(badge, display_name(editor_.project(), ref));
            row->setData(key(ref), Qt::UserRole);
            row->setToolTip(kind_label(ref) + " · " + key(ref));
            group->appendRow(row);
            references_.emplace(key(ref), ref);
        }
    };
    append("Entities", Glyph::Entity, editor_.project().entities);
    append("Attributes", Glyph::Attribute, editor_.project().attributes);
    append("Relationships", Glyph::Relationship, editor_.project().relationships);
    explorer_->expandAll();
    highlight_explorer();
}

void MainWindow::highlight_explorer() {
    const QSignalBlocker blocker(explorer_->selectionModel());
    explorer_->selectionModel()->clearSelection();
    for (const auto ref : selection_) {
        const auto matches = explorer_model_->match(explorer_model_->index(0, 0), Qt::UserRole,
                                                    key(ref), 1, Qt::MatchExactly | Qt::MatchRecursive);
        if (!matches.isEmpty()) explorer_->selectionModel()->select(matches.front(), QItemSelectionModel::Select | QItemSelectionModel::Rows);
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
    auto* heading = new QLabel(kind_label(ref), panel);
    heading->setObjectName("propertyHeading");
    // The heading says what kind of thing this is, so it stays a title and is
    // left to the theme. Only the name carries the element's own colour: it is
    // the box holding the thing being named, and colouring the kind as well
    // would say the same thing twice and leave the panel shouting.
    const auto surface = surface_of(project, theme(theme_), ref);
    const auto ink = readable_on(surface);
    layout->addWidget(heading);
    auto* form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    auto* name_edit = new QLineEdit(text(name(project, ref)), panel);
    name_edit->setObjectName("elementName");
    // The name field carries the same colour. Its border is darkened from the
    // fill rather than taken from the theme, which would otherwise draw a line
    // the element's colour knows nothing about around it.
    // The focus ring has to be restated: a style set on the widget wins over the
    // application's, so the theme's focus rule no longer reaches this field, and
    // it is drawn in the ink rather than the accent, which is the one colour
    // already known to contrast with whatever fill the element carries.
    name_edit->setStyleSheet(QStringLiteral(
        "QLineEdit#elementName { background: %1; color: %2; border: 1px solid %3; }"
        "QLineEdit#elementName:focus { border: 2px solid %2; }")
        .arg(surface.name(), ink.name(), surface.darker(135).name()));
    name_edit->setMaxLength(512);
    form->addRow("Name", name_edit);
    connect(name_edit, &QLineEdit::editingFinished, this, [this, ref, name_edit] {
        if (refreshing_ || !exists(editor_.project(), ref)) return;
        const auto value = bytes(name_edit->text());
        if (value != name(editor_.project(), ref)) show_result(editor_.rename(ref, value));
    });
    layout->addLayout(form);
    if (const auto* id = std::get_if<AttributeId>(&ref)) {
        const auto attribute_id = *id;
        const auto attribute = project.attributes.at(attribute_id);
        auto* attr_form = new QFormLayout;
        attr_form->setRowWrapPolicy(QFormLayout::WrapAllRows);
        auto* kind = new QComboBox(panel);
        kind->setObjectName("attributeKind");
        kind->addItems({"Normal", "Key", "Composite", "Multivalued", "Derived"});
        kind->setCurrentIndex(static_cast<int>(attribute.kind));
        attr_form->addRow("Attribute kind", kind);
        connect(kind, &QComboBox::activated, this, [this, attribute_id](int index) {
            show_result(editor_.set_attribute_kind(attribute_id, static_cast<AttributeKind>(index)));
        });
        auto* owner = new QComboBox(panel);
        owner->setObjectName("attributeOwner");
        owner->addItem("Unassigned");
        std::vector<std::optional<AttributeOwner>> owners{{}};
        for (const auto& [candidate_key, candidate] : references_) {
            (void)candidate_key;
            if (candidate == ref) continue;
            if (const auto* attr = std::get_if<AttributeId>(&candidate);
                attr && project.attributes.at(*attr).kind != AttributeKind::Composite) continue;
            if (attribute.kind == AttributeKind::Key && std::holds_alternative<RelationshipId>(candidate)) continue;
            owners.push_back(candidate);
            owner->addItem(kind_label(candidate) + ": " + display_name(project, candidate));
            if (attribute.owner == owners.back()) owner->setCurrentIndex(owner->count() - 1);
        }
        attr_form->addRow("Owner", owner);
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
        auto* direction = new QComboBox(panel);
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
        direction_form->addRow("Direction", direction);
        layout->addLayout(direction_form);
        auto* super = new QLabel(specialization.supertype
            ? "Supertype: " + display_name(project, ElementRef{*specialization.supertype})
            : QStringLiteral("Supertype: not connected yet"), panel);
        super->setObjectName("specializationSupertype");
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
        auto* constraint = new QComboBox(panel);
        constraint->setObjectName("specializationConstraint");
        constraint->addItem("Disjoint — at most one subtype");
        constraint->addItem("Overlapping — may be several subtypes");
        constraint->setCurrentIndex(specialization.constraint == Disjointness::Overlapping ? 1 : 0);
        auto* completeness = new QComboBox(panel);
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
        rules->addRow("Constraint", constraint);
        rules->addRow("Completeness", completeness);
        layout->addLayout(rules);
        for (const auto& subtype : specialization.subtypes) {
            auto* card = new QWidget(panel);
            card->setObjectName("participantCard");
            auto* row = new QFormLayout(card);
            row->setRowWrapPolicy(QFormLayout::WrapAllRows);
            row->addRow(new QLabel(display_name(project, ElementRef{subtype}), card));
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
        // An associative relationship keeps its own identity, so it can take
        // part in further relationships exactly as an entity does.
        auto* associative = new QCheckBox("Associative entity", panel);
        associative->setObjectName("relationshipAssociative");
        associative->setChecked(relationship.associative);
        associative->setToolTip("Give this relationship its own identity so it can take part in other relationships.");
        connect(associative, &QCheckBox::toggled, this, [this, id = relationship.id](bool on) {
            if (refreshing_) return;
            // An associative entity takes the entity body size, since that is
            // what it behaves as. Keep it centred so the diagram does not shift.
            std::optional<domain::Rect> body;
            const auto found = editor_.project().layout.find(domain::ElementRef{id});
            if (found != editor_.project().layout.end()) {
                const auto& size = on ? entity_body : relationship_body;
                const auto& current = found->second;
                body = domain::Rect{current.x + current.width / 2 - size.width / 2,
                                    current.y + current.height / 2 - size.height / 2,
                                    size.width, size.height};
            }
            show_result(editor_.set_associative(id, on, body));
        });
        layout->addWidget(associative);
        // The ratio is the two maximums read together. Setting it here writes
        // both participants at once, so every notation redraws consistently.
        auto* ratio = new QComboBox(panel);
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
        ratio_form->addRow("Ratio", ratio);
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
            auto* title = new QLabel(display_name(project, target_ref(participant.target)), card);
            participant_form->addRow(title);
            auto* maximum = new QComboBox(card);
            maximum->addItems({"1 — One", "M — Many"});
            maximum->setCurrentIndex(participant.maximum == Cardinality::One ? 0 : 1);
            participant_form->addRow("Maximum cardinality", maximum);
            auto* participation = new QComboBox(card);
            participation->addItems({"Partial — optional", "Total — required"});
            participation->setCurrentIndex(participant.participation == Participation::Total ? 1 : 0);
            participant_form->addRow("Participation", participation);
            auto* role = new QLineEdit(text(participant.role), card);
            role->setPlaceholderText("Role (especially for recursive links)");
            role->setMaxLength(512);
            participant_form->addRow("Role", role);
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
    layout->addWidget(new QLabel("Description", panel));
    auto* description_edit = new DescriptionEdit(panel);
    description_edit->setObjectName("elementDescription");
    description_edit->setPlainText(text(description(project, ref)));
    description_edit->setPlaceholderText("Explain this object’s meaning…");
    description_edit->setFixedHeight(100);
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
        geometry->addRow(names[i], fields[i]);
    }
    layout->addLayout(geometry);
    auto* apply_geometry = new QPushButton("Apply position and size", panel);
    apply_geometry->setObjectName("applyGeometry");
    connect(apply_geometry, &QPushButton::clicked, this, [this, ref, fields] {
        show_result(editor_.move({{ref, {fields[0]->value(), fields[1]->value(), fields[2]->value(), fields[3]->value()}}}));
    });
    layout->addWidget(apply_geometry);
    layout->addStretch();
    properties_->setWidget(panel);
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

void MainWindow::selection_changed(const std::vector<ElementRef>& selection) {
    if (refreshing_ || selection_ == selection) return;
    refreshing_ = true;
    selection_ = selection;
    highlight_explorer();
    refresh_properties();
    duplicate_->setEnabled(!selection_.empty());
    rename_->setEnabled(selection_.size() == 1);
    refreshing_ = false;
}

void MainWindow::show_result(const application::EditResult& result) {
    refresh();
    if (!result) {
        statusBar()->showMessage(text(result.error), 12000);
        QMessageBox::warning(this, "Change could not be applied", text(result.error));
    } else if (result.created) canvas_->select_elements({*result.created});
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

void MainWindow::set_icon_mode(IconMode mode) {
    icon_mode_ = mode;
    QSettings().setValue("iconMode", icon_mode_key(mode));
    for (const auto& [candidate, action] : icon_mode_actions_) action->setChecked(candidate == mode);
    refresh_icons();
    refresh_explorer();
}

void MainWindow::set_theme(ThemeId id) {
    theme_ = id;
    if (auto* application = qobject_cast<QApplication*>(QCoreApplication::instance()))
        apply_theme(*application, id);
    canvas_->set_theme(id);
    QSettings().setValue("theme", theme(id).key);
    for (const auto& [candidate, action] : theme_actions_) action->setChecked(candidate == id);
    refresh_icons();
    refresh_explorer();
}

// Icons are drawn from the theme, so they are rebuilt whenever it changes.
void MainWindow::refresh_icons() {
    const auto& colors = theme(theme_);
    for (const auto& [action, glyph] : action_glyphs_)
        action->setIcon(glyph_icon(glyph, colors, 22, icon_mode_));
    if (theme_button_) theme_button_->setIcon(glyph_icon(Glyph::Theme, colors, 22, icon_mode_));
    if (notation_box_) {
        for (int index = 0; index < notation_box_->count(); ++index)
            notation_box_->setItemIcon(index, QIcon(canvas_->notation_preview(
                static_cast<Notation>(index), notation_sample)));
    }
    for (const auto& [style, action] : line_actions_)
        action->setIcon(QIcon(canvas_->line_style_preview(style, line_style_sample)));
}

void MainWindow::refresh_tool_labels() {
    const auto active = canvas_->tool();
    const auto locked = canvas_->tool_locked();
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
    if (event->type() == QEvent::MouseButtonDblClick) {
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

void MainWindow::open_dialog() {
    const auto location = QFileDialog::getOpenFileName(this, "Open ERDFlow project", path_, "ERDFlow project (*.erdx)");
    if (!location.isEmpty()) open_path(location);
}

bool MainWindow::open_path(const QString& path) {
    // Validate the complete candidate before asking to replace the open work.
    auto candidate = store_.load(bytes(path));
    if (!candidate) {
        QMessageBox::warning(this, "Project could not be opened", text(candidate.error));
        return false;
    }
    if (!confirm_discard()) return false;
    // Save in the discard prompt may have updated this very file. Re-read it
    // before installing so the pre-prompt candidate cannot restore old data.
    candidate = store_.load(bytes(path));
    if (!candidate) {
        QMessageBox::warning(this, "Project could not be opened", text(candidate.error));
        return false;
    }
    const auto result = editor_.replace_project(std::move(*candidate.project));
    if (!result) { show_result(result); return false; }
    path_ = path;
    refresh();
    canvas_->fit_diagram();
    return true;
}

void MainWindow::load_example() {
    if (!confirm_discard()) return;
    application::Editor example(ids_);
    example.rename_project("University · Student enrollment");
    const auto student = std::get<EntityId>(*example.create_entity("Student", {-330, -60, 170, 84}).created);
    const auto course = std::get<EntityId>(*example.create_entity("Course", {330, -60, 170, 84}).created);
    const auto enrolled = std::get<RelationshipId>(*example.create_relationship("Enrolled", {0, -70, 180, 104}).created);
    const auto connection = example.connect(enrolled, student);
    example.update_participant(enrolled, *connection.participant, Cardinality::Many, Participation::Total, "student");
    example.connect(enrolled, course);
    const auto student_id = std::get<AttributeId>(*example.create_attribute("StudentID", {-390, -230, 150, 70}, student).created);
    example.set_attribute_kind(student_id, AttributeKind::Key);
    example.create_attribute("Name", {-190, -230, 140, 70}, student);
    const auto course_id = std::get<AttributeId>(*example.create_attribute("CourseID", {270, -230, 150, 70}, course).created);
    example.set_attribute_kind(course_id, AttributeKind::Key);
    example.create_attribute("Title", {470, -230, 140, 70}, course);
    example.create_attribute("Grade", {15, 135, 150, 70}, enrolled);
    show_result(editor_.replace_project(example.project()));
    path_.clear();
    canvas_->select_elements({enrolled});
    canvas_->fit_diagram();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (confirm_discard()) event->accept(); else event->ignore();
}

} // namespace erdflow::desktop
