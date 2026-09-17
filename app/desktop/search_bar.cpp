#include "search_bar.hpp"

#include <QAction>
#include <QComboBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QTimer>
#include <QToolButton>

namespace erdflow::desktop {
namespace {
// How long the typing has to pause before the diagram is filtered on it. Long
// enough that writing a word is one change rather than six, short enough that
// it never feels like waiting.
constexpr int settling_ms = 220;

struct KindEntry { SearchKind kind; const char* label; };
const std::vector<KindEntry>& kinds() {
    static const std::vector<KindEntry> list{
        {SearchKind::Everything, "All kinds"},
        {SearchKind::Entities, "Entities"},
        {SearchKind::Attributes, "Attributes"},
        {SearchKind::Relationships, "Relationships"},
        {SearchKind::Hierarchies, "Hierarchies"},
    };
    return list;
}
} // namespace

SearchBar::SearchBar(QWidget* parent) : QWidget(parent) {
    setObjectName("searchBar");
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(22, 6, 12, 6);
    layout->setSpacing(8);

    text_ = new QLineEdit(this);
    text_->setObjectName("searchText");
    text_->setPlaceholderText("Search the diagram by name");
    text_->setClearButtonEnabled(true);
    text_->setMinimumWidth(160);
    layout->addWidget(text_, 1);

    // Choosing a kind and typing nothing asks for every element of that kind,
    // which is how "show me only the entities" is asked for.
    kind_ = new QComboBox(this);
    kind_->setObjectName("searchKind");
    kind_->setToolTip("Look among one kind of element, or among all of them. "
                      "Choosing a kind and typing nothing shows every element of that kind.");
    for (const auto& entry : kinds()) kind_->addItem(QString::fromUtf8(entry.label),
                                                     static_cast<int>(entry.kind));
    layout->addWidget(kind_);

    settings_ = new QToolButton(this);
    settings_->setObjectName("searchSettings");
    settings_->setText("Options");
    settings_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    settings_->setPopupMode(QToolButton::InstantPopup);
    auto* menu = new QMenu(settings_);
    menu->setObjectName("searchSettingsMenu");
    relatives_ = menu->addAction("Show what it touches");
    relatives_->setObjectName("searchRelatives");
    relatives_->setCheckable(true);
    relatives_->setToolTip("Keep what a match belongs to and what it is joined to: its attributes, the "
                           "relationships and hierarchies it takes part in, and the far side of those.");
    hide_rest_ = menu->addAction("Hide everything else");
    hide_rest_->setObjectName("searchHideRest");
    hide_rest_->setCheckable(true);
    hide_rest_->setToolTip("Take the rest of the diagram away rather than fading it. A line goes with "
                           "whichever of its ends goes, so nothing is left hanging.");
    settings_->setMenu(menu);
    layout->addWidget(settings_);

    count_ = new QLabel(this);
    count_->setObjectName("searchCount");
    count_->setMinimumWidth(80);
    layout->addWidget(count_);

    auto* close = new QToolButton(this);
    close->setObjectName("searchClose");
    close->setText("✕");
    close->setToolTip("Close the search and put the whole diagram back.\tEsc");
    layout->addWidget(close);

    // Typing settles before the diagram is filtered on it; everything else is
    // a deliberate choice and takes effect as it is made.
    settling_ = new QTimer(this);
    settling_->setSingleShot(true);
    settling_->setInterval(settling_ms);
    connect(settling_, &QTimer::timeout, this, [this] { if (on_changed) on_changed(); });
    connect(text_, &QLineEdit::textChanged, this, [this] { changed(false); });
    connect(text_, &QLineEdit::returnPressed, this, [this] { changed(true); });
    connect(kind_, &QComboBox::currentIndexChanged, this, [this] { changed(true); });
    connect(relatives_, &QAction::toggled, this, [this] { changed(true); });
    connect(hide_rest_, &QAction::toggled, this, [this] { changed(true); });
    connect(close, &QToolButton::clicked, this, [this] { if (on_closed) on_closed(); });
}

void SearchBar::changed(bool at_once) {
    if (at_once) {
        settling_->stop();
        if (on_changed) on_changed();
        return;
    }
    settling_->start();
}

DiagramSearch SearchBar::search() const {
    DiagramSearch asked;
    asked.text = text_->text().trimmed();
    asked.kind = static_cast<SearchKind>(kind_->currentData().toInt());
    asked.with_relatives = relatives_->isChecked();
    asked.hide_the_rest = hide_rest_->isChecked();
    return asked;
}

void SearchBar::open() {
    show();
    text_->setFocus();
    text_->selectAll();
}

void SearchBar::report(int found) {
    if (!search().looking()) { count_->clear(); return; }
    count_->setText(found == 0 ? QStringLiteral("Nothing found")
                   : found == 1 ? QStringLiteral("1 found")
                                : QStringLiteral("%1 found").arg(found));
}

void SearchBar::keyPressEvent(QKeyEvent* event) {
    // Escape puts the whole diagram back, wherever in the bar the caret is.
    if (event->key() == Qt::Key_Escape) {
        if (on_closed) on_closed();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace erdflow::desktop
