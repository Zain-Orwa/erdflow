// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "domain/schema_preview.hpp"
#include "diagram_view.hpp"

#include "icons.hpp"
#include "schema_selection.hpp"

#include <QPainterPath>
#include <QPixmap>
#include <QWidget>

class QLineEdit;
class QAbstractScrollArea;
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <variant>
#include <vector>

namespace erdflow::application { class Editor; struct EditResult; }

namespace erdflow::desktop {

struct Theme;

// Which tables the schema is being asked about, by where they came from. The
// same narrowing the diagram offers, carried across rather than reinvented:
// work done on one view of the model should not have to be done again on the
// other. Everything else is faded rather than removed, so the schema keeps its
// shape and no line is left hanging from a table that is not there.
enum class SchemaShowing { Everything, FromEntities, FromRelationships, FromAttributes };

// How lines find their way between the tables.
enum class SchemaRouting { AroundTables, Straight };

// A column's type as its Type cell writes it, int or varchar(255), and "?"
// while nobody has said. For whatever else describes a column, so the two
// never put it differently.
[[nodiscard]] QString written_type(const domain::PreviewColumn& column);
// The type's own name with nothing about its size, as the Type cell writes it
// where the size is a cell of its own: varchar beside 255.
[[nodiscard]] QString written_type_name(const domain::PreviewColumn& column);

// The schema the diagram would become, drawn beside the diagram itself rather
// than in another window. It is a picture of the model read as tables: nothing
// here is stored, and everything it shows is worked out afresh from the project
// each time it is drawn.
//
// The tables can be moved. Where a table has not been moved it is placed by an
// arrangement that packs the columns evenly, so a schema is legible before
// anybody has touched it.
//
// The lines can be moved too, and every part of one answers to the hand.
// Dragging a line anywhere along its length puts a corner there which it is
// then routed through. Dragging either of its ends moves where it meets its
// table, anywhere around that table's outline; pulling an end off the table
// altogether leaves it wherever it was let go, because an end that springs
// back to the edge cannot be aimed either. An end left off its table is a loose
// end: the line still draws the foreign key it always did, and the schema says
// how many are loose rather than quietly tidying them away.
// Double-clicking a line gives the whole of it back to the router.
class SchemaView : public QWidget {
public:
    explicit SchemaView(application::Editor& editor, QWidget* parent = nullptr);

