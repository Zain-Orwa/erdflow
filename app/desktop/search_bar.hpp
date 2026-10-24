// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
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
//
// The Relational Schema has one too (C8, 2026-10-14; Zain): the same bar and
// the same two questions, over the schema's own kinds and in its own words.
enum class SearchSubject { Diagram, Schema };

class SearchBar final : public QWidget {
public:
    explicit SearchBar(QWidget* parent = nullptr, SearchSubject subject = SearchSubject::Diagram);
    // What was asked, for the diagram.
    [[nodiscard]] DiagramSearch search() const;
    // What was asked, answer by answer, for whichever subject the bar has:
    // the words, the kind (a SearchKind or a SchemaSearchKind), whether what a
    // match touches is kept, and whether the rest is hidden.
    [[nodiscard]] QString text() const;
    [[nodiscard]] int kind() const;
    [[nodiscard]] bool with_relatives() const;
    [[nodiscard]] bool hide_the_rest() const;
    // Every answer back to its default: all kinds, only what matches, the
    // rest faded. The words are left to whoever holds them.
    void reset();
    // Opens the bar and puts the caret in the box, keeping whatever was last
    // looked for so pressing the shortcut twice does not throw the search away.
    void open();
    // Opens it already looking for something, which is how the command line
    // and a test ask for a search without typing one.
    void look_for(const QString& text);
    // Says how the search went, in the bar itself, so nobody has to count the
    // shapes that are left to know whether anything was found.
    void report(int found);
    // Looks for what is typed in a box outside the bar instead of its own,
    // which it then puts away (Zain, 2026-10-08): the diagram's row holds the
    // search field, as the schema's does, and the bar keeps the rest of the
    // search -- the kind, the options, the count and the way to close it.
    // Typing there opens the bar, and Escape there closes it, as in its own.
    // The schema's field searches by itself as it always has, so typing there
    // leaves the bar where it is, and Escape closes the bar only while it is
    // open (typing_opens false).
    void use_text(QLineEdit* field, bool typing_opens = true);

    // Called when what is being looked for changes. Typing settles first: a
    // name is filtered on once the typing pauses rather than on every letter,
    // so the diagram does not jump about while a word is being written.
    std::function<void()> on_changed;
    // Called when the bar is closed, which puts the whole diagram back.
    std::function<void()> on_closed;

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    QLineEdit* text_ = nullptr;
    QLineEdit* own_text_ = nullptr;
    QComboBox* kind_ = nullptr;
    QToolButton* settings_ = nullptr;
    QLabel* count_ = nullptr;
    QAction* only_matches_ = nullptr;
    QAction* relatives_ = nullptr;
    QAction* fade_rest_ = nullptr;
    QAction* hide_rest_ = nullptr;
    QTimer* settling_ = nullptr;
    bool typing_opens_ = true;
    void changed(bool at_once);
};

} // namespace erdflow::desktop
