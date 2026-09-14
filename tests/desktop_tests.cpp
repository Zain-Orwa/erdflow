#include "app/desktop/main_window.hpp"
#include "infrastructure/project_store.hpp"

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFile>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>

#include <iostream>
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
        auto combos = window.findChildren<QComboBox*>();
        require(combos.size() == 4, "Relationship renders two independent participant forms");
        combos.front()->setCurrentIndex(0);
        QMetaObject::invokeMethod(combos.front(), "activated", Q_ARG(int, 0));
        settle();
        require(window.editor().project().relationships.at(relationship).participants.front().id == participant,
                "Cardinality edit preserves participant ID");
        require(window.editor().project().relationships.at(relationship).participants.front().maximum == domain::Cardinality::One,
                "Participant form applies cardinality");

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

        child<QAction>(window, "toolEntity")->trigger();
        click_canvas(*window.canvas(), QPointF(200, 250));
        require(window.editor().project().entities.size() == 3 && window.canvas()->tool() == desktop::Tool::Select,
                "Toolbar create routes through canvas and returns to Select");
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
