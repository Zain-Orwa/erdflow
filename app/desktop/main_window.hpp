// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "diagram_view.hpp"
#include "export_dialog.hpp"
#include "icons.hpp"
#include "home_page.hpp"
#include "notice.hpp"
#include "schema_view.hpp"
#include "application/project_store.hpp"

#include <QMainWindow>
#include <QPointF>
#include <QIcon>
#include <QPointer>
#include <QTimer>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

class QAction;
class QActionGroup;
class QDockWidget;
class QLabel;
class QStackedWidget;
class QLineEdit;
class QMenu;
class QComboBox;
class QScrollArea;
class QPushButton;
class QToolButton;
class QVBoxLayout;
class QStandardItemModel;
class QTreeView;
class QTreeWidget;

namespace erdflow::desktop
{

    class Ribbon;
    class SearchBar;
    class SymbolPicker;

    class MainWindow final : public QMainWindow
    {
    public:
        MainWindow(application::Editor &editor, application::ProjectStore &store,
                   application::IdGenerator &ids, QWidget *parent = nullptr);
        ~MainWindow() override;
        bool open_path(const QString &path);
        // Applies a theme to the window and its canvas, and remembers it.
        void set_theme(ThemeId id);
        // Which of the two pages is in front. The home screen is where the
        // application starts; the workspace is where the work is. Public because
        // starting a project moves between them, and because a test has no other
        // way to ask which one somebody is looking at.
        void show_home(bool on);
        [[nodiscard]] bool showing_home() const;
        // Shows a theme without choosing it, so one can be judged on the window
        // itself rather than on its name. Leaving the menu puts back the chosen one.
        void preview_theme(ThemeId id);
        // Which icon set the window draws its actions with, and remembers it.
        void set_icon_mode(IconMode mode);
        [[nodiscard]] IconMode icon_mode() const { return icon_mode_; }
        void load_example();
        // Two larger Conceptual diagrams, built with the same commands the
        // workspace already offers. They sit beside the original university
        // example and do not replace it.
        void load_company_database();
        void load_university_database();
        // A new project started from the template: not the example, but the
        // general things a diagram is made of, named for what they are (Zain,
        // 2026-09-26).
        void load_template();
        // Places a picture read from a file, centred on the given canvas point or
        // else in the middle of the view. The file's own bytes are kept when it is
        // a PNG or JPEG of modest size; anything else is re-encoded, scaled down if
        // it is large. False if it could not be read.
        bool insert_picture(const QString &path, std::optional<QPointF> at = {});
        // Writes a picture of the diagram. With no location it asks where the
        // picture should go; with one it writes there and asks nothing, which is
        // how anything that already knows the destination drives it. The options
        // are whatever the export dialog last settled on, so the quick entries
        // and the dialog cannot produce differently sized pictures of one diagram.
        bool export_picture(const PictureOptions &options, const QString &location = {});
        // Writes the project as a listing rather than as a picture: a report, a
        // data dictionary or a spreadsheet of what the diagram says.
        bool export_document(DocumentFormat format, const QString &location = {});
        // Puts a picture of the selection, or of the whole diagram when nothing is
        // selected, on the clipboard as both a PNG and an SVG, so whatever it is
        // pasted into can take whichever it prefers.
        bool copy_picture();
        // Writes a copy of the project itself, losing nothing. Saving keeps working
        // on the file it wrote; this leaves the open project where it is.
        bool export_project_file(const QString &location = {});
        // Brings another project's contents into this one, from a project file or
        // from a picture carrying one. Everything arrives with fresh identities and
        // clear of what is already drawn, and the whole import undoes in one step.
        bool import_project(const QString &path);
        // Narrows the diagram to what is being looked for, bringing what it finds
        // into the middle of the view. Also how anything that already knows what to
        // look for drives the search.
        void search_diagram(const DiagramSearch &search);
        // Raises the schema panel, optionally at full height. Development tooling
        // beside open_search: it is how anything that already knows the panel is
        // wanted drives it, including a screenshot taken with nobody there to
        // click the button.
        void open_schema(bool full = false);
        // Chooses the notation every end is drawn in, on the diagram and on the
        // schema alike. Public so that anything already knowing which notation is
        // wanted can drive it, a screenshot of one of the four included.
        void set_notation(Notation notation) { choose_notation(notation); }
        // Opens the search bar, or closes it and puts the whole diagram back.
        void open_search(const QString &looking_for = {});
        void close_search();
        // Leaves a remark on the given things, asking for the words. With text
        // supplied it asks nothing, which is how anything that already has the
        // words drives it. One remark covers everything it is given at once.
        bool add_comment(std::vector<domain::CommentTarget> targets, const QString &said = {});
        // Pins a remark into the range of text now selected in the named property
        // field, which is how a remark comes to be about one word rather than a
        // whole element. False when nothing is selected there.
        bool comment_on_selected_text(const QString &field_name, const QString &said = {});
        [[nodiscard]] const ExportChoice &export_choice() const { return export_choice_; }
        [[nodiscard]] const application::Editor &editor() const { return editor_; }
        [[nodiscard]] DiagramView *canvas() const { return canvas_; }
        // The schema the diagram would become, once the panel holding it has been
        // raised. Reached the same way as the canvas, since neither carries the
        // Qt object macro that would let it be found by type.
        [[nodiscard]] SchemaView *schema() const { return schema_; }
        // The row of tabs above the tool row: File, Home, Insert, Design, Export,
        // Import, View, Help.
        [[nodiscard]] Ribbon *ribbon() const { return ribbon_; }
        // Opens the symbol gallery, on the named group if one is named. The picker
        // is built the first time it is asked for and kept afterwards, so a search
        // and a chosen group survive being closed and opened again.
        void show_symbols(const QString &group = {});
        // Puts one character into whatever text field was last being written in.
        // With nothing being written in it goes on the diagram instead, as a note
        // carrying that character, since putting a character somewhere is the
        // whole purpose of picking one. False only if the edit itself was refused.
        bool insert_symbol(const QString &character);

