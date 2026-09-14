#pragma once

#include "diagram_view.hpp"
#include "application/project_store.hpp"

#include <QMainWindow>
#include <map>

class QAction;
class QDockWidget;
class QLabel;
class QComboBox;
class QScrollArea;
class QStandardItemModel;
class QTreeView;

namespace erdflow::desktop {

class MainWindow final : public QMainWindow {
public:
    MainWindow(application::Editor& editor, application::ProjectStore& store,
               application::IdGenerator& ids, QWidget* parent = nullptr);
    ~MainWindow() override;
    bool open_path(const QString& path);
    void load_example();
    [[nodiscard]] const application::Editor& editor() const { return editor_; }
    [[nodiscard]] DiagramView* canvas() const { return canvas_; }

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    application::IdGenerator& ids_;
    application::Editor& editor_;
    application::ProjectStore& store_;
    DiagramView* canvas_ = nullptr;
    QTreeView* explorer_ = nullptr;
    QTreeView* issues_ = nullptr;
    QStandardItemModel* explorer_model_ = nullptr;
    QStandardItemModel* issue_model_ = nullptr;
    QScrollArea* properties_ = nullptr;
    QDockWidget* validation_dock_ = nullptr;
    QLabel* document_label_ = nullptr;
    QLabel* count_label_ = nullptr;
    QLabel* zoom_label_ = nullptr;
    QLabel* readiness_label_ = nullptr;
    QAction* undo_ = nullptr;
    QAction* redo_ = nullptr;
    QAction* duplicate_ = nullptr;
    QAction* rename_ = nullptr;
    std::map<Tool, QAction*> tool_actions_;
    std::map<Notation, QAction*> notation_actions_;
    // Generalization and specialization share one toolbar entry; this is the
    // mode its main button uses, chosen from its dropdown.
    Tool isa_mode_ = Tool::Specialization;
    QAction* isa_action_ = nullptr;
    QComboBox* notation_box_ = nullptr;
    std::map<QString, domain::ElementRef> references_;
    std::vector<domain::ElementRef> selection_;
    QString path_;
    bool refreshing_ = false;

    void build_shell();
    void build_actions();
    void choose_tool(Tool tool, bool locked);
    void refresh_tool_labels();
    [[nodiscard]] QWidget* toolbar_widget(QAction* action) const;
    void choose_notation(Notation notation);
    void refresh();
    void refresh_explorer();
    void highlight_explorer();
    void refresh_properties();
    void refresh_validation();
    void show_result(const application::EditResult& result);
    void selection_changed(const std::vector<domain::ElementRef>& selection);
    void rename_selection();
    bool confirm_discard();
    bool save(bool choose_path = false);
    void new_project();
    void open_dialog();
};

} // namespace erdflow::desktop
