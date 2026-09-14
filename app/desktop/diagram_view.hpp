#pragma once

#include "application/editor.hpp"
#include "theme.hpp"

#include <QGraphicsView>
#include <QPixmap>
#include <functional>
#include <memory>

namespace erdflow::desktop {

enum class Tool { Select, Entity, Attribute, Relationship, Isa, Connect, Pan };

// How each participant end is drawn. The model is the same in every notation:
// participation supplies the minimum (0 or 1) and cardinality the maximum
// (1 or M), so every notation reads the same two values off each participant.
enum class Notation { Chen, MinMax, CrowsFoot, Bachman };

// Default body sizes for newly created elements. An associative relationship
// adopts the entity size, because that is what it behaves as on the diagram.
struct BodySize { double width, height; };
inline constexpr BodySize entity_body{160, 80};
inline constexpr BodySize attribute_body{150, 60};
inline constexpr BodySize relationship_body{190, 110};
inline constexpr BodySize isa_body{96, 74};

class DiagramView : public QGraphicsView {
public:
    explicit DiagramView(application::Editor& editor, QWidget* parent = nullptr);
    ~DiagramView() override;
    void synchronize();
    void set_tool(Tool tool);
    [[nodiscard]] Tool tool() const;
    [[nodiscard]] std::vector<domain::ElementRef> selected_elements() const;
    void select_elements(const std::vector<domain::ElementRef>& elements, bool bring_into_view = false);
    void fit_diagram();
    void actual_size();
    void zoom_in();
    void zoom_out();
    void set_notation(Notation notation);
    // A sample of how a notation draws one participant end, for the picker.
    // It uses the same drawing code as the canvas, so it cannot misrepresent it.
    [[nodiscard]] QPixmap notation_preview(Notation notation, QSize size) const;
    [[nodiscard]] Notation notation() const;
    void set_grid_visible(bool enabled);
    void set_snap_enabled(bool enabled);
    void set_theme(ThemeId id);
    [[nodiscard]] ThemeId theme_id() const;
    void delete_selection();
    void cancel_interaction();
    // Editing a name on the canvas itself, as an alternative to the properties
    // panel. Commit writes the pending text through the normal command path.
    void begin_rename(const domain::ElementRef& element);
    [[nodiscard]] bool renaming() const;
    void commit_rename();
    [[nodiscard]] double zoom_factor() const;

    std::function<void(const application::EditResult&)> on_edit;
    std::function<void(const std::vector<domain::ElementRef>&)> on_selection;
    std::function<void(Tool)> on_tool;
    std::function<void(const QString&)> on_status;
    std::function<void(double)> on_zoom;

protected:
    void drawBackground(QPainter*, const QRectF&) override;
    void drawForeground(QPainter*, const QRectF&) override;
    void scrollContentsBy(int, int) override;
    bool eventFilter(QObject*, QEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace erdflow::desktop