    protected:
        void closeEvent(QCloseEvent *event) override;
        void resizeEvent(QResizeEvent *event) override;
        bool eventFilter(QObject *watched, QEvent *event) override;

    private:
        application::IdGenerator &ids_;
        application::Editor &editor_;
        application::ProjectStore &store_;
        DiagramView *canvas_ = nullptr;
        QTreeView *explorer_ = nullptr;
        QTreeView *issues_ = nullptr;
        QStandardItemModel *explorer_model_ = nullptr;
        QStandardItemModel *issue_model_ = nullptr;
        QScrollArea *properties_ = nullptr;
        // The schema the diagram would become, and the panel it rises in. It is
        // derived from the model every time it is shown, and holds nothing.
        SchemaView *schema_ = nullptr;
        QWidget *schema_panel_ = nullptr;
        // The area the diagram and the schema panel share. Watched, because it is
        // resized by the side panels as well as by the window.
        QWidget *stage_ = nullptr;
        QScrollArea *schema_scroll_ = nullptr;
        QLabel *schema_state_ = nullptr;
        // The schema's own settings, as groups of choices in its two menus
        // rather than as pickers in a row of their own.
        QActionGroup *schema_names_ = nullptr;
        QActionGroup *schema_lines_ = nullptr;
        QActionGroup *schema_sizing_ = nullptr;
        QPointer<class TypePicker> type_picker_;
        QPointer<class SizePicker> size_picker_;
        // What the header shows while the schema has the whole window. The
        // drawing tools go away with the diagram, so the few things still worth
        // reaching for come out here instead of being lost with them.
        QWidget *schema_header_tools_ = nullptr;
        QLineEdit *schema_search_ = nullptr;
        QToolButton *schema_theme_ = nullptr;
        std::vector<QPointer<QWidget>> hidden_chrome_;
        QActionGroup *schema_notation_ = nullptr;
        std::vector<QPushButton *> schema_chips_;
        QWidget *shared_names_ = nullptr;
        QWidget *shared_names_body_ = nullptr;
        QPushButton *shared_names_head_ = nullptr;
        bool shared_names_open_ = false;
        bool schema_open_ = false;
        // How much of the stage the panel takes, and what it was when a resize
        // began. Remembered so it opens again at the height it was left at.
        double schema_share_ = 0.62;
        double schema_share_at_grab_ = 0.62;
        // Raises or lowers the schema over the lower part of the diagram.
        void show_schema(bool shown);
        // The whole window for the schema, and back again.
        void set_schema_full(bool full);
        // What the header offers the schema: undo and redo while it is open,
        // and its search and the theme too while it has the whole window.
        void place_schema_header_tools();
        bool schema_full_ = false;
        bool laying_out_schema_ = false;
        // The diagram's own furniture, put away while the schema has the window.
        std::vector<QPointer<QWidget>> hidden_for_schema_;
        bool schema_took_full_view_ = false;
        double schema_share_before_full_ = 0.62;
        void lay_out_schema();
        // Arrange and Appearance in a schema's header give up their words, keeping
        // their icons, where the header is too narrow for every word (Zain,
        // 2026-10-01, when Select joined the row), and take them back where it is
        // wide enough.
        std::vector<QToolButton *> schema_header_words_;
        QPointer<QWidget> schema_header_;
        void fit_schema_header_words();
        void refresh_schema();
        void refresh_schema_state();
        // The strip under the schema that gathers columns sharing a name, so a
        // type can be given to all of them at once.
        void refresh_shared_names();
        [[nodiscard]] std::vector<QWidget *> chrome_for_drawing() const;
        // What can be done to the table or column that was asked about, and doing
        // it. Adding and removing a column may or may not mean the same change to
        // the diagram, and ADR-010 says the user is asked before the diagram is
        // touched and never after.
        void offer_schema_actions(const SchemaView::Spot &spot);
        void populate_schema_key_actions(QMenu &menu, const SchemaView::Spot &spot);
        // Classified, as the table's Properties list offers them: NULL and NOT
        // NULL as two opposite choices, then UNIQUE and IDENTITY, each saying
        // what it means. The canvas's own constraints menu is not classified.
        void populate_schema_rules(QMenu &menu, const SchemaView::Constrained &hit, bool classified = false);
        void pick_schema_column_type(const SchemaColumnRef &handle, QPoint at, bool size, bool compact = false);
        // One of a row's constraint marks was pressed. Which command that is
        // depends on what the column is made of, and this is the only place that
        // knows: an ordinary column carries its own rules, a foreign key's
        // nullability belongs to the relationship behind it, and a column the
        // conversion invented has nothing to carry them at all.
        void toggle_schema_constraint(const SchemaView::Constrained &hit,
                                      SchemaView::Constraint which);
        // The list of what can be said about one column's constraints, opened
        // where the cell is. Several of them apply at once, so it is a list of
        // things to tick rather than a choice between them.
        void offer_schema_rules(const SchemaView::Constrained &hit, QPoint at);
        // Which side of a relationship carries a foreign key, given the side it
        // points at. The preview remembers the target, because that is what
        // decides the key's nullability; whether the key is unique belongs to the
        // other side, the one whose rows hold it.
        [[nodiscard]] std::optional<domain::ParticipantId> carrying_side(
            const domain::PreviewColumn &column) const;
        // A conversion question answered on the schema. Each is an ordinary edit,
        // so each undoes, and the schema and the diagram both follow it.
        void answer_decision(const domain::OpenDecision &decision, std::size_t choice);
        // Ask for a column's type where the column is, rather than sending the
        // reader to the Properties panel for something the schema is already
        // showing them a blank for.
        void ask_column_type(const domain::PreviewColumn &column, QPoint at);
        void ask_column_size(const domain::PreviewColumn &column, QPoint at);
        void rename_from_schema(const SchemaView::Spot &spot, const QString &typed);
        void add_schema_column(domain::ElementRef table, bool schema_only = false);
        // A schema drawn by hand (Zain, 2026-09-27). A table made where it was
        // asked for and opened for its name; a foreign key drawn from one row to
        // another; the schema turned into the diagram it would have come from.
        void add_schema_table(std::optional<QPointF> at);
        void link_schema_rows(const SchemaView::Linked &link);
        void convert_schema_to_diagram();
        // What the window offers follows whether the project starts from its
        // schema: while it does, Relational Design has the whole window and
        // cannot be closed onto a diagram that is not there, and it offers Add
        // table and Convert; once converted, the diagram comes in front with the
        // schema open beneath it. Asked on every refresh, acting only when that
        // changes.
        void follow_schema_first();
        bool schema_first_ = false;
        domain::ProjectId schema_first_project_;
        QToolButton *schema_add_table_ = nullptr;
        QPushButton *schema_convert_ = nullptr;
        QWidget *schema_narrowing_ = nullptr;
        // The schema's tools in the header while the project starts from its
        // schema (Zain, 2026-09-27): Table, Connect, Arrange and Appearance, up
        // where the header already says Relational Design, in place of the bar
        // on the schema that held them.
        QWidget *schema_top_tools_ = nullptr;
        // Connect on the schema, and whether a double click locked it so it
        // stays in hand for several foreign keys, as a tool on the diagram does.
        QAction *schema_connect_ = nullptr;
        bool schema_connect_locked_ = false;
        void choose_schema_connect(bool on, bool locked);
        // The schema's tool in hand (Zain, 2026-10-01): Select, Table or Connect,
        // one at a time, as on the diagram's tool row. Table and Connect are put
        // down after one use unless locked. It is the interface's state only and
        // never reaches the project.
        enum class SchemaTool
        {
            Select,
            Table,
            Connect
        };
        SchemaTool schema_tool_ = SchemaTool::Select;
        bool schema_tool_locked_ = false;
        QAction *schema_select_ = nullptr;
        QAction *schema_table_ = nullptr;
        void choose_schema_tool(SchemaTool tool, bool locked);
        // A press elsewhere in the window puts the schema's tool down, as one
        // puts the diagram's down.
        void pressed_outside_schema(QWidget *pressed);
        // The header of a schema drawn by hand, put on or taken off.
        void wear_schema_first_header(bool first);
        // The header laid over the whole window, above the Explorer and
        // Properties, or put back at the top of the workspace between them.
        void lay_header_over_panels(bool over);
        // The Schema workspace's own Explorer and Properties (Zain, 2026-09-27,
        // Stage 1), held in the same two docks the diagram's are, which carry the
        // schema's whenever the schema is what is being worked on and the
        // diagram's whenever the diagram is (Zain, 2026-09-29): always, in a
        // project that starts from its schema; in a diagram's, while the schema
        // has the whole window, and while it shares the stage, whichever of the
        // two was raised or pressed last. The docks, their places, widths,
        // closing and the View menu are shared; what is in them is not. Both only
        // read what is chosen on the schema, which is kept by the schema view
        // alone.
        // The header of a schema drawn by hand runs the whole width of the
        // window, over its Explorer and Properties, as the diagram's tool row runs
        // over the diagram's: held in a dock along the top while the project
        // starts from its schema, and back in the workspace otherwise.
        QDockWidget *header_dock_ = nullptr;
        QVBoxLayout *workspace_layout_ = nullptr;
        QTreeView *schema_explorer_ = nullptr;
        QStandardItemModel *schema_explorer_model_ = nullptr;
        QScrollArea *schema_properties_ = nullptr;
        void wear_schema_panels(bool schema);
        bool wearing_schema_panels_ = false;
        // Which half of the stage was pressed, while the schema shares it with
        // the diagram, so the docks can follow it.
        void follow_pressed_half(QWidget *pressed);
        void show_schema_panels();
        void refresh_schema_explorer();
        void refresh_schema_properties();
        // The Schema Explorer's rows and the schema's one selection kept in step
        // (Zain, 2026-09-29, Stage 2): the rows for what is chosen lit, opened
        // onto where asked; and a row chosen there chosen on the schema, and
        // brought into view on the canvas.
        void highlight_schema_explorer(bool reveal);
        void schema_explorer_chose();
        void reveal_on_schema(const SchemaSelection &chosen);
        // Which groups and tables have been opened or folded by hand, by what
        // they stand for; everything else opens as it does the first time.
        std::map<QString, bool> schema_folds_;
        // While the tree is being made or lit from the schema, it is not asking.
        bool schema_explorer_quiet_ = false;
        bool choosing_from_schema_explorer_ = false;
        // While the schema's Properties is being made afresh (Stage 4): a field
        // put away with it is not finishing an edit, so what it held is not
        // written anywhere.
        bool schema_properties_rebuilding_ = false;
        QAction *schema_search_mark_ = nullptr;
        // What a schema drawn by hand is converted from: where each table is on
        // the schema now, and how big each kind of element is made. Asked by
        // Convert and by the Conceptual preview alike, so the preview shows
        // exactly the diagram Convert would draw.
        [[nodiscard]] std::pair<std::map<domain::RelationId, domain::Point>, domain::DiagramSizes>
        schema_conversion_inputs() const;
        // The Conceptual Design a schema drawn by hand would become (Zain,
        // 2026-09-27): the schema raised over a diagram, turned the other way up.
        // While a project starts from its schema, the schema is the main surface
        // and this rises over the lower part of it. It is drawn on a canvas of its
        // own, from a copy of the project worked out again whenever the schema
        // changes, so nothing is converted and nothing is written.
        void show_conceptual(bool shown);
        void lay_out_conceptual();
        void refresh_conceptual();
        std::unique_ptr<application::Editor> conceptual_editor_;
        DiagramView *conceptual_ = nullptr;
        QWidget *conceptual_panel_ = nullptr;
        QLabel *conceptual_state_ = nullptr;
        bool conceptual_open_ = false;
        // What was last drawn there -- which project, at which revision, with its
        // tables where -- so it is only worked out again when one of them changes.
        struct ConceptualDrawn
        {
            domain::ProjectId project;
            std::uint64_t revision = 0;
            std::map<domain::RelationId, domain::Point> places;
            bool operator==(const ConceptualDrawn &) const = default;
        };
        std::optional<ConceptualDrawn> conceptual_drawn_;
        double conceptual_share_ = 0.62;
        double conceptual_share_at_grab_ = 0.62;
        void remove_schema_column(domain::ElementRef table, const domain::PreviewColumn &column);
        QDockWidget *validation_dock_ = nullptr;
        // The History (Zain, 2026-09-26): every step Undo can take back, in the
        // order it was made, each in words, and any of them a place to go back
        // or forward to. A panel of its own, closed until it is opened from View.
        // The way back to Home, always there in the workspace's header.
        QPushButton *back_to_home_ = nullptr;
        QDockWidget *history_dock_ = nullptr;
        QTreeWidget *history_list_ = nullptr;
        // What the panel last showed, so it is only rebuilt when there is more.
        std::uint64_t history_shown_ = 0;
        void build_history();
        void refresh_history(bool again = false);
        QLabel *document_label_ = nullptr;
        QLabel *count_label_ = nullptr;
        QLabel *zoom_label_ = nullptr;
        QLabel *readiness_label_ = nullptr;
        // A remark laid over the work when something goes wrong under the hand.
        // The status bar keeps the record; this is for the moment itself, when a
        // line along the bottom of the window is too far from where the person is
        // looking to be read at all.
        Notice *notice_ = nullptr;
        // The screen the application opens on, and the workspace behind it. Both
        // exist from the start; which one is in front is all that changes, so
        // nothing has to be built or torn down when somebody moves between them.
        HomePage *home_ = nullptr;
        QStackedWidget *pages_ = nullptr;
        // The panels that were open when the home screen came forward, so exactly
        // those come back and no others.
        std::vector<QPointer<QDockWidget>> hidden_for_home_;
        // Whichever ribbon rows were actually showing when Home came forward. Home
        // has its own slim bar in their place; the native menu bar and the status
        // line stay, since Home is never shown without its menus (ADR-022 section
        // 9.14). Keeping this exact set means the row that was in front, not an
        // assumed Home row, comes back afterwards.
        std::vector<QPointer<QWidget>> hidden_chrome_for_home_;
        bool home_chrome_hidden_ = false;
        // Whether a workspace has been in front yet, so Home can offer the way
        // back into it (Zain, 2026-09-27). A fresh start has none to return to.
        bool workspace_seen_ = false;
        // Projects opened, saved or created lately, newest first, read from and
        // kept in the settings. The Home screen's Recent row and the Home menu
        // both open this one menu.
        QMenu *recent_menu_ = nullptr;
        // What the Home screen's Settings opens: the choices that belong to the
        // whole application rather than to a project -- theme, icons, notation.
        // The same menus the View menu holds, not copies of them.
        QMenu *settings_menu_ = nullptr;
        QAction *undo_ = nullptr;
        QAction *redo_ = nullptr;
        QAction *duplicate_ = nullptr;
        QAction *rename_ = nullptr;
        // Size, for the one element that has one to choose: a symbol. They are
        // enabled only while everything selected is a symbol, so the pair never
        // offers to act on half of a selection.
        QAction *enlarge_ = nullptr;
        QAction *shrink_ = nullptr;
        std::map<Tool, QAction *> tool_actions_;
        std::map<Notation, QAction *> notation_actions_;
        std::map<ThemeId, QAction *> theme_actions_;
        std::map<QAction *, Glyph> action_glyphs_;
        // What the window is currently showing, and what the user actually chose.
        // They differ only while a theme is being previewed under the pointer.
        ThemeId theme_ = ThemeId::OfficeLight;
        ThemeId committed_theme_ = ThemeId::OfficeLight;
        // Whether the window has worn a theme yet. Without it the guard in
        // apply_appearance would mistake the very first application for a repeat,
        // since the member above already names the theme the window starts on.
        bool appearance_applied_ = false;
        // A theme hovered but not yet shown, and the wait before it is. Wearing a
        // theme costs the whole window, so a pointer travelling down the menu
        // spends it on every entry it crosses rather than on the one it stops at.
        // The wait collapses the crossings into the resting place.
        ThemeId pending_preview_ = ThemeId::OfficeLight;
        QTimer *theme_preview_timer_ = nullptr;
        void preview_theme_soon(ThemeId id);
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
        std::map<IconMode, QAction *> icon_mode_actions_;
        // Generalization and specialization share one toolbar entry; this is the
        // mode its main button uses, chosen from its dropdown.
        Tool isa_mode_ = Tool::Specialization;
        QAction *isa_action_ = nullptr;
        std::map<LineStyle, QAction *> line_actions_;
        QComboBox *notation_box_ = nullptr;
        QWidget *canvas_controls_ = nullptr;
        QAction *notation_action_ = nullptr;
        QAction *notation_label_action_ = nullptr;
        QAction *notation_separator_ = nullptr;
        bool fitting_ = false;
        QToolButton *theme_button_ = nullptr;
        QAction *full_view_ = nullptr;
        QAction *check_ = nullptr;
        // Opens the search. It is on the tool row and in the Edit menu, so it is
        // made once and shown in both.
        QAction *find_action_ = nullptr;
        QToolButton *search_button_ = nullptr;
        // Keeps a wheel from changing whatever the pointer happens to be over.
        QObject *wheel_guard_ = nullptr;
        std::map<domain::BackgroundStyle, QAction *> background_actions_;
        // What the export dialog last settled on, kept for the session so a
        // second export of the same work takes one press rather than four.
        ExportChoice export_choice_;
        // Everything under Export, kept so they can be turned off together while
        // there is nothing drawn to make anything of.
        std::vector<QAction *> export_actions_;
        void export_dialog();
        // Asks which file to import, looking among projects or among pictures.
        void import_dialog(bool pictures);
        std::vector<QAction *> import_actions_;
        // Asks where an export should go, suggesting a name beside the project.
        // Empty when the person cancelled or declined to replace a file.
        [[nodiscard]] QString export_location(const QString &suffix, const QString &label);
        // The project's own bytes, for a picture asked to carry them. Empty with a
        // reason when the project is too large to travel inside a picture, which is
        // said plainly rather than failing the export.
        [[nodiscard]] QByteArray project_payload(QString &note);
        // Reads a project from a file that may be a project or a picture carrying
        // one, so the two open the same way.
        [[nodiscard]] application::LoadResult read_project(const QString &path);
        void refresh_export_actions();
        // The remarks on whatever is selected, listed in the properties panel so
        // each can be read, put away, brought back, reworded or deleted.
        void build_comment_section(QWidget *panel, QVBoxLayout *layout);
        // Asks for the words of a remark, starting from whatever it says now.
        // Empty when the person cancelled or wrote nothing.
        [[nodiscard]] QString ask_for_comment(const QString &said, const QString &about);
        // Gives a text field a Comment entry beneath its usual cut-and-paste one.
        void offer_text_comment(QWidget *field);
        // Keeps the Background menu showing the paper the document actually has.
        void refresh_background_menu();
        void choose_background(domain::BackgroundStyle style);
        void choose_background_image();
        // Keeps the Check model button saying what pressing it will do, whichever
        // way the findings were opened or closed.
        void refresh_check_action();
        // The panels put away by full view, so exactly those come back. Model
        // checks is often closed already, and full view must not open it.
        std::vector<QDockWidget *> hidden_panels_;
        Ribbon *ribbon_ = nullptr;
        SearchBar *search_bar_ = nullptr;
        SymbolPicker *symbols_ = nullptr;
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
        void remember_text_target(QWidget *widget);
        void remember_caret(QWidget *widget);
        // Puts a character on the canvas as a note, for when no field is open.
        bool place_symbol(const QString &character);
        [[nodiscard]] QWidget *text_target();
        // Keeps the picker saying where the next character will land.
        void refresh_symbol_destination();
        std::map<QString, domain::ElementRef> references_;
        std::vector<domain::ElementRef> selection_;
        QString path_;
        bool refreshing_ = false;
        // Whether the Properties panel's "For the schema" section is open. It is
        // a view preference rather than anything the project holds, so it is read
        // from the application's settings once and written back when the user
        // folds the section; the panel is rebuilt on every selection and reads
        // this rather than the settings.
        bool schema_section_open_ = false;

