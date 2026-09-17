#include "symbol_picker.hpp"

#include "symbols.hpp"

#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QScrollArea>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>

namespace erdflow::desktop {
namespace {

// Wide enough for eight characters a row, which keeps a group of thirty to
// four lines and the window to something that can sit beside the diagram.
constexpr int columns = 8;
// Emoji are square and about as wide as they are tall, and the style elides
// any text that does not fit, turning one into an ellipsis. The cell is sized
// for the widest of them at the size they are drawn, and the buttons are given
// no padding of their own, so nothing is ever elided.
constexpr int cell = 44;
constexpr qreal character_points = 17;

} // namespace

SymbolPicker::SymbolPicker(QWidget* parent) : QDialog(parent) {
    setObjectName("symbolPicker");
    setWindowTitle("Symbols");
    // A tool window that does not come forward when it opens: the field being
    // written in keeps its caret, which is the whole point of the picker.
    setWindowFlags(Qt::Tool);
    setAttribute(Qt::WA_ShowWithoutActivating);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(8);

    search_ = new QLineEdit(this);
    search_->setObjectName("symbolSearch");
    search_->setPlaceholderText("Search by name, such as join, subset or key");
    search_->setClearButtonEnabled(true);
    layout->addWidget(search_);

    auto* middle = new QHBoxLayout;
    middle->setSpacing(8);

    groups_ = new QListWidget(this);
    groups_->setObjectName("symbolGroups");
    for (const auto& group : symbol_groups()) groups_->addItem(group.name);
    groups_->setCurrentRow(0);
    groups_->setFixedWidth(150);
    middle->addWidget(groups_);

    grid_host_ = new QWidget(this);
    grid_ = new QGridLayout(grid_host_);
    grid_->setContentsMargins(0, 0, 0, 0);
    grid_->setSpacing(2);
    area_ = new QScrollArea(this);
    area_->setObjectName("symbolArea");
    area_->setWidget(grid_host_);
    area_->setWidgetResizable(true);
    area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    area_->setMinimumWidth(columns * cell + 28);
    area_->setMinimumHeight(cell * 4);
    middle->addWidget(area_, 1);
    layout->addLayout(middle);

    destination_ = new QLabel(this);
    destination_->setObjectName("symbolDestination");
    destination_->setWordWrap(true);
    layout->addWidget(destination_);

    connect(groups_, &QListWidget::currentRowChanged, this, [this](int) {
        // Choosing a group is a way of browsing, so it clears a search rather
        // than fighting with one; otherwise the grid would look empty for no
        // reason the user can see.
        if (!search_->text().isEmpty()) {
            const QSignalBlocker quiet(search_);
            search_->clear();
        }
        rebuild();
    });
    connect(search_, &QLineEdit::textChanged, this, [this](const QString&) { rebuild(); });
    rebuild();
    // Big enough for a group of thirty without scrolling, small enough to sit
    // beside the diagram rather than over it.
    resize(600, 430);
}

void SymbolPicker::set_theme(const Theme& colors) {
    colors_ = colors;
    grid_host_->setStyleSheet(QStringLiteral(
        "QToolButton { border: 1px solid transparent; border-radius: 4px; background: transparent;"
        " color: %1; padding: 0px; margin: 0px; }"
        "QToolButton:hover { background: %2; border-color: %3; }"
        "QToolButton:pressed { background: %4; }")
        .arg(colors.text.name(), hover_surface(colors).name(), colors.border.name(), colors.accent.name()));
    destination_->setStyleSheet(QStringLiteral("QLabel#symbolDestination { color: %1; font-size: 11px; }")
                                    .arg(colors.muted.name()));
}

void SymbolPicker::show_group(const QString& name) {
    const auto row = symbol_group_index(name);
    if (!search_->text().isEmpty()) search_->clear();
    groups_->setCurrentRow(row);
    rebuild();
}

void SymbolPicker::set_destination(const QString& where) {
    destination_->setText(where.isEmpty()
                              ? QStringLiteral("Goes on the diagram, where the pointer last was. Click into a name, "
                                               "a role or a description first to put one there instead.")
                              : QStringLiteral("Goes into %1.").arg(where));
}

void SymbolPicker::rebuild() {
    while (auto* item = grid_->takeAt(0)) {
        if (auto* widget = item->widget()) {
            // Taking a widget out of the layout does not take it off the
            // screen, and a deferred delete does not run until the event loop
            // gets a turn, so the old grid would go on painting underneath the
            // new one. Unparenting it now is what actually removes it.
            widget->setParent(nullptr);
            widget->deleteLater();
        }
        delete item;
    }
    // A search reaches across every group, because the point of naming the
    // characters is not having to know which drawer one lives in.
    const auto needle = search_->text().trimmed();
    {
        // A search reaches across every group, so leaving one highlighted would
        // claim the grid is showing that group when it is not.
        const QSignalBlocker quiet(groups_);
        if (needle.isEmpty()) {
            if (groups_->currentRow() < 0) groups_->setCurrentRow(last_group_);
        } else {
            if (groups_->currentRow() >= 0) last_group_ = groups_->currentRow();
            groups_->setCurrentRow(-1);
        }
    }
    std::vector<Symbol> showing;
    if (needle.isEmpty()) {
        const auto row = groups_->currentRow() < 0 ? last_group_ : groups_->currentRow();
        const auto& groups = symbol_groups();
        if (row >= 0 && row < static_cast<int>(groups.size())) showing = groups[static_cast<std::size_t>(row)].symbols;
    } else {
        for (const auto& group : symbol_groups())
            for (const auto& symbol : group.symbols)
                if (symbol.name.contains(needle, Qt::CaseInsensitive) || symbol.character == needle)
                    showing.push_back(symbol);
    }

    if (showing.empty()) {
        auto* empty = new QLabel(QStringLiteral("Nothing matches “%1”.").arg(needle), grid_host_);
        empty->setObjectName("symbolNoMatch");
        grid_->addWidget(empty, 0, 0, 1, columns);
        return;
    }

    int row = 0, column = 0;
    for (const auto& symbol : showing) {
        auto* button = new QToolButton(grid_host_);
        button->setText(symbol.character);
        button->setToolTip(symbol.name);
        button->setAccessibleName(symbol.name);
        button->setFixedSize(cell, cell);
        button->setAutoRaise(true);
        // Refusing focus is what lets the field being written in keep its caret
        // while its character is picked.
        button->setFocusPolicy(Qt::NoFocus);
        auto font = button->font();
        font.setPointSizeF(character_points);
        button->setFont(font);
        connect(button, &QToolButton::clicked, this, [this, character = symbol.character] {
            if (on_chosen) on_chosen(character);
        });
        grid_->addWidget(button, row, column);
        if (++column == columns) { column = 0; ++row; }
    }
    grid_->setRowStretch(row + 1, 1);
}

} // namespace erdflow::desktop
