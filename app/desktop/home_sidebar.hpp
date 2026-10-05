// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "app/desktop/theme.hpp"

#include <QSize>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <array>
#include <cstddef>
#include <functional>
#include <vector>

class QAction;
class QAbstractButton;
class QEvent;
class QObject;

namespace erdflow::desktop {

// The places named by the Home screen's left rail. This is navigation, not the
// four-level product model: several entries are commands or catalogues.
//
// Examples, Templates and Import are not here: each belongs to the design it
// works on, and is offered in that workspace rather than on Home (Zain,
// 2026-10-03).
enum class HomeSection {
    Home,
    OpenProject,
    Recent,
    Settings,
    Help,
};

// No New Project row: the Home screen's cards are where a project is started
// (Zain, 2026-09-24, ADR-022 section 9.20).
inline constexpr std::size_t home_section_count = 5;

// Which group a row belongs to. The first runs down from the top; the last is
// pushed to the foot of the rail, where settings and help are looked for.
enum class HomeGroup { Start, Foot };

struct HomeNavigationDefinition {
    HomeSection section;
    HomeGroup group;
    const char* object_name;
    const char* action_object_name;
    const char* label;
    // The drawing in the line-art set, by file name.
    const char* icon;
};

// Kept in one public, immutable table so production code and tests ask the
// same source for the required order and copy.
[[nodiscard]] const std::array<HomeNavigationDefinition, home_section_count>&
home_navigation();

// The fixed-width navigation rail used by the start page.
//
// Every row is a genuine button backed by a QAction. Consequently it supports
// pointer activation with a single press, Tab/Shift+Tab, Space, Return, and
// accessible names without a parallel imitation in a paint event. Up/Down/
// Home/End move keyboard focus between the nine rows; activation is left to
// Space or Return.
//
// It replaces a list widget, which on most platforms only answered a double
// press: a single press on Open Project lit the row and did nothing else.
class HomeSidebar final : public QWidget {
public:
    using Callback = std::function<void()>;

    explicit HomeSidebar(QWidget* parent = nullptr);

    void wear(ThemeId id);

    void set_selected(HomeSection section);
    [[nodiscard]] HomeSection selected() const { return selected_; }
    [[nodiscard]] QString selected_label() const;

    // One action and one button exist for every visible label. Callers may
    // connect to the action in ordinary Qt style or install a small callback.
    [[nodiscard]] QAction* action(HomeSection section) const;
    [[nodiscard]] QAbstractButton* button(HomeSection section) const;
    void set_callback(HomeSection section, Callback callback);

    // Fired after the item-specific callback whenever a row is activated.
    std::function<void(HomeSection)> activated;

    [[nodiscard]] QString label(HomeSection section) const;
    [[nodiscard]] QStringList labels() const;
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    [[nodiscard]] static std::size_t index_of(HomeSection section);
    void lay_out();
    void move_focus(int from, int step);

    ThemeId theme_ = ThemeId::Azure;
    HomeSection selected_ = HomeSection::Home;
    std::array<QAbstractButton*, home_section_count> buttons_{};
    std::array<QAction*, home_section_count> actions_{};
    std::array<Callback, home_section_count> callbacks_{};
    // Where the rules between the groups fall, worked out by the layout so
    // the paint does not have to know the geometry twice.
    std::vector<int> rules_;
};

// The fill a chosen row takes. Azure's primary with white lettering reads at
// 3.68:1, under the 4.5:1 this project holds ordinary text to, so the first
// of primary, hover and pressed that reaches it is used (ADR-022 section 9.3).
// For Azure that is #1976D2. Exposed so tests can hold it to the ratio.
[[nodiscard]] QColor chosen_row_fill(const Tokens& t);
[[nodiscard]] double contrast_ratio(const QColor& a, const QColor& b);
// The same colour, darkened (or on a dark surface lightened) only as far as it
// must go to be read as ordinary text on the surface given: 4.5:1. A colour
// that already reads is returned unchanged, so Azure's own values are kept
// wherever they pass and moved only where they do not (ADR-022 section 9.3).
[[nodiscard]] QColor legible_on(const QColor& ink, const QColor& surface);

} // namespace erdflow::desktop
