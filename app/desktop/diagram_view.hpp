#pragma once

#include "application/editor.hpp"
#include "theme.hpp"

#include <QGraphicsView>
#include <QPixmap>
#include <functional>
#include <memory>
#include <optional>

namespace erdflow::desktop {

// Generalization and specialization produce the same ISA structure; they differ
// in direction. Specialization works top-down: name the supertype and attach
// subtypes afterwards. Generalization works bottom-up: select the subtypes
// first, then name the entity that generalises them.
// Note places a note, the one visual aid put down by a click; a picture comes
// from a file, so it is an action of the window rather than a tool.
enum class Tool { Select, Entity, Attribute, Relationship, Specialization, Generalization, Connect, Pan, Note };

// How each participant end is drawn. The model is the same in every notation:
// participation supplies the minimum (0 or 1) and cardinality the maximum
// (1 or M), so every notation reads the same two values off each participant.
enum class Notation { Chen, MinMax, CrowsFoot, Bachman };

// How connectors are drawn between elements. Elbow breaks a line at right
// angles, which is how an ERD is drawn by hand and the only shape that reads
// cleanly when both of its ends are pinned where they were clicked; the other
// two draw one line from end to end, curved or straight.
enum class LineStyle { Curved, Straight, Elbow };

// Where a new connection meets each shape. Automatic lets each join slide
// around its outline to face the other end as things are moved. WhereClicked
// pins each end at the point that was clicked to make the connection, and it
// stays there until the line's padlock releases it or its end is dragged
// somewhere else on the same shape.
enum class JoinMode { Automatic, WhereClicked };

// Default body sizes for newly created elements. An associative relationship
// adopts the entity size, because that is what it behaves as on the diagram.
struct BodySize { double width, height; };
inline constexpr BodySize entity_body{160, 80};
inline constexpr BodySize attribute_body{150, 60};
inline constexpr BodySize relationship_body{190, 110};
inline constexpr BodySize isa_body{96, 74};
inline constexpr BodySize note_body{200, 120};
// The size a symbol is placed at, and the step Enlarge and Shrink move it by.
// A quarter is enough to see at a glance and small enough that three or four
// presses land on the size that was wanted, rather than overshooting it.
inline constexpr BodySize symbol_body{56, 56};
inline constexpr double symbol_step = 1.25;

class DiagramView : public QGraphicsView {
public:
    explicit DiagramView(application::Editor& editor, QWidget* parent = nullptr);
    ~DiagramView() override;
    void synchronize();
    // A tool used once returns to Select; a locked tool stays until changed, so
    // several elements can be placed without reaching for the toolbar each time.
    void set_tool(Tool tool, bool locked = false);
    [[nodiscard]] bool tool_locked() const;
    [[nodiscard]] Tool tool() const;
    [[nodiscard]] std::vector<domain::ElementRef> selected_elements() const;
    void select_elements(const std::vector<domain::ElementRef>& elements, bool bring_into_view = false);
    void fit_diagram();
    // What a picture of the diagram would cover, for each of the three extents
    // a picture may be taken of. An empty rectangle means there is nothing to
    // take a picture of, which the caller reports rather than writing a file
    // with nothing in it.
    [[nodiscard]] QRectF diagram_bounds() const;
    [[nodiscard]] QRectF selection_bounds() const;
    [[nodiscard]] QRectF view_bounds() const;
    // Draws the diagram, and none of the editor looking at it: no grid, no
    // selection rings, no handles and no paper. The background an exported
    // picture stands on is the caller's choice, so it is painted by the
    // caller before this is called rather than assumed here.
    void render_diagram(QPainter& painter, const QRectF& target, const QRectF& source);
    // The canvas colour of the theme in use, for a picture asked to stand on it.
    [[nodiscard]] QColor canvas_colour() const;
    void actual_size();
    void zoom_in();
    void zoom_out();
    void set_line_style(LineStyle style);
    [[nodiscard]] LineStyle line_style() const;
    void set_join_mode(JoinMode mode);
    [[nodiscard]] JoinMode join_mode() const;
    void set_notation(Notation notation);
    // A sample of how a notation draws one participant end, for the picker.
    // It uses the same drawing code as the canvas, so it cannot misrepresent it.
    [[nodiscard]] QPixmap notation_preview(Notation notation, QSize size) const;
    // One element's own shape, drawn small and in its own colour, so anything
    // that names an element can show what it is rather than a coloured box.
    // It is the drawing the canvas uses, so the two cannot disagree.
    // Filling the room stretches the shape to the size asked for rather than
    // fitting it inside; a name is written in a shape that spans the panel.
    [[nodiscard]] QPixmap element_preview(const domain::ElementRef& ref, QSize size,
                                          bool fill_the_room = false) const;
    // A sample of a line style, drawn the way the canvas draws it.
    [[nodiscard]] QPixmap line_style_preview(LineStyle style, QSize size) const;
    [[nodiscard]] Notation notation() const;
    // Whether remarks are shown at all. This is how the diagram is being looked
    // at rather than part of it, like the grid, so it is not saved with the
    // document and does not pass through the history. The mark on a commented
    // element stays either way: hiding quiets the diagram, it does not lose
    // what a reviewer said.
    void set_comments_visible(bool shown);
    [[nodiscard]] bool comments_visible() const;
    // The comments pinned to what is under the pointer, so the window can offer
    // them where they were found. Empty when nothing there carries one.
    [[nodiscard]] std::vector<domain::CommentId> comments_at(const QPoint& viewport_position) const;
    // What is under the pointer that a comment could be pinned to: an element,
    // or one of the lines. Nothing when the pointer is over empty canvas.
    [[nodiscard]] std::optional<domain::CommentTarget> target_at(const QPoint& viewport_position) const;
    // The lines that are selected, as the connector references a comment pins
    // itself to. Inheritance links are left out: they are anchored to their
    // triangle and are not connectors, so a remark about one goes on the
    // triangle instead.
    [[nodiscard]] std::vector<domain::ConnectorRef> selected_connectors() const;
    void set_grid_visible(bool enabled);
    // Aligning to the grid rounds a dragged or placed element's position to
    // the nearest grid point, so elements put down near each other line up.
    void set_align_to_grid(bool enabled);
    void set_theme(ThemeId id);
    [[nodiscard]] ThemeId theme_id() const;
    void delete_selection();
    void cancel_interaction();
    // How see-through the named elements are drawn, in percent. Preview shows
    // it without writing it, for a slider being dragged; set writes it as one
    // edit and shows the result, or puts the preview back if it was refused.
    void preview_transparency(const std::vector<domain::ElementRef>& elements, int percent);
    // Lines the named elements up on one edge or one middle, as one edit: the
    // same thing the right-click menu's Align offers, reachable without the
    // menu so it can be driven from a test.
    void align_selection_for_test(const std::vector<domain::ElementRef>& elements, bool along_x, int which);
    void set_transparency(const std::vector<domain::ElementRef>& elements, int percent);
    // The selected elements that are symbols. A symbol is the one thing on the
    // diagram whose size is the user's to choose, so the commands that enlarge
    // and shrink ask this first and do nothing when the answer is empty.
    [[nodiscard]] std::vector<domain::ElementRef> selected_symbols() const;
    // Grows every selected symbol about its own centre by the given factor, or
    // shrinks it when the factor is below one, as a single edit. Each symbol
    // keeps its own size and its own place: a selection of several does not
    // collapse onto one size or drift together.
    void resize_symbols(double factor);
    // Editing a name on the canvas itself, as an alternative to the properties
    // panel. Commit writes the pending text through the normal command path.
    void begin_rename(const domain::ElementRef& element);
    [[nodiscard]] bool renaming() const;
    void commit_rename();
    [[nodiscard]] double zoom_factor() const;
    // Where the pointer last was over the canvas, in scene coordinates.
    // Nothing is returned until the pointer has been over it, so a caller can
    // fall back to the middle of the view rather than guess at the corner.
    [[nodiscard]] std::optional<QPointF> pointer_place() const;

