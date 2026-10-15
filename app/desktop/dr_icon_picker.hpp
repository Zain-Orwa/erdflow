// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "theme.hpp"

#include <QDialog>
#include <functional>

class QLabel;

namespace erdflow::desktop {

// The Dr.icons gallery: the sixteen illustrations, four to a row, each with
// its name beneath it. Picking one puts it on the diagram as a picture. It
// stays open while several are placed, as the symbol gallery does, and like
// that gallery it is a tool window that never takes the keyboard, so the
// diagram keeps it and Undo takes back a placed icon straight away.
class DrIconPicker final : public QDialog {
public:
    explicit DrIconPicker(QWidget* parent = nullptr);
    // Repaints the gallery in a theme. The application stylesheet covers the
    // frame; the icon buttons are drawn flat and have to light up under the
    // pointer, so they are styled here.
    void set_theme(const Theme& colors);

    // Called with the icon's key when one is picked.
    std::function<void(const QString&)> on_chosen;

private:
    QWidget* grid_host_ = nullptr;
    QLabel* destination_ = nullptr;
};

} // namespace erdflow::desktop
