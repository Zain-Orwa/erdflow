#pragma once

#include "application/editor.hpp"
#include "theme.hpp"

#include <QGraphicsView>
#include <functional>
#include <memory>

namespace erdflow::desktop {

enum class Tool { Select, Entity, Attribute, Relationship, Connect, Pan };

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
    void set_grid_visible(bool enabled);
    void set_snap_enabled(bool enabled);
    void set_theme(ThemeId id);
    [[nodiscard]] ThemeId theme_id() const;
    void delete_selection();
    void cancel_interaction();
    [[nodiscard]] double zoom_factor() const;

    std::function<void(const application::EditResult&)> on_edit;
    std::function<void(const std::vector<domain::ElementRef>&)> on_selection;
    std::function<void(Tool)> on_tool;
    std::function<void(const QString&)> on_status;
    std::function<void(double)> on_zoom;

protected:
    void drawBackground(QPainter*, const QRectF&) override;
    void drawForeground(QPainter*, const QRectF&) override;
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