    // Read the project again. Called whenever the model changes, because the
    // preview is derived and never stored.
    void refresh();
    void set_theme(const Theme& theme);
    // Which icon set the key beside a primary key is taken from. The artwork is
    // a theme of its own, so the mark follows whatever set is on rather than
    // being drawn into the schema.
    void set_icon_mode(IconMode mode);
    // How the ends of a line are drawn, shared with the diagram's own notation.
    void set_notation(Notation notation);
    // How a line gets from one table to the other: found around the tables, or
    // taken straight there in right angles and allowed to cross them. Around
    // is right for a schema being read; straight is right for one being laid
    // out, where the router's detours hide what is actually joined to what.
    void set_routing(SchemaRouting routing);
    // Whether a line shaped by hand gives way when a table is moved onto it.
    // Off by default: a shape somebody made is theirs, and a line that undid
    // itself because a table drifted over it would be undoing their work for
    // a reason they never asked about. On, a line that a move has left
    // crossing a table is handed back to the router, which takes it around.
    void set_lines_give_way(bool give_way);
    // Whether a table can be pulled about by its edges. Every edge and every
    // corner answers: the side that is pulled moves and the opposite one stays
    // where it was, so a table is widened, narrowed, raised or lowered from
    // whichever side the hand reaches for. Room given to a table's height is
    // shared out between its rows, so a taller table is a roomier one rather
    // than one with a gap under its last row.
    void set_tables_resizable(bool resizable);
    [[nodiscard]] bool tables_resizable() const { return tables_resizable_; }
    // Compact schema (Zain, 2026-09-27): every table shows its key marks and its
    // columns' names and nothing more -- no heading row, no Type, no
    // Constraints or configuration footer -- each table as wide as its names.
    // Off unless chosen. While
    // it is on, sizes given by hand are set aside and tables are not pulled
    // about, so a narrow size given here never folds the columns of the full
    // view; turned off, every table is exactly as it was.
    void set_names_only(bool on);
    [[nodiscard]] bool names_only() const { return names_only_; }
    [[nodiscard]] bool lines_give_way() const { return lines_give_way_; }
    [[nodiscard]] SchemaRouting routing() const { return routing_; }
    [[nodiscard]] Notation notation() const { return notation_; }
    // Put every table back to the automatic arrangement, and every line back to
    // the automatic route.
    void tidy();
    // Give every line back to the router, leaving the tables where they are.
    // Tidy throws away both; this throws away only the lines, which is what
    // somebody wants after shaping a few and changing their mind.
    void release_lines();
    void set_showing(SchemaShowing showing);
    // Pick out the tables and columns whose names contain this. A search of
    // the schema, not of the diagram: the two are looked at separately and
    // the names are not always the same, since a table is named for the many
    // rows it holds and a bridge is named for the relationship behind it.
    void set_looking_for(const QString& looking_for);
    [[nodiscard]] const QString& looking_for() const { return looking_for_; }
    [[nodiscard]] SchemaShowing showing() const { return showing_; }
    // Which table is being asked about. Selecting one picks out what it is
    // joined to and fades the rest, because the question a reader has in front
    // of a schema is almost always "what does this one touch".
    void select(std::optional<domain::ElementRef> table);
    // Add one to what is marked, or take it out again. What holding a
    // modifier and pressing a table does.
    void toggle_mark(const domain::ElementRef& table);
    // Mark every table, as Select All does on the diagram. What Ctrl+A does
    // while the schema has the keyboard.
    void select_all();
    // The one table being asked about, where exactly one is. Several tables
    // marked together is a different question -- "these ones" rather than
    // "this one and what it is joined to" -- so it answers nothing here.
    [[nodiscard]] std::optional<domain::ElementRef> selected() const {
        return selected_.size() == 1 ? std::optional{selected_.front()} : std::nullopt;
    }
    // Everything marked, in the order it was marked, so that whatever acts on
    // a selection can open on what the first of them already wears.
    [[nodiscard]] const std::vector<domain::ElementRef>& selection() const { return selected_; }
    // Which table that is, by its place in the preview. The lines belonging
    // to it are matched on this, so it is part of what the selection means
    // rather than an internal convenience.
    [[nodiscard]] std::optional<std::size_t> selected_table() const;
    // What is chosen on the schema, in the schema's own terms (Zain,
    // 2026-09-27, Stage 1): kept here and nowhere else -- the tables marked
    // above, and the column or line picked on them -- and read by everything
    // that shows it. Told through chose, as the marks are. Choosing is how the
    // schema is being looked at, so it never reaches the Editor.
    [[nodiscard]] SchemaSelection selection_now() const;
    // Choose something from elsewhere, and show it here as if it had been
    // pressed: what the Explorer will do.
    void choose(const SchemaSelection& wanted);
    // The handle for the column drawn at a table and row, and where the
    // column a handle names is drawn now, if it still is.
    [[nodiscard]] std::optional<SchemaColumnRef> column_ref(std::size_t table, std::size_t row) const;
    [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> locate(const SchemaColumnRef& column) const;
    // Told whenever the selection changes, so whatever is showing the state of
    // the schema can follow it.
    std::function<void()> chose;
    // A question answered on the schema: which one, and which answer. What to
    // do about it is the Editor's, not the view's.
    std::function<void(const domain::OpenDecision&, std::size_t choice)> decided;
    // A column's type cell was pressed: which column, and where on screen to
    // put whatever asks for the answer. Only columns that can carry an answer
    // ever ask -- a key the conversion invented takes its type from what it
    // points at, and there is nothing to choose.
    std::function<void(const domain::PreviewColumn&, QPoint at)> asked_type;
    // A column's size cell was pressed. Only types that are measured have one.
    std::function<void(const domain::PreviewColumn&, QPoint at)> asked_size;

    // Which cell is being answered at this moment. While something is open
    // over it waiting for the answer, the cell is drawn as the empty slot it
    // has become rather than as the question it was: the question has been
    // asked, and what is on screen should say so.
    using Answering = std::variant<domain::AttributeId, domain::SchemaColumnId>;
    void set_answering(std::optional<Answering> which, bool size);
    [[nodiscard]] const std::optional<Answering>& answering() const { return answering_; }
    // Round every table's position to the grid, once. Moving is never snapped:
    // a table follows the pointer exactly, and is only aligned when asked.
    void align();
    [[nodiscard]] const domain::SchemaPreview& preview() const { return preview_; }
    // Where every line runs, as the corners it is drawn through. What a schema
    // looks like is otherwise only knowable by reading its pixels, and a line
    // that can be moved by hand needs a shape that can be checked.
    [[nodiscard]] std::vector<std::vector<QPointF>> line_shapes() const;
    // Where each table has been put, in the order the preview lists them. The
    // companion to line_shapes: between them they are the whole of what the
    // schema looks like, which is otherwise knowable only by reading pixels.
    [[nodiscard]] std::vector<QRectF> table_boxes() const;
    // How wide a table is drawn with every column shown, before a hand has
    // said otherwise. The width is measured from the lettering, which each
    // platform draws at its own size, so a layout made in advance asks here
    // how much room to leave (the Relational examples, 2026-10-06).
    [[nodiscard]] double full_width(const domain::PreviewTable& table) const;
    // Where each table's rows were drawn, table by table. A table pulled
    // taller shares that room out between its rows, so how deep a row is
    // drawn is part of what the schema looks like rather than a constant.
    [[nodiscard]] std::vector<std::vector<QRectF>> row_boxes() const;
    // What a row was divided into, which is the rest of what it looks like.
    // Where the type and its size were drawn, and the constraints beyond
    // them. Given out rather than left to be guessed at from the table's
    // edge: the constraints take room at the right, so an offset from the
    // edge stopped meaning what it used to.
    struct Cell {
        QRectF type;
        QRectF size;
        // What the column enforces, all of it in one cell. Present on every
        // column that is really a column, including the keys the conversion
        // invented, and empty only where the row is too narrow to hold it.
        QRectF rules;
    };
    [[nodiscard]] std::vector<std::vector<Cell>> cell_boxes() const;
    // What a row's Constraints cell writes, in the order it writes it: PK, FK,
    // NULL or NOT NULL, UNIQUE, IDENTITY. Empty for a row with no such cell.
    [[nodiscard]] QString constraints_said(std::size_t table, std::size_t row) const;
    // How many lines have been bent by hand rather than left to the router.
    [[nodiscard]] std::size_t shaped_lines() const;
    // How many line ends have been pulled off the table they belong to. They
    // are not an error and nothing is refused for them; they are counted so
    // that the schema can say a connection has been left hanging.
    [[nodiscard]] std::size_t loose_ends() const;
    // How many times every line has been routed since the view was made.
    // Routing is the costliest work the schema does and a drag does it on
    // every movement of the pointer, so how often it happens is worth being
    // able to ask: one movement is one routing, never two (2026-10-06).
    [[nodiscard]] std::size_t routings() const { return routings_; }
    // How many times a table's lettering has been measured since the view was
    // made. A drag moves tables without changing a word of them, so it
    // measures nothing; a table is measured again only once the schema, the
    // font or the screen has changed (2026-10-06).
    [[nodiscard]] std::size_t measurings() const { return measurings_; }
    // Which tables and which lines the last paint drew: tables by their place
    // in the preview, lines by their place among line_shapes. Only what can
    // reach the part of the schema being painted is drawn (2026-10-06), so this
    // is how it can be seen that the rest of a large schema was left alone and
    // that nothing on view was.
    struct Painted {
        std::vector<std::size_t> tables;
        std::vector<std::size_t> lines;
    };
    [[nodiscard]] const Painted& last_painted() const { return painted_; }
    // Called whenever a line's shape changes, so whatever reports the state of
    // the schema can say that an end has been left hanging. A shape is not an
    // edit -- nothing here reaches the model -- so this is not the Editor's
    // business and does not go through it.
    std::function<void()> shaped;
    // An arrangement the hand has finished with, handed on to whatever owns
    // the Editor: the view does not decide what becomes of an edit's result.
    std::function<void(const application::EditResult&)> arranged;
    // What is wrong with where an end has just been put, in words, and empty
    // where nothing is. Reported rather than prevented: the end stays where the
    // hand left it, because a line that sprang back would be arguing with the
    // person drawing it.
    // Something is wrong with what the hand just did, and where on the screen
    // it did it. The place travels with the words because a warning about a
    // connection belongs where the connection was attempted: that is where the
    // pointer is, and so where the person is looking. In screen coordinates,
    // since whoever shows it has a different idea of the origin.
    std::function<void(const QString& warning, QPoint at)> warned;

    // Where the pointer asked what can be done here: which table, and which of
    // its columns where it was on one. The view finds the place; what may be
    // done with it is not its business, because that means editing the model
    // and the model is the Editor's.
    struct Spot {
        std::size_t table = 0;
        std::optional<std::size_t> column;
        QPoint at;   // where a menu should open, in screen coordinates
    };
    std::function<void(const Spot&)> asked;

    // Which of the three constraints a row carries. Nullability is not a flag
    // like the other two: it is two-valued and always says which way it went,
    // because a column that may be empty and a column nobody has decided about
    // generate different SQL and must not look alike.
    enum class Constraint { Nullability, Unique, AutoIncrement };
    // Which row's constraints were asked about. They share one column and are
    // chosen from a list rather than pressed one by one, because between them
    // they are the single question of what the table enforces -- and because
    // what the column says is what the generated SQL will say, which a list
    // of words can be and a row of switches cannot.
    //
    // What any of them means is the Editor's business: on an ordinary column
    // it is the attribute's rule, and on a foreign key the nullability is the
    // relationship's participation, so choosing it reaches the diagram.
    struct Constrained {
        std::size_t table = 0;
        std::size_t row = 0;
    };
    std::function<void(const Constrained&, QPoint at)> rules_asked;
    // A name was typed over a table's or a column's. The view collects the
    // text; what that means -- renaming the entity a table came from, the
    // attribute behind a column, or the key the conversion invented -- is the
    // Editor's business and is decided by whoever owns it.
    std::function<void(const Spot&, const QString& typed)> renamed;
    // A table was asked for another column. Which table; what happens next --
    // asking for the name, and whether the diagram gains an attribute too --
    // belongs to whoever owns the Editor.
    std::function<void(std::size_t table)> add_column;
    // Open the row that was just made for typing, found by the attribute behind
    // it. Adding a column and naming it are one act to the user, so the name is
    // waiting to be typed the moment the row appears.
    void open_column_for(domain::AttributeId attribute);
    // Open the name of a table, or of one of its columns, for typing. Public so
    // that a menu can offer renaming as well as a double click.
    void begin_rename(std::size_t table, std::optional<std::size_t> column);
    // The same, for a column the schema holds on its own, and for a table,
    // found by what they are rather than where they are drawn: a table or a
    // column made a moment ago is named where it has just appeared.
    void open_column_for(domain::SchemaColumnId column);
    void open_table_for(const domain::ElementRef& table);

    // A schema drawn by hand (Zain, 2026-09-27), where a project starts from
    // its schema and there is no diagram behind it. A foreign key is drawn by
    // pressing a primary key's key gutter -- where PK and FK are written --
    // and letting go on the table that refers to it, or on the column there
    // that is to hold it (Zain, 2026-10-01: the row a connection starts on is
    // the key being referenced). The view says which row was taken to which
    // table, and which row there if it was let go on one; what that makes is
    // decided elsewhere, asked about first, and anything wrong with it is said
    // where the hand let go.
    struct Linked {
        std::size_t from_table = 0;
        std::size_t from_row = 0;
        std::size_t to_table = 0;
        std::optional<std::size_t> to_row;
        QPoint at;   // where the hand let go, in screen coordinates
    };
    std::function<void(const Linked&)> linked;
    // Where a foreign key being drawn would land if it were let go now, and
    // whether letting go there would go on -- straight away, or after asking
    // -- or be turned away, by the very plan the connection follows when it is
    // let go (plan_connection). The row under the pointer, or the table where
    // the pointer is over the rest of it. Nothing while no line is being
    // drawn, before it has left the row it started on, or over the empty
    // schema. Shown while the line is drawn and nowhere else: it is not a
    // selection and changes nothing. Below, where it is the strip offered
    // under the table while a line is drawn, which stands for the table
    // itself and is never a column.
    struct LinkTarget {
        std::size_t table = 0;
        std::optional<std::size_t> row;
        bool takes = false;
        bool below = false;
    };
    [[nodiscard]] std::optional<LinkTarget> link_target() const;
    // Connect in hand (Zain, 2026-09-27): the Connect tool in the top bar,
    // as the Conceptual canvas has one. While it is on, a press anywhere on a
    // row draws a connection from it, not only on its key gutter, and it is
    // let go on a table or a row there as above. Escape puts it down, and
    // says so.
    void set_connecting(bool on);
    [[nodiscard]] bool connecting() const { return connecting_; }
    std::function<void(bool)> connecting_changed;
    // Table in hand (Zain, 2026-10-01): the Table tool in the header, placing
    // as the diagram's placing tools place. While it is on, a press on a
    // schema drawn by hand asks for a table where it lands, through
    // add_table, and does nothing else a press would do. Escape puts it down,
    // and says so.
    void set_placing(bool on);
    [[nodiscard]] bool placing() const { return placing_; }
    std::function<void(bool)> placing_changed;
    // Pan in hand (Zain, 2026-10-08): the floating controls' Pan, as the
    // diagram's. While it is on, a press takes hold of the view rather than
    // of anything drawn on it, and dragging scrolls the schema; nothing is
    // chosen, moved, opened or routed. Escape puts it down, and says so
    // through panning_changed; panned says one drag has been let go.
    void set_panning(bool on);
    [[nodiscard]] bool panning() const { return panning_; }
    std::function<void(bool)> panning_changed;
    std::function<void()> panned;
    // Somewhere on the empty schema asked for a table, by a press there with
    // the Table tool in hand, in the view's own coordinates. Only where the
    // schema is drawn by hand: on a schema worked out from a diagram a table
    // comes from the diagram.
    std::function<void(QPointF at)> add_table;
    // The empty schema asked what can be done there, by the right button: the
    // place in the view's coordinates, and where a menu should open.
    std::function<void(QPointF at, QPoint menu_at)> asked_nowhere;
    // Delete pressed while the schema has the keyboard, for whatever is
    // marked. Only where the schema is drawn by hand.
    std::function<void()> delete_asked;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    bool event(QEvent* happening) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    // One line, already routed. Routing every line before any is drawn is what
    // lets the knockouts go down first: a knockout drawn after a line would
    // erase the line it crossed, which is how a line comes to disappear
    // halfway along and another appear to continue through it.
    struct Routed {
        // Which of the model's links this draws, where it draws one. This is
        // what a corner put in by hand is remembered against, so a shaped line
        // is still the same line after the preview is worked out again.
        std::optional<domain::LinkSource> link;
        // The route as corners, which is what the path is built from and what
        // the pointer is measured against. Keeping both saves walking a
        // QPainterPath to find out whether a line was clicked.
        std::vector<QPointF> corners;
        QPainterPath path;
        QColor colour;
        QPointF from;
        QPointF to;
        QPointF from_step;   // the way the line leaves its table
        QPointF to_step;
        // The tables the two ends belong to, so an end being dragged knows
        // which outline it may sit on and which box it has been pulled off.
        std::size_t from_table = 0;
        std::size_t to_table = 0;
        // The rows the two ends belong to: the foreign key this line draws, and
        // the key it points at. An end put anywhere else is still put there --
        // a hand's placement is never undone -- but it is no longer joining the
        // rows the line is about, and that is what a warning has to be able to
        // say.
        std::size_t from_column = 0;
        std::size_t to_column = 0;
        bool many = false;
        bool optional = false;
        // Whether each end has been pulled off its table and left hanging.
        bool from_loose = false;
        bool to_loose = false;
    };

    // Where one end of a line has been put by hand. On its table, the place is
    // kept as a fraction of the table's box rather than as a point, so the join
    // keeps its position when the table is moved or gains a row. Off its table,
    // it is a point on the schema and the line simply stops there.
    struct EndAnchor {
        bool on_table = true;
        QPointF at;
    };

    // Everything a hand has said about one line: the whole route it takes, and
    // where each of its ends was put.
    //
    // The route is kept whole rather than as a few corners the router is asked
    // to pass through. A route half worked out and half insisted on cannot be
    // drawn without one of the two winning somewhere, and it wins in the wrong
    // place: a single point dropped into a straight length makes the line
    // detour out to it and back, which is a spur, not a line that was pushed
    // aside. So the first time a hand moves any part of a line, the route it
    // has at that moment becomes the route it keeps.
    struct Shape {
        std::vector<QPointF> route;   // empty while the router still decides
        std::optional<EndAnchor> from;
        std::optional<EndAnchor> to;
    };

    // Which part of a line the pointer has hold of: one of its two ends, or one
    // straight run of it.
    enum class Grip { Run, FromEnd, ToEnd };

    // What a line is being shaped by: which line, which part of it, and which
    // run where that part is a run. A press that has not travelled far enough
    // moves nothing, so a line can still be clicked without shifting under a
    // hand that merely twitched.
    struct Shaping {
        domain::LinkSource link;
        Grip grip = Grip::Run;
        std::size_t index = 0;
        QPointF press;
        bool grabbed = false;
    };

    void reroute();
    void drag_end(QPointF here);
    // Write whatever the hand has been moving into the project, as one edit.
    void commit_arrangement();
    // The shaped lines a move has left crossing a table. Only asked for when
    // lines have been told to give way.
    [[nodiscard]] std::vector<domain::LinkSource> lines_over_tables() const;
    // Which edge or corner of a table the pointer has hold of. A corner pulls
    // two sides at once; every one of the eight leaves the sides it does not
    // name exactly where they were.
    enum class Pull { Left, Right, Top, Bottom, TopLeft, TopRight, BottomLeft, BottomRight };
    // Whether this pull carries the table's top-left corner with it, which is
    // true of every edge on the left or the top: the side opposite the one
    // being pulled is the side that stays still.
    [[nodiscard]] static bool moves_corner(Pull pull);
    [[nodiscard]] static bool pulls_left(Pull pull);
    [[nodiscard]] static bool pulls_right(Pull pull);
    [[nodiscard]] static bool pulls_top(Pull pull);
    [[nodiscard]] static bool pulls_bottom(Pull pull);
    // What the pointer says an edge would do, which is the same arrow whether
    // it is hovering over the edge or already hauling it.
    [[nodiscard]] static Qt::CursorShape cursor_for(Pull pull);

    // A table being pulled: which one, by which of its edges, the box it had
    // when the edge was taken hold of, and where on the widget that happened.
    // The box follows the pointer's travel from there, so the edge stays under
    // the hand however far along the strip it was grabbed.
    struct Resizing {
        std::size_t table = 0;
        Pull pull = Pull::Right;
        QRectF start;
        QPointF grab;
    };
    // A table's box while the hand still has hold of it: the box it has
    // reached, and which edge is doing the pulling -- which says both whether
    // the table is moving as well as being sized, and which of its two sizes
    // the drag is entitled to write.
    struct Pulled {
        QRectF box;
        Pull pull = Pull::Right;
    };

    // How wide and how tall a table is drawn. The height a hand has given is
    // never less than the rows need, since rows are what a table is for.
    [[nodiscard]] double width_of(const domain::PreviewTable& table) const;
    [[nodiscard]] double height_of(const domain::PreviewTable& table, double natural) const;
    // How tall a table would be if nobody had pulled it: a header, a row for
    // each column, and whatever room its questions take underneath.
    [[nodiscard]] double natural_height(const domain::PreviewTable& table, double wide) const;
    // How tall the questions under a table's rows make it, which depends on how
    // wide the table is because the answers wrap.
    [[nodiscard]] double footer_height(const domain::PreviewTable& table, double wide) const;
    // Which edge or corner is under the point, and of which table.
    [[nodiscard]] std::optional<Resizing> edge_at(QPointF point) const;
    // The box a pulled edge asks for: the side being pulled follows the
    // pointer and the opposite side stays where it was, between the smallest a
    // table may be -- which vertically is the room its own rows need -- and the
    // largest.
    [[nodiscard]] QRectF pulled_box(const Resizing& drag, QPointF pointer, double floor) const;
    void drag_run(QPointF here);

    // One answer offered inside a table: which of that table's questions it
    // belongs to, which answer it is, and where it was drawn.
    struct Chip {
        std::size_t decision = 0;
        std::size_t choice = 0;
        QRectF box;
    };

    // Where a row's answerable parts were drawn: the type, and the size where
    // the type takes one. Empty rectangles where the row offers neither --
    // a key the conversion invented is not the reader's to change.

    struct Placed {
        QRectF box;
        std::vector<QRectF> rows;
        std::vector<Cell> cells;
        // Where the rules between a row's columns fall, left to right. Held
        // for the table rather than for each row: they are a grid, and a rule
        // that moved from row to row would be worse than no rule at all.
        std::vector<double> dividers;
        // Where each question's words go, and the answers offered under them.
        std::vector<QRectF> asked;
        std::vector<Chip> chips;
    };

    // Typing a name over the one that is drawn. The box is made once and
    // reused, as the diagram's own inline editor is.
    QLineEdit* naming_ = nullptr;
    std::optional<Spot> naming_what_;
    // Which table the pointer is over, so the one being looked at can offer the
    // thing most often wanted of it. A menu on the right button has always
    // offered this; nothing on screen said so.
    std::optional<std::size_t> hovered_table_;
    // The constraint mark under the pointer. A mark wears its box only while
    // it is pointed at: three boxes on every row would drown the rows they are
    // meant to describe, and the box is only there to say it can be pressed.
    std::optional<Constrained> hovered_constraint_;
    // Where the pointer last was, in this widget's own coordinates. The slot
    // lights when the pointer is on it, and asking the desktop where the cursor
    // is would answer for the screen rather than for this view.
    QPointF pointer_;
    [[nodiscard]] QRectF add_slot(std::size_t table) const;
    // What is wrong with one end of one line, where anything is.
    [[nodiscard]] QString end_complaint(const Routed& routed, Grip which) const;
    void place_naming_box();
    void commit_rename();
    void cancel_rename();
    // Where a name is written, so a box typed into sits exactly over it.
    [[nodiscard]] QRectF name_cell(std::size_t table, std::optional<std::size_t> column) const;
    // What to say about the row under the pointer, where there is anything
    // worth saying. A key the conversion invented is the case this exists for.
    [[nodiscard]] QString hint_for(std::size_t table, std::optional<std::size_t> column) const;

    void arrange();
    [[nodiscard]] QColor surface_for(const domain::PreviewTable& table) const;
    [[nodiscard]] QColor edge_for(const domain::PreviewTable& table) const;
    [[nodiscard]] std::optional<std::size_t> table_at(QPointF point) const;
    // Which row of which table the point is on, where it is on a row at all. A
    // point on a table's header is on the table but on no column.
    [[nodiscard]] std::optional<std::size_t> row_at(std::size_t table, QPointF point) const;
    // Which line is under the pointer, and which of its corners the pointer
    // would take hold of: an existing one where it is near enough to one, and
    // otherwise where a new one would be put to bend the line there.
    [[nodiscard]] std::optional<Shaping> line_at(QPointF point) const;
    // The parts of a line that are asked about before the tables are. An end
    // sits on a table's outline and a corner may have been dragged over one, so
    // if the table answered first neither could be taken hold of again.
    [[nodiscard]] std::optional<Shaping> grip_at(QPointF point) const;
    // Which way a run would move if it were dragged: across for an upright run,
    // up and down for a level one. Nothing where the pointer is on no run.
    [[nodiscard]] std::optional<Qt::CursorShape> run_cursor(QPointF point) const;

    // Whether each table is being pointed out or faded back, worked out once
    // per paint from the selection and the narrowing together.
    [[nodiscard]] std::vector<bool> lit_tables() const;
    // Where a table came from, in words. The preview says what kind of thing
    // it was and which element it was, and the words are put together here
    // because what to call a thing belongs to whatever is showing it.
    [[nodiscard]] QString provenance_of(const domain::PreviewTable& table) const;
    // How a question is put, and what the answers are called. Both are words,
    // so both are made here rather than in the core.
    [[nodiscard]] QString question_of(const domain::OpenDecision& decision) const;
    [[nodiscard]] QStringList answers_to(const domain::OpenDecision& decision) const;
    // Which answer the pointer is over, where it is over one.
    [[nodiscard]] std::optional<std::pair<std::size_t, Chip>> chip_at(QPointF point) const;
    // The type cell of a column that can be given one, where the pointer is
    // over it: which table, and which of its rows.
    // Which part of which row the pointer is over, where it is over one that
    // can be answered: the table, the row, and whether it is the size rather
    // than the type.
    struct Answerable {
        std::size_t table = 0;
        std::size_t row = 0;
        bool size = false;
    };
    [[nodiscard]] std::optional<Answerable> type_cell_at(QPointF point) const;
    // The half of a type column under the pointer, for the same reason: the
    // type and its length are read as one word and pressed as two, so the box
    // that says which one the hand is on appears only when there is a hand.
    std::optional<Answerable> hovered_type_;
    // Which of a row's three constraint marks the pointer is over. Kept apart
    // from the type and size cells because pressing one is not answering a
    // question the conversion asked: it is changing what the table enforces,
    // and for a foreign key it reaches the relationship behind it.
    [[nodiscard]] std::optional<Constrained> rules_at(QPointF point) const;
    // The font a row's contents are drawn in, which is what every width here
    // is measured against.
    [[nodiscard]] QFont row_font() const;
    // How much room each of a table's columns needs to write what it holds
    // without cutting it off. Worked out for the whole table, because a
    // column whose edge moved from row to row could not be ruled off.
    struct Columns {
        double name = 0;
        double type = 0;
        double rules = 0;
    };
    [[nodiscard]] bool is_marked(const domain::PreviewTable& table) const;
    void draw_band(QPainter& painter) const;
    [[nodiscard]] Columns columns_of(const domain::PreviewTable& table) const;
    [[nodiscard]] double natural_width(const domain::PreviewTable& table) const;
    // Everything a table's lettering measures, worked out once for each table
    // the view draws and kept until the schema is read again or the font or
    // the screen changes. Moving a table changes none of it, so a drag that
    // measured every word of every table on every movement of the pointer was
    // getting the same answers over and over (2026-10-06).
    struct Measured {
        Columns room;
        double title = 0;                          // the table's name, bold, as names-only sizes it
        std::vector<double> labels;                // each row's type as written: int, varchar(255)
        std::vector<double> type_names;            // and the type's own name, where its size splits off
        std::vector<std::vector<double>> answers;  // each question's answers, as their chips are sized
    };
    [[nodiscard]] Measured measure(const domain::PreviewTable& table) const;
    // The measurements of one of the tables in the preview, measured the first
    // time they are asked for. Nothing for a table from anywhere else, which
    // is measured afresh where it is asked about.
    [[nodiscard]] const Measured* measurements(const domain::PreviewTable& table) const;
    // What a column's type and size are called where they are drawn.
    [[nodiscard]] static QString size_text(const domain::PreviewColumn& column);

    application::Editor& editor_;
    domain::SchemaPreview preview_;
    std::vector<Placed> placed_;
    // One for each table of the preview, in its order; emptied whenever the
    // preview is read again or the font or the screen changes.
    mutable std::vector<std::optional<Measured>> measured_;
    mutable std::size_t measurings_ = 0;
    std::vector<Routed> routes_;
    std::size_t routings_ = 0;
    Painted painted_;
    // Set while the arrangement grows the canvas to fit what it has placed.
    // That resizes the view there and then, and a resize routes every line;
    // but whatever asked for the arrangement routes them as soon as it is
    // done, so routing on that resize as well would do the whole of the
    // costliest work twice for one movement of the pointer (2026-10-06).
    bool sizing_canvas_ = false;
    // What the project says about the arrangement, in the painter's units.
    // Read from the model rather than kept here, so that an undo of a move or
    // of a shape is seen the same way as an undo of anything else.
    // What the model keeps, as the painter wants it. The one place the two
    // representations meet.
    [[nodiscard]] static Shape as_shape(const domain::SchemaLine& line);
    [[nodiscard]] Shape shape_of(const domain::LinkSource& link) const;
    [[nodiscard]] bool line_is_shaped(const domain::LinkSource& link) const;
    [[nodiscard]] std::optional<QPointF> placed_by_hand(const domain::ElementRef& table) const;
    // The one table or line under the hand right now. It moves freely and is
    // written into the project once, when it is let go: a drag that edited on
    // every frame would fill the history with a pixel of movement each.
    std::map<domain::ElementRef, QPointF> dragging_at_;
    std::map<domain::ElementRef, Pulled> resizing_to_;
    std::optional<Resizing> resizing_;
    std::optional<std::pair<domain::LinkSource, Shape>> shaping_shape_;
    std::optional<std::size_t> dragging_;
    std::optional<Shaping> shaping_;
    // The line under the pointer, drawn a little heavier with its corners shown
    // so that it is clear which one a drag would take hold of.
    std::optional<domain::LinkSource> hovered_;
    // Whether this schema is drawn by hand rather than worked out from a
    // diagram, which is what decides whether its key gutters draw foreign keys.
    [[nodiscard]] bool drawn_by_hand() const;
    // The row whose key gutter is under the point, where the schema is drawn
    // by hand: which table, and which row.
    [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> gutter_at(QPointF point) const;
    // A foreign key being drawn: from which row, where the press was, and
    // whether the hand has yet travelled far enough for it to be a drag
    // rather than a click. The line follows the pointer until it is let go.
    struct Linking {
        std::size_t table = 0;
        std::size_t row = 0;
        QPointF press;
        bool travelled = false;
    };
    std::optional<Linking> linking_;
    QPointF linking_to_;
    // The column or the line picked on the schema, beside the tables marked.
    // A column counts while its table is marked; a line while none is.
    std::optional<std::variant<SchemaColumnRef, domain::ForeignKeyId>> picked_;
    void pick(std::optional<std::variant<SchemaColumnRef, domain::ForeignKeyId>> what);
    // The key a line stands for, and the line a key is drawn as.
    [[nodiscard]] std::optional<domain::ForeignKeyId> key_of(const domain::LinkSource& link) const;
    [[nodiscard]] std::optional<domain::LinkSource> link_of(domain::ForeignKeyId key) const;
    bool connecting_ = false;
    bool placing_ = false;
    bool panning_ = false;
    // Where a drag with Pan in hand was taken hold of, and where the schema
    // was scrolled to then.
    struct PanHold {
        QPointF from;
        int across = 0;
        int down = 0;
    };
    std::optional<PanHold> pan_hold_;
    [[nodiscard]] QAbstractScrollArea* scroller() const;
    // The press that placed a table, so the double click it may turn out to
    // be the first half of does not do a double click's work as well.
    bool placed_on_press_ = false;
    // The row under the point, on a schema drawn by hand with Connect in
    // hand.
    [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> connect_row_at(QPointF point) const;
    // The strip under a table that stands for the table itself while a line
    // is drawn, read off where the table is drawn now; and where a line would
    // land at a point -- a row, then that strip, then the rest of a table --
    // which is asked alike while it is drawn and when it is let go.
    [[nodiscard]] QRectF drop_strip(std::size_t table) const;
    [[nodiscard]] std::optional<LinkTarget> link_spot(QPointF point) const;
    void draw_linking(QPainter& painter) const;
    // The tables a drag carries, each with where it stood when taken hold of,
    // and where the pointer was then. The table pressed, ordinarily; every
    // marked table when the one pressed is among several marked.
    std::vector<std::pair<domain::ElementRef, QPointF>> carried_;
    // The lines shaped by hand that a drag carries whole, because both of
    // their tables are among those carried, and how far the drag has taken
    // them. Their corners, and any end left off its table, move the same
    // distance as the tables, so the shape a hand gave a line travels with
    // what it joins rather than being left behind (Zain, 2026-09-25). An end
    // on its table already follows it, being kept as a place on the table.
    std::vector<domain::LinkSource> carried_lines_;
    QPointF carried_by_;
    [[nodiscard]] static Shape moved_by(Shape shape, QPointF by);
    [[nodiscard]] static domain::SchemaLine as_line(const Shape& shape);
    QPointF carried_from_;
    SchemaShowing showing_ = SchemaShowing::Everything;
    QString looking_for_;
    SchemaRouting routing_ = SchemaRouting::AroundTables;
    bool lines_give_way_ = false;
    bool tables_resizable_ = true;
    bool names_only_ = false;
    // How tall the row naming the columns is: none when only the names show.
    [[nodiscard]] double heading_room() const;
    // What is marked. One is the ordinary case and behaves as it always did:
    // the table is ringed, what it is joined to is ringed with it, and the
    // rest of the schema fades. Several is a deliberate act -- a band drawn
    // round them, or held down and pressed one by one -- and means exactly
    // those, so nothing is drawn in with them.
    std::vector<domain::ElementRef> selected_;
    // The entity tables already told about the key made for them, so each is
    // said once rather than on every change to the schema.
    std::set<domain::ElementRef> announced_keys_;
    void announce_invented_keys();
    // The band being drawn round tables, while one is being drawn. Kept in the
    // view's own coordinates, like everything else the hand is doing.
    std::optional<QRectF> band_;
    QPointF band_from_;
    std::optional<Answering> answering_;
    bool answering_size_ = false;
    Notation notation_ = Notation::CrowsFoot;
    const Theme* theme_ = nullptr;
    IconMode icon_mode_ = IconMode::Outline;
    // The key drawn beside PK, kept rather than asked for again on every row of
    // every repaint. A schema of any size is hundreds of rows.
    QPixmap key_mark_;
    int key_mark_side_ = 0;
    QColor key_mark_ink_;
    IconMode key_mark_mode_ = IconMode::Outline;
    qreal key_mark_ratio_ = 0;
    [[nodiscard]] const QPixmap& key_mark(int side);
};

} // namespace erdflow::desktop
