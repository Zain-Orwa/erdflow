#pragma once

#include "diagram_view.hpp"

#include <QWidget>
#include <functional>

class QComboBox;
class QLabel;
class QLineEdit;
class QTimer;
class QToolButton;

namespace erdflow::desktop {

// The bar that narrows the diagram to what is being looked for. It drops in
// above the canvas when it is asked for and takes no room when it is not, the
// way Find does in a document application, and it sits next to the thing it
// filters rather than across the window from it.
//
// It holds what to look for, which kind to look among, and the two choices that
// decide how much of the rest survives. It knows nothing about the diagram: it
// says what was asked for and the window hands that to the canvas.
class SearchBar final : public QWidget {
public:
    explicit SearchBar(QWidget* parent = nullptr);
    [[nodiscard]] DiagramSearch search() const;
    // Opens the bar and puts the caret in the box, keeping whatever was last
    // looked for so pressing the shortcut twice does not throw the search away.
    void open();
    // Says how the search went, in the bar itself, so nobody has to count the
    // shapes that are left to know whether anything was found.
    void report(int found);

    // Called when what is being looked for changes. Typing settles first: a
    // name is filtered on once the typing pauses rather than on every letter,
    // so the diagram does not jump about while a word is being written.
    std::function<void()> on_changed;
    // Called when the bar is closed, which puts the whole diagram back.
    std::function<void()> on_closed;

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    QLineEdit* text_ = nullptr;
    QComboBox* kind_ = nullptr;
    QToolButton* settings_ = nullptr;
    QLabel* count_ = nullptr;
    QAction* relatives_ = nullptr;
    QAction* hide_rest_ = nullptr;
    QTimer* settling_ = nullptr;
    void changed(bool at_once);
};

} // namespace erdflow::desktop
