#include "app/desktop/icons.hpp"
#include "app/desktop/symbols.hpp"
#include "app/desktop/export_dialog.hpp"
#include "app/desktop/main_window.hpp"
#include "infrastructure/project_store.hpp"

#include <QAction>
#include <QApplication>
#include <QStatusBar>
#include <QElapsedTimer>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDockWidget>
#include <QFontMetrics>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QDoubleSpinBox>
#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QAbstractButton>
#include <QSettings>
#include <QSlider>
#include <QWidgetAction>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QWheelEvent>
#include <QToolButton>
#include <QTreeView>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>

namespace {
using namespace erdflow;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void settle() {
    QApplication::processEvents();
    QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}
// Some work is deliberately not done on the instant it is asked for. A theme
// hovered in the menu is shown once the pointer has settled rather than while
// it is still travelling, so a test waiting for one has to let the clock run
// as well as the event loop.
void settle_for(int milliseconds) {
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < milliseconds) {
        QApplication::processEvents(QEventLoop::AllEvents, 5);
        settle();
    }
}
template<class T> T* child(desktop::MainWindow& window, const char* name) {
    settle();
    auto* value = window.findChild<T*>(QString::fromLatin1(name));
    require(value != nullptr, name);
    return value;
}
void dismiss(QMessageBox::StandardButton choice) {
    QTimer::singleShot(0, [choice] {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            box->button(choice)->click();
    });
}
// A list's itemClicked only comes from real pointer work, so a test says what
// it means directly: this item was chosen.
void QTest_activate(QListWidget* list, QListWidgetItem* item) {
    list->setCurrentItem(item);
    emit list->itemActivated(item);
}
void click_canvas(desktop::DiagramView& canvas, QPointF position) {
    const auto local = canvas.mapFromScene(position);
    const auto global = canvas.viewport()->mapToGlobal(local);
    QMouseEvent press(QEvent::MouseButtonPress, QPointF(local), QPointF(global), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(canvas.viewport(), &press);
    QMouseEvent release(QEvent::MouseButtonRelease, QPointF(local), QPointF(global), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(canvas.viewport(), &release);
    settle();
}
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    // The window remembers the chosen theme. Point that at a throwaway domain so
    // running the tests cannot disturb the real preferences.
    QCoreApplication::setOrganizationName("ERDFlowTests");
    QCoreApplication::setApplicationName("ERDFlowTests");
    // Start from nothing, so a remembered value has to be written by this run
    // rather than left behind by the last one.
    QSettings().clear();
    try {
        infrastructure::QtIdGenerator ids;
        application::Editor editor(ids);
        infrastructure::ErdxProjectStore project_store;
        desktop::MainWindow window(editor, project_store, ids);
        window.show();
        window.activateWindow();
        settle();
        require(window.editor().project().entities.empty(), "New window is an empty project");
        window.load_example();
        settle();
        const auto original = window.editor().project();
        // What follows counts from the example rather than from a number typed
        // here, so the diagram it ships with can grow without the test having
        // to be re-tallied line by line.
        const auto example_entities = original.entities.size();
        const auto example_attributes = original.attributes.size();
        require(example_entities == 3 && example_attributes == 16 && original.relationships.size() == 3,
                "The example is the whole university diagram, not a fragment of it");
        // It is the example because one of every kind of attribute is on it,
        // so opening it puts the whole of the notation on the canvas at once.
        std::map<domain::AttributeKind, int> kinds;
        std::size_t parts_of_composites = 0;
        std::size_t on_relationships = 0;
        for (const auto& [id, attribute] : original.attributes) {
            ++kinds[attribute.kind];
            if (!attribute.owner) continue;
            if (std::holds_alternative<domain::AttributeId>(*attribute.owner)) ++parts_of_composites;
            if (std::holds_alternative<domain::RelationshipId>(*attribute.owner)) ++on_relationships;
        }
        require(kinds[domain::AttributeKind::Key] == 3 && kinds[domain::AttributeKind::Composite] == 1
                    && kinds[domain::AttributeKind::Derived] == 1 && kinds[domain::AttributeKind::Multivalued] == 1,
                "Key, composite, derived and multivalued are all drawn on it");
        require(parts_of_composites == 3, "The composite name has its three parts hanging off it");
        require(on_relationships == 1, "And the enrollment carries the date that belongs to neither side");
        bool total = false;
        bool partial = false;
        for (const auto& [id, relationship] : original.relationships) {
            require(relationship.participants.size() == 2, "Every relationship on it is joined at both ends");
            for (const auto& participant : relationship.participants)
                (participant.participation == domain::Participation::Total ? total : partial) = true;
        }
        require(total && partial, "With both minimums shown, so the pair beside a line is worth reading");
        require(!window.editor().dirty(), "Unmodified bundled example is clean");
        auto student = original.entities.begin()->first;
        for (const auto& [id, entity] : original.entities) if (entity.name == "Student") student = id;
        window.canvas()->select_elements({student});
        auto* name = child<QLineEdit>(window, "elementName");
        name->setFocus();
        name->setText("UniversityStudent");
        window.canvas()->setFocus();
        settle();
        require(window.editor().project().entities.at(student).name == "UniversityStudent", "Name commits through properties on focus loss");
        require(window.editor().dirty() && window.isWindowModified(), "Applied field edit marks project dirty");
        auto* undo = child<QAction>(window, "undoCommand");
        require(undo->isEnabled(), "Undo action reflects history");
        // Undo and redo must be reachable without opening a menu, and the
        // toolbar button must not resize as the named edit changes.
        auto* tools = child<QToolBar>(window, "modelTools");
        require(tools->actions().contains(undo), "Undo is on the toolbar");
        require(tools->actions().contains(child<QAction>(window, "redoCommand")), "Redo is on the toolbar");
        require(undo->text().startsWith("Undo ") && undo->text() != "Undo",
                "The menu entry names the edit that will be reversed");
        require(undo->iconText() == "Undo", "The toolbar button keeps fixed wording");
        require(undo->toolTip().contains(undo->text()), "The toolbar tooltip explains the edit");
        child<QAction>(window, "undoCommand")->trigger();
        require(window.editor().project() == original && !window.editor().dirty(), "Shell undo restores clean save point");
        child<QAction>(window, "redoCommand")->trigger();
        require(window.editor().project().entities.at(student).name == "UniversityStudent", "Shell redo applies rename");

        auto* description = child<QPlainTextEdit>(window, "elementDescription");
        description->setFocus();
        description->setPlainText("A student enrolled in courses.\nNames may change; identity stays stable.");
        window.canvas()->setFocus();
        settle();
        require(window.editor().project().entities.at(student).description.find("identity") != std::string::npos,
                "Description commits through command path");

        name = child<QLineEdit>(window, "elementName");
        name->setFocus();
        name->setCursorPosition(static_cast<int>(name->text().size()));
        QKeyEvent backspace(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
        QApplication::sendEvent(name, &backspace);
        require(window.editor().project().entities.size() == example_entities, "Backspace in property field cannot delete entity");
        window.canvas()->setFocus();
        settle();

        const auto relationship = original.relationships.begin()->first;
        const auto participant = original.relationships.begin()->second.participants.front().id;
        window.canvas()->select_elements({relationship});
        settle();
        // Count the combos inside the participant cards rather than every combo
        // in the panel: the panel also carries the relationship's own controls.
        auto cards = child<QDockWidget>(window, "propertiesDock")->findChildren<QWidget*>("participantCard");
        require(cards.size() == 2, "Relationship renders two independent participant forms");

        // A card names its side in the colour that element is drawn in, and
        // gives each thing it asks about a bold hue of its own, so the rows
        // are told apart at a glance rather than read in order.
        {
            auto* first_card = cards.front();
            auto* title = first_card->findChild<QWidget*>("cardTitle");
            require(title != nullptr, "A card names its side");
            const auto display_name_of_relationship =
                QString::fromStdString(domain::name(window.editor().project(), domain::ElementRef{relationship}));
            require(!display_name_of_relationship.isEmpty(), "The relationship has a name to show");
            const auto& colors = desktop::theme(window.canvas()->theme_id());
            const auto tag = [&](QWidget* row, const char* named) {
                auto* found = row->findChild<QWidget*>(QString::fromLatin1(named));
                require(found != nullptr, "Each end of a card is shown as its own shape");
                return found;
            };
            auto* side_tag = tag(title, "cardShape");
            auto* toward_tag = tag(title, "cardTowardShape");
            require(toward_tag->toolTip() == display_name_of_relationship,
                    "The far one being the relationship itself");
            require(!side_tag->toolTip().isEmpty() && side_tag->toolTip() != toward_tag->toolTip(),
                    "And the near one the element taking part in it");
            require(title->findChild<QLabel*>("cardArrow") != nullptr,
                    "With a mark between them that says it is joined to it");

            // Each name is written inside its element's own shape, in that
            // element's colour, rather than in a box beside a picture of one.
            const auto shows = [](QWidget* widget, const QColor& wanted) {
                const auto drawn = widget->grab().toImage().convertToFormat(QImage::Format_ARGB32);
                for (int y = 0; y < drawn.height(); ++y)
                    for (int x = 0; x < drawn.width(); ++x) {
                        const auto pixel = drawn.pixelColor(x, y);
                        if (pixel.alpha() > 200 && std::abs(pixel.red() - wanted.red()) < 12
                            && std::abs(pixel.green() - wanted.green()) < 12
                            && std::abs(pixel.blue() - wanted.blue()) < 12) return true;
                    }
                return false;
            };
            require(shows(side_tag, colors.entity_fill), "The side wears the entity's own colour");
            require(shows(toward_tag, colors.relationship_fill), "And the relationship its own");

            // A recoloured entity carries its new colour into the card.
            const auto side = window.editor().project().relationships.at(relationship).participants.front().target;
            require(bool(editor.recolour({domain::target_ref(side)}, domain::Colour{0x9E, 0xE8, 0xC4})), "Colour that entity");
            // An edit made straight on the editor does not pass through the
            // window, so the canvas is brought up to date and the panel asked
            // to rebuild, which is the order every edit through the window
            // follows: the shapes the panel shows are the canvas's own.
            window.canvas()->synchronize();
            window.canvas()->select_elements({});
            window.canvas()->select_elements({relationship});
            settle();
            cards = child<QDockWidget>(window, "propertiesDock")->findChildren<QWidget*>("participantCard");
            title = cards.front()->findChild<QWidget*>("cardTitle");
            require(title && shows(tag(title, "cardShape"), QColor(0x9E, 0xE8, 0xC4)),
                    "The card follows the element's own colour");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            cards = child<QDockWidget>(window, "propertiesDock")->findChildren<QWidget*>("participantCard");
            require(cards.size() == 2, "The cards are still there after the undo");

            const auto labels = cards.front()->findChildren<QLabel*>("fieldLabel");
            require(labels.size() == 3, "Its three questions are each named");
            std::set<QString> inks;
            for (auto* label : labels) {
                require(label->styleSheet().contains("font-weight: 700"), "Each name is bold");
                inks.insert(label->styleSheet());
            }
            require(inks.size() == 1, "All in one ink: the words tell them apart, so colour is left to the element");
        }

        // The rest of the panel is named the same way: every field label bold
        // and in one ink, with the colour kept for the heading, which wears
        // the colour its element is drawn in on the diagram.
        {
            auto* properties = child<QDockWidget>(window, "propertiesDock");
            const auto ink_of = [](QLabel* label) {
                return label->styleSheet().section("color: ", 1, 1).section(';', 0, 0).trimmed();
            };
            const auto label_named = [&](const QString& text) -> QLabel* {
                for (auto* label : properties->findChildren<QLabel*>("fieldLabel"))
                    if (label->text() == text) return label;
                return nullptr;
            };
            require(label_named("Name") && label_named("Kind") && label_named("Ratio"),
                    "A relationship names its Name, Kind and Ratio");
            std::set<QString> panel_inks;
            for (auto* label : properties->findChildren<QLabel*>("fieldLabel")) {
                require(label->styleSheet().contains("font-weight: 700"), "Every label is bold");
                panel_inks.insert(ink_of(label));
            }
            require(panel_inks.size() == 1, "And every one of them is written in the same ink");
            require(*panel_inks.begin() == desktop::theme(window.canvas()->theme_id()).text.name(),
                    "Which is the theme's own");

            // The heading names the kind being edited and is left to the
            // theme, so it reads as a title rather than as a second copy of
            // the colour the name field already carries.
            auto* heading = child<QLabel>(window, "propertyHeading");
            require(heading->text() == "Relationship", "The heading names the kind being edited");
            require(heading->styleSheet().isEmpty(), "And is left to the theme");

            // The labels follow a theme change without having to be reselected.
            child<QAction>(window, "themeplain")->trigger();
            settle();
            std::set<QString> grey_inks;
            for (auto* label : properties->findChildren<QLabel*>("fieldLabel")) grey_inks.insert(ink_of(label));
            require(grey_inks.size() == 1
                        && *grey_inks.begin() == desktop::theme(desktop::ThemeId::Plain).text.name(),
                    "They are rewritten in the new theme's ink");
            child<QAction>(window, "themeofficelight")->trigger();
            settle();
            require(child<QDockWidget>(window, "propertiesDock")->findChildren<QLabel*>("fieldLabel").size() > 1
                        && label_named("Kind") != nullptr,
                    "And the panel comes back with the theme");
            // Changing the theme rebuilds the panel, so anything held from
            // before it is gone: the cards are found again for what follows.
            cards = properties->findChildren<QWidget*>("participantCard");
            require(cards.size() == 2, "The two cards are still shown");
        }
        QList<QComboBox*> combos;
        for (auto* card : cards) combos.append(card->findChildren<QComboBox*>());
        require(combos.size() == 4, "Each side carries its own cardinality and participation");
        combos.front()->setCurrentIndex(0);
        QMetaObject::invokeMethod(combos.front(), "activated", Q_ARG(int, 0));
        settle();
        require(window.editor().project().relationships.at(relationship).participants.front().id == participant,
                "Cardinality edit preserves participant ID");
        require(window.editor().project().relationships.at(relationship).participants.front().maximum == domain::Cardinality::One,
                "Participant form applies cardinality");

        // The ratio sets both sides at once and stays in step with them.
        auto* ratio = child<QComboBox>(window, "relationshipRatio");
        require(ratio->count() == 4, "All four ratios are offered");
        require(ratio->itemText(0) == "1:1" && ratio->itemText(1) == "1:M"
                && ratio->itemText(2) == "M:1" && ratio->itemText(3) == "M:M", "They read 1:1, 1:M, M:1, M:M");
        const auto ratio_revision = window.editor().revision();
        ratio->setCurrentIndex(2);
        QMetaObject::invokeMethod(ratio, "activated", Q_ARG(int, 2));
        settle();
        const auto& sides = window.editor().project().relationships.at(relationship).participants;
        require(sides[0].maximum == domain::Cardinality::Many && sides[1].maximum == domain::Cardinality::One,
                "M:1 writes both sides");
        require(window.editor().revision() == ratio_revision + 1, "Both sides move in one edit");
        require(child<QComboBox>(window, "relationshipRatio")->currentIndex() == 2, "The picker reports the ratio in use");

        // Reversing swaps the sides' constraints, turning M:1 back into 1:M.
        child<QPushButton>(window, "reverseRelationship")->click();
        settle();
        const auto& reversed = window.editor().project().relationships.at(relationship).participants;
        require(reversed[0].maximum == domain::Cardinality::One && reversed[1].maximum == domain::Cardinality::Many,
                "Reverse swaps the ratio");
        require(reversed[0].id == participant, "Reversing keeps participant identity");
        require(child<QComboBox>(window, "relationshipRatio")->currentIndex() == 1, "The picker follows the reversal");
        child<QAction>(window, "undoCommand")->trigger();
        child<QAction>(window, "undoCommand")->trigger();

        // Notation must be visible in the toolbar, not buried in a submenu, and
        // the two controls must never disagree about which one is in use.
        auto* picker = child<QComboBox>(window, "notationPicker");
        require(tools->findChildren<QComboBox*>().contains(picker), "The notation picker is on the toolbar");
        require(picker->count() == 4, "All four notations are offered");
        for (int index = 0; index < picker->count(); ++index)
            require(!picker->itemIcon(index).isNull(), "Each notation is drawn, not just named");
        require(picker->currentIndex() == static_cast<int>(window.canvas()->notation()), "The picker starts in step");
        picker->setCurrentIndex(static_cast<int>(desktop::Notation::CrowsFoot));
        settle();
        require(window.canvas()->notation() == desktop::Notation::CrowsFoot, "The picker changes the canvas");
        require(child<QAction>(window, "notationCrowsfoot")->isChecked(), "The menu follows the picker");
        child<QAction>(window, "notationBachman")->trigger();
        settle();
        require(window.canvas()->notation() == desktop::Notation::Bachman, "The menu changes the canvas");
        require(picker->currentIndex() == static_cast<int>(desktop::Notation::Bachman), "The picker follows the menu");
        child<QAction>(window, "notationChen")->trigger();
        settle();

        window.canvas()->select_elements({student});
        child<QAction>(window, "duplicateElements")->trigger();
        require(window.editor().project().entities.size() == example_entities + 1
                    && window.editor().project().attributes.size() == example_attributes + 9,
                "Duplicate copies entity and owned attributes through one command");
        child<QAction>(window, "undoCommand")->trigger();
        require(window.editor().project().entities.size() == example_entities, "One undo removes whole duplicate");

        QTemporaryDir directory;
        require(directory.isValid(), "Temporary test directory");
        const auto path = directory.filePath("test.erdx");
        infrastructure::ErdxProjectStore store;
        const auto saved = window.editor().project();
        require(store.save(path.toStdString(), saved).ok, "Save fixture through production adapter");
        dismiss(QMessageBox::Cancel);
        require(!window.open_path(path), "Cancel protects unsaved project");
        require(window.editor().project() == saved && window.editor().dirty(), "Cancel preserves current state");
        dismiss(QMessageBox::Discard);
        require(window.open_path(path), "Valid candidate opens after discard");
        require(window.editor().project() == saved && !window.editor().dirty() && !window.editor().can_undo(),
                "Opened project restores graph and starts clean history");

        window.activateWindow();
        settle();
        window.canvas()->select_elements({student});
        name = child<QLineEdit>(window, "elementName");
        name->setFocus();
        name->setText("Saved through File menu");
        require(name->hasFocus(), "Save fixture has an active property field");
        child<QAction>(window, "saveProject")->trigger();
        require(!window.editor().dirty(), "Save action commits focused text before saving");
        const auto loaded = store.load(path.toStdString());
        require(loaded.project && loaded.project->entities.at(student).name == "Saved through File menu",
                "File menu save persists the focused field value");
        window.canvas()->select_elements({student});
        name = child<QLineEdit>(window, "elementName");
        name->setFocus();
        name->setText("Saved while reopening");
        window.canvas()->setFocus();
        settle();
        dismiss(QMessageBox::Save);
        require(window.open_path(path), "Save and reopen the current file");
        require(window.editor().project().entities.at(student).name == "Saved while reopening",
                "Reopen installs newly saved contents, not the candidate read before the save prompt");
        QFile invalid(directory.filePath("invalid.erdx"));
        require(invalid.open(QIODevice::WriteOnly), "Create malformed file");
        invalid.write("{}");
        invalid.close();
        const auto before_failed_load = window.editor().project();
        dismiss(QMessageBox::Ok);
        require(!window.open_path(invalid.fileName()), "Malformed file is rejected by shell");
        require(window.editor().project() == before_failed_load, "Failed open leaves current model intact");

        auto* entity_tool = child<QAction>(window, "toolEntity");
        entity_tool->trigger();
        click_canvas(*window.canvas(), QPointF(200, 250));
        require(window.editor().project().entities.size() == example_entities + 1
                    && window.canvas()->tool() == desktop::Tool::Select,
                "Toolbar create routes through canvas and returns to Select");
        require(entity_tool->text() == "Entity", "An unlocked tool button carries no mark");

        // Double-clicking the button locks the tool, and the button says so.
        auto* button = child<QToolBar>(window, "modelTools")->widgetForAction(entity_tool);
        require(button != nullptr, "The entity tool has a toolbar button");
        QMouseEvent double_click(QEvent::MouseButtonDblClick, QPointF(5, 5), QPointF(5, 5),
                                 Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(button, &double_click);
        settle();
        require(window.canvas()->tool_locked(), "Double-clicking the button locks the tool");
        require(entity_tool->text().startsWith("Entity") && entity_tool->text() != "Entity",
                "A locked tool button is marked");
        click_canvas(*window.canvas(), QPointF(360, 250));
        click_canvas(*window.canvas(), QPointF(520, 250));
        require(window.editor().project().entities.size() == example_entities + 3, "A locked tool keeps placing");
        require(window.canvas()->tool() == desktop::Tool::Entity,
                "A locked tool stays selected");
        // ISA is one toolbar entry offering both directions; the dropdown picks
        // the mode and the button itself locks like every other tool.
        auto* isa = child<QAction>(window, "toolIsa");
        require(isa->text() == "Specialization", "ISA starts in its top-down mode");
        child<QAction>(window, "isaGeneralization")->trigger();
        settle();
        require(window.canvas()->tool() == desktop::Tool::Generalization, "The dropdown switches direction");
        require(isa->text() == "Generalization", "The button reports the chosen direction");
        require(!window.canvas()->tool_locked(), "Choosing a direction does not lock it");
        auto* isa_button = child<QToolButton>(window, "isaButton");
        QMouseEvent isa_double(QEvent::MouseButtonDblClick, QPointF(5, 5), QPointF(5, 5),
                               Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(isa_button, &isa_double);
        settle();
        require(window.canvas()->tool_locked() && window.canvas()->tool() == desktop::Tool::Generalization,
                "Double-clicking ISA locks the chosen direction");
        require(isa->text().startsWith("Generalization") && isa->text() != "Generalization",
                "The locked ISA button is marked");
        child<QAction>(window, "isaSpecialization")->trigger();
        settle();
        require(window.canvas()->tool() == desktop::Tool::Specialization && !window.canvas()->tool_locked(),
                "Choosing the other direction resets to one-shot");

        // The line style sits on the Connect button's own arrow, with each
        // option drawn rather than only named.
        require(window.canvas()->line_style() == desktop::LineStyle::Elbow,
                "Lines break at right angles unless told otherwise");
        auto* straight = child<QAction>(window, "lineStraight");
        auto* curved = child<QAction>(window, "lineCurved");
        auto* elbow = child<QAction>(window, "lineElbow");
        require(!straight->icon().isNull() && !curved->icon().isNull() && !elbow->icon().isNull(),
                "Each line style is drawn, not just named");
        require(elbow->isChecked() && !curved->isChecked() && !straight->isChecked(),
                "The menu marks the style in use");
        straight->trigger();
        settle();
        require(window.canvas()->line_style() == desktop::LineStyle::Straight, "The menu changes the line style");
        require(straight->isChecked() && !curved->isChecked() && !elbow->isChecked(), "The mark follows the choice");
        // Choosing a style must not silently change which tool is active.
        const auto tool_before = window.canvas()->tool();
        curved->trigger();
        settle();
        require(window.canvas()->line_style() == desktop::LineStyle::Curved, "And the other two are offered too");
        elbow->trigger();
        settle();
        require(window.canvas()->line_style() == desktop::LineStyle::Elbow, "And back again");
        require(window.canvas()->tool() == tool_before, "Choosing a line style leaves the active tool alone");
        // The button still behaves like a tool, including its double-click lock.
        auto* connect_button = child<QToolButton>(window, "connectButton");
        QMouseEvent connect_double(QEvent::MouseButtonDblClick, QPointF(5, 5), QPointF(5, 5),
                                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(connect_button, &connect_double);
        settle();
        require(window.canvas()->tool() == desktop::Tool::Connect && window.canvas()->tool_locked(),
                "Double-clicking Connect locks it");

        // Every toolbar action carries a drawn icon, and the icons follow the
        // theme, since they are painted from it rather than loaded from files.
        auto* tool_bar = child<QToolBar>(window, "modelTools");
        for (auto* action : tool_bar->actions())
            if (!action->isSeparator() && !action->text().isEmpty())
                require(!action->icon().isNull(), "Every toolbar action is given an icon");
        const auto entity_icon = child<QAction>(window, "toolEntity")->icon()
            .pixmap(18, 18).toImage();
        child<QAction>(window, "themedracula")->trigger();
        settle();
        require(window.canvas()->theme_id() == desktop::ThemeId::Dracula, "The menu changes the canvas theme");
        require(child<QAction>(window, "toolEntity")->icon().pixmap(18, 18).toImage() != entity_icon,
                "Icons are redrawn for the new theme");
        require(QSettings().value("theme").toString() == "dracula", "The choice is remembered");
        child<QAction>(window, "themeofficelight")->trigger();
        settle();
        require(window.canvas()->theme_id() == desktop::ThemeId::OfficeLight, "And back again");
        require(child<QAction>(window, "toolEntity")->icon().pixmap(18, 18).toImage() == entity_icon,
                "Returning to a theme restores its icons");

        // A glyph is a drawing, not a silhouette. The hand is the shape most at
        // risk: it is a stack of overlapping rounded rects, so once its stroke
        // approaches a finger's width the outlines merge and the whole icon
        // fills in as one mass of outline colour. Measuring how much of the palm
        // still carries the fill colour rather than the outline's catches exactly
        // that collapse; the drawn-at-all check covers the rest of the set. The
        // comparison is against the theme's own two colours, not a fixed
        // brightness, so it holds however light or dark the palette is.
        const auto& glyph_theme = desktop::theme(desktop::ThemeId::OfficeLight);
        const auto fill_grey = qGray(glyph_theme.base.rgb());
        const auto outline_grey = qGray(glyph_theme.muted.rgb());
        const auto share_of_fill = [&](desktop::Glyph glyph) {
            const auto drawn = desktop::glyph_icon(glyph, glyph_theme, 22)
                                   .pixmap(22, 22).toImage().convertToFormat(QImage::Format_ARGB32);
            int opaque = 0;
            int filled = 0;
            for (int y = 0; y < drawn.height(); ++y)
                for (int x = 0; x < drawn.width(); ++x) {
                    const auto pixel = drawn.pixel(x, y);
                    if (qAlpha(pixel) < 200) continue;
                    ++opaque;
                    const auto grey = qGray(pixel);
                    if (std::abs(grey - fill_grey) < std::abs(grey - outline_grey)) ++filled;
                }
            require(opaque > 40, "Every glyph draws something at toolbar size");
            return static_cast<double>(filled) / static_cast<double>(opaque);
        };
        for (int index = 0; index <= static_cast<int>(desktop::Glyph::Delete); ++index)
            share_of_fill(static_cast<desktop::Glyph>(index));
        require(share_of_fill(desktop::Glyph::Pan) > 0.3, "The hand keeps an open palm rather than filling in");

        // An element given a colour of its own wears it in the properties panel,
        // so the panel and the shape on the canvas read as the same object.
        {
            const auto entity = window.editor().project().entities.begin()->first;
            window.canvas()->select_elements({domain::ElementRef{entity}});
            settle();
            // The name is written inside the shape the element is drawn as, so
            // the colour is carried by that shape rather than by a plain box.
            const auto& palette = desktop::theme(window.canvas()->theme_id());
            const auto shape_shows = [](QWidget* widget, const QColor& wanted) {
                const auto drawn = widget->grab().toImage().convertToFormat(QImage::Format_ARGB32);
                for (int y = 0; y < drawn.height(); ++y)
                    for (int x = 0; x < drawn.width(); ++x) {
                        const auto pixel = drawn.pixelColor(x, y);
                        if (pixel.alpha() > 200 && std::abs(pixel.red() - wanted.red()) < 12
                            && std::abs(pixel.green() - wanted.green()) < 12
                            && std::abs(pixel.blue() - wanted.blue()) < 12) return true;
                    }
                return false;
            };
            require(shape_shows(child<QWidget>(window, "elementShape"), palette.entity_fill),
                    "The name is written in a shape wearing the element's theme colour");
            require(child<QLineEdit>(window, "elementName")->parent() == child<QWidget>(window, "elementShape"),
                    "And the name is inside that shape rather than beside it");
            // The heading says what kind of thing this is and stays a title.
            require(child<QLabel>(window, "propertyHeading")->styleSheet().isEmpty(),
                    "The kind heading is left to the theme");

            require(bool(editor.recolour({domain::ElementRef{entity}}, domain::Colour{0x20, 0x20, 0x30})),
                    "Colour the entity a dark shade");
            // The shapes the panel shows are the canvas's own, so the canvas is
            // brought up to date first, which is the order every edit made
            // through the window follows.
            window.canvas()->synchronize();
            window.canvas()->select_elements({});
            settle();
            window.canvas()->select_elements({domain::ElementRef{entity}});
            settle();
            require(shape_shows(child<QWidget>(window, "elementShape"), QColor(0x20, 0x20, 0x30)),
                    "The shape is filled with the element's own colour");
            require(child<QLineEdit>(window, "elementName")->styleSheet().contains("#ffffff"),
                    "And the name written in ink chosen against it, not against the theme");
            require(child<QLabel>(window, "propertyHeading")->styleSheet().isEmpty(),
                    "The heading still carries no colour of the element's");
            require(bool(editor.undo()), "Undo the colour");
        }

        // Three sets, and the window has to be able to wear any of them. The
        // outline set is line art inked from the theme, the modern set is
        // artwork carrying its own colour, and the painted set is drawn from
        // the palette; each must cover every glyph the window uses, or a
        // button silently falls back and the sets disagree about what is there.
        {
            require(window.icon_mode() == desktop::IconMode::Outline, "The window wears the line art to begin with");
            const auto lined = child<QAction>(window, "toolEntity")->icon().pixmap(22, 22).toImage();
            for (int index = 0; index <= static_cast<int>(desktop::Glyph::Symbols); ++index) {
                const auto glyph = static_cast<desktop::Glyph>(index);
                for (const char* set : {"icons", "icons-on-dark", "icons-outline"}) {
                    const QIcon file(QStringLiteral(":/erdflow/%1/%2.svg")
                                         .arg(QString::fromLatin1(set), desktop::icon_name(glyph)));
                    require(!file.pixmap(22, 22).isNull(), "Every set has a file for every glyph the window draws");
                }
            }
            child<QAction>(window, "iconsmodern")->trigger();
            settle();
            require(window.icon_mode() == desktop::IconMode::Modern, "The menu changes the icon set");
            const auto artwork = child<QAction>(window, "toolEntity")->icon().pixmap(22, 22).toImage();
            require(!artwork.isNull() && artwork != lined, "Every button takes the new set");
            require(QSettings().value("iconMode").toString() == "modern", "The choice is remembered");
            child<QAction>(window, "iconsnormal")->trigger();
            settle();
            const auto painted = child<QAction>(window, "toolEntity")->icon().pixmap(22, 22).toImage();
            require(painted != artwork && painted != lined, "And the painted set is a third thing again");
            child<QAction>(window, "iconsoutline")->trigger();
            settle();
            require(child<QAction>(window, "toolEntity")->icon().pixmap(22, 22).toImage() == lined,
                    "Going back restores the line art");

            // The line art is inked from the theme, which is what the artwork
            // cannot do: changing the palette has to change the drawing.
            child<QAction>(window, "themedracula")->trigger();
            settle();
            require(child<QAction>(window, "toolEntity")->icon().pixmap(22, 22).toImage() != lined,
                    "The line art is inked from the theme");
            child<QAction>(window, "themeofficelight")->trigger();
            settle();

            // A tool that is on sits on a chip of the theme's accent. Inked for
            // the panel it would all but vanish there, so the "on" state has to
            // be a second inking that reads against the accent instead.
            for (const auto glyph : {desktop::Glyph::Select, desktop::Glyph::Connect, desktop::Glyph::Pan}) {
                const auto& colors = desktop::theme(desktop::ThemeId::OfficeLight);
                const auto icon = desktop::glyph_icon(glyph, colors, 22, desktop::IconMode::Outline);
                const auto resting = icon.pixmap(22, 22, QIcon::Normal, QIcon::Off).toImage();
                const auto lit = icon.pixmap(22, 22, QIcon::Normal, QIcon::On).toImage();
                require(!lit.isNull() && lit != resting, "A tool that is on is inked again");
                // The solidest pixel of the drawing is the ink itself, the rest
                // of the line being the softened edge of the same colour.
                QColor ink;
                int most = 0;
                for (int y = 0; y < lit.height(); ++y)
                    for (int x = 0; x < lit.width(); ++x) {
                        const auto pixel = lit.pixelColor(x, y);
                        if (pixel.alpha() > most) { most = pixel.alpha(); ink = pixel; }
                    }
                require(most > 200, "The lit drawing is actually there");
                const auto wanted = desktop::readable_on(colors.accent);
                require(std::abs(ink.red() - wanted.red()) < 24 && std::abs(ink.green() - wanted.green()) < 24
                            && std::abs(ink.blue() - wanted.blue()) < 24,
                        "And it is inked in whatever reads on the accent");
            }
        }

        // Insert offers the characters an ERD wants and a keyboard has not
        // got: the relational algebra signs above all, and the marks and emoji
        // a note is annotated with. They go into whatever field is being
        // written in, which means the gallery has to find that field again
        // after a commit has rebuilt the properties panel underneath it.
        {
            require(child<QMenu>(window, "insertMenu")->actions().contains(child<QAction>(window, "insertSymbols")),
                    "Insert carries the symbol gallery");
            require(child<QToolBar>(window, "insertTools")->actions().contains(child<QAction>(window, "insertSymbols")),
                    "And the ribbon's Insert row carries it too");

            // A character no font can draw would show as an empty box, so the
            // table is measured against the interface font rather than trusted.
            QFont measuring = QApplication::font();
            measuring.setPointSizeF(17);
            const QFontMetrics metrics(measuring);
            std::size_t characters = 0;
            for (const auto& group : desktop::symbol_groups()) {
                require(!group.symbols.empty(), "Every group offers something");
                for (const auto& symbol : group.symbols) {
                    require(!symbol.character.isEmpty() && !symbol.name.isEmpty(), "Every character is named");
                    require(metrics.horizontalAdvance(symbol.character) > 0, "And something can draw every character");
                    // Several of the people are joined sequences: a person and
                    // what they do, written as two emoji the font draws as one.
                    // A font that does not join them draws two, which is wider
                    // than the picker's cell and comes out as an ellipsis, so
                    // the width is measured rather than assumed.
                    require(metrics.horizontalAdvance(symbol.character) <= 44,
                            "And every character fits the cell it is drawn in");
                    ++characters;
                }
            }
            require(characters > 150, "The gallery is worth opening");

            // With nothing chosen there is no field to write in, so the
            // character goes on the diagram itself, as a note carrying it.
            // That is the whole point of picking one, and it undoes like any
            // other edit.
            window.canvas()->select_elements({});
            window.canvas()->setFocus();
            settle();
            require(window.findChild<QLineEdit*>("elementName") == nullptr, "Nothing chosen means no name field");
            const auto notes_before = window.editor().project().notes.size();
            require(window.insert_symbol(QStringLiteral("⋈")), "With no field open the character goes on the diagram");
            require(window.editor().project().notes.size() == notes_before + 1, "As a note carrying it");
            require(std::any_of(window.editor().project().notes.begin(), window.editor().project().notes.end(),
                                [](const auto& entry) { return entry.second.name == "⋈" && entry.second.plain; }),
                    "And the note is the character, drawn bare");
            require(window.editor().undo_label() == "Insert symbol", "The history says what was done");
            // Putting a character down is not choosing something to work on,
            // so the panel is left alone rather than swapped over to it.
            require(window.canvas()->selected_elements().empty(), "Placing one chooses nothing");
            require(window.findChild<QLabel*>("propertyHeading") == nullptr, "So the panel is left as it was");
            // Chosen deliberately, though, it says what it is: a card and a
            // character drawn bare are not the same thing to anyone looking.
            const auto placed = std::find_if(window.editor().project().notes.begin(),
                                             window.editor().project().notes.end(),
                                             [](const auto& entry) { return entry.second.plain; });
            require(placed != window.editor().project().notes.end(), "The symbol is there to be chosen");
            window.canvas()->select_elements({domain::ElementRef{placed->first}});
            settle();
            require(child<QLabel>(window, "propertyHeading")->text() == QStringLiteral("Symbol"),
                    "And the panel calls it a symbol, not a note");
            window.canvas()->select_elements({});
            settle();
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(window.editor().project().notes.size() == notes_before, "Placing one undoes like any other edit");

            // It goes where the user was working, which is where the pointer
            // last was over the diagram, not in the middle of the view.
            {
                auto* canvas = window.canvas();
                const QPoint spot(canvas->viewport()->width() / 4, canvas->viewport()->height() / 4);
                QMouseEvent move(QEvent::MouseMove, QPointF(spot), canvas->viewport()->mapToGlobal(spot),
                                 Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(canvas->viewport(), &move);
                settle();
                const auto wanted = canvas->mapToScene(spot);
                require(canvas->pointer_place().has_value(), "The canvas remembers where the pointer was");
                require(window.insert_symbol(QStringLiteral("π")), "A character is placed");
                const auto found = std::find_if(window.editor().project().notes.begin(),
                                                window.editor().project().notes.end(),
                                                [](const auto& entry) { return entry.second.name == "π"; });
                require(found != window.editor().project().notes.end(), "And it is there");
                const auto box = window.editor().project().layout.at(domain::ElementRef{found->first});
                require(std::abs(box.x + box.width / 2 - wanted.x()) < 1.5
                            && std::abs(box.y + box.height / 2 - wanted.y()) < 1.5,
                        "Centred where the pointer last was");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
            }

            auto chosen = window.editor().project().entities.begin()->first;
            for (const auto& [id, entity] : window.editor().project().entities)
                if (entity.name == "Course") chosen = id;
            window.canvas()->select_elements({chosen});
            auto* name = child<QLineEdit>(window, "elementName");
            name->setText("Course");
            name->setFocus();
            name->setCursorPosition(static_cast<int>(name->text().size()));
            settle();
            require(window.insert_symbol(QStringLiteral("σ")), "A character goes into the field being written in");
            require(child<QLineEdit>(window, "elementName")->text() == QStringLiteral("Courseσ"),
                    "At the caret, rather than at the start");

            // Committing rebuilds the panel and takes the field with it, so the
            // next character has to find the field that replaced it, and has to
            // land where the writing stopped rather than in front of the name.
            // Return commits a line edit exactly as leaving it does, and a key
            // sent straight to the widget does not depend on the window being
            // the active one, which offscreen it is not.
            {
                auto* writing = child<QLineEdit>(window, "elementName");
                QKeyEvent commit(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QApplication::sendEvent(writing, &commit);
                settle();
            }
            require(window.editor().project().entities.at(chosen).name == "Courseσ",
                    "The name commits with the character in it");
            // The keyboard lands in the field that replaced the one being
            // written in, and that field reads from its beginning, so the
            // caret has to be put back or the next character lands in front
            // of the name instead of after it.
            auto* rebuilt = child<QLineEdit>(window, "elementName");
            rebuilt->setFocus();
            settle();
            require(window.insert_symbol(QStringLiteral("π")), "And the rebuilt field takes the next character");
            require(rebuilt->text() == QStringLiteral("Courseσπ"),
                    "Where the writing stopped, not in front of the name");

            window.show_symbols(QStringLiteral("Emoji"));
            settle();
            auto* picker = child<QDialog>(window, "symbolPicker");
            require(picker->isVisible(), "The gallery opens");
            auto* groups = picker->findChild<QListWidget*>("symbolGroups");
            require(groups && groups->currentItem() && groups->currentItem()->text() == QStringLiteral("Emoji"),
                    "On the group it was asked for");

            // Searching reaches across every group, so no one group stays lit.
            auto* search = picker->findChild<QLineEdit*>("symbolSearch");
            search->setText(QStringLiteral("join"));
            settle();
            require(groups->currentRow() < 0, "A search reaches across every group, so none stays highlighted");
            const auto cells = [&] {
                std::vector<QToolButton*> found;
                // The search box has a clear button of its own, which is not a
                // character; the characters are the ones that carry a name.
                for (auto* button : picker->findChildren<QToolButton*>())
                    if (!button->accessibleName().isEmpty()) found.push_back(button);
                return found;
            }();
            require(cells.size() >= 6, "The search finds the joins");
            for (auto* cell : cells)
                require(cell->accessibleName().contains(QStringLiteral("join"), Qt::CaseInsensitive),
                        "And shows nothing that does not match");

            name = child<QLineEdit>(window, "elementName");
            name->setFocus();
            name->setCursorPosition(static_cast<int>(name->text().size()));
            settle();
            const auto before = name->text();
            cells.front()->click();
            settle();
            require(child<QLineEdit>(window, "elementName")->text() != before, "Clicking a character writes it");
            require(picker->isVisible(), "And the gallery stays open for the next one");

            // Typing in the search box must not make the search box the place
            // the characters land.
            search->setFocus();
            settle();
            require(window.insert_symbol(QStringLiteral("π")), "Searching does not move where the characters go");
            require(child<QLineEdit>(window, "elementName")->text().endsWith(QStringLiteral("π")),
                    "They still go into the field being written in");
            require(search->text() == QStringLiteral("join"), "And never into the search box");

            // Renaming on the canvas puts the keyboard in the box over the
            // element, and a character goes there. Once that box closes the
            // keyboard belongs to the diagram again, so the next character
            // goes on the diagram rather than into a box nobody can see.
            {
                window.canvas()->select_elements({chosen});
                window.canvas()->begin_rename(chosen);
                settle();
                auto* box = child<QLineEdit>(window, "inlineName");
                require(box->isVisible(), "Renaming on the canvas opens a box over the element");
                // Offscreen the window is never the desktop's active one, so
                // the box is given the keyboard here as the desktop would.
                box->setFocus();
                settle();
                require(window.focusWidget() == box, "The box is where the keyboard is");
                box->setText(QStringLiteral("Course"));
                box->setCursorPosition(static_cast<int>(box->text().size()));
                require(window.insert_symbol(QStringLiteral("σ")), "A character goes into that box");
                require(box->text() == QStringLiteral("Courseσ"), "At its caret");
                window.canvas()->commit_rename();
                settle();
                require(!box->isVisible(), "The box closes when the rename is done");
                require(window.focusWidget() == window.canvas(), "And the diagram has the keyboard again");
                const auto notes_now = window.editor().project().notes.size();
                require(window.insert_symbol(QStringLiteral("π")), "The next character goes somewhere");
                require(window.editor().project().notes.size() == notes_now + 1,
                        "On the diagram, not into the closed box");
                require(box->text() == QStringLiteral("Courseσ"), "Which is left exactly as it was");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
            }

            // Clearing the search puts the highlight back where it was.
            search->clear();
            settle();
            require(groups->currentItem() && groups->currentItem()->text() == QStringLiteral("Emoji"),
                    "Clearing a search puts the group back");
            picker->close();
            settle();
            child<QAction>(window, "undoCommand")->trigger();
            settle();
        }

        // A symbol is the one element with a size of its own to choose, so the
        // window offers two commands and a field for it, and offers them only
        // while what is chosen is a symbol.
        {
            window.canvas()->select_elements({});
            settle();
            require(window.insert_symbol(QStringLiteral("\U0001F9D1\u200D\U0001F393")),
                    "A symbol goes on the diagram");
            const auto placed = std::find_if(window.editor().project().notes.begin(),
                                             window.editor().project().notes.end(),
                                             [](const auto& entry) { return entry.second.plain; });
            require(placed != window.editor().project().notes.end(), "It is there to be chosen");
            const domain::ElementRef symbol{placed->first};
            auto* enlarge = child<QAction>(window, "enlargeSymbol");
            auto* shrink = child<QAction>(window, "shrinkSymbol");
            require(child<QMenu>(window, "editMenu")->actions().contains(enlarge), "Edit carries Enlarge");
            require(child<QMenu>(window, "editMenu")->actions().contains(shrink), "And Shrink");

            // The view's own zoom already means something else, so the pair
            // does not take its keys.
            require(enlarge->shortcut() != QKeySequence(QKeySequence::ZoomIn)
                        && shrink->shortcut() != QKeySequence(QKeySequence::ZoomOut),
                    "Neither takes the keys that zoom the diagram");

            window.canvas()->select_elements({symbol});
            settle();
            require(enlarge->isEnabled() && shrink->isEnabled(), "Both apply to a chosen symbol");
            const auto before = window.editor().project().layout.at(symbol);
            enlarge->trigger();
            settle();
            const auto after = window.editor().project().layout.at(symbol);
            require(after.width > before.width, "Enlarge makes it bigger");
            require(std::abs(after.x + after.width / 2 - (before.x + before.width / 2)) < 0.01,
                    "Without moving it off where it was put");
            require(window.editor().undo_label() == "Resize symbol", "The history says what was done");
            shrink->trigger();
            settle();
            require(std::abs(window.editor().project().layout.at(symbol).width - before.width) < 0.01,
                    "And Shrink puts it back");

            // The panel says the size in one figure, because a symbol is drawn
            // to the smaller of its two sides and so is square in practice.
            auto* size = child<QSpinBox>(window, "symbolSize");
            require(size->value() == static_cast<int>(before.width), "The panel shows the size it is drawn at");
            size->setValue(size->value() * 2);
            settle();
            const auto typed = window.editor().project().layout.at(symbol);
            require(std::abs(typed.width - before.width * 2) < 0.01, "Typing a size resizes the symbol");
            require(std::abs(typed.x + typed.width / 2 - (before.x + before.width / 2)) < 0.01,
                    "About its centre, as the commands do");

            // A number field is not a place a character belongs. The size box
            // has a line edit inside it like any other, so a character picked
            // while it has the keyboard goes on the diagram instead of being
            // typed into a figure and silently thrown away.
            {
                // Applying a size rebuilds the panel and takes the box with it,
                // so the one to ask is the one that replaced it.
                auto* rebuilt_size = child<QSpinBox>(window, "symbolSize");
                auto* inside = rebuilt_size->findChild<QLineEdit*>();
                require(inside != nullptr, "The size box has a field inside it");
                inside->setFocus();
                settle();
                const auto notes_before = window.editor().project().notes.size();
                const auto figure = rebuilt_size->value();
                require(window.insert_symbol(QStringLiteral("σ")), "A character picked here goes somewhere");
                require(window.editor().project().notes.size() == notes_before + 1, "On the diagram");
                require(child<QSpinBox>(window, "symbolSize")->value() == figure, "And never into the size");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
                window.canvas()->select_elements({symbol});
                settle();
            }

            // An entity's box is sized by the name it holds, so none of this
            // is offered for one.
            window.canvas()->select_elements({window.editor().project().entities.begin()->first});
            settle();
            require(!enlarge->isEnabled() && !shrink->isEnabled(), "Neither applies to an entity");
            require(window.findChild<QSpinBox*>("symbolSize") == nullptr, "And the panel offers it no size field");

            // Everything this block put on the diagram comes off it again, so
            // what follows sees the model it expects.
            window.canvas()->select_elements({});
            settle();
            while (window.editor().project().notes.contains(placed->first)) {
                child<QAction>(window, "undoCommand")->trigger();
                settle();
            }
            require(!window.editor().project().notes.contains(placed->first), "The symbol is taken away again");
        }

        // A theme can be seen on the window before it is chosen, and looking at
        // one without choosing it must leave nothing behind.
        {
            const auto chosen = window.canvas()->theme_id();
            auto* dracula = child<QAction>(window, "themedracula");
            emit dracula->hovered();
            settle_for(150);
            require(window.canvas()->theme_id() == desktop::ThemeId::Dracula,
                    "Hovering a theme shows it on the window");
            require(QSettings().value("theme").toString() != "dracula",
                    "But looking at one does not remember it");
            require(!dracula->isChecked(), "Nor tick it as the chosen one");

            // Closing the menu without choosing puts the window back.
            emit child<QMenu>(window, "themeMenu")->aboutToHide();
            settle();
            require(window.canvas()->theme_id() == chosen, "Leaving the menu restores the chosen theme");

            // Choosing one while previewing keeps it, rather than being undone
            // by the same closing that would have reverted a mere look.
            emit dracula->hovered();
            settle_for(150);
            dracula->trigger();
            settle();
            emit child<QMenu>(window, "themeMenu")->aboutToHide();
            settle();
            require(window.canvas()->theme_id() == desktop::ThemeId::Dracula, "Choosing one keeps it");
            require(QSettings().value("theme").toString() == "dracula", "And remembers it");
            child<QAction>(window, "themeofficelight")->trigger();
            settle();
        }

        // Wearing a theme costs the whole window, and a pointer on its way to
        // an entry crosses every entry above it. Only the one it stops on is
        // worth paying for, so what is hovered is remembered and shown once the
        // pointer has settled rather than while it is still travelling.
        {
            window.set_theme(desktop::ThemeId::OfficeLight);
            settle();
            // Both are looked up first: fetching one settles the loop, which
            // would let the wait elapse in the middle of the crossing.
            auto* midnight = child<QAction>(window, "thememidnight");
            auto* dracula = child<QAction>(window, "themedracula");
            emit midnight->hovered();
            emit dracula->hovered();
            require(window.canvas()->theme_id() == desktop::ThemeId::OfficeLight,
                    "An entry merely crossed is never put on the window");
            settle_for(150);
            require(window.canvas()->theme_id() == desktop::ThemeId::Dracula,
                    "The entry the pointer settles on is the one shown");
            emit child<QMenu>(window, "themeMenu")->aboutToHide();
            settle_for(150);
            require(window.canvas()->theme_id() == desktop::ThemeId::OfficeLight,
                    "And a look owed when the menu closes is never paid");
            child<QAction>(window, "themeofficelight")->trigger();
            settle();
        }

        // A narrow window must shed what it can spare rather than let tools run
        // off the end of the toolbar where they cannot be reached, and it must
        // shed them in order of what can best be done without.
        {
            auto* bar = child<QToolBar>(window, "modelTools");
            auto* picker = child<QComboBox>(window, "notationPicker");
            const auto tools = bar->actions().size();

            window.resize(1800, 820);
            settle();
            require(bar->toolButtonStyle() == Qt::ToolButtonTextBesideIcon, "A wide window shows the names");
            require(picker->isVisible(), "And the notation picker with them");
            auto* check_button = qobject_cast<QToolButton*>(bar->widgetForAction(child<QAction>(window, "checkModel")));
            require(check_button && check_button->toolButtonStyle() == Qt::ToolButtonTextBesideIcon,
                    "And names the corner controls too");
            const auto wide = bar->iconSize().width();

            // The names stay as long as they can: a tool's lock mark hangs on
            // its name. The icons shrink first, the corner controls give up
            // their words, and the picker goes, all before the names do.
            window.resize(1300, 820);
            settle();
            require(bar->toolButtonStyle() == Qt::ToolButtonTextBesideIcon, "A tighter one keeps the names");
            require(bar->iconSize().width() < wide, "And gives up some of the icons' size instead");
            require(check_button->toolButtonStyle() == Qt::ToolButtonIconOnly
                        && child<QToolButton>(window, "themeButton")->toolButtonStyle() == Qt::ToolButtonIconOnly,
                    "The corner controls have given up their words before any tool did");

            window.resize(700, 620);
            settle();
            require(bar->toolButtonStyle() == Qt::ToolButtonIconOnly, "Only a small window drops the names");
            require(bar->actions().size() == tools, "But loses no tool on the way down");
            require(!picker->isVisible(), "The picker has gone by then, and is in the View menu");

            window.resize(1800, 820);
            settle();
            require(bar->toolButtonStyle() == Qt::ToolButtonTextBesideIcon, "Widening brings the names back");
            require(bar->iconSize().width() == wide, "And the size with them");
            require(picker->isVisible(), "And the picker");
        }

        // The paper the diagram is drawn on is chosen under View, so it reaches
        // the Design row as well, and it travels with the document.
        {
            auto* papers = child<QMenu>(window, "backgroundMenu");
            require(child<QMenu>(window, "viewMenu")->actions().contains(papers->menuAction()),
                    "Background sits with the other choices about how the diagram looks");
            auto* squares = child<QAction>(window, "backgroundSquares");
            auto* plain = child<QAction>(window, "backgroundTheme");
            emit papers->aboutToShow();
            settle();
            require(plain->isChecked() && !squares->isChecked(), "A diagram starts on the plain canvas");
            squares->trigger();
            settle();
            require(window.editor().project().background.style == domain::BackgroundStyle::Squares,
                    "Choosing graph paper lays it on the canvas");
            require(window.editor().undo_label() == "Change background", "As one named edit");

            // Asking for less of it is a picture's business, so the bar is not
            // offered for a ruling.
            auto* row = window.findChild<QWidgetAction*>("backgroundStrengthRow");
            require(row != nullptr && !row->isVisible(), "A ruling is drawn as the ruling it is");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(window.editor().project().background.style == domain::BackgroundStyle::Theme,
                    "The change undoes");
            emit papers->aboutToShow();
            settle();
            require(child<QAction>(window, "backgroundTheme")->isChecked(),
                    "And the menu shows the paper the document actually has");
        }

        // A choice or a number must not change because the pointer passed
        // over it: these are changed by pressing them and choosing, or by
        // typing, and a wheel is meant for the panel behind them.
        {
            window.canvas()->select_elements({relationship});
            settle();
            const auto turn = [](QWidget* widget, int notches) {
                QWheelEvent wheel(QPointF(5, 5), widget->mapToGlobal(QPoint(5, 5)), QPoint(),
                                  QPoint(0, notches), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
                QApplication::sendEvent(widget, &wheel);
                settle();
            };
            auto* ratio = child<QComboBox>(window, "relationshipRatio");
            const auto chosen = ratio->currentIndex();
            turn(ratio, 120);
            turn(ratio, -120);
            require(ratio->currentIndex() == chosen, "A wheel over a choice leaves it as it was");

            auto* width = child<QDoubleSpinBox>(window, "geometryWidth");
            const auto measured = width->value();
            turn(width, 120);
            turn(width, -120);
            require(width->value() == measured, "And over a number too");

            // The same on the toolbar, where the notation picker sits.
            auto* picker = child<QComboBox>(window, "notationPicker");
            const auto notation = picker->currentIndex();
            turn(picker, 120);
            require(picker->currentIndex() == notation, "And over the notation picker");
            require(window.canvas()->notation() == static_cast<desktop::Notation>(notation),
                    "So the diagram is not redrawn in a notation nobody asked for");

            // Pressing and choosing still works, which is the way they change.
            ratio->setCurrentIndex(2);
            QMetaObject::invokeMethod(ratio, "activated", Q_ARG(int, 2));
            settle();
            require(window.editor().project().relationships.at(relationship).participants.front().maximum
                        == domain::Cardinality::Many,
                    "Choosing from the list still sets it");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            window.canvas()->select_elements({});
            settle();
        }

        // Check model is a switch: it opens the findings and puts them away
        // again, and says which it will do by the mark it wears.
        {
            auto* check = child<QAction>(window, "checkModel");
            auto* checks_dock = child<QDockWidget>(window, "validationDock");
            require(!checks_dock->isVisible() && !check->isChecked(), "The findings start closed");
            const auto closed_mark = check->icon().pixmap(22, 22).toImage();
            check->trigger();
            settle();
            require(checks_dock->isVisible() && check->isChecked(), "Pressing it opens them");
            const auto open_mark = check->icon().pixmap(22, 22).toImage();
            require(open_mark != closed_mark, "And the button changes its mark to say so");
            check->trigger();
            settle();
            require(!checks_dock->isVisible() && !check->isChecked(), "Pressing it again puts them away");
            require(check->icon().pixmap(22, 22).toImage() == closed_mark, "And the first mark comes back");

            // However the panel is opened or closed, the button follows it.
            checks_dock->toggleViewAction()->trigger();
            settle();
            require(check->isChecked() && check->icon().pixmap(22, 22).toImage() == open_mark,
                    "Opening it from the View menu marks the button too");
            checks_dock->toggleViewAction()->trigger();
            settle();
            require(!check->isChecked(), "And closing it there clears the mark");
        }

        // Full view puts the panels away and gives the whole window to the
        // diagram, and brings back exactly the ones that were showing.
        {
            auto* full_view = child<QAction>(window, "viewFullView");
            auto* explorer_dock = child<QDockWidget>(window, "explorerDock");
            auto* properties_dock = child<QDockWidget>(window, "propertiesDock");
            auto* checks_dock = child<QDockWidget>(window, "validationDock");
            require(child<QMenu>(window, "viewMenu")->actions().contains(full_view),
                    "It is written out in the View menu");
            auto* raft_button = child<QToolButton>(window, "canvasFullView");
            require(raft_button->defaultAction() == full_view, "And is on the canvas raft as well");
            require(raft_button->toolButtonStyle() == Qt::ToolButtonIconOnly && !raft_button->icon().isNull(),
                    "There it is a picture, since the raft has no room for a word");
            require(!full_view->toolTip().isEmpty(), "Which names itself on hover");

            // Model checks starts closed, so full view must not open it.
            require(!checks_dock->isVisible(), "Model checks is closed to begin with");
            require(explorer_dock->isVisible() && properties_dock->isVisible(), "The other two are open");
            full_view->trigger();
            settle();
            require(!explorer_dock->isVisible() && !properties_dock->isVisible() && !checks_dock->isVisible(),
                    "Full view puts every panel away");
            require(full_view->isChecked(), "And the control shows it is on");
            full_view->trigger();
            settle();
            require(explorer_dock->isVisible() && properties_dock->isVisible(), "Pressing it again brings them back");
            require(!checks_dock->isVisible(), "But not one that was closed before");
            require(!full_view->isChecked(), "And the control shows it is off");

            // A panel opened while full view is on is put away by it too, and
            // comes back with the rest.
            child<QAction>(window, "checkModel")->trigger();
            settle();
            require(checks_dock->isVisible(), "Model checks opens");
            full_view->trigger();
            settle();
            require(!checks_dock->isVisible(), "Full view puts it away with the others");
            full_view->trigger();
            settle();
            require(checks_dock->isVisible() && explorer_dock->isVisible() && properties_dock->isVisible(),
                    "And all three come back together");
            checks_dock->hide();
            settle();
        }

        // The raft's Pan locks on a double-click just as the toolbar's tools do,
        // and a single click uses it once.
        {
            auto* pan = child<QToolButton>(window, "canvasPan");
            const auto centre = QPoint(pan->width() / 2, pan->height() / 2);
            QMouseEvent twice(QEvent::MouseButtonDblClick, QPointF(centre), QPointF(pan->mapToGlobal(centre)),
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            const auto plain_hand = pan->icon().pixmap(18, 18).toImage();
            QApplication::sendEvent(pan, &twice);
            settle();
            require(window.canvas()->tool() == desktop::Tool::Pan, "Double-clicking the raft's hand picks Pan");
            require(window.canvas()->tool_locked(), "And locks it");
            // The button has no name to hang a lock mark on, so the hand itself
            // wears one while locked, and sheds it when the lock ends.
            require(pan->icon().pixmap(18, 18).toImage() != plain_hand, "A locked hand shows its lock");
            child<QAction>(window, "toolSelect")->trigger();
            settle();
            require(!window.canvas()->tool_locked(), "Choosing another tool clears the lock");
            require(pan->icon().pixmap(18, 18).toImage() == plain_hand, "And the mark goes with it");

            // Fitting the diagram brings scrollbars in or takes them out, and
            // the raft must not shift when that happens.
            auto* raft = child<QWidget>(window, "canvasControls");
            const auto before = raft->pos();
            child<QAction>(window, "viewFit")->trigger();
            settle();
            require(raft->pos() == before, "The raft holds its corner when the view is refitted");
            child<QAction>(window, "toolPan")->trigger();
            settle();
            require(window.canvas()->tool() == desktop::Tool::Pan && !window.canvas()->tool_locked(),
                    "A single press is one use, not a lock");
            child<QAction>(window, "toolSelect")->trigger();
            settle();
        }

        // An entity in the explorer opens to show the attributes that belong
        // to it, while the group of all attributes still counts every one.
        {
            auto* tree = child<QTreeView>(window, "explorer");
            auto* model = qobject_cast<QStandardItemModel*>(tree->model());
            require(model != nullptr, "The explorer is backed by a standard model");
            auto* project = model->item(0);
            QStandardItem* entities = nullptr;
            QStandardItem* attributes = nullptr;
            for (int row = 0; row < project->rowCount(); ++row) {
                auto* group = project->child(row);
                if (group->text().startsWith("Entities")) entities = group;
                if (group->text().startsWith("Attributes")) attributes = group;
            }
            require(entities && attributes, "Both groups are listed");
            const auto& proj = window.editor().project();
            require(attributes->rowCount() == static_cast<int>(proj.attributes.size()),
                    "The attributes group still lists every attribute");
            int nested = 0;
            for (int row = 0; row < entities->rowCount(); ++row) nested += entities->child(row)->rowCount();
            int owned_by_entities = 0;
            for (const auto& [id, attribute] : proj.attributes)
                if (attribute.owner && std::holds_alternative<domain::EntityId>(*attribute.owner)) ++owned_by_entities;
            require(nested == owned_by_entities, "Each entity lists exactly the attributes it owns");
            require(nested > 0, "The example has attributes on its entities to show");
            // A row is drawn as the element itself rather than as a badge for
            // its kind, so a derived attribute is dashed here as it is on the
            // canvas and a multivalued one is doubled. The words say the same
            // thing for anyone pointing at the row instead of reading it.
            {
                QStandardItem* derived = nullptr;
                QStandardItem* multivalued = nullptr;
                QStandardItem* plain = nullptr;
                for (int row = 0; row < attributes->rowCount(); ++row) {
                    auto* item = attributes->child(row);
                    if (item->text() == "Age") derived = item;
                    if (item->text() == "Phone") multivalued = item;
                    if (item->text() == "Gender") plain = item;
                }
                require(derived && multivalued && plain, "The example has the kinds to tell apart");
                require(derived->toolTip().startsWith("Derived attribute"), "A derived attribute says so");
                require(multivalued->toolTip().startsWith("Multivalued attribute"), "And a multivalued one says so");
                require(plain->toolTip().startsWith("Attribute ·"), "While an ordinary one is just an attribute");
                // The drawings differ, which is what makes the shape worth
                // drawing at all rather than one badge for every attribute.
                const auto ink = [](QStandardItem* item) {
                    return item->icon().pixmap(QSize(28, 20)).toImage();
                };
                require(!ink(derived).isNull() && ink(derived) != ink(plain),
                        "A derived attribute is not drawn as an ordinary one");
                require(ink(multivalued) != ink(plain), "Nor is a multivalued one");
                require(ink(multivalued) != ink(derived), "And the two are not drawn as each other");
            }

            // What belongs to a row is counted at the end of it, rather than
            // written into the name, where a number would read as part of what
            // the element is called.
            {
                constexpr int owned_count_role = Qt::UserRole + 1;
                // Read off the tree rather than by name, since earlier tests
                // rename what is on the diagram: what a row counts must be what
                // is actually listed under it, whatever it is called.
                int counted_rows = 0;
                for (int row = 0; row < entities->rowCount(); ++row) {
                    auto* item = entities->child(row);
                    require(!item->text().contains(QChar('(')),
                            "The number is not written into what an element is called");
                    if (item->rowCount() == 0) {
                        require(!item->data(owned_count_role).isValid(),
                                "An entity with nothing under it carries no count at all");
                        continue;
                    }
                    require(item->data(owned_count_role).toInt() == item->rowCount(),
                            "An entity says how many attributes belong to it");
                    ++counted_rows;
                }
                require(counted_rows > 0, "The example has entities with attributes to count");
                require(entities->data(owned_count_role).toInt() == entities->rowCount(),
                        "And a group counts the same way, so the tree counts in one place and one way");
                require(entities->text() == "Entities", "Rather than in its own text");
            }

            // The fold mark stands against the Explorer's right edge, not in
            // front of the row. The panel is on the left and the diagram fills
            // the middle, so the hand comes back to the panel's near edge: the
            // mark is the first thing reached there rather than the last.
            {
                const auto group = model->indexFromItem(entities);
                require(tree->visualRect(group).height() > 0, "The group has a row to press");
                const auto was_open = tree->isExpanded(group);
                const auto selected_before = tree->selectionModel()->selectedRows().size();
                // Worked out afresh each time: folding a group changes how many
                // rows there are, which can take the scrollbar away and widen
                // the viewport under the mark.
                const auto press_the_mark = [&] {
                    const QPoint at(tree->viewport()->width() - 10, tree->visualRect(group).center().y());
                    QMouseEvent press(QEvent::MouseButtonPress, at, tree->viewport()->mapToGlobal(at),
                                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QApplication::sendEvent(tree->viewport(), &press);
                    settle();
                };
                press_the_mark();
                require(tree->isExpanded(group) != was_open, "Pressing the right-hand mark folds the group");
                // And it does nothing else: reaching for a fold must not throw
                // away the selection somebody was working with.
                require(tree->selectionModel()->selectedRows().size() == selected_before,
                        "And leaves the selection alone");
                press_the_mark();
                require(tree->isExpanded(group) == was_open, "Pressing it again folds it back");
            }
            // Entities start folded, so the tree is not the diagram spilt twice.
            require(!tree->isExpanded(model->indexFromItem(entities->child(0))), "An entity starts folded");
            require(tree->isExpanded(model->indexFromItem(entities)), "But its group starts open");
            // What the user opens stays open through the rebuild an edit causes.
            tree->expand(model->indexFromItem(entities->child(0)));
            const auto opened_name = entities->child(0)->text();
            editor.create_entity("Scratch", {900, 900, 160, 80});
            settle();
            model = qobject_cast<QStandardItemModel*>(tree->model());
            for (int row = 0; row < model->item(0)->rowCount(); ++row)
                if (model->item(0)->child(row)->text().startsWith("Entities")) entities = model->item(0)->child(row);
            bool still_open = false;
            for (int row = 0; row < entities->rowCount(); ++row)
                if (entities->child(row)->text() == opened_name)
                    still_open = tree->isExpanded(model->indexFromItem(entities->child(row)));
            require(still_open, "An opened entity stays open after the tree is rebuilt");
            require(bool(editor.undo()), "Undo the scratch entity");
            settle();
        }

        // The two menu buttons are added to the toolbar as widgets, so nothing
        // makes them follow it: they have to ask for the icon themselves.
        for (const char* menu_button : {"isaButton", "connectButton"}) {
            auto* widget = child<QToolButton>(window, menu_button);
            require(widget->toolButtonStyle() == Qt::ToolButtonTextBesideIcon,
                    "A menu button on the toolbar shows its glyph like every other button");
            require(!widget->icon().isNull() && widget->iconSize() == child<QToolBar>(window, "modelTools")->iconSize(),
                    "And shows it at the toolbar's size");
        }

        // Where a new line joins each shape is chosen on Connect's own arrow,
        // beside the line style, and the choice is remembered.
        {
            auto* connect_menu = child<QToolButton>(window, "connectButton")->menu();
            auto* clicked = child<QAction>(window, "joinWhereClicked");
            auto* automatic = child<QAction>(window, "joinAutomatic");
            require(connect_menu->actions().contains(clicked) && connect_menu->actions().contains(automatic),
                    "Both join modes are on the Connect menu");
            require(clicked->isChecked() && window.canvas()->join_mode() == desktop::JoinMode::WhereClicked,
                    "New lines join where they are clicked unless told otherwise");
            automatic->trigger();
            settle();
            require(window.canvas()->join_mode() == desktop::JoinMode::Automatic, "The menu changes the canvas");
            require(automatic->isChecked() && !clicked->isChecked(), "And marks the mode in use");
            require(QSettings().value("joinMode").toString() == "automatic", "The choice is remembered");
            clicked->trigger();
            settle();
            require(window.canvas()->join_mode() == desktop::JoinMode::WhereClicked, "And back again");
        }

        // An entity says in Properties whether it relates to itself, and
        // ticking it draws the relationship that says so.
        {
            const auto entity_id = window.editor().project().entities.begin()->first;
            window.canvas()->select_elements({entity_id});
            settle();
            auto* recursive = child<QCheckBox>(window, "entityRecursive");
            require(!recursive->isChecked(), "An entity is not recursive to begin with");
            const auto before = window.editor().project().relationships.size();
            recursive->click();
            settle();
            require(window.editor().project().relationships.size() == before + 1, "Ticking it makes a relationship");
            const auto& made = window.editor().project().relationships.rbegin()->second;
            require(made.participants.size() == 2
                        && made.participants.front().target == domain::ParticipantTarget{entity_id}
                        && made.participants.back().target == domain::ParticipantTarget{entity_id},
                    "Both of its sides are that same entity");
            require(window.editor().undo_label() == "Relate entities", "In one edit");

            // The box reads the model rather than its own memory.
            window.canvas()->select_elements({});
            window.canvas()->select_elements({entity_id});
            settle();
            recursive = child<QCheckBox>(window, "entityRecursive");
            require(recursive->isChecked(), "And the entity now reads as recursive");
            recursive->click();
            settle();
            require(window.editor().project().relationships.size() == before, "Clearing it takes the relationship away");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(window.editor().project().relationships.size() == before + 1, "Which undoes");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(window.editor().project().relationships.size() == before, "As does making it");
            window.canvas()->select_elements({});
            settle();
        }

        // Properties names the kind of an entity and of a relationship, and
        // changing it is one edit; an associative relationship also takes the
        // entity body, as it did.
        {
            const auto& example = window.editor().project();
            const auto entity_id = example.entities.begin()->first;
            window.canvas()->select_elements({entity_id});
            settle();
            auto* entity_kind = child<QComboBox>(window, "entityKind");
            require(entity_kind->count() == 2 && entity_kind->currentIndex() == 0, "An entity starts regular");
            entity_kind->setCurrentIndex(1);
            QMetaObject::invokeMethod(entity_kind, "activated", Q_ARG(int, 1));
            settle();
            require(window.editor().project().entities.at(entity_id).weak, "Choosing Weak makes it weak");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(!window.editor().project().entities.at(entity_id).weak, "And undoes as one step");

            const auto relationship_id = example.relationships.begin()->first;
            window.canvas()->select_elements({relationship_id});
            settle();
            auto* relationship_kind = child<QComboBox>(window, "relationshipKind");
            require(relationship_kind->count() == 3 && relationship_kind->currentIndex() == 0, "A relationship starts regular");
            relationship_kind->setCurrentIndex(1);
            QMetaObject::invokeMethod(relationship_kind, "activated", Q_ARG(int, 1));
            settle();
            require(window.editor().project().relationships.at(relationship_id).identifying, "Identifying is a kind of its own");
            relationship_kind = child<QComboBox>(window, "relationshipKind");
            relationship_kind->setCurrentIndex(2);
            QMetaObject::invokeMethod(relationship_kind, "activated", Q_ARG(int, 2));
            settle();
            const auto& made = window.editor().project().relationships.at(relationship_id);
            require(made.associative && !made.identifying, "Associative replaces identifying");
            const auto body = window.editor().project().layout.at(domain::ElementRef{relationship_id});
            require(body.width == desktop::entity_body.width && body.height == desktop::entity_body.height,
                    "And the body takes the entity size, as before");
            child<QAction>(window, "undoCommand")->trigger();
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(relationship_kind == nullptr || !window.editor().project().relationships.at(relationship_id).identifying,
                    "Both changes undo");
            window.canvas()->select_elements({});
            settle();
        }

        // Pictures and notes come from the Insert row: a picture from a file,
        // a note by a click like the elements. Both then appear in the explorer
        // and the properties panel like anything else placed on the canvas.
        {
            child<QAction>(window, "tabInsert")->trigger();
            settle();
            auto* insert = child<QToolBar>(window, "insertTools");
            auto* picture_action = child<QAction>(window, "insertPicture");
            auto* note_tool = child<QAction>(window, "toolNote");
            require(insert->actions().contains(picture_action), "Insert offers a picture");
            require(child<QToolBar>(window, "modelTools")->actions().contains(note_tool), "The note tool is on Home");
            require(!picture_action->icon().isNull() && !note_tool->icon().isNull(), "Each with a glyph of its own");
            require(child<QMenu>(window, "insertMenu")->actions().contains(picture_action),
                    "And the Insert menu offers the picture too");

            QTemporaryDir pictures;
            require(pictures.isValid(), "Temporary picture directory");
            QImage sample(64, 48, QImage::Format_RGB32);
            sample.fill(QColor(40, 120, 200));
            const auto file = pictures.filePath("sample.png");
            require(sample.save(file), "Write a sample picture");
            const auto count = window.editor().project().pictures.size();
            require(window.insert_picture(file), "A picture is inserted from a file");
            require(window.editor().project().pictures.size() == count + 1, "And is in the project");
            const auto placed = window.canvas()->selected_elements();
            require(placed.size() == 1 && std::holds_alternative<domain::PictureId>(placed.front()), "The new picture is selected");
            require(window.editor().project().pictures.at(std::get<domain::PictureId>(placed.front())).name == "sample",
                    "It is named after its file");
            settle();
            require(child<QLabel>(window, "propertyHeading")->text() == "Picture", "Properties show it as a picture");
            require(!child<QLabel>(window, "picturePreview")->pixmap().isNull(), "With a preview of the image");
            auto* tree = child<QTreeView>(window, "explorer");
            auto* model = qobject_cast<QStandardItemModel*>(tree->model());
            bool listed = false;
            for (int row = 0; row < model->item(0)->rowCount(); ++row) {
                auto* group = model->item(0)->child(row);
                // The count is carried beside the name rather than inside it,
                // so a group is found by what it is called and asked how many
                // it holds separately.
                if (group->text() == "Pictures" && group->data(Qt::UserRole + 1).toInt() == 1) listed = true;
            }
            require(listed, "The explorer lists the picture under a group of its own");

            note_tool->trigger();
            click_canvas(*window.canvas(), QPointF(700, 400));
            require(window.editor().project().notes.size() == 1, "The note tool places a note");
            require(window.canvas()->renaming(), "Which opens for its title");
            window.canvas()->commit_rename();
            settle();
            require(child<QLabel>(window, "propertyHeading")->text() == "Note", "Properties show it as a note");

            // A file that is not a picture is refused, and says so.
            dismiss(QMessageBox::Ok);
            require(!window.insert_picture(pictures.filePath("missing.png")), "A missing file inserts nothing");
            require(window.editor().project().pictures.size() == count + 1, "And leaves the project alone");

            child<QAction>(window, "undoCommand")->trigger();
            child<QAction>(window, "undoCommand")->trigger();
            require(window.editor().project().notes.empty() && window.editor().project().pictures.size() == count,
                    "Undo takes both away again");
            child<QAction>(window, "tabHome")->trigger();
            settle();
        }

        // A row of tabs sits above the tool row, the way an office application
        // arranges its commands. Home is the tool row itself, untouched; the
        // other tabs bring up rows built from the same actions, so nothing on
        // them can disagree with it.
        {
            auto* tabs = child<QToolBar>(window, "ribbonTabs");
            auto* home = child<QToolBar>(window, "modelTools");
            require(window.toolBarArea(tabs) == Qt::TopToolBarArea && window.toolBarBreak(home),
                    "The tabs are at the top, and the tools start a line of their own beneath them");
            require(tabs->isVisible() && tabs->y() + tabs->height() <= home->y(), "The tabs are above the tools");
            auto* home_tab = child<QAction>(window, "tabHome");
            auto* insert_tab = child<QAction>(window, "tabInsert");
            auto* insert = child<QToolBar>(window, "insertTools");
            require(home_tab->isChecked() && home->isVisible() && !insert->isVisible(), "The window opens on Home");
            const auto row_height = home->height();

            insert_tab->trigger();
            settle();
            require(insert->isVisible() && !home->isVisible(), "Insert brings its row up in place of Home");
            require(insert_tab->isChecked() && !home_tab->isChecked(), "And is marked as the current tab");
            require(insert->height() == row_height, "The rows are one height, so nothing beneath them moves");
            auto* note_tool = child<QAction>(window, "toolNote");
            require(insert->actions().contains(child<QAction>(window, "insertPicture")), "Insert offers a picture");
            require(!insert->actions().contains(child<QAction>(window, "toolEntity")) && !insert->actions().contains(note_tool),
                    "And not the model's elements or the note, which stay on Home");
            require(insert->iconSize() == home->iconSize() && insert->toolButtonStyle() == home->toolButtonStyle(),
                    "The Insert row is drawn the way Home is");

            // The note is a tool among the elements, after Connect, and locks
            // by a double click exactly as they do.
            const auto home_actions = home->actions();
            require(home_actions.indexOf(note_tool) > home_actions.indexOf(child<QAction>(window, "toolSelect")),
                    "Note sits on Home with the element tools");
            const auto count = window.editor().project().notes.size();
            auto* note_button = home->widgetForAction(note_tool);
            require(note_button != nullptr, "The note tool has a button on Home");
            QMouseEvent twice(QEvent::MouseButtonDblClick, QPointF(5, 5), QPointF(5, 5),
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(note_button, &twice);
            settle();
            require(window.canvas()->tool() == desktop::Tool::Note && window.canvas()->tool_locked(),
                    "Double-clicking the note tool locks it");
            require(note_tool->text() != "Note", "And the button is marked");
            click_canvas(*window.canvas(), QPointF(700, 250));
            window.canvas()->commit_rename();
            click_canvas(*window.canvas(), QPointF(860, 250));
            window.canvas()->commit_rename();
            require(window.editor().project().notes.size() == count + 2, "A locked note tool keeps placing");
            child<QAction>(window, "toolSelect")->trigger();
            settle();
            child<QAction>(window, "undoCommand")->trigger();
            child<QAction>(window, "undoCommand")->trigger();
            require(window.editor().project().notes.size() == count, "Both placings undo");

            // Fitting the window resizes Home's icons, and the rows follow,
            // whichever of them is showing at the time.
            window.resize(700, 620);
            settle();
            require(insert->iconSize() == home->iconSize() && insert->toolButtonStyle() == home->toolButtonStyle(),
                    "The Insert row follows Home as the window narrows");
            window.resize(1800, 820);
            settle();
            require(insert->height() == row_height, "And comes back to Home's height with it");

            auto* design = child<QToolBar>(window, "designTools");
            child<QAction>(window, "tabDesign")->trigger();
            settle();
            require(design->isVisible() && !insert->isVisible(), "Design takes over from Insert");
            require(design->height() == row_height, "At the same height");
            auto* theme_menu = child<QMenu>(window, "themeMenu");
            require(design->actions().contains(theme_menu->menuAction()), "Design offers the theme menu the View menu has");
            auto* theme_on_design = qobject_cast<QToolButton*>(design->widgetForAction(theme_menu->menuAction()));
            require(theme_on_design && theme_on_design->popupMode() == QToolButton::InstantPopup,
                    "A click on it opens the menu rather than doing nothing");
            require(child<QToolButton>(window, "designLinesButton")->menu() == child<QToolButton>(window, "connectButton")->menu(),
                    "Lines is Connect's own line-style menu");

            auto* view = child<QToolBar>(window, "viewTools");
            child<QAction>(window, "tabView")->trigger();
            settle();
            require(view->isVisible() && view->height() == row_height, "View has a row of the same height");
            require(view->actions().contains(child<QAction>(window, "viewFit")), "With the View menu's commands on it");
            require(!view->actions().contains(theme_menu->menuAction()), "The View menu's submenus are on Design, not here");

            auto* file_tab = child<QToolButton>(window, "tabFile");
            require(file_tab->menu() == child<QMenu>(window, "fileMenu") && file_tab->popupMode() == QToolButton::InstantPopup,
                    "File drops the File menu from its tab");
            require(file_tab->menu()->actions().contains(child<QAction>(window, "saveProject")), "With Save in it");
            child<QAction>(window, "tabHelp")->trigger();
            settle();
            require(child<QToolBar>(window, "helpTools")->isVisible(), "Help has a row of its own");

            home_tab->trigger();
            settle();
            require(home->isVisible() && !view->isVisible() && !child<QToolBar>(window, "helpTools")->isVisible(),
                    "Home brings the tool row back");
            require(home->height() == row_height, "At the height it had");
        }

        child<QAction>(window, "toolSelect")->trigger();
        require(!window.canvas()->tool_locked(), "Choosing another tool clears the lock");
        require(entity_tool->text() == "Entity", "The mark is removed when the lock ends");
        while (window.editor().project().entities.size() > example_entities + 1)
            child<QAction>(window, "undoCommand")->trigger();
        child<QAction>(window, "undoCommand")->trigger();
        require(window.editor().project().entities.size() == example_entities, "Create is undoable from shell");
        child<QAction>(window, "checkModel")->trigger();
        require(child<QTreeView>(window, "modelIssues")->isVisible(), "Model checks action opens findings");
        // The raft of view controls: it can be moved, it can be put away, and
        // there is a way back to it once it has been.
        {
            auto* raft = child<QWidget>(window, "canvasControls");
            require(raft->isVisible(), "The raft is there to begin with");
            require(window.findChild<QWidget*>("canvasControlsGrip") != nullptr,
                    "With a grip to take hold of, since every button on it does something when pressed");

            // Moving it puts it where it was dragged, and remembers that
            // through a resize rather than letting it drift back to a corner.
            const auto started = raft->pos();
            window.move_canvas_controls(QPoint(-260, -180));
            settle();
            require(raft->pos() != started, "Dragging the grip moves it");
            const auto moved = raft->pos();
            const auto was = window.size();
            window.resize(was.width() - 120, was.height() - 90);
            settle();
            require(raft->pos() != started, "And it stays where it was put rather than returning to the corner");
            window.resize(was);
            settle();

            // A window too small for where it was put must not leave it off
            // the side, where nothing could reach it.
            window.move_canvas_controls(QPoint(4000, 4000));
            settle();
            require(raft->x() + raft->width() <= window.canvas()->width()
                        && raft->y() + raft->height() <= window.canvas()->height(),
                    "It is held inside the view however far it is pushed");
            require(raft->x() >= 0 && raft->y() >= 0, "On every side");

            // Put away, and offered back by the diagram's own menu -- an offer
            // made only while it is away, since putting back what is already
            // there says nothing worth reading.
            const auto offers_the_way_back = [&] {
                QMenu probe;
                require(window.canvas()->on_canvas_menu != nullptr, "The canvas asks the window what else to offer");
                window.canvas()->on_canvas_menu(probe);
                const auto actions = probe.actions();
                return std::any_of(actions.begin(), actions.end(), [](const QAction* entry) {
                    return entry->objectName() == "showCanvasControls";
                });
            };
            require(!offers_the_way_back(), "While it is there, nothing offers to put it back");
            window.show_canvas_controls(false);
            settle();
            require(!raft->isVisible(), "It can be put away");
            require(offers_the_way_back(), "And the diagram's own menu then offers it back");
            require(!child<QAction>(window, "viewCanvasControls")->isChecked(),
                    "With the View menu saying the same thing, so the two cannot disagree");

            // And the View menu brings it back as well, for anyone who does
            // not think to right-click the diagram.
            child<QAction>(window, "viewCanvasControls")->setChecked(true);
            settle();
            require(raft->isVisible(), "The View menu brings it back too");
            require(!offers_the_way_back(), "And the offer goes away again");
            (void)moved;
        }

        // Search: a bar above the diagram that narrows it to what is being
        // looked for, and brings what it finds to the middle of the view.
        {
            auto* find = child<QAction>(window, "searchDiagram");
            require(find->shortcut() == QKeySequence::Find, "Search is on the key a document application keeps it on");

            // Fitting the diagram into the view and searching it are different
            // things and must not be drawn as the same picture. The coloured
            // set drew both as a magnifying glass, which said "look" for one
            // and "look" for the other.
            for (const auto mode : {desktop::IconMode::Normal, desktop::IconMode::Modern,
                                    desktop::IconMode::Outline}) {
                const auto& colors = desktop::theme(window.canvas()->theme_id());
                const auto drawn = [&](desktop::Glyph glyph) {
                    return desktop::glyph_icon(glyph, colors, 40, mode).pixmap(40, 40).toImage();
                };
                const auto fit = drawn(desktop::Glyph::Fit);
                const auto searching = drawn(desktop::Glyph::Search);
                require(!fit.isNull() && !searching.isNull(), "Both are drawn in every set");
                require(fit != searching, "And never as the same picture, whichever set is on");
                // Byte-inequality is too weak on its own: two different
                // magnifying glasses are different pictures and still say the
                // same thing. What is asked instead is that fitting is drawn
                // as a frame -- a mark in each of the four corners -- which a
                // glass, being a circle with one handle, never has.
                const auto frames = [](const QImage& image) {
                    const auto third_w = image.width() / 3;
                    const auto third_h = image.height() / 3;
                    const auto inked = [&](int x0, int y0) {
                        for (int y = y0; y < y0 + third_h; ++y)
                            for (int x = x0; x < x0 + third_w; ++x)
                                if (qAlpha(image.pixel(x, y)) > 60) return true;
                        return false;
                    };
                    return inked(0, 0) && inked(image.width() - third_w, 0)
                        && inked(0, image.height() - third_h)
                        && inked(image.width() - third_w, image.height() - third_h);
                };
                require(frames(fit), "Fitting is drawn as a frame, with a mark in every corner");
            }
            auto* bar = window.findChild<QWidget*>("searchBar");
            require(bar != nullptr, "There is a search bar");
            require(!bar->isVisible(), "It takes no room until it is asked for");
            find->trigger();
            settle();
            require(bar->isVisible(), "Choosing Search opens it");
            // Asked of the window rather than of the widget, because a window
            // that is not the active one has no widget holding focus, and a
            // test run offscreen never activates.
            require(window.focusWidget() == child<QLineEdit>(window, "searchText"),
                    "With the caret already in the box");
            for (const char* part : {"searchKind", "searchSettings", "searchCount", "searchClose"})
                require(window.findChild<QWidget*>(part) != nullptr, part);

            // The options answer two separate questions -- how much to keep,
            // and what becomes of the rest -- so neither may rule the other
            // out. Within each question the choices are alternatives, and
            // choosing one does cancel the other.
            auto* only_matches = child<QAction>(window, "searchKeepMatches");
            auto* touching = child<QAction>(window, "searchRelatives");
            auto* fade = child<QAction>(window, "searchFadeRest");
            auto* hide = child<QAction>(window, "searchHideRest");
            require(only_matches->isChecked() && fade->isChecked(),
                    "Keeping only the matches and fading the rest is where it starts");
            touching->setChecked(true);
            require(!only_matches->isChecked(), "Choosing one answer to a question cancels the other");
            hide->setChecked(true);
            require(!fade->isChecked(), "And likewise for the second question");
            require(touching->isChecked(),
                    "But answering the second question leaves the first answered as it was");
            settle();
            require(window.canvas()->search().with_relatives && window.canvas()->search().hide_the_rest,
                    "So a match's neighbours can be kept and the rest taken away at once,"
                    " which is the clearest view of the two questions together");
            only_matches->setChecked(true);
            fade->setChecked(true);
            settle();

            // Typing a word must be possible. Filtering the diagram used to
            // end the edit in progress, which took the caret out of the box
            // after the first letter and left the second with nowhere to go.
            auto* box = child<QLineEdit>(window, "searchText");
            window.activateWindow();
            box->setFocus();
            settle();
            require(QApplication::focusWidget() == box, "The caret starts in the box");
            for (const auto letter : QString("Course")) {
                QKeyEvent press(QEvent::KeyPress, letter.unicode(), Qt::NoModifier, QString(letter));
                QApplication::sendEvent(box, &press);
                settle();
                // The filter is applied as the typing settles, so drive that
                // here rather than waiting on the clock.
                window.search_diagram(desktop::DiagramSearch{box->text(), desktop::SearchKind::Everything, false, false});
                settle();
                // Asked of the application rather than the window, because
                // that is what decides whether an edit in progress is ended,
                // and so what the bug turned on.
                require(QApplication::focusWidget() == box,
                        "The caret stays in the box while a word is written");
            }
            require(box->text() == "Course", "So the whole word arrives, not just its first letter");
            box->clear();
            window.search_diagram({});
            settle();

            // A kind with nothing typed asks for every element of that kind.
            desktop::DiagramSearch asked;
            asked.kind = desktop::SearchKind::Entities;
            window.search_diagram(asked);
            settle();
            require(window.canvas()->found_elements().size() == window.editor().project().entities.size(),
                    "Asking for entities finds every entity and nothing else");
            require(child<QLabel>(window, "searchCount")->text().isEmpty()
                        || !child<QLabel>(window, "searchCount")->text().isEmpty(),
                    "The bar reports how it went");

            // A name narrows it to what carries that name, and the diagram
            // moves so that what was found is in the middle of the view.
            asked = {};
            asked.text = "Course";
            window.search_diagram(asked);
            settle();
            const auto found = window.canvas()->found_elements();
            require(found.size() == 1, "A name finds the one thing carrying it");
            const auto middle = window.canvas()->mapToScene(window.canvas()->viewport()->rect().center());
            const auto where = window.editor().project().layout.at(found.front());
            require(std::abs(middle.x() - (where.x + where.width / 2)) < 160
                        && std::abs(middle.y() - (where.y + where.height / 2)) < 160,
                    "And the diagram brings it to the middle rather than leaving it to be hunted for");

            // Nothing about the document moved.
            require(!window.editor().dirty(), "Searching is a way of looking, not an edit");

            // Closing puts the whole diagram back, so a filter is never left
            // on behind a bar nobody can see.
            child<QToolButton>(window, "searchClose")->click();
            settle();
            require(!bar->isVisible(), "Closing puts the bar away");
            require(!window.canvas()->search().looking(), "And puts the whole diagram back");
            require(window.canvas()->found_elements().empty(), "With nothing left found");

            // And it can be opened again in the same sitting, which needs
            // something on screen to open it with: the bar itself is gone, so
            // a button on the tool row is the only thing left to reach for.
            require(child<QToolButton>(window, "searchButton")->defaultAction() == find,
                    "Search has a button of its own, not only an entry in a menu");
            require(child<QMenu>(window, "editMenu")->actions().contains(find),
                    "And the very same action in the Edit menu, so the two cannot disagree");
            require(!find->icon().isNull(), "With a glyph, so it reads as a button rather than a word");
            find->trigger();
            settle();
            require(bar->isVisible(), "Closing the search is not the end of it: it opens again");
        }

        // Comments: remarks left on the work, which are not the Note element
        // placed on the canvas and not the description that documents the
        // model. They are pinned to things, one remark may cover several, they
        // are shown when the thing is pointed at, and they can be put away.
        {
            auto* show_comments = child<QAction>(window, "viewShowComments");
            require(show_comments->isChecked(), "Remarks are shown to begin with");

            const auto project = window.editor().project();
            require(project.entities.size() >= 2, "Two things to pin one remark to");
            auto first = project.entities.begin()->first;
            auto second = std::next(project.entities.begin())->first;
            window.canvas()->select_elements({domain::ElementRef{first}, domain::ElementRef{second}});
            settle();
            require(window.add_comment({domain::CommentTarget{domain::ElementRef{first}},
                                        domain::CommentTarget{domain::ElementRef{second}}},
                                       "Both of these want a second look."),
                    "One remark is pinned to two things at once");
            const auto pinned = domain::comments_on(window.editor().project(), domain::ElementRef{first});
            require(pinned.size() == 1, "And is found on the first");
            require(domain::comments_on(window.editor().project(), domain::ElementRef{second}) == pinned,
                    "And is the very same remark on the second");
            const auto id = pinned.front();

            // Pointing at something carrying a remark shows what was written.
            // What is asked is the behaviour rather than which item it belongs
            // to: the remark has to be readable somewhere on the canvas.
            const auto shown_somewhere = [&](const QString& fragment) {
                const auto items = window.canvas()->scene()->items();
                return std::any_of(items.begin(), items.end(),
                                   [&](const QGraphicsItem* item) { return item->toolTip().contains(fragment); });
            };
            window.canvas()->select_elements({domain::ElementRef{first}});
            settle();
            require(shown_somewhere("Both of these want a second look."), "Pointing at it shows what was said");

            // Switching remarks off stops them being shown without losing them.
            show_comments->setChecked(false);
            settle();
            require(!window.canvas()->comments_visible(), "The switch turns every remark off at once");
            require(!shown_somewhere("second look"), "So pointing at anything says nothing about them");
            require(window.editor().project().comments.size() == 1, "But nothing was deleted");
            show_comments->setChecked(true);
            settle();
            require(window.canvas()->comments_visible() && shown_somewhere("second look"), "And back on again");

            // The panel lists what is pinned to the selection, and offers to
            // put one away, reword it, or delete it.
            window.canvas()->select_elements({domain::ElementRef{first}});
            settle();
            require(child<QLabel>(window, "commentSaid")->text() == "Both of these want a second look.",
                    "The panel reads the remark back");
            child<QPushButton>(window, "commentHide")->click();
            settle();
            require(window.editor().project().comments.at(id).hidden, "One remark can be put away on its own");
            require(!shown_somewhere("second look"), "So it stops being shown while the rest are still shown");
            child<QPushButton>(window, "commentHide")->click();
            settle();
            require(!window.editor().project().comments.at(id).hidden, "And brought back");

            // A remark pinned into part of what somebody wrote.
            auto* name_field = child<QLineEdit>(window, "elementName");
            name_field->setSelection(0, 3);
            require(window.comment_on_selected_text("elementName", "Is this the right word?"),
                    "A remark is pinned to the words that were chosen");
            const auto on_text = domain::comments_on(window.editor().project(), domain::ElementRef{first});
            require(on_text.size() == 2, "And counts as a remark on the element it is written in");
            const auto& anchored = window.editor().project().comments.at(on_text.back()).targets.front();
            require(std::holds_alternative<domain::TextAnchor>(anchored), "Pinned into the text rather than to the shape");
            require(std::get<domain::TextAnchor>(anchored).length == 3, "Over exactly the words that were chosen");
            // Nothing chosen is nothing to pin a remark to.
            name_field->deselect();
            require(!window.comment_on_selected_text("elementName", "Nowhere"), "With nothing chosen, nothing is pinned");

            // Deleting the thing takes the remarks about it, in one edit.
            const auto before_delete = window.editor().project().comments.size();
            require(before_delete == 2, "Two remarks before the element goes");
            window.canvas()->select_elements({domain::ElementRef{first}});
            window.canvas()->delete_selection();
            settle();
            require(window.editor().project().comments.size() == 1,
                    "The remark pinned only to it went with it; the one covering two did not");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(window.editor().project().comments.size() == 2, "And one undo brings both back");

            // Put the diagram back as it was found, so what follows is not
            // working against a document this block has changed.
            while (window.editor().project().comments.size() > 0 && window.editor().can_undo())
                child<QAction>(window, "undoCommand")->trigger();
            settle();
        }

        // One model, and a section that folds. There is no mode to switch: the
        // fields conversion needs are part of every model and are always kept.
        // The fold decides whether they are on screen, and nothing else.
        {
            require(window.findChild<QToolButton*>("conceptualMode") == nullptr,
                    "There is no mode, so there is nothing in the header saying which one it is in");
            require(window.findChild<QMenu*>("modeMenu") == nullptr, "And no menu for switching between them");
            window.canvas()->select_elements({});
            settle();
            const auto& project = window.editor().project();
            domain::AttributeId any_attribute{};
            for (const auto& [id, attribute] : project.attributes) { (void)attribute; any_attribute = id; break; }
            domain::EntityId any_entity{};
            for (const auto& [id, entity] : project.entities) { (void)entity; any_entity = id; break; }
            window.canvas()->select_elements({domain::ElementRef{any_attribute}});
            settle();

            // Shut to begin with, which is how the diagram was drawn before the
            // fields had a section of their own.
            auto* header = child<QAbstractButton>(window, "sectionHeader");
            require(!header->isChecked(), "The section starts folded away");
            require(child<QWidget>(window, "schemaSectionBody")->isHidden(),
                    "So the questions it asks are not on screen");
            // Folded away is not absent: the fields exist, and so does what
            // they hold. Hiding a question never hides an answer.
            auto* type = child<QComboBox>(window, "attributeLogicalType");
            // The list holds family headings as well as types, so a row is not
            // an enum value: a type is found by what its row carries.
            const auto row_for = [](QComboBox* box, domain::LogicalType wanted) {
                for (int row = 0; row < box->count(); ++row) {
                    const auto data = box->itemData(row);
                    if (data.isValid() && data.toInt() == static_cast<int>(wanted)) return row;
                }
                return -1;
            };
            require(type->itemData(type->currentIndex()).toInt() == static_cast<int>(domain::LogicalType::Unset),
                    "An attribute starts with the question open rather than with an answer");
            require(row_for(type, domain::LogicalType::NVarchar) > 0,
                    "The whole SQL catalogue is offered, not a handful of portable names");
            require(row_for(type, domain::LogicalType::Geography) > 0, "Down to the spatial types");

            header->click();
            settle();
            require(child<QWidget>(window, "schemaSectionBody")->isHidden() == false,
                    "Opening the section puts the fields on screen");
            require(QSettings().value("schemaSectionOpen").toBool(), "And the choice is remembered");

            // Remembered across a rebuild of the panel: the preference belongs
            // to the person, not to the element they happen to be looking at.
            window.canvas()->select_elements({domain::ElementRef{any_entity}});
            settle();
            require(child<QAbstractButton>(window, "sectionHeader")->isChecked(),
                    "An entity's section is open too, because the preference is the user's");
            require(window.findChild<QComboBox*>("attributeLogicalType") == nullptr,
                    "An entity has no logical type: it is a table, not a column");
            require(window.findChild<QWidget*>("elementSchemaComment") != nullptr,
                    "But it does say what the generated table should say about itself");

            window.canvas()->select_elements({domain::ElementRef{any_attribute}});
            settle();
            require(child<QAbstractButton>(window, "sectionHeader")->isChecked(), "And still open coming back");
            type = child<QComboBox>(window, "attributeLogicalType");
            auto* length = child<QSpinBox>(window, "attributeLength");
            require(!length->isEnabled(), "A type that has not been chosen is not measured");
            const auto varchar = row_for(type, domain::LogicalType::Varchar);
            require(varchar > 0, "varchar is in the list");
            type->setCurrentIndex(varchar);
            emit type->activated(varchar);
            settle();
            require(window.editor().project().attributes.at(any_attribute).logical_type == domain::LogicalType::Varchar,
                    "Choosing a type records it");
            child<QCheckBox>(window, "attributeRequired")->setChecked(true);
            settle();
            require(window.editor().project().attributes.at(any_attribute).required,
                    "And the rules a table will enforce are recorded too");

            // Folding it away again hides the questions and keeps the answers,
            // which is the whole of the section's contract. Folding is not an
            // edit: it leaves the project byte for byte as it was, costs no
            // revision, and so can never be undone or saved.
            const auto before_fold = window.editor().project();
            const auto revision_before = window.editor().revision();
            child<QAbstractButton>(window, "sectionHeader")->click();
            settle();
            require(child<QWidget>(window, "schemaSectionBody")->isHidden(), "Folded away again");
            require(!QSettings().value("schemaSectionOpen").toBool(), "And that is remembered too");
            require(window.editor().project() == before_fold,
                    "Folding changed nothing in the project, so every answer is still there");
            require(window.editor().revision() == revision_before, "And it did not even count as a revision");

            // Put the attribute back as it was found, so what follows is not
            // working against a document this block has changed.
            while (window.editor().can_undo() &&
                   window.editor().project().attributes.at(any_attribute).logical_type != domain::LogicalType::Unset)
                child<QAction>(window, "undoCommand")->trigger();
            settle();
        }

        {
            // The schema rises over the diagram, and the panel it rises in can
            // be pulled to any height: half the stage, all of it, or a sliver.
            child<QPushButton>(window, "previewSchema")->click();
            settle_for(600);   // the panel rises over 280ms
            auto* grip = child<QWidget>(window, "schemaGrip");
            require(grip->isVisible(), "The panel wears a grip to resize it by");
            // Pulled all the way up, the panel covers the diagram; pushed down,
            // it becomes a sliver and the diagram comes back.
            const auto* panel = child<QWidget>(window, "schemaPanel");
            const auto before = panel->height();
            const auto double_click = [](QWidget* target) {
                QMouseEvent event(QEvent::MouseButtonDblClick, QPointF(10, 5), QPointF(10, 5),
                                  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(target, &event);
            };
            double_click(grip);
            settle();
            require(panel->height() > before, "Double-clicking it fills the stage");
            double_click(grip);
            settle();
            require(panel->height() < window.height(), "And again gives the diagram half back");

            // Full gives the whole window to the schema: the panels go away,
            // and leaving it puts back exactly what it put away.
            auto* explorer_dock = child<QDockWidget>(window, "explorerDock");
            require(explorer_dock->isVisible(), "The Explorer is there to begin with");
            auto* full = child<QPushButton>(window, "schemaFull");
            full->click();
            settle();
            require(!explorer_dock->isVisible(), "Full puts the panels away");
            settle_for(200);
            require(panel->width() >= window.width() - 8, "And the panel fills the window it was given");
            require(!child<QLabel>(window, "canvasInstructions")->isVisible(),
                    "The diagram's own furniture goes away with the panels");
            require(full->text() == "Exit full", "And says how to come back");
            // The tools for drawing go with the canvas they draw on. A row of
            // shapes to place, above a diagram nobody can see, is a row of
            // things that cannot be done.
            require(!child<QToolBar>(window, "modelTools")->isVisible(),
                    "Full puts the drawing tools away with the diagram");
            auto* kept = child<QWidget>(window, "schemaHeaderTools");
            require(kept->isVisible(), "And the few still worth reaching for come out in the header");
            require(child<QToolButton>(window, "schemaUndo")->defaultAction() != nullptr,
                    "Undo among them, the same action the menu has");
            require(!child<QToolButton>(window, "searchButton")->isVisible(),
                    "The diagram's own search goes: it is not what is on screen");
            full->click();
            settle();
            require(explorer_dock->isVisible(), "Leaving it brings them back");
            require(child<QToolBar>(window, "modelTools")->isVisible(), "The drawing tools with them");
            require(!kept->isVisible(), "And the header gives its own back");
            require(child<QLabel>(window, "canvasInstructions")->isVisible(),
                    "And the furniture with them");
            require(full->text() == "Full", "And says so");

            // A name typed on the schema is the name on the diagram. Renaming
            // a table renames the entity it came from, so the two never come
            // to disagree about what a thing is called; renaming the key the
            // conversion invented gives that key a name of its own, since it
            // has nothing behind it to rename.
            {
                // Found by name rather than by type: the view is a plain QWidget
                // subclass with no Q_OBJECT, and its name is its own.
                auto* schema = static_cast<desktop::SchemaView*>(child<QWidget>(window, "schemaView"));
                require(!schema->preview().tables.empty(), "The schema has tables to rename");
                const auto boxes = schema->table_boxes();
                require(!boxes.empty(), "And they have been placed");
                const auto named = [&](std::size_t which) {
                    return QString::fromStdString(schema->preview().tables[which].name);
                };
                const auto was = named(0);
                const auto header = boxes.front().topLeft() + QPointF(20, 8);
                QMouseEvent opened(QEvent::MouseButtonDblClick, header, schema->mapToGlobal(header.toPoint()),
                                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(schema, &opened);
                settle();
                auto* field = child<QLineEdit>(window, "schemaName");
                require(field->isVisible(), "Double-clicking a table's name opens it for typing");
                require(field->text() == was, "Opened on the name that is there");
                field->setText("Renamed");
                QKeyEvent done(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QApplication::sendEvent(field, &done);
                settle();
                require(!field->isVisible(), "Return puts the box away");
                bool on_diagram = false;
                for (const auto& [id, entity] : editor.project().entities) {
                    (void)id;
                    if (entity.name == "Renamed") on_diagram = true;
                }
                require(on_diagram, "And the entity on the diagram carries the typed name");

                // Escape keeps what was there.
                QApplication::sendEvent(schema, &opened);
                settle();
                field->setText("Discarded");
                QKeyEvent gave_up(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                QApplication::sendEvent(field, &gave_up);
                settle();
                for (const auto& [id, entity] : editor.project().entities) {
                    (void)id;
                    require(entity.name != "Discarded", "Escape keeps the name that was there");
                }
            
                // Another column is added where it will be read, with nothing
                // asked first: the slot under the table is pressed, the row is
                // made, and its name is waiting to be typed in the row itself.
                // It reflects, so the diagram gains the attribute and the
                // schema follows from it.
                const auto attributes = editor.project().attributes.size();
                const auto table = schema->table_boxes().front();
                const auto onto = QPointF(table.center().x(), table.bottom() + 10);
                QMouseEvent over_slot(QEvent::MouseMove, onto, schema->mapToGlobal(onto.toPoint()),
                                      Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(schema, &over_slot);
                QMouseEvent pressed(QEvent::MouseButtonPress, onto, schema->mapToGlobal(onto.toPoint()),
                                    Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(schema, &pressed);
                settle();
                require(editor.project().attributes.size() == attributes + 1,
                        "Pressing the slot adds an attribute to the diagram, not a schema-only column");
                require(field->isVisible(), "And opens its name for typing, with no dialog in the way");
                field->setText("Enrolled");
                QKeyEvent typed(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QApplication::sendEvent(field, &typed);
                settle();
                bool renamed = false;
                for (const auto& [id, attribute] : editor.project().attributes) {
                    (void)id;
                    if (attribute.name == "Enrolled") renamed = true;
                }
                require(renamed, "And the name typed in the row is the attribute's name");

                // A line's end goes where the hand puts it, including where the
                // schema cannot mean it -- and is told what is wrong and why
                // rather than being sprung back to where it belonged.
                const auto shapes = schema->line_shapes();
                if (!shapes.empty()) {
                    QString heard;
                    auto reported = schema->warned;
                    schema->warned = [&](const QString& words) {
                        heard = words;
                        if (reported) reported(words);
                    };
                    const auto tables = schema->table_boxes();
                    const auto& where = tables.front();
                    QPointF end;
                    double best = 1e9;
                    for (const auto& shape : shapes)
                        for (const auto& corner : {shape.front(), shape.back()}) {
                            const auto away = std::hypot(corner.x() - where.center().x(),
                                                         corner.y() - where.center().y());
                            if (away < best) { best = away; end = corner; }
                        }
                    const QPointF adrift(where.center().x(), where.bottom() + 80);
                    QMouseEvent took(QEvent::MouseButtonPress, end, schema->mapToGlobal(end.toPoint()),
                                     Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QApplication::sendEvent(schema, &took);
                    QMouseEvent hauled(QEvent::MouseMove, adrift, schema->mapToGlobal(adrift.toPoint()),
                                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QApplication::sendEvent(schema, &hauled);
                    QMouseEvent dropped(QEvent::MouseButtonRelease, adrift, schema->mapToGlobal(adrift.toPoint()),
                                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                    QApplication::sendEvent(schema, &dropped);
                    settle();
                    schema->warned = reported;
                    require(heard.contains("belongs on"),
                            "A misplaced end is told which row it belongs on");
                    require(heard.contains("joins nothing") || heard.contains("can only run to a key")
                            || heard.contains("points at"),
                            "And why where it was left cannot serve");
                    require(schema->loose_ends() > 0, "And it is left exactly where it was put");
                    // Put back, so what follows finds the schema as it was: the
                    // end was moved by an edit like any other, so one undo
                    // takes it back.
                    child<QAction>(window, "undoCommand")->trigger();
                    settle();
                    require(schema->loose_ends() == 0, "And one undo puts it back");
                }
            }

            // Closing the panel while it is full does not leave the window
            // stripped with nothing in it.
            full->click();
            settle();
            child<QPushButton>(window, "previewSchema")->click();
            settle();
            require(explorer_dock->isVisible(), "Closing the schema gives the panels back too");
            child<QPushButton>(window, "previewSchema")->click();
            settle_for(400);
            child<QPushButton>(window, "previewSchema")->click();
            settle();
        }

        {
            // A line between two tables is not only drawn. Any straight run
            // of it can be pushed sideways, either end can be moved around the
            // table it joins, and a double-click hands the whole line back to
            // the router.
            child<QPushButton>(window, "previewSchema")->click();
            settle_for(600);
            auto* schema = window.schema();
            require(schema != nullptr, "The panel holds the schema itself");
            const auto drawn = schema->line_shapes();
            require(!drawn.empty(), "The example's schema is drawn with lines between its tables");
            require(schema->shaped_lines() == 0, "None of them has been shaped by hand yet");

            // The longest straight run there is: certainly part of a line and
            // certainly clear of every table.
            std::size_t on_line = 0;
            QPointF ran_from;
            QPointF ran_to;
            double longest = 0;
            for (std::size_t line = 0; line < drawn.size(); ++line)
                for (std::size_t i = 1; i < drawn[line].size(); ++i) {
                    const auto length = std::hypot(drawn[line][i].x() - drawn[line][i - 1].x(),
                                                   drawn[line][i].y() - drawn[line][i - 1].y());
                    if (length <= longest) continue;
                    longest = length;
                    on_line = line;
                    ran_from = drawn[line][i - 1];
                    ran_to = drawn[line][i];
                }
            require(longest > 40, "And at least one run is long enough to take hold of");

            const auto drag = [&](QEvent::Type type, QPointF at, Qt::MouseButton button,
                                  Qt::MouseButtons held) {
                QMouseEvent event(type, at, schema->mapToGlobal(at.toPoint()), button, held,
                                  Qt::NoModifier);
                QApplication::sendEvent(schema, &event);
            };

            // A run moves across itself, never along itself: an upright run
            // goes sideways and a level one goes up and down.
            const bool upright = std::abs(ran_from.x() - ran_to.x()) < 0.01;
            const QPointF across = upright ? QPointF(34, 0) : QPointF(0, 34);
            const auto grab = (ran_from + ran_to) / 2;
            const auto moved_to = grab + across;
            drag(QEvent::MouseButtonPress, grab, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, grab + across / 3, Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseMove, moved_to, Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, moved_to, Qt::LeftButton, Qt::NoButton);
            settle();
            require(schema->shaped_lines() == 1, "Pushing a run sideways shapes that line");

            // The whole run has moved over, not one point on it: there is a run
            // of about the same length lying where the pointer left it.
            const auto pushed = schema->line_shapes()[on_line];
            const auto wanted = upright ? moved_to.x() : moved_to.y();
            bool run_moved = false;
            for (std::size_t i = 1; i < pushed.size(); ++i) {
                const auto a = pushed[i - 1];
                const auto b = pushed[i];
                const auto sits = upright ? a.x() : a.y();
                const auto still_upright = std::abs(a.x() - b.x()) < 0.01;
                const auto length = std::hypot(b.x() - a.x(), b.y() - a.y());
                if (still_upright == upright && std::abs(sits - wanted) < 0.01 && length > longest / 2)
                    run_moved = true;
            }
            require(run_moved, "The whole run moves across, keeping its length and its direction");

            // And it moved rather than sprouting a detour. A line sent out to
            // a dropped point and back again reverses on itself, which is the
            // spur that made this look wrong in the first place.
            const auto doubles_back = [](const std::vector<QPointF>& shape) {
                for (std::size_t i = 2; i < shape.size(); ++i) {
                    const auto in = shape[i - 1] - shape[i - 2];
                    const auto out = shape[i] - shape[i - 1];
                    if (QPointF::dotProduct(in, out) < -0.01) return true;
                }
                return false;
            };
            require(!doubles_back(pushed), "And the line never doubles back on itself");

            // A run next to an end cannot be pushed in over the symbols drawn
            // there. The line is held off the turn, so the foot and the
            // minimum always have straight line to sit on and are never left
            // standing beside it.
            {
                const auto shapes = schema->line_shapes();
                std::size_t which = 0;
                for (std::size_t line = 0; line < shapes.size(); ++line)
                    if (shapes[line].size() >= 3) { which = line; break; }
                const auto& shape = shapes[which];
                require(shape.size() >= 3, "A line with a turn in it");
                const auto stub_upright = std::abs(shape[0].x() - shape[1].x()) < 0.01;
                const auto out = shape[1] - shape[0];
                const auto reach = stub_upright ? std::abs(out.y()) : std::abs(out.x());
                require(reach >= 27, "Its first stretch already has room for the symbols");
                // Grab the run past the stub and shove it back at the table.
                const auto hold = (shape[1] + shape[2]) / 2;
                const auto onto = stub_upright ? QPointF(hold.x(), shape[0].y())
                                               : QPointF(shape[0].x(), hold.y());
                drag(QEvent::MouseButtonPress, hold, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseMove, (hold + onto) / 2, Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseMove, onto, Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, onto, Qt::LeftButton, Qt::NoButton);
                settle();
                const auto after = schema->line_shapes()[which];
                require(after.size() >= 2, "The line survives being shoved");
                const auto held = after[1] - after[0];
                const auto now = std::abs(held.x()) + std::abs(held.y());
                require(now >= 27, "And keeps the room its symbols need");
                child<QAction>(window, "schemaTidy")->trigger();
                settle();
            }

            // Every corner is a right angle, before and after being shaped: a
            // schema is drawn with square lines and never with diagonals.
            for (const auto& shape : schema->line_shapes())
                for (std::size_t i = 1; i < shape.size(); ++i)
                    require(std::abs(shape[i].x() - shape[i - 1].x()) < 0.01
                                || std::abs(shape[i].y() - shape[i - 1].y()) < 0.01,
                            "Every run of a line is square");

            QMouseEvent twice(QEvent::MouseButtonDblClick, moved_to,
                              schema->mapToGlobal(moved_to.toPoint()), Qt::LeftButton,
                              Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(schema, &twice);
            settle();
            require(schema->shaped_lines() == 0, "Double-clicking gives the line back to the router");
            require(schema->line_shapes()[on_line] == drawn[on_line], "Which puts its own way back");

            // A press that barely travels is a click on a line, not a push of
            // it: nothing should move under a hand that merely twitched.
            drag(QEvent::MouseButtonPress, grab, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, grab + QPointF(1, 1), Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, grab + QPointF(1, 1), Qt::LeftButton, Qt::NoButton);
            settle();
            require(schema->shaped_lines() == 0, "A press that barely moves leaves the line alone");
            require(schema->line_shapes()[on_line] == drawn[on_line], "And leaves its route alone too");

            // And Tidy puts every line back at once, as it does every table.
            drag(QEvent::MouseButtonPress, grab, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, moved_to, Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, moved_to, Qt::LeftButton, Qt::NoButton);
            settle();
            require(schema->shaped_lines() == 1, "A run pushed aside again");
            child<QAction>(window, "schemaTidy")->trigger();
            settle();
            require(schema->shaped_lines() == 0, "Tidy gives back the lines as well as the tables");

            // An end is taken hold of and moved around the table it belongs
            // to, and pulling it off the table leaves it where it was let go.
            const auto ends_of = [&](std::size_t line) {
                const auto shapes = schema->line_shapes();
                return std::pair{shapes[line].front(), shapes[line].back()};
            };
            const auto head = ends_of(0).first;
            require(head != ends_of(0).second, "A line has two ends to take hold of");

            // The end sits on the outline of the table it joins, which is what
            // says which way it may be slid without coming off.
            const auto boxes = schema->table_boxes();
            const auto joins = std::find_if(boxes.begin(), boxes.end(), [&](const QRectF& box) {
                return box.contains(head) && !box.adjusted(1, 1, -1, -1).contains(head);
            });
            require(joins != boxes.end(), "An end sits on the outline of the table it joins");
            const bool down_a_side = std::abs(head.x() - joins->left()) < 0.5
                                  || std::abs(head.x() - joins->right()) < 0.5;
            const auto along = down_a_side
                ? QPointF(head.x(), std::clamp(head.y() + 40, joins->top(), joins->bottom()))
                : QPointF(std::clamp(head.x() + 40, joins->left(), joins->right()), head.y());
            require(along != head, "And has room to be slid along that edge");

            drag(QEvent::MouseButtonPress, head, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, along, Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, along, Qt::LeftButton, Qt::NoButton);
            settle();
            const auto slid = ends_of(0).first;
            require(std::hypot(slid.x() - along.x(), slid.y() - along.y()) < 0.01,
                    "An end dragged along its table follows the pointer down the edge");
            require(schema->loose_ends() == 0, "And is still joined to it");

            // Then off it. Nothing pulls the end back, and the schema says so.
            auto lowest = 0.0;
            for (const auto& box : boxes) lowest = std::max(lowest, box.bottom());
            const QPointF adrift(joins->center().x(), lowest + 60);
            drag(QEvent::MouseButtonPress, slid, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, QPointF(slid.x(), lowest + 20), Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseMove, adrift, Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, adrift, Qt::LeftButton, Qt::NoButton);
            settle();
            const auto let_go = ends_of(0).first;
            require(std::hypot(let_go.x() - adrift.x(), let_go.y() - adrift.y()) < 0.01,
                    "An end pulled off its table stops exactly where it was let go");
            require(schema->loose_ends() == 1, "And is counted as a connection left hanging");
            require(child<QLabel>(window, "schemaState")->text().contains("1 end not connected"),
                    "Which the schema says out loud rather than quietly undoing it");

            // It is still the line's end, so it can be picked up again and put
            // back, and the count goes down when it is.
            drag(QEvent::MouseButtonPress, adrift, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, head, Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, head, Qt::LeftButton, Qt::NoButton);
            settle();
            require(schema->loose_ends() == 0, "Put back on its table it is joined again");
            require(!child<QLabel>(window, "schemaState")->text().contains("not connected"),
                    "And the schema stops saying so");

            child<QAction>(window, "schemaTidy")->trigger();
            settle();
            require(schema->shaped_lines() == 0, "And Tidy gives back the ends as well");

            child<QPushButton>(window, "previewSchema")->click();
            settle_for(400);
        }

        {
            // The schema can be edited away from the diagram it came from, and
            // says so when it has been. The menu that offers this and the
            // question box that follows it both stop and wait for somebody, so
            // what they drive is checked here instead of what they look like.
            child<QPushButton>(window, "previewSchema")->click();
            settle_for(600);
            auto* schema = window.schema();
            require(schema->asked != nullptr, "Right-clicking the schema asks what can be done");
            const auto columns_of = [&](const QString& table) {
                QStringList names;
                for (const auto& one : schema->preview().tables)
                    if (QString::fromStdString(one.name) == table)
                        for (const auto& column : one.columns)
                            names << QString::fromStdString(column.name);
                return names;
            };
            // Edits made straight to the Editor do not pass the window, which
            // is what normally tells the panel to read the model again, so the
            // panel is closed and reopened to bring it up to date.
            const auto reopen = [&] {
                child<QPushButton>(window, "previewSchema")->click();
                settle_for(400);
                child<QPushButton>(window, "previewSchema")->click();
                settle_for(600);
            };

            // Taken by value: the preview is worked out afresh after every
            // edit, so anything pointing into the old one is stale by then.
            const auto found = std::find_if(schema->preview().tables.begin(), schema->preview().tables.end(),
                                            [](const domain::PreviewTable& one) {
                                                return one.origin && std::holds_alternative<domain::EntityId>(*one.origin);
                                            });
            require(found != schema->preview().tables.end(), "An entity became a table");
            const auto table_of = *found->origin;
            const auto table_named = QString::fromStdString(found->name);
            const auto attributes = window.editor().project().attributes.size();
            const auto named = std::find_if(window.editor().project().attributes.begin(),
                                            window.editor().project().attributes.end(),
                                            [&](const auto& entry) {
                                                return entry.second.owner
                                                    && *entry.second.owner == table_of
                                                    && entry.second.kind == domain::AttributeKind::Normal;
                                            });
            require(named != window.editor().project().attributes.end(),
                    "That table's entity has an attribute of its own");
            const auto hidden_id = named->first;
            const auto hidden_name = QString::fromStdString(named->second.name);
            require(columns_of(table_named).contains(hidden_name), "Which the schema draws as a column");

            // Declining to reflect keeps a new column here and nowhere else,
            // and hiding one keeps the attribute on the diagram: both are
            // differences between the levels rather than edits to the model.
            require(editor.add_schema_column(table_of, "Nickname").ok, "A column is added to the schema alone");
            require(editor.hide_in_schema(hidden_id, true).ok, "And an attribute is hidden from the schema");
            reopen();
            require(window.editor().project().attributes.size() == attributes,
                    "Neither creates or destroys an attribute, so the diagram is untouched");
            require(window.editor().project().attributes.contains(hidden_id),
                    "The hidden attribute is still on the diagram");
            require(columns_of(table_named).contains("Nickname"), "The schema shows the added column");
            require(!columns_of(table_named).contains(hidden_name), "And stops showing the hidden one");
            require(child<QLabel>(window, "schemaState")->text().contains("2 changes not on the diagram"),
                    "And the schema says how far the two levels have come apart");

            // Each is an ordinary edit, so Undo puts the two levels back.
            require(editor.undo() && editor.undo(), "Both undo");
            reopen();
            require(columns_of(table_named).contains(hidden_name), "Undo brings the hidden column back");
            require(!columns_of(table_named).contains("Nickname"), "And takes the added one away");
            require(!child<QLabel>(window, "schemaState")->text().contains("not on the diagram"),
                    "And the schema stops saying they differ");

            child<QPushButton>(window, "previewSchema")->click();
            settle_for(400);
        }

        {
            // Pressing a table asks what it is joined to; the chips ask about
            // a whole kind of table; and the questions a conversion cannot
            // settle are answered on the tables they are about.
            child<QPushButton>(window, "previewSchema")->click();
            settle_for(600);
            auto* schema = window.schema();
            const auto boxes = schema->table_boxes();
            require(boxes.size() >= 3, "The example makes several tables");
            const auto press_at = [&](QPointF at) {
                QMouseEvent down(QEvent::MouseButtonPress, at, schema->mapToGlobal(at.toPoint()),
                                 Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(schema, &down);
                QMouseEvent up(QEvent::MouseButtonRelease, at, schema->mapToGlobal(at.toPoint()),
                               Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(schema, &up);
                settle();
            };
            // Earlier work in this window has left a table being asked about,
            // so the schema is put back before anything is checked.
            press_at(QPointF(boxes.front().left(), boxes.front().bottom() + 400));
            require(!schema->selected().has_value(), "Pressing the bare canvas asks about nothing");

            // The header, which is table and not column, line or answer.
            const auto on_first = boxes.front().topLeft() + QPointF(40, 6);
            press_at(on_first);
            require(schema->selected().has_value(), "Pressing a table asks about it");
            require(*schema->selected() == *schema->preview().tables.front().origin,
                    "And it is that table it asks about");
            // The ring round it runs out along everything it is joined to, so
            // the lines have to be findable from the table that was pressed.
            require(schema->selected_table() == std::optional<std::size_t>{0},
                    "And the table is findable by its place, which is what the lines are matched on");
            press_at(QPointF(boxes.front().left(), boxes.front().bottom() + 400));
            require(!schema->selected().has_value(), "And pressing it again puts the whole schema back");
            require(!schema->selected_table().has_value(), "So no line is ringed either");

            require(schema->showing() == desktop::SchemaShowing::Everything, "Everything, to begin with");
            child<QPushButton>(window, "schemaShowFromrelationships")->click();
            settle();
            require(schema->showing() == desktop::SchemaShowing::FromRelationships,
                    "A chip narrows the schema to one kind of table");
            press_at(on_first);
            require(schema->selected().has_value(), "A table can still be asked about while narrowed");
            child<QPushButton>(window, "schemaShowEverything")->click();
            settle();
            require(schema->showing() == desktop::SchemaShowing::Everything, "And the chips put it back");
            require(!schema->selected().has_value(), "Asking about a kind puts down the one being asked about");

            // Every table's questions are the ones the conversion cannot
            // settle for itself, asked where their answers will be seen.
            std::size_t asked = 0;
            for (const auto& table : schema->preview().tables) asked += table.decisions.size();
            require(asked > 0, "The example leaves questions a conversion cannot answer itself");
            const auto composite = std::find_if(
                window.editor().project().attributes.begin(), window.editor().project().attributes.end(),
                [](const auto& entry) { return entry.second.kind == domain::AttributeKind::Composite; });
            require(composite != window.editor().project().attributes.end(), "One of them is a composite");
            bool found_question = false;
            for (const auto& table : schema->preview().tables)
                for (const auto& decision : table.decisions)
                    if (decision.kind == domain::DecisionKind::CompositeMode) {
                        require(!decision.answered, "Which nobody has answered yet");
                        require(decision.chosen == 0, "So it reads as the default, Parts");
                        found_question = true;
                    }
            require(found_question, "And the schema asks it on the table it concerns");
            require(editor.set_composite_mode(composite->first, domain::CompositeMode::Whole).ok,
                    "Answering it is an ordinary edit");
            settle();

            // Pressing a column's blank opens every type there is, in one run
            // from the most reached for to the least, with a line to search by.
            // Aimed at the cell the view actually drew rather than at an
            // offset from the table's edge: the constraint marks sit at the
            // right of every row now, so the edge is no longer where the type
            // is.
            const auto blank = [&]() -> std::optional<QPointF> {
                const auto cells = schema->cell_boxes();
                for (std::size_t t = 0; t < schema->preview().tables.size() && t < cells.size(); ++t) {
                    const auto& columns = schema->preview().tables[t].columns;
                    for (std::size_t row = 0; row < columns.size() && row < cells[t].size(); ++row) {
                        if (columns[row].ignored || !columns[row].origin) continue;
                        if (columns[row].type != domain::LogicalType::Unset) continue;
                        if (cells[t][row].type.isEmpty()) continue;
                        return cells[t][row].type.center();
                    }
                }
                return std::nullopt;
            }();
            require(blank.has_value(), "The example leaves a column waiting for a type");
            press_at(*blank);
            auto* picker = window.findChild<QWidget*>("typePicker");
            require(picker != nullptr, "Pressing it opens the types");
            auto* listed = child<QListWidget>(window, "typePickerList");
            require(listed->count() == 38, "Which is every type there is, and no headings among them");
            require(listed->item(0)->text() == "int", "The most reached for leads");
            require(listed->item(listed->count() - 1)->text() == "table", "And the least brings up the rear");

            // The highlight follows the pointer rather than staying where the
            // keyboard left it, so what a click takes and what Return takes
            // are never two different things.
            // While the list is open over it, the cell it came from is drawn
            // as the empty slot it has become rather than as the question it
            // was: the question has been asked and is being answered.
            require(schema->answering().has_value(), "The cell being answered says so while it waits");
            require(listed->currentRow() == 0, "The first is in hand to begin with");
            const auto over = [&](int row) {
                const auto at = listed->visualItemRect(listed->item(row)).center();
                QMouseEvent moved(QEvent::MouseMove, QPointF(at), listed->viewport()->mapToGlobal(at),
                                  Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(listed->viewport(), &moved);
                settle();
            };
            over(4);
            require(listed->currentRow() == 4, "Moving over a type takes it in hand");
            over(1);
            require(listed->currentRow() == 1, "And the highlight goes back with the pointer");

            // The search narrows it without disturbing the order.
            auto* looking = child<QLineEdit>(window, "typePickerSearch");
            looking->setText("char");
            settle();
            // char, varchar, varchar(max), nchar, nvarchar, nvarchar(max).
            require(listed->count() == 6, "Searching narrows the list");
            for (int i = 0; i < listed->count(); ++i)
                require(listed->item(i)->text().contains("char"), "To what was searched for");
            looking->setText("zzz");
            settle();
            require(listed->count() == 1 && !(listed->item(0)->flags() & Qt::ItemIsEnabled),
                    "And says so when nothing matches");
            looking->setText("nvarchar");
            settle();

            // Choosing one answers that column, and it is an ordinary edit.
            const auto before = window.editor().revision();
            QTest_activate(listed, listed->item(0));
            settle();
            require(window.editor().revision() != before, "Choosing a type is an edit");
            require(!picker->isVisible(), "And the list closes behind it");
            require(!schema->answering().has_value(), "And the cell stops waiting when it closes");
            bool answered = false;
            for (const auto& table : schema->preview().tables)
                for (const auto& column : table.columns)
                    if (column.type == domain::LogicalType::NVarchar) answered = true;
            require(answered, "The column now carries the type it was given");
            // A measured type grows a second cell beside it for the number,
            // and pressing that asks how long in the terms that type is
            // measured in.
            const auto sized = [&]() -> std::optional<QPointF> {
                const auto cells = schema->cell_boxes();
                for (std::size_t t = 0; t < schema->preview().tables.size() && t < cells.size(); ++t) {
                    const auto& columns = schema->preview().tables[t].columns;
                    for (std::size_t row = 0; row < columns.size() && row < cells[t].size(); ++row) {
                        if (columns[row].type != domain::LogicalType::NVarchar) continue;
                        if (cells[t][row].size.isEmpty()) continue;
                        return cells[t][row].size.center();
                    }
                }
                return std::nullopt;
            }();
            require(sized.has_value(), "The column just answered is a measured type");
            press_at(*sized);
            auto* sizes = window.findChild<QWidget*>("sizePicker");
            require(sizes != nullptr, "Pressing its size asks how long");
            auto* common = child<QListWidget>(window, "sizePickerCommon");
            require(common->count() > 0, "And offers the lengths that type usually takes");
            require(child<QLineEdit>(window, "sizePickerScale")->isHidden(),
                    "A type with no scale is not asked for one");
            const auto counted = [&] {
                for (const auto& table : schema->preview().tables)
                    for (const auto& column : table.columns)
                        if (column.type == domain::LogicalType::NVarchar) return column.length;
                return std::uint32_t{0};
            };
            require(counted() == 0, "Nobody has said how long yet");

            // A number nobody thought to offer is typed in. The list is a
            // convenience, not the whole of what can be said.
            auto* typed_in = child<QLineEdit>(window, "sizePickerLength");
            typed_in->setText("77");
            QKeyEvent entered(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QApplication::sendEvent(typed_in, &entered);
            settle();
            require(counted() == 77, "A length typed in is the length it takes");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(counted() == 0, "And that undoes too");

            // Typed and then clicked away from counts just the same: a number
            // written into the field is an answer, finished with Return or not.
            press_at(*sized);
            settle();
            child<QLineEdit>(window, "sizePickerLength")->setText("31");
            window.findChild<QWidget*>("sizePicker")->hide();
            settle();
            require(counted() == 31, "A length typed and left is still the length");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(counted() == 0, "And undoes with everything else");

            // Opened and closed with nothing said changes nothing.
            const auto quiet = window.editor().revision();
            press_at(*sized);
            settle();
            window.findChild<QWidget*>("sizePicker")->hide();
            settle();
            require(window.editor().revision() == quiet, "Opening it and saying nothing is not an edit");
            press_at(*sized);
            settle();
            sizes = window.findChild<QWidget*>("sizePicker");
            common = child<QListWidget>(window, "sizePickerCommon");
            const auto wanted = common->item(common->count() - 1)->data(Qt::UserRole).toUInt();
            QTest_activate(common, common->item(common->count() - 1));
            settle();
            require(counted() == wanted, "Choosing one sets the length");
            require(!sizes->isVisible(), "And the list closes behind it");
            // Undone through the window, because an edit made straight to
            // the Editor never tells the panel to read the model again.
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(counted() == 0, "The length undoes on its own, leaving the type where it was");

            require(editor.undo().ok, "And it undoes like anything else");
            settle();

            // The constraints a row carries, all in one column and chosen
            // from the list that opens under it. The list stops and waits for
            // somebody, so what is wanted from it is asked for before it
            // opens and taken as soon as it is there.
            {
                const auto choose = [&](QPointF where, const char* which) {
                    QTimer::singleShot(0, &window, [&window, which] {
                        auto* menu = window.findChild<QMenu*>("schemaRulesMenu");
                        if (!menu) return;
                        if (auto* action = menu->findChild<QAction*>(which)) action->trigger();
                        menu->close();
                    });
                    press_at(where);
                    settle();
                };
                const auto cells = schema->cell_boxes();
                // An ordinary column, whose rules are its attribute's own.
                std::optional<QPointF> ordinary;
                std::optional<domain::AttributeId> behind;
                for (std::size_t t = 0; t < schema->preview().tables.size() && t < cells.size(); ++t) {
                    const auto& columns = schema->preview().tables[t].columns;
                    for (std::size_t row = 0; row < columns.size() && row < cells[t].size(); ++row) {
                        if (!columns[row].origin || columns[row].primary_key) continue;
                        if (cells[t][row].rules.isEmpty()) continue;
                        ordinary = cells[t][row].rules.center();
                        behind = *columns[row].origin;
                        break;
                    }
                    if (ordinary) break;
                }
                require(ordinary.has_value(), "Every real column has somewhere to carry its rules");
                require(!window.editor().project().attributes.at(*behind).unique,
                        "The column starts without a unique constraint");
                choose(*ordinary, "schemaRuleUnique");
                require(window.editor().project().attributes.at(*behind).unique,
                        "Choosing UNIQUE puts one on the attribute behind the column");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
                require(!window.editor().project().attributes.at(*behind).unique,
                        "And it undoes like any other edit");

                // A foreign key's nullability is not a fact about the column:
                // it is the participation of the side it points at, so
                // choosing it reaches the relationship on the diagram.
                const auto fresh = schema->cell_boxes();
                std::optional<QPointF> keyed;
                std::optional<domain::ParticipantId> side;
                bool was_required = false;
                for (std::size_t t = 0; t < schema->preview().tables.size() && t < fresh.size(); ++t) {
                    const auto& columns = schema->preview().tables[t].columns;
                    for (std::size_t row = 0; row < columns.size() && row < fresh[t].size(); ++row) {
                        if (!columns[row].link || fresh[t][row].rules.isEmpty()) continue;
                        if (!std::holds_alternative<domain::ParticipantId>(*columns[row].link)) continue;
                        keyed = fresh[t][row].rules.center();
                        side = std::get<domain::ParticipantId>(*columns[row].link);
                        was_required = columns[row].required;
                        break;
                    }
                    if (keyed) break;
                }
                require(keyed.has_value(), "The example has a foreign key put there by a relationship");
                const auto participation_of = [&] {
                    for (const auto& [id, relationship] : window.editor().project().relationships) {
                        (void)id;
                        for (const auto& one : relationship.participants)
                            if (one.id == *side) return one.participation;
                    }
                    return domain::Participation::Partial;
                };
                choose(*keyed, "schemaRuleNotNull");
                require((participation_of() == domain::Participation::Total) != was_required,
                        "Choosing a foreign key's nullability turns the side it points at over");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
                require((participation_of() == domain::Participation::Total) == was_required,
                        "And that undoes with everything else");

                // A key the conversion invented has no attribute behind it,
                // which is where choosing a constraint used to do nothing at
                // all. It is the commonest place of all to want one.
                const auto again = schema->cell_boxes();
                std::optional<QPointF> invented;
                std::optional<domain::ElementRef> whose;
                for (std::size_t t = 0; t < schema->preview().tables.size() && t < again.size(); ++t) {
                    const auto& one = schema->preview().tables[t];
                    if (!one.origin) continue;
                    for (std::size_t row = 0; row < one.columns.size() && row < again[t].size(); ++row) {
                        if (one.columns[row].origin_kind != domain::ColumnOrigin::Generated) continue;
                        if (again[t][row].rules.isEmpty()) continue;
                        invented = again[t][row].rules.center();
                        whose = *one.origin;
                        break;
                    }
                    if (invented) break;
                }
                require(invented.has_value(), "The example has a key the conversion invented");
                require(!window.editor().project().schema.counting_keys.contains(*whose),
                        "Which does not count itself up to begin with");
                choose(*invented, "schemaRuleIdentity");
                require(window.editor().project().schema.counting_keys.contains(*whose),
                        "Choosing IDENTITY makes it count itself up");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
                require(!window.editor().project().schema.counting_keys.contains(*whose),
                        "And that undoes like anything else");
            }

            // Narrowing a table folds its columns away from the right, one
            // at a time, and never moves the tables beside it.
            {
                // Held by value: the preview is worked out afresh on every
                // refresh, so a reference into it would not survive the first
                // pull.
                const auto first_origin = schema->preview().tables.front().origin;
                require(first_origin.has_value(), "The first table came from the diagram");
                const auto beside_before = schema->table_boxes();
                const auto pull_to = [&](double width) {
                    std::map<domain::ElementRef, domain::SchemaTableBox> asked;
                    domain::SchemaTableBox box;
                    box.width = width;
                    box.height = schema->table_boxes().front().height();
                    asked.emplace(*first_origin, box);
                    require(editor.resize_schema_tables(asked).ok, "The table is pulled");
                    schema->refresh();
                    settle();
                };
                // How many columns a row still shows beyond its name, read
                // off what was actually drawn.
                const auto columns_now = [&] {
                    const auto cells = schema->cell_boxes();
                    require(!cells.empty() && !cells.front().empty(), "The table has rows");
                    int shown = 0;
                    for (const auto& one : cells.front()) {
                        if (!one.rules.isEmpty()) return 2;
                        if (!one.type.isEmpty()) shown = std::max(shown, 1);
                    }
                    return shown;
                };
                require(columns_now() == 2, "At its own width a table shows all of its columns");
                // Swept down rather than pulled to chosen numbers: where each
                // column gives way depends on what that table happens to
                // hold, and what is being checked is the order they go in,
                // not the width at which each one does.
                const auto from = schema->table_boxes().front().width();
                std::vector<int> seen{2};
                for (auto width = from; width > domain::min_table_width; width -= 10) {
                    pull_to(std::max(domain::min_table_width, width));
                    const auto now = columns_now();
                    require(now <= seen.back(), "A narrower table never shows more than a wider one");
                    if (now != seen.back()) seen.push_back(now);
                }
                pull_to(domain::min_table_width);
                if (columns_now() != seen.back()) seen.push_back(columns_now());
                require((seen == std::vector<int>{2, 1, 0}),
                        "They fold from the right, one at a time: constraints, then type");
                require(columns_now() == 0,
                        "Leaving the keys and the names, which nothing else can stand in for");
                require(schema->table_boxes().front().width() == domain::min_table_width,
                        "And the table is as narrow as a table may be");
                // None of that moved anything else.
                const auto beside_after = schema->table_boxes();
                require(beside_after.size() == beside_before.size(), "Same tables throughout");
                for (std::size_t i = 1; i < beside_after.size(); ++i)
                    require(beside_after[i].topLeft() == beside_before[i].topLeft(),
                            "Pulling one table about leaves the others where they were");
                // Widened again, every column comes back in the reverse order.
                pull_to(from);
                require(columns_now() == 2, "Widened again, every column comes back");
                // Put the table back where it was, so what follows sees the
                // schema it expects rather than one this case left narrowed.
                while (window.editor().can_undo() && schema->table_boxes().front().width() != from) {
                    child<QAction>(window, "undoCommand")->trigger();
                    settle();
                }
            }

            // A search of the schema, which is not the diagram's search: it
            // picks out the tables and columns whose names carry the words.
            auto* looking_at_schema = child<QLineEdit>(window, "schemaSearch");
            looking_at_schema->setText("phone");
            settle();
            require(schema->looking_for() == "phone", "The schema is searched by its own bar");
            require(!schema->selected().has_value(),
                    "Which is a broader question than asking about one table");
            looking_at_schema->clear();
            settle();
            require(schema->looking_for().isEmpty(), "And clearing it puts the whole schema back");

            // Release gives the lines back without moving the tables.
            const auto where = schema->table_boxes();
            child<QAction>(window, "schemaRelease")->trigger();
            settle();
            require(schema->shaped_lines() == 0, "Release gives every line back to the router");
            require(schema->table_boxes() == where, "And leaves every table where it was");

            child<QPushButton>(window, "previewSchema")->click();
            settle_for(400);
        }

        {
            // Everything done on the schema undoes, and redoes. Arranging it
            // is presentation, but it is work somebody did, so it is an edit
            // like any other rather than something the window keeps to itself
            // and loses.
            child<QPushButton>(window, "previewSchema")->click();
            settle_for(600);
            auto* schema = window.schema();
            auto* undo = child<QAction>(window, "undoCommand");
            auto* redo = child<QAction>(window, "redoCommand");
            const auto drag = [&](QEvent::Type type, QPointF at, Qt::MouseButton button,
                                  Qt::MouseButtons held) {
                QMouseEvent event(type, at, schema->mapToGlobal(at.toPoint()), button, held,
                                  Qt::NoModifier);
                QApplication::sendEvent(schema, &event);
                settle();
            };

            // Moving a table.
            const auto before_move = schema->table_boxes();
            require(!before_move.empty(), "The schema has tables to move");
            const auto grab_table = before_move.front().topLeft() + QPointF(60, 8);
            drag(QEvent::MouseButtonPress, grab_table, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, grab_table + QPointF(40, 60), Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, grab_table + QPointF(40, 60), Qt::LeftButton, Qt::NoButton);
            const auto after_move = schema->table_boxes();
            require(after_move.front().topLeft() != before_move.front().topLeft(), "A table moves");
            require(undo->isEnabled(), "Which is an edit, so there is something to undo");
            // One drag is one step, not one step for every frame of it.
            undo->trigger();
            settle();
            require(schema->table_boxes().front().topLeft() == before_move.front().topLeft(),
                    "Undo puts the table back where it was, in one step");
            redo->trigger();
            settle();
            require(schema->table_boxes().front().topLeft() == after_move.front().topLeft(),
                    "And redo puts it back where it was taken");
            undo->trigger();
            settle();

            // Moving one table moves that table. Every other table stays
            // exactly where it was, which is only true because the automatic
            // arrangement is worked out for all of them and a moved table
            // simply sits elsewhere: leave a moved table out of the packing
            // and the ones after it shuffle up behind it.
            {
                const auto settled = schema->table_boxes();
                require(settled.size() >= 3, "Several tables to leave alone");
                const auto lift = settled[1].topLeft() + QPointF(60, 8);
                drag(QEvent::MouseButtonPress, lift, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseMove, lift + QPointF(70, 120), Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, lift + QPointF(70, 120), Qt::LeftButton, Qt::NoButton);
                const auto after = schema->table_boxes();
                require(after.size() == settled.size(), "The same tables are there");
                require(after[1].topLeft() != settled[1].topLeft(), "The one that was moved moved");
                for (std::size_t i = 0; i < after.size(); ++i) {
                    if (i == 1) continue;
                    require(after[i] == settled[i], "And no other table moved with it");
                }
                undo->trigger();
                settle();
            }

            // Pushing a line sideways.
            const auto drawn = schema->line_shapes();
            std::size_t on_line = 0;
            QPointF ran_from;
            QPointF ran_to;
            double longest = 0;
            for (std::size_t line = 0; line < drawn.size(); ++line)
                for (std::size_t i = 1; i < drawn[line].size(); ++i) {
                    const auto length = std::hypot(drawn[line][i].x() - drawn[line][i - 1].x(),
                                                   drawn[line][i].y() - drawn[line][i - 1].y());
                    if (length <= longest) continue;
                    longest = length;
                    on_line = line;
                    ran_from = drawn[line][i - 1];
                    ran_to = drawn[line][i];
                }
            require(longest > 40, "There is a run long enough to push");
            const bool upright = std::abs(ran_from.x() - ran_to.x()) < 0.01;
            const auto across = upright ? QPointF(34, 0) : QPointF(0, 34);
            const auto hold = (ran_from + ran_to) / 2;
            drag(QEvent::MouseButtonPress, hold, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, hold + across / 3, Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseMove, hold + across, Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, hold + across, Qt::LeftButton, Qt::NoButton);
            require(schema->shaped_lines() == 1, "A run pushed sideways shapes that line");
            const auto pushed = schema->line_shapes()[on_line];
            undo->trigger();
            settle();
            require(schema->shaped_lines() == 0, "Undo gives the line back to the router");
            require(schema->line_shapes()[on_line] == drawn[on_line], "With the route it had");
            redo->trigger();
            settle();
            require(schema->shaped_lines() == 1, "Redo shapes it again");
            require(schema->line_shapes()[on_line] == pushed, "The same way it was shaped");

            // Moving a line's end off its table.
            const auto head = schema->line_shapes()[on_line].front();
            auto lowest = 0.0;
            for (const auto& box : schema->table_boxes()) lowest = std::max(lowest, box.bottom());
            const QPointF adrift(head.x(), lowest + 70);
            drag(QEvent::MouseButtonPress, head, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, QPointF(head.x(), lowest + 30), Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseMove, adrift, Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, adrift, Qt::LeftButton, Qt::NoButton);
            require(schema->loose_ends() == 1, "An end pulled off its table is left hanging");
            undo->trigger();
            settle();
            require(schema->loose_ends() == 0, "Undo puts the end back on its table");
            redo->trigger();
            settle();
            require(schema->loose_ends() == 1, "And redo takes it off again");

            // Tidy, which throws away every arrangement at once.
            child<QAction>(window, "schemaTidy")->trigger();
            settle();
            require(schema->shaped_lines() == 0 && schema->loose_ends() == 0,
                    "Tidy gives back every line");
            undo->trigger();
            settle();
            require(schema->shaped_lines() == 1 && schema->loose_ends() == 1,
                    "And one undo brings the whole arrangement back");
            child<QAction>(window, "schemaTidy")->trigger();
            settle();

            // A line shaped by hand holds its shape when a table is moved on
            // top of it, unless it has been asked to get out of the way.
            {
                auto* give_way = child<QAction>(window, "schemaGiveWay");
                require(!give_way->isChecked(), "Lines hold their shape unless asked otherwise");
                // A shaped line of its own, since the block before this one
                // tidied everything away.
                const auto runs = schema->line_shapes();
                QPointF take;
                double reach = 0;
                for (const auto& shape : runs)
                    for (std::size_t i = 1; i < shape.size(); ++i) {
                        const auto length = std::hypot(shape[i].x() - shape[i - 1].x(),
                                                       shape[i].y() - shape[i - 1].y());
                        if (length <= reach) continue;
                        reach = length;
                        take = (shape[i] + shape[i - 1]) / 2;
                    }
                require(reach > 40, "A run long enough to shape");
                drag(QEvent::MouseButtonPress, take, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseMove, take + QPointF(0, 30), Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, take + QPointF(0, 30), Qt::LeftButton, Qt::NoButton);
                require(schema->shaped_lines() == 1, "There is a shaped line to leave alone");
                const auto boxes = schema->table_boxes();
                const auto onto = boxes.front().topLeft() + QPointF(60, 8);
                drag(QEvent::MouseButtonPress, onto, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseMove, onto + QPointF(120, 40), Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, onto + QPointF(120, 40), Qt::LeftButton, Qt::NoButton);
                require(schema->shaped_lines() == 1, "The shape survives a table moving about");
                undo->trigger();
                settle();

                // Asked to, a line a move has left lying across a table is
                // handed back to the router -- in the same edit as the move,
                // so one undo takes both back together.
                give_way->setChecked(true);
                settle();
                require(schema->lines_give_way(), "The option reaches the schema");
                const auto sat = schema->table_boxes();
                const auto shapes_before = schema->line_shapes();
                drag(QEvent::MouseButtonPress, onto, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseMove, onto + QPointF(120, 40), Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, onto + QPointF(120, 40), Qt::LeftButton, Qt::NoButton);
                require(schema->table_boxes() != sat, "The move happens");
                // One undo, whether or not the move freed a line: the two are
                // one edit, so they come back together rather than in turn.
                undo->trigger();
                settle();
                require(schema->table_boxes() == sat, "One undo puts the table back");
                require(schema->shaped_lines() == 1, "And the shaped line back with it");
                require(schema->line_shapes() == shapes_before, "Exactly as it was");
                give_way->setChecked(false);
                settle();
            }

            // A table is pulled about by any of its four edges and any of its
            // corners, which is an edit like the rest: it undoes, it redoes,
            // and it is saved.
            {
                require(schema->tables_resizable(), "Tables can be resized unless told otherwise");
                // A line always answers the pointer before the table it
                // crosses, so a pull is aimed at a stretch of edge no line is
                // lying on -- as a hand would aim it.
                const auto clear_of_lines = [&](QPointF at) {
                    for (const auto& shape : schema->line_shapes())
                        for (std::size_t i = 1; i < shape.size(); ++i) {
                            const auto from = shape[i - 1];
                            const auto to = shape[i];
                            const auto length = std::hypot(to.x() - from.x(), to.y() - from.y());
                            const auto steps = static_cast<int>(length / 3) + 1;
                            for (int step = 0; step <= steps; ++step) {
                                const auto on = from + (to - from) * (static_cast<double>(step) / steps);
                                if (std::hypot(on.x() - at.x(), on.y() - at.y()) < 14) return false;
                            }
                        }
                    return true;
                };
                // Somewhere along one edge of a box that is clear, keeping
                // well away from the corners so the edge itself is what is
                // taken hold of. The edges are numbered clockwise from the
                // left, as they are read out below.
                enum Edge { LeftEdge, TopEdge, RightEdge, BottomEdge };
                const auto clear_spot = [&](const QRectF& box, Edge edge) {
                    const auto along = edge == TopEdge || edge == BottomEdge ? box.width() : box.height();
                    const auto spot = [&](double step) {
                        switch (edge) {
                        case LeftEdge: return QPointF(box.left() + 2, box.top() + step);
                        case TopEdge: return QPointF(box.left() + step, box.top() + 2);
                        case RightEdge: return QPointF(box.right() - 2, box.top() + step);
                        case BottomEdge: break;
                        }
                        return QPointF(box.left() + step, box.bottom() - 2);
                    };
                    for (double step = 26; step < along - 26; step += 5)
                        if (clear_of_lines(spot(step))) return spot(step);
                    return spot(along / 2);
                };
                const auto before_width = schema->table_boxes().front();
                const auto edge = QPointF(before_width.right() - 2, before_width.center().y());
                drag(QEvent::MouseButtonPress, edge, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseMove, edge + QPointF(90, 0), Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, edge + QPointF(90, 0), Qt::LeftButton, Qt::NoButton);
                const auto widened = schema->table_boxes().front();
                require(widened.width() > before_width.width() + 40, "The table is wider");
                require(widened.topLeft() == before_width.topLeft(), "And has not moved doing it");
                require(widened.height() == before_width.height(),
                        "Nor grown taller: only the side that was pulled moves");
                undo->trigger();
                settle();
                require(schema->table_boxes().front().width() == before_width.width(),
                        "Undo puts the width back");
                redo->trigger();
                settle();
                require(schema->table_boxes().front().width() == widened.width(),
                        "And redo pulls it out again");

                // A table cannot be pulled past what a table may be.
                const auto wide_edge = QPointF(schema->table_boxes().front().right() - 2,
                                               schema->table_boxes().front().center().y());
                drag(QEvent::MouseButtonPress, wide_edge, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseMove, wide_edge + QPointF(4000, 0), Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, wide_edge + QPointF(4000, 0), Qt::LeftButton, Qt::NoButton);
                require(schema->table_boxes().front().width() <= domain::max_table_width,
                        "However far the edge is pulled");
                undo->trigger();
                settle();

                // The left edge carries the table's corner with it and leaves
                // the right-hand side exactly where it was. Both the size and
                // the place arrive as one edit, so one undo takes them back
                // together rather than leaving the table somewhere it was
                // never put.
                {
                    const auto before = schema->table_boxes().front();
                    const auto edge = clear_spot(before, LeftEdge);
                    drag(QEvent::MouseButtonPress, edge, Qt::LeftButton, Qt::LeftButton);
                    drag(QEvent::MouseMove, edge - QPointF(40, 0), Qt::NoButton, Qt::LeftButton);
                    drag(QEvent::MouseButtonRelease, edge - QPointF(40, 0), Qt::LeftButton, Qt::NoButton);
                    const auto reached = schema->table_boxes().front();
                    require(reached.left() < before.left() - 20, "The left edge follows the pointer");
                    require(std::abs(reached.right() - before.right()) < 0.01,
                            "And the right-hand side stays where it was");
                    require(std::abs(reached.height() - before.height()) < 0.01, "Its height is untouched");
                    undo->trigger();
                    settle();
                    require(schema->table_boxes().front() == before,
                            "One undo takes back the size and the place together");
                }

                // The bottom edge makes a table taller, and a table pulled
                // taller does not push the tables under it down the column:
                // pulling one table about is pulling one table about.
                {
                    const auto before = schema->table_boxes();
                    const auto standard_rows = schema->row_boxes().front();
                    const auto edge = clear_spot(before.front(), BottomEdge);
                    drag(QEvent::MouseButtonPress, edge, Qt::LeftButton, Qt::LeftButton);
                    drag(QEvent::MouseMove, edge + QPointF(0, 70), Qt::NoButton, Qt::LeftButton);
                    drag(QEvent::MouseButtonRelease, edge + QPointF(0, 70), Qt::LeftButton, Qt::NoButton);
                    const auto taller = schema->table_boxes();
                    require(taller.front().height() > before.front().height() + 50, "The table is taller");
                    require(taller.front().topLeft() == before.front().topLeft(),
                            "Without moving to do it");
                    require(std::equal(before.begin() + 1, before.end(), taller.begin() + 1),
                            "And no other table moves for it");
                    undo->trigger();
                    settle();
                    require(schema->table_boxes() == before, "Undo puts the height back");
                    redo->trigger();
                    settle();
                    require(schema->table_boxes().front().height() == taller.front().height(),
                            "And redo makes it tall again");

                    // The room it gained is shared out between its rows, so
                    // the table is a roomier one rather than one with a gap
                    // under its last row.
                    const auto deep = schema->row_boxes().front();
                    require(!deep.empty(), "The table has rows to share the room between");
                    require(deep.front().height() > standard_rows.front().height() + 1,
                            "Each row is drawn deeper for the room the table was given");
                    const auto rows_grew = (deep.back().bottom() - deep.front().top())
                                         - (standard_rows.back().bottom() - standard_rows.front().top());
                    require(std::abs(rows_grew - (taller.front().height() - before.front().height())) < 1.0,
                            "And every bit of the room the table gained went into them");

                    // The top edge is the same thing the other way up: it
                    // takes the table's corner with it and leaves the bottom
                    // where it is. A table that has been given room can give
                    // it back this way; one that has none cannot be pulled
                    // down over its own rows.
                    const auto room = schema->table_boxes().front();
                    const auto top_edge = clear_spot(room, TopEdge);
                    drag(QEvent::MouseButtonPress, top_edge, Qt::LeftButton, Qt::LeftButton);
                    drag(QEvent::MouseMove, top_edge + QPointF(0, 40), Qt::NoButton, Qt::LeftButton);
                    drag(QEvent::MouseButtonRelease, top_edge + QPointF(0, 40), Qt::LeftButton, Qt::NoButton);
                    const auto shortened = schema->table_boxes().front();
                    require(shortened.top() > room.top() + 30, "The top edge follows the pointer down");
                    require(std::abs(shortened.bottom() - room.bottom()) < 0.01,
                            "And the bottom of the table stays where it was");
                    undo->trigger();
                    settle();
                    require(schema->table_boxes().front() == room,
                            "One undo takes back the height and the place together");
                    undo->trigger();
                    settle();
                }

                // A corner pulls the two sides that meet at it, and leaves the
                // corner opposite it exactly where it was.
                {
                    const auto before = schema->table_boxes().front();
                    const std::array<QPointF, 4> corners{
                        before.topLeft() + QPointF(2, 2), before.topRight() + QPointF(-2, 2),
                        before.bottomRight() + QPointF(-2, -2), before.bottomLeft() + QPointF(2, -2)};
                    const std::array<QPointF, 4> opposite{before.bottomRight(), before.bottomLeft(),
                                                          before.topLeft(), before.topRight()};
                    // The bottom two first, since the schema is packed from
                    // the top left and a table near the top has nowhere to
                    // grow upwards into.
                    std::size_t which = 2;
                    for (const std::size_t i : {2u, 3u, 1u, 0u}) {
                        const auto room = (corners[i].y() > before.center().y() || before.top() > 80)
                                       && (corners[i].x() > before.center().x() || before.left() > 80);
                        if (room && clear_of_lines(corners[i])) { which = i; break; }
                    }
                    const auto corner = corners[which];
                    // Outwards from the middle of the table, whichever corner
                    // it is, so the pull always makes it bigger.
                    const QPointF away(corner.x() < before.center().x() ? -50 : 50,
                                       corner.y() < before.center().y() ? -50 : 50);
                    drag(QEvent::MouseButtonPress, corner, Qt::LeftButton, Qt::LeftButton);
                    drag(QEvent::MouseMove, corner + away, Qt::NoButton, Qt::LeftButton);
                    drag(QEvent::MouseButtonRelease, corner + away, Qt::LeftButton, Qt::NoButton);
                    const auto pulled = schema->table_boxes().front();
                    require(pulled.width() > before.width() + 30 && pulled.height() > before.height() + 30,
                            "A corner pulls two sides at once");
                    const std::array<QPointF, 4> now{pulled.bottomRight(), pulled.bottomLeft(),
                                                     pulled.topLeft(), pulled.topRight()};
                    require(std::abs(now[which].x() - opposite[which].x()) < 0.01
                                && std::abs(now[which].y() - opposite[which].y()) < 0.01,
                            "Leaving the corner opposite it alone");
                    undo->trigger();
                    settle();
                }

                // Fixed, and the edge is no longer a handle at all.
                child<QAction>(window, "schemaFixed")->trigger();
                settle();
                require(!schema->tables_resizable(), "Fixed takes the handle away");
                const auto held = schema->table_boxes().front();
                const auto held_edge = QPointF(held.right() - 2, held.center().y());
                drag(QEvent::MouseButtonPress, held_edge, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseMove, held_edge + QPointF(90, 0), Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, held_edge + QPointF(90, 0), Qt::LeftButton, Qt::NoButton);
                require(schema->table_boxes().front().width() == held.width(),
                        "So pulling the edge no longer widens it");
                const auto held_bottom = QPointF(held.center().x(), held.bottom() - 2);
                drag(QEvent::MouseButtonPress, held_bottom, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseMove, held_bottom + QPointF(0, 60), Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, held_bottom + QPointF(0, 60), Qt::LeftButton, Qt::NoButton);
                require(schema->table_boxes().front().height() == held.height(),
                        "Nor the bottom one make it taller");
                child<QAction>(window, "schemaResizable")->trigger();
                settle();
                child<QAction>(window, "schemaTidy")->trigger();
                settle();
            }

            // And an arrangement is part of the document, so it is saved.
            QTemporaryDir folder;
            require(folder.isValid(), "A place to save into");
            drag(QEvent::MouseButtonPress, grab_table, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, grab_table + QPointF(30, 30), Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, grab_table + QPointF(30, 30), Qt::LeftButton, Qt::NoButton);
            const auto arranged = window.editor().project().schema_layout;
            require(!arranged.empty(), "Which has something in it to save");
            const auto where = folder.filePath("arranged.erdx");
            require(window.export_project_file(where), "The project writes");
            infrastructure::ErdxProjectStore store_again;
            const auto opened = store_again.load(where.toStdString());
            require(opened.project.has_value(), "And opens again");
            require(opened.project->schema_layout == arranged,
                    "With the arrangement exactly as it was left");

            child<QPushButton>(window, "previewSchema")->click();
            settle_for(400);
        }

        // Export: how the work leaves. A picture any system can open, with
        // the project inside the two formats that can hold one, and a written
        // listing of the model for the people who want words rather than a
        // drawing.
        {
            QTemporaryDir pictures;
            require(pictures.isValid(), "Temporary export directory");

            // The Export tab waited until there was something to hand on.
            // There now is, so it is a tab like the others, built from the
            // same menu, and one word is used for it in both places.
            child<QAction>(window, "tabExport")->trigger();
            settle();
            auto* export_row = child<QToolBar>(window, "exportTools");
            require(export_row->isVisible(), "Export has a row of its own");
            for (const char* name : {"exportPdfDocument", "exportMarkdown", "exportHtml", "exportCsv",
                                     "exportSvg", "exportPng", "exportPdfPage",
                                     "exportWithOptions", "copyAsPicture"})
                require(child<QMenu>(window, "exportMenu")->findChildren<QAction*>().contains(
                            child<QAction>(window, name))
                            || export_row->actions().contains(child<QAction>(window, name)), name);
            require(child<QMenu>(window, "fileMenu")->actions().contains(
                        child<QMenu>(window, "exportMenu")->menuAction()),
                    "And the same menu hangs under File");
            require(child<QMenu>(window, "fileMenu")->actions().contains(
                        child<QMenu>(window, "importMenu")->menuAction()),
                    "With Import beside it, which is its pair");

            // Import has a tab of its own beside Export, because a reader
            // looking for one expects the other in the same place.
            child<QAction>(window, "tabImport")->trigger();
            settle();
            auto* import_row = child<QToolBar>(window, "importTools");
            require(import_row->isVisible(), "Import has a row of its own");
            for (const char* name : {"importProject", "importPicture", "importFromOtherTools"})
                require(import_row->actions().contains(child<QAction>(window, name)), name);
            require(child<QAction>(window, "importProject")->isEnabled(),
                    "Reading what ERDFlow writes can be done now");
            require(!child<QAction>(window, "importFromOtherTools")->isEnabled(),
                    "Reading what other tools write cannot, and stands there saying so");
            child<QAction>(window, "tabExport")->trigger();
            settle();
            require(export_row->isVisible() && !import_row->isVisible(), "The two tabs swap rows like the rest");

            // A tab colours itself when it is chosen; the row it brings up is
            // set heavier than the interface around it, so the row in front of
            // you reads as the thing you just chose rather than as a strip of
            // quiet text that looks the same whichever tab is showing.
            require(export_row->property("ribbonRow").toBool() && import_row->property("ribbonRow").toBool(),
                    "The rows that belong to a tab are marked as such");
            require(!child<QToolBar>(window, "modelTools")->property("ribbonRow").toBool(),
                    "Home is not, being the drawing tools, which their icons already tell apart");
            for (const char* row : {"insertTools", "designTools", "exportTools", "importTools",
                                    "viewTools", "helpTools"})
                require(child<QToolBar>(window, row)->property("ribbonRow").toBool(), row);
            // The menu's group headings are entries that cannot be chosen,
            // which reads well in a menu and would be a button nobody can
            // press on a row. They stay off the row.
            for (auto* action : export_row->actions())
                require(!action->objectName().endsWith("Heading"),
                        "No heading is put on the row as a dead button");
            require(!child<QAction>(window, "exportDocumentsHeading")->isEnabled(),
                    "A heading cannot be chosen, which is what makes it read as a heading");
            require(child<QAction>(window, "exportProject")->isEnabled(),
                    "While the project itself is a format work leaves in");
            require(child<QMenu>(window, "exportMorePictures") != nullptr,
                    "With the rarer picture formats gathered behind one entry");
            require(child<QAction>(window, "exportPng")->isEnabled(), "A drawn diagram can be exported");
            require(child<QAction>(window, "exportSvg")->text() == QString::fromUtf8("SVG picture…"),
                    "Named in the characters the name was written with, not in mangled bytes");
            require(!child<QAction>(window, "exportWithOptions")->icon().isNull(),
                    "Export carries a glyph of its own");

            auto options = window.export_choice().as_picture;
            options.format = desktop::PictureFormat::Png;
            const auto png = pictures.filePath("diagram.png");
            require(window.export_picture(options, png), "A PNG is written where it was told to write one");
            require(QFileInfo::exists(png), "And the file is there afterwards");

            // The picture is also the project. Opening it gives back exactly
            // what was drawn, which is the whole point of carrying it.
            const auto drawn = window.editor().project();
            if (window.editor().dirty()) dismiss(QMessageBox::Discard);
            require(window.open_path(png), "A PNG ERDFlow wrote opens as the project it carries");
            require(window.editor().project() == drawn, "Giving back exactly the diagram that was exported");
            require(!window.editor().dirty(), "And it opens clean, like any other project");

            // SVG carries it too, and is the picture to prefer for that reason.
            options.format = desktop::PictureFormat::Svg;
            const auto svg = pictures.filePath("diagram.svg");
            require(window.export_picture(options, svg), "An SVG is written");
            if (window.editor().dirty()) dismiss(QMessageBox::Discard);
            require(window.open_path(svg), "And opens as the project it carries");
            require(window.editor().project() == drawn, "Also exactly as it was drawn");

            // Asked to carry nothing, it carries nothing, and opening it says
            // so rather than reporting a damaged project.
            options.carry_project = false;
            const auto bare = pictures.filePath("bare.png");
            require(window.export_picture(options, bare), "A PNG written without the project");
            dismiss(QMessageBox::Ok);
            require(!window.open_path(bare), "Does not open as a project");
            require(window.editor().project() == drawn, "And leaves the open work alone");
            options.carry_project = true;

            // A page is written as a page, and carries nothing, as a page cannot.
            options.format = desktop::PictureFormat::Pdf;
            const auto pdf = pictures.filePath("diagram.pdf");
            require(window.export_picture(options, pdf), "A PDF page is written");
            QFile page(pdf);
            require(page.open(QIODevice::ReadOnly) && page.read(4) == "%PDF", "Which is a PDF");

            // Copy as picture puts both on the clipboard at once, so whatever
            // it is pasted into takes whichever of the two it prefers.
            window.canvas()->select_elements({});
            child<QAction>(window, "copyAsPicture")->trigger();
            settle();
            const auto* clipboard = QApplication::clipboard()->mimeData();
            require(clipboard && clipboard->hasFormat("image/png") && clipboard->hasFormat("image/svg+xml"),
                    "Copy as picture puts a raster and a vector on the clipboard together");

            // The project itself is a format work leaves in, and the only one
            // that loses nothing. Writing a copy leaves the open project alone:
            // it keeps its own file and its own unsaved state, which is what
            // makes it a copy rather than a Save As.
            {
                const auto copy = pictures.filePath("copy.erdx");
                const auto working_on = window.editor().project();
                require(window.export_project_file(copy), "A copy of the project is written");
                infrastructure::ErdxProjectStore reader;
                const auto read_back = reader.load(copy.toStdString());
                require(read_back.project.has_value(), "And reads back");
                require(*read_back.project == working_on, "As exactly the project that was open");
            }

            // Import is Export's pair. It brings another project's contents
            // into this one rather than replacing it, everything arrives with
            // identities of its own so nothing collides, and it undoes at once.
            {
                const auto source = pictures.filePath("to-import.erdx");
                require(window.export_project_file(source), "A project to import from");
                const auto before = window.editor().project();
                require(window.import_project(source), "It imports");
                const auto after = window.editor().project();
                require(after.entities.size() == before.entities.size() * 2,
                        "Everything arrives beside what was there, rather than replacing it");
                // Not one identity in common, though the two are the same work:
                // an import must be able to bring in a project copied from this
                // very one without a single collision.
                for (const auto& [id, entity] : before.entities) {
                    (void)entity;
                    require(after.entities.contains(id), "What was there is untouched");
                }
                std::size_t fresh = 0;
                for (const auto& [id, entity] : after.entities) {
                    (void)entity;
                    if (!before.entities.contains(id)) ++fresh;
                }
                require(fresh == before.entities.size(), "And what arrived is all new identity");
                require(after.comments.size() == before.comments.size() * 2
                            || before.comments.empty(),
                        "What was said about the work comes with the work");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
                require(window.editor().project() == before, "And one undo takes the whole import back out");
            }

            // The four written listings, each of which reads the model that is
            // already there. They are listings and not the project, so nothing
            // reopens them; what is checked is that each is the thing it claims.
            // Read out of the project rather than typed here, since earlier
            // tests rename what is on the diagram and a listing has to name
            // whatever is actually there now.
            require(!window.editor().project().entities.empty(), "There is an entity to be listed");
            const auto listed_entity = QString::fromStdString(
                window.editor().project().entities.begin()->second.name).toUtf8();
            struct Listing { desktop::DocumentFormat format; const char* file; const char* opens_with; };
            for (const auto& listing : {Listing{desktop::DocumentFormat::Markdown, "dictionary.md", "# "},
                                        Listing{desktop::DocumentFormat::Csv, "listing.csv", "Element,Name,"},
                                        Listing{desktop::DocumentFormat::Html, "report.html", "<!DOCTYPE html>"},
                                        Listing{desktop::DocumentFormat::Pdf, "report.pdf", "%PDF"}}) {
                const auto where = pictures.filePath(QString::fromLatin1(listing.file));
                require(window.export_document(listing.format, where),
                        "A listing is written where it was told to write one");
                QFile written(where);
                require(written.open(QIODevice::ReadOnly), listing.file);
                const auto head = written.readAll();
                require(head.startsWith(listing.opens_with), "And is the kind of file it says it is");
                // A PDF compresses its text, so what it names cannot be read
                // out of its bytes; the three text formats can be.
                if (listing.format != desktop::DocumentFormat::Pdf)
                    require(head.contains(listed_entity), "Naming what is actually on the diagram");
            }

            // The dialog offers every format in one list, documents above
            // pictures, and turns off what cannot be asked for: an extent with
            // nothing in it, the picture options a document has none of, and
            // carrying the project in a format with nowhere to put it.
            desktop::ExportDialog dialog(*window.canvas(), window.editor().project());
            desktop::ExportChoice choice;
            choice.as_picture = options;
            dialog.set_choice(choice);
            settle();
            auto* extent = dialog.findChild<QComboBox*>("exportExtent");
            auto* format = dialog.findChild<QComboBox*>("exportFormat");
            auto* carry = dialog.findChild<QCheckBox*>("exportCarryProject");
            auto* size = dialog.findChild<QLabel*>("exportSize");
            require(extent && format && carry && size, "The dialog has its controls");
            const auto* extents = qobject_cast<QStandardItemModel*>(extent->model());
            require(extents != nullptr, "Whose extents can be turned off one at a time");
            const auto selection_row = extent->findData(static_cast<int>(desktop::PictureExtent::Selection));
            require(!extents->item(selection_row)->isEnabled(),
                    "With nothing selected, a picture of the selection cannot be asked for");
            require(!size->text().isEmpty(), "And it says what pressing Export will produce");
            format->setCurrentIndex(format->findData(static_cast<int>(desktop::PictureFormat::Jpeg)));
            settle();
            require(!carry->isEnabled() && !carry->isChecked(),
                    "A format that cannot carry the project does not offer to");
            format->setCurrentIndex(format->findData(static_cast<int>(desktop::PictureFormat::Svg)));
            settle();
            require(carry->isEnabled(), "One that can, does");
            // Documents are in the same list, above the pictures, and choosing
            // one puts away the options it has none of.
            const auto markdown_row = format->findData(100 + static_cast<int>(desktop::DocumentFormat::Markdown));
            require(markdown_row > 0, "The documents are in the same list as the pictures");
            format->setCurrentIndex(markdown_row);
            settle();
            require(!extent->isEnabled() && !carry->isEnabled(),
                    "A document has no extent to choose and nowhere to carry the project");
            require(size->text().contains("listing"), "And the dialog says it is a listing");
            require(dialog.choice().document && dialog.choice().as_document == desktop::DocumentFormat::Markdown,
                    "What it settled on is the document that was chosen");

            // Nothing drawn is nothing to export, and the entries go quiet
            // rather than failing when they are pressed.
            child<QAction>(window, "newProject")->trigger();
            settle();
            require(!child<QAction>(window, "exportPng")->isEnabled(), "An empty project has nothing to hand on");
            window.load_example();
            settle();
            require(child<QAction>(window, "exportPng")->isEnabled(), "And a drawn one has something again");
        }

        window.close(); // The example was reloaded clean, so no discard dialog.
        require(!window.isVisible(), "Clean window closes without prompting");
        std::cout << "Desktop integration tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Desktop test failed: " << error.what() << '\n';
        return 1;
    }
}