        void build_shell();
        void build_actions();
        void choose_tool(Tool tool, bool locked);
        // Back to Select after a click outside the diagram.
        void pressed_outside_canvas(QWidget *pressed);
        void choose_line_style(LineStyle style);
        void refresh_tool_labels();
        void refresh_icons();
        // A sample drawn for an ordinary row and again for a highlighted one, so it
        // is never drawn in the colour it is standing on. See the definition.
        [[nodiscard]] QIcon two_tone(const std::function<QPixmap(std::optional<QColor>)> &draw) const;
        [[nodiscard]] QIcon notation_icon(Notation notation) const;
        [[nodiscard]] QIcon line_style_icon(LineStyle style) const;
        [[nodiscard]] QWidget *toolbar_widget(QAction *action) const;
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
        void show_result(const application::EditResult &result, bool choose_what_was_made = true);
        void selection_changed(const std::vector<domain::ElementRef> &selection);
        void rename_selection();
        // Which of the selection commands apply to what is chosen now. Duplicate
        // and Rename go by how much is selected; Enlarge and Shrink go by what
        // kind it is, since only a symbol has a size of its own to choose.
        void refresh_selection_commands();
        bool confirm_discard();
        bool save(bool choose_path = false);
        // Reset the editor only after the current work has actually been
        // discarded or saved. The Home routes use the result so cancelling never
        // navigates away or renames the project that was already open.
        bool begin_new_project();
        // The same, for a project that starts from its schema (Zain, 2026-09-27):
        // Relational Design fills the window, with no diagram behind it until the
        // schema is converted into one.
        bool begin_new_schema_project();
        void new_project();
        bool open_dialog();
        // Says where each of the Home screen's rows and links goes. Done once every
        // menu they open exists.
        void wire_home();
        // Which workspace the window is showing in front: the badge says its name,
        // and anything only the other one can take is put away (ADR-022 9.12).
        void set_workspace_in_front(bool relational);
        void remember_recent(const QString &path);
        void refresh_recent_menu();
        void show_quick_guide();
        // Hides the panels and gives the whole window to the diagram, or brings
        // back exactly the panels that were showing when it was turned on.
        void set_full_view(bool on);
        void insert_picture_dialog(std::optional<QPointF> at = {});
    };

} // namespace erdflow::desktop
