#pragma once

#include "theme.hpp"

#include <QDialog>
#include <functional>

class QGridLayout;
class QLabel;
class QLineEdit;
class QListWidget;
class QScrollArea;

namespace erdflow::desktop {

// The gallery of characters that can be put into a name, a description or a
// note: relational algebra, logic, arrows, Greek, marks, emoji and people. It
// stays open while several are inserted, the way a document editor's does,
// because a caption or a role is rarely one character.
//
// It is a tool window that does not take activation, and every character
// button refuses focus, so clicking one never moves the caret out of the field
// being written in. Only the search box takes focus, and the window that owns
// the picker knows how to find its field again afterwards.
class SymbolPicker final : public QDialog {
public:
    explicit SymbolPicker(QWidget* parent = nullptr);
    // Repaints the gallery in a theme. The application stylesheet covers the
    // frame; the character buttons are styled here because they are drawn flat
    // and have to light up under the pointer.
    void set_theme(const Theme& colors);
    // Opens the picker on one group, for an entry that names it. An unknown
    // name leaves it where it was.
    void show_group(const QString& name);
    // Says where the next character will land, so nobody inserts into nothing.
    // An empty description means there is nowhere to put one yet.
    void set_destination(const QString& where);

    // Called with the character when one is picked.
    std::function<void(const QString&)> on_chosen;

private:
    QLineEdit* search_ = nullptr;
    QListWidget* groups_ = nullptr;
    QScrollArea* area_ = nullptr;
    QWidget* grid_host_ = nullptr;
    QGridLayout* grid_ = nullptr;
    QLabel* destination_ = nullptr;
    Theme colors_;
    // The group to go back to when a search is cleared, since searching takes
    // the highlight off the list while it reaches across every group.
    int last_group_ = 0;
    void rebuild();
};

} // namespace erdflow::desktop
