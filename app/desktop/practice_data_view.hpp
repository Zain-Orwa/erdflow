// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "app/desktop/icons.hpp"
#include "app/desktop/theme.hpp"
#include "application/editor.hpp"

#include <QWidget>

#include <functional>
#include <memory>

namespace erdflow::desktop {

// Practice Data (Zain, 2026-10-23): the schema's tables as cards of rows, in
// place of the schema while it is put away. Every table the schema has is a
// card, built from the schema each time it is read, so nothing here describes
// a table of its own. A row is added, typed over and deleted here; whether the
// schema's keys and rules allow it is the Editor's to say, and when it does not
// the card says why, in words for somebody learning what the rule is for.
//
// No SQL is written or shown here, and the schema's own lines are not drawn.
class PracticeDataView final : public QWidget {
public:
    explicit PracticeDataView(application::Editor& editor, QWidget* parent = nullptr);
    ~PracticeDataView() override;

    void set_theme(const Theme& theme, IconMode mode);
    // Reads the project again: which tables there are, their columns, their
    // rows and what those rows break. A card keeps a row being typed in, and
    // whatever it last said.
    void synchronize();
    // How many cards stand side by side: four or three where there is room, two, one;
    // one whenever the list is chosen.
    [[nodiscard]] int columns_across() const;

    // Show Schema.
    std::function<void()> on_show_schema;
    // Told after every change made here, whether it was made or refused, so
    // the window can bring everything else up to date.
    std::function<void(const application::EditResult&)> on_changed;

protected:
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace erdflow::desktop
