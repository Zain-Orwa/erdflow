#include "app/desktop/icons.hpp"
#include "app/desktop/main_window.hpp"
#include "infrastructure/project_store.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QFile>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
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
        require(original.entities.size() == 2 && original.attributes.size() == 5 && original.relationships.size() == 1,
                "Example contains complete basic Chen graph");
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
        require(window.editor().project().entities.size() == 2, "Backspace in property field cannot delete entity");
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
        require(window.editor().project().entities.size() == 3 && window.editor().project().attributes.size() == 7,
                "Duplicate copies entity and owned attributes through one command");
        child<QAction>(window, "undoCommand")->trigger();
        require(window.editor().project().entities.size() == 2, "One undo removes whole duplicate");

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
        require(window.editor().project().entities.size() == 3 && window.canvas()->tool() == desktop::Tool::Select,
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
        require(window.editor().project().entities.size() == 5, "A locked tool keeps placing");
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

        // The modern set is artwork rather than drawing, so it neither follows
        // the theme nor needs to: switching to it must change every button, and
        // switching back must restore what the theme was drawing.
        {
            const auto drawn = child<QAction>(window, "toolEntity")->icon().pixmap(22, 22).toImage();
            child<QAction>(window, "iconsmodern")->trigger();
            settle();
            require(window.icon_mode() == desktop::IconMode::Modern, "The menu changes the icon set");
            const auto artwork = child<QAction>(window, "toolEntity")->icon().pixmap(22, 22).toImage();
            require(!artwork.isNull() && artwork != drawn, "Every button takes the new set");
            require(QSettings().value("iconMode").toString() == "modern", "The choice is remembered");
            // Every glyph the window uses has to exist in the set, or a button
            // silently falls back and the two sets disagree about what is there.
            for (int index = 0; index <= static_cast<int>(desktop::Glyph::Theme); ++index) {
                const auto glyph = static_cast<desktop::Glyph>(index);
                const QIcon file(QStringLiteral(":/erdflow/icons/%1.svg").arg(desktop::icon_name(glyph)));
                require(!file.pixmap(22, 22).isNull(),
                        "The modern set has artwork for every glyph the window draws");
            }
            child<QAction>(window, "iconsnormal")->trigger();
            settle();
            require(child<QAction>(window, "toolEntity")->icon().pixmap(22, 22).toImage() == drawn,
                    "Going back restores the drawn glyphs");
        }

        // A theme can be seen on the window before it is chosen, and looking at
        // one without choosing it must leave nothing behind.
        {
            const auto chosen = window.canvas()->theme_id();
            auto* dracula = child<QAction>(window, "themedracula");
            emit dracula->hovered();
            settle();
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
            settle();
            dracula->trigger();
            settle();
            emit child<QMenu>(window, "themeMenu")->aboutToHide();
            settle();
            require(window.canvas()->theme_id() == desktop::ThemeId::Dracula, "Choosing one keeps it");
            require(QSettings().value("theme").toString() == "dracula", "And remembers it");
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
            for (int row = 0; row < model->item(0)->rowCount(); ++row)
                if (model->item(0)->child(row)->text().startsWith("Pictures (1)")) listed = true;
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
        while (window.editor().project().entities.size() > 3) child<QAction>(window, "undoCommand")->trigger();
        child<QAction>(window, "undoCommand")->trigger();
        require(window.editor().project().entities.size() == 2, "Create is undoable from shell");
        child<QAction>(window, "checkModel")->trigger();
        require(child<QTreeView>(window, "modelIssues")->isVisible(), "Model checks action opens findings");
        window.close(); // Undo returned to the saved revision, so no discard dialog.
        require(!window.isVisible(), "Clean window closes without prompting");
        std::cout << "Desktop integration tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Desktop test failed: " << error.what() << '\n';
        return 1;
    }
}
