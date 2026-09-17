#pragma once

#include "diagram_view.hpp"
#include "export_dialog.hpp"
#include "icons.hpp"
#include "application/project_store.hpp"

#include <QMainWindow>
#include <QPointF>
#include <QIcon>
#include <QPointer>
#include <functional>
#include <map>
#include <optional>
#include <vector>

class QAction;
class QDockWidget;
class QLabel;
class QComboBox;
class QScrollArea;
class QToolButton;
class QVBoxLayout;
class QStandardItemModel;
class QTreeView;

namespace erdflow::desktop {

class Ribbon;
class SearchBar;
class SymbolPicker;

class MainWindow final : public QMainWindow {
public:
    MainWindow(application::Editor& editor, application::ProjectStore& store,
               application::IdGenerator& ids, QWidget* parent = nullptr);
    ~MainWindow() override;
    bool open_path(const QString& path);
    // Applies a theme to the window and its canvas, and remembers it.
    void set_theme(ThemeId id);
    // Shows a theme without choosing it, so one can be judged on the window
    // itself rather than on its name. Leaving the menu puts back the chosen one.
    void preview_theme(ThemeId id);
    // Which icon set the window draws its actions with, and remembers it.
    void set_icon_mode(IconMode mode);
    [[nodiscard]] IconMode icon_mode() const { return icon_mode_; }
    void load_example();
    // Places a picture read from a file, centred on the given canvas point or
    // else in the middle of the view. The file's own bytes are kept when it is
    // a PNG or JPEG of modest size; anything else is re-encoded, scaled down if
    // it is large. False if it could not be read.
    bool insert_picture(const QString& path, std::optional<QPointF> at = {});
    // Writes a picture of the diagram. With no location it asks where the
    // picture should go; with one it writes there and asks nothing, which is
    // how anything that already knows the destination drives it. The options
    // are whatever the export dialog last settled on, so the quick entries
    // and the dialog cannot produce differently sized pictures of one diagram.
    bool export_picture(const PictureOptions& options, const QString& location = {});
    // Writes the project as a listing rather than as a picture: a report, a
    // data dictionary or a spreadsheet of what the diagram says.
    bool export_document(DocumentFormat format, const QString& location = {});
    // Puts a picture of the selection, or of the whole diagram when nothing is
    // selected, on the clipboard as both a PNG and an SVG, so whatever it is
    // pasted into can take whichever it prefers.
    bool copy_picture();
    // Writes a copy of the project itself, losing nothing. Saving keeps working
    // on the file it wrote; this leaves the open project where it is.
    bool export_project_file(const QString& location = {});
    // Brings another project's contents into this one, from a project file or
    // from a picture carrying one. Everything arrives with fresh identities and
    // clear of what is already drawn, and the whole import undoes in one step.
    bool import_project(const QString& path);
    // Narrows the diagram to what is being looked for, bringing what it finds
    // into the middle of the view. Also how anything that already knows what to
    // look for drives the search.
    void search_diagram(const DiagramSearch& search);
    // Opens the search bar, or closes it and puts the whole diagram back.
    void open_search(const QString& looking_for = {});
    void close_search();
    // Leaves a remark on the given things, asking for the words. With text
    // supplied it asks nothing, which is how anything that already has the
    // words drives it. One remark covers everything it is given at once.
    bool add_comment(std::vector<domain::CommentTarget> targets, const QString& said = {});
    // Pins a remark into the range of text now selected in the named property
    // field, which is how a remark comes to be about one word rather than a
    // whole element. False when nothing is selected there.
    bool comment_on_selected_text(const QString& field_name, const QString& said = {});
    [[nodiscard]] const ExportChoice& export_choice() const { return export_choice_; }
    [[nodiscard]] const application::Editor& editor() const { return editor_; }
    [[nodiscard]] DiagramView* canvas() const { return canvas_; }
    // The row of tabs above the tool row: File, Home, Insert, Design, Export,
    // View, Help.
    [[nodiscard]] Ribbon* ribbon() const { return ribbon_; }
    // Opens the symbol gallery, on the named group if one is named. The picker
    // is built the first time it is asked for and kept afterwards, so a search
    // and a chosen group survive being closed and opened again.
    void show_symbols(const QString& group = {});
    // Puts one character into whatever text field was last being written in.
    // With nothing being written in it goes on the diagram instead, as a note
    // carrying that character, since putting a character somewhere is the
    // whole purpose of picking one. False only if the edit itself was refused.
    bool insert_symbol(const QString& character);

protected:
    void closeEvent(QCloseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
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
    // Size, for the one element that has one to choose: a symbol. They are
    // enabled only while everything selected is a symbol, so the pair never
    // offers to act on half of a selection.
    QAction* enlarge_ = nullptr;
    QAction* shrink_ = nullptr;
    std::map<Tool, QAction*> tool_actions_;
    std::map<Notation, QAction*> notation_actions_;
    std::map<ThemeId, QAction*> theme_actions_;
    std::map<QAction*, Glyph> action_glyphs_;
    // What the window is currently showing, and what the user actually chose.
    // They differ only while a theme is being previewed under the pointer.
    ThemeId theme_ = ThemeId::OfficeLight;
    ThemeId committed_theme_ = ThemeId::OfficeLight;
    void apply_appearance(ThemeId id);
    // Fits the toolbar to the width there is, rather than letting it run off
    // the end of the window.
    void fit_toolbar();
    [[nodiscard]] int icon_pixels() const;
    // Keeps the canvas's own controls in the corner of the view as it resizes.
    void place_canvas_controls();
public:
    // Moves the raft by the given amount and remembers where it was put, as a
    // fraction of the view, so resizing the window keeps it where it was.
    void move_canvas_controls(QPoint by);
    // Puts the raft away, or brings it back. It is shown to begin with.
    void show_canvas_controls(bool shown);
private:
    // Where the raft has been dragged to, if anywhere. Nothing means the
    // corner it starts in.
    std::optional<QPointF> canvas_controls_place_;
    IconMode icon_mode_ = IconMode::Outline;
    std::map<IconMode, QAction*> icon_mode_actions_;
    // Generalization and specialization share one toolbar entry; this is the
    // mode its main button uses, chosen from its dropdown.
    Tool isa_mode_ = Tool::Specialization;
    QAction* isa_action_ = nullptr;
    std::map<LineStyle, QAction*> line_actions_;
    QComboBox* notation_box_ = nullptr;
    QWidget* canvas_controls_ = nullptr;
    QAction* notation_action_ = nullptr;
    QAction* notation_label_action_ = nullptr;
    QAction* notation_separator_ = nullptr;
    bool fitting_ = false;
    QToolButton* theme_button_ = nullptr;
    QAction* full_view_ = nullptr;
    QAction* check_ = nullptr;
    // Opens the search. It is on the tool row and in the Edit menu, so it is
    // made once and shown in both.
    QAction* find_action_ = nullptr;
    QToolButton* search_button_ = nullptr;
    // Keeps a wheel from changing whatever the pointer happens to be over.
    QObject* wheel_guard_ = nullptr;
    std::map<domain::BackgroundStyle, QAction*> background_actions_;
    // What the export dialog last settled on, kept for the session so a
    // second export of the same work takes one press rather than four.
    ExportChoice export_choice_;
    // Everything under Export, kept so they can be turned off together while
    // there is nothing drawn to make anything of.
    std::vector<QAction*> export_actions_;
    void export_dialog();
    // Asks which file to import, looking among projects or among pictures.
    void import_dialog(bool pictures);
    std::vector<QAction*> import_actions_;
    // Asks where an export should go, suggesting a name beside the project.
    // Empty when the person cancelled or declined to replace a file.
    [[nodiscard]] QString export_location(const QString& suffix, const QString& label);
    // The project's own bytes, for a picture asked to carry them. Empty with a
    // reason when the project is too large to travel inside a picture, which is
    // said plainly rather than failing the export.
    [[nodiscard]] QByteArray project_payload(QString& note);
    // Reads a project from a file that may be a project or a picture carrying
    // one, so the two open the same way.
    [[nodiscard]] application::LoadResult read_project(const QString& path);
    void refresh_export_actions();
    // The remarks on whatever is selected, listed in the properties panel so
    // each can be read, put away, brought back, reworded or deleted.
    void build_comment_section(QWidget* panel, QVBoxLayout* layout);
    // Asks for the words of a remark, starting from whatever it says now.
    // Empty when the person cancelled or wrote nothing.
    [[nodiscard]] QString ask_for_comment(const QString& said, const QString& about);
    // Gives a text field a Comment entry beneath its usual cut-and-paste one.
    void offer_text_comment(QWidget* field);
    // Keeps the Background menu showing the paper the document actually has.
    void refresh_background_menu();
    void choose_background(domain::BackgroundStyle style);
    void choose_background_image();
    // Keeps the Check model button saying what pressing it will do, whichever
    // way the findings were opened or closed.
    void refresh_check_action();
    // The panels put away by full view, so exactly those come back. Model
    // checks is often closed already, and full view must not open it.
    std::vector<QDockWidget*> hidden_panels_;
    Ribbon* ribbon_ = nullptr;
    SearchBar* search_bar_ = nullptr;
    SymbolPicker* symbols_ = nullptr;
    // The text field a picked character goes into: the last one that was being
    // written in. Committing an edit rebuilds the properties panel and takes
    // the widget with it, so the field is remembered by name as well and looked
    // up again when the pointer has gone stale.
    QPointer<QWidget> text_target_;
    QString text_target_name_;
    // Where the caret was when it left that field. A rebuilt name field starts
    // reading from its beginning, so without this a character picked after a
    // commit would land in front of the name instead of where it was wanted.
    int text_target_caret_ = -1;
    // The field that caret was taken from. It goes null of its own accord when
    // a commit rebuilds the panel and takes the field with it, which is exactly
    // the case the caret has to be put back for.
    QPointer<QWidget> caret_owner_;
    void remember_text_target(QWidget* widget);
    void remember_caret(QWidget* widget);
    // Puts a character on the canvas as a note, for when no field is open.
    bool place_symbol(const QString& character);
    [[nodiscard]] QWidget* text_target();
    // Keeps the picker saying where the next character will land.
    void refresh_symbol_destination();
    std::map<QString, domain::ElementRef> references_;
    std::vector<domain::ElementRef> selection_;
    QString path_;
    bool refreshing_ = false;

    void build_shell();
    void build_actions();
    void choose_tool(Tool tool, bool locked);
    void choose_line_style(LineStyle style);
    void refresh_tool_labels();
    void refresh_icons();
    // A sample drawn for an ordinary row and again for a highlighted one, so it
    // is never drawn in the colour it is standing on. See the definition.
    [[nodiscard]] QIcon two_tone(const std::function<QPixmap(std::optional<QColor>)>& draw) const;
    [[nodiscard]] QIcon notation_icon(Notation notation) const;
    [[nodiscard]] QIcon line_style_icon(LineStyle style) const;
    [[nodiscard]] QWidget* toolbar_widget(QAction* action) const;
    void choose_notation(Notation notation);
    void refresh();
    void refresh_explorer();
    void highlight_explorer();
    void refresh_properties();
    void refresh_validation();
    // Shows what an edit did: refreshes everything, reports a refusal, and
    // chooses whatever the edit made, since making something is almost always
    // the start of working on it. Placing a symbol is the exception, so that
    // picking characters does not keep swapping the properties panel over.
    void show_result(const application::EditResult& result, bool choose_what_was_made = true);
    void selection_changed(const std::vector<domain::ElementRef>& selection);
    void rename_selection();
    // Which of the selection commands apply to what is chosen now. Duplicate
    // and Rename go by how much is selected; Enlarge and Shrink go by what
    // kind it is, since only a symbol has a size of its own to choose.
    void refresh_selection_commands();
    bool confirm_discard();
    bool save(bool choose_path = false);
    void new_project();
    void open_dialog();
    // Hides the panels and gives the whole window to the diagram, or brings
    // back exactly the panels that were showing when it was turned on.
    void set_full_view(bool on);
    void insert_picture_dialog(std::optional<QPointF> at = {});
};

} // namespace erdflow::desktop