    std::function<void(const application::EditResult&)> on_edit;
    std::function<void(const std::vector<domain::ElementRef>&)> on_selection;
    std::function<void(Tool)> on_tool;
    std::function<void(const QString&)> on_status;
    std::function<void(double)> on_zoom;
    // Asked when the canvas's own menu offers to insert a picture at a point;
    // the file is the window's business, the place is the canvas's.
    std::function<void(QPointF)> on_insert_picture;
    // Asked when the canvas's own menu offers to leave a remark on what was
    // right-clicked: the words are the window's business, what they are pinned
    // to is the canvas's. Several targets arrive together when several things
    // were selected, which is how one remark comes to cover a whole area.
    std::function<void(std::vector<domain::CommentTarget>)> on_comment;

protected:
    void drawBackground(QPainter*, const QRectF&) override;
    void drawForeground(QPainter*, const QRectF&) override;
    void scrollContentsBy(int, int) override;
    bool eventFilter(QObject*, QEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    bool viewportEvent(QEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;

private:
    // The connector menu. It finds the line itself rather than being handed one,
    // since the item type is private to the implementation and has no name here.
    void participant_menu(QContextMenuEvent* event);
    // The menu for empty canvas, which offers what can be put there.
    void canvas_menu(QContextMenuEvent* event);

protected:

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace erdflow::desktop
