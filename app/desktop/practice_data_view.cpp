// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/practice_data_view.hpp"

#include "app/desktop/home_sidebar.hpp"

#include "domain/practice_data.hpp"
#include "domain/schema_preview.hpp"

#include <QAbstractTableModel>
#include <QAction>
#include <QButtonGroup>
#include <QCheckBox>
#include <QApplication>
#include <QComboBox>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace erdflow::desktop {
namespace {

using domain::PracticeRow;
using domain::PracticeValue;
using domain::PreviewColumn;
using domain::PreviewTable;

QString text(const std::string& value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }
std::string bytes(const QString& value) { return value.toUtf8().toStdString(); }

// What a card being moved carries: its table's identity, and where on the card
// it was taken hold of, so it is put down under the hand just as it was held.
constexpr auto moved_table = "application/x-erdflow-practice-table";
constexpr auto moved_hold = "application/x-erdflow-practice-hold";
// The room between cards, and the height of a row.
constexpr int card_gap = 16;
constexpr int row_height = 26;

QColor mix(const QColor& from, const QColor& to, double share) {
    const auto part = static_cast<float>(share);
    return QColor::fromRgbF(from.redF() + (to.redF() - from.redF()) * part,
                            from.greenF() + (to.greenF() - from.greenF()) * part,
                            from.blueF() + (to.blueF() - from.blueF()) * part);
}

// Text, where an empty value is a value: the empty string. Anywhere else an
// empty cell can only mean NULL.
bool written_as_text(const PreviewColumn& column) {
    using domain::LogicalType;
    switch (column.type) {
    case LogicalType::Char: case LogicalType::Varchar: case LogicalType::VarcharMax: case LogicalType::Text:
    case LogicalType::NChar: case LogicalType::NVarchar: case LogicalType::NVarcharMax: case LogicalType::NText:
    case LogicalType::Xml:
        return true;
    case LogicalType::Unset:
        return !QString::fromStdString(column.name).endsWith("ID", Qt::CaseInsensitive);
    default:
        return false;
    }
}

// Three rows and their bullets: the list, beside the grid's own mark.
QPixmap list_mark(const QColor& ink, int size) {
    QPixmap drawn(QSize(size, size) * 3);
    drawn.setDevicePixelRatio(3);
    drawn.fill(Qt::transparent);
    QPainter painter(&drawn);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(ink, size / 12.0, Qt::SolidLine, Qt::RoundCap));
    for (int line = 0; line < 3; ++line) {
        const double y = size * (0.27 + 0.23 * line);
        painter.drawPoint(QPointF(size * 0.18, y));
        painter.drawLine(QPointF(size * 0.36, y), QPointF(size * 0.84, y));
    }
    return drawn;
}

QIcon two_state(const QPixmap& off, const QPixmap& on) {
    QIcon icon;
    icon.addPixmap(off, QIcon::Normal, QIcon::Off);
    icon.addPixmap(on, QIcon::Normal, QIcon::On);
    return icon;
}

// On or off, drawn as a switch: whether a change that breaks a rule is refused.
class PracticeSwitch final : public QCheckBox {
public:
    explicit PracticeSwitch(QWidget* parent) : QCheckBox(parent) {
        setCursor(Qt::PointingHandCursor);
        setFixedSize(sizeHint());
    }
    void wear(const Theme& theme) {
        theme_ = theme;
        update();
    }
    [[nodiscard]] QSize sizeHint() const override { return {38, 22}; }

protected:
    bool hitButton(const QPoint&) const override { return true; }
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF track = QRectF(rect()).adjusted(1, 2, -1, -2);
        painter.setPen(hasFocus() ? QPen(theme_.accent, 1.5) : Qt::NoPen);
        painter.setBrush(isChecked() ? theme_.accent : theme_.border);
        painter.drawRoundedRect(track, track.height() / 2, track.height() / 2);
        const double knob = track.height() - 4;
        const double x = isChecked() ? track.right() - 2 - knob : track.left() + 2;
        painter.setPen(Qt::NoPen);
        painter.setBrush(isChecked() ? theme_.selected_text : theme_.base);
        painter.drawEllipse(QRectF(x, track.top() + 2, knob, knob));
    }

private:
    Theme theme_{};
};

// One table's rows, and a row being typed in below them.
class PracticeModel final : public QAbstractTableModel {
public:
    struct Option {
        QString value;
        QString label;
    };
    PreviewTable shape;
    std::vector<std::size_t> columns;
    std::vector<PracticeRow> rows;
    std::map<std::pair<std::size_t, std::size_t>, domain::PracticeProblem> problems;
    std::optional<PracticeRow> draft;
    // What a foreign key can be given, by the column's place in the table.
    std::map<std::size_t, std::vector<Option>> options;
    bool enforced = true;
    Theme theme{};
    // A value typed over one already kept: the row, the column and the value.
    std::function<void(std::size_t, domain::ColumnIdentity, PracticeValue)> on_edit;

    void refill() {
        beginResetModel();
        endResetModel();
    }
    [[nodiscard]] const PreviewColumn& column(int section) const {
        return shape.columns[columns[static_cast<std::size_t>(section)]];
    }
    [[nodiscard]] bool is_draft(int row) const { return draft && static_cast<std::size_t>(row) == rows.size(); }
    [[nodiscard]] bool counted(int section) const { return column(section).auto_increment; }
    [[nodiscard]] PracticeValue value(int row, int section) const {
        const auto& described = column(section);
        if (is_draft(row)) {
            if (described.auto_increment) return domain::next_counted_value(domain::PracticeTable{rows}, described);
            return domain::practice_value(*draft, described);
        }
        return domain::practice_value(rows[static_cast<std::size_t>(row)], described);
    }
    [[nodiscard]] const domain::PracticeProblem* problem(int row, int section) const {
        if (is_draft(row)) return nullptr;
        const auto found = problems.find({static_cast<std::size_t>(row), columns[static_cast<std::size_t>(section)]});
        return found == problems.end() ? nullptr : &found->second;
    }
    [[nodiscard]] QString option_label(int section, const QString& value) const {
        const auto found = options.find(columns[static_cast<std::size_t>(section)]);
        if (found == options.end()) return {};
        for (const auto& option : found->second)
            if (option.value == value) return option.label;
        return {};
    }

    int rowCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() ? 0 : static_cast<int>(rows.size() + (draft ? 1 : 0));
    }
    int columnCount(const QModelIndex& parent = {}) const override {
        return parent.isValid() ? 0 : static_cast<int>(columns.size());
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if (orientation != Qt::Horizontal || section < 0 || section >= columnCount()) return {};
        if (role == Qt::DisplayRole) return text(column(section).name);
        if (role == Qt::ToolTipRole) {
            const auto& described = column(section);
            QStringList said;
            if (described.primary_key) said << "Primary key: no two rows share it, and it is never NULL.";
            if (described.foreign_key && described.references)
                said << "Foreign key: points at a row of a table that must exist.";
            if (described.unique && !described.primary_key) said << "Unique: no two rows share a value.";
            if (described.required && !described.primary_key) said << "Required (NOT NULL).";
            if (described.auto_increment) said << "Counted up by the database (IDENTITY).";
            return said.join("\n");
        }
        return {};
    }
    QVariant data(const QModelIndex& index, int role) const override {
        if (!index.isValid()) return {};
        const int row = index.row();
        const int section = index.column();
        const auto held = value(row, section);
        const auto* wrong = problem(row, section);
        switch (role) {
        case Qt::DisplayRole:
            return held ? text(*held) : QStringLiteral("NULL");
        case Qt::EditRole:
            return held ? text(*held) : QString();
        case Qt::FontRole:
            if (!held || (is_draft(row) && counted(section))) {
                QFont font;
                font.setItalic(true);
                return font;
            }
            return {};
        case Qt::ForegroundRole:
            if (wrong) return legible_on(theme.error, theme.base);
            if (!held || (is_draft(row) && counted(section))) return theme.muted;
            return theme.text;
        case Qt::BackgroundRole:
            if (wrong) return mix(theme.base, theme.error, 0.12);
            if (is_draft(row)) return mix(theme.base, theme.accent, 0.08);
            return {};
        case Qt::ToolTipRole: {
            if (wrong) return QString("<b>%1</b><br>%2").arg(text(wrong->title).toHtmlEscaped(),
                                                               text(wrong->message).toHtmlEscaped());
            if (counted(section)) return QStringLiteral("Counted up by the database (IDENTITY).");
            if (held) {
                const auto label = option_label(section, text(*held));
                if (!label.isEmpty()) return label;
            }
            return {};
        }
        default:
            return {};
        }
    }
    Qt::ItemFlags flags(const QModelIndex& index) const override {
        if (!index.isValid()) return Qt::NoItemFlags;
        auto result = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
        if (!(counted(index.column()) && (enforced || is_draft(index.row())))) result |= Qt::ItemIsEditable;
        return result;
    }
    bool setData(const QModelIndex& index, const QVariant& given, int role) override {
        if (!index.isValid() || role != Qt::EditRole) return false;
        const auto& described = column(index.column());
        const auto identity = domain::column_identity(described);
        if (!identity) return false;
        PracticeValue typed;
        if (given.isValid()) {
            const auto words = given.toString();
            if (!words.isEmpty() || written_as_text(described)) typed = bytes(words);
        }
        if (is_draft(index.row())) {
            if (typed) {
                draft->values[*identity] = typed;
            } else {
                draft->values.erase(*identity);
            }
            emit dataChanged(index, index);
            return true;
        }
        const auto row = static_cast<std::size_t>(index.row());
        // An empty cell left empty is not a change to the empty string.
        const auto held = value(index.row(), index.column());
        if (!held && typed && typed->empty()) return false;
        if (typed == held) return false;
        if (on_edit) on_edit(row, *identity, std::move(typed));
        return true;
    }
};

// A foreign key is chosen from the rows it can point at, each named by what
// it holds, as "1 — Computer Science"; the key alone is what is kept. Anything
// else can still be typed, which is how pointing at nothing is tried.
class PracticeDelegate final : public QStyledItemDelegate {
public:
    PracticeDelegate(PracticeModel* model, QObject* parent) : QStyledItemDelegate(parent), model_(model) {}

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        const auto found = model_->options.find(model_->columns[static_cast<std::size_t>(index.column())]);
        if (found == model_->options.end()) return QStyledItemDelegate::createEditor(parent, option, index);
        auto* box = new QComboBox(parent);
        box->setObjectName("practiceChoice");
        box->setEditable(true);
        box->setInsertPolicy(QComboBox::NoInsert);
        for (const auto& choice : found->second) box->addItem(choice.label, choice.value);
        return box;
    }
    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        auto* box = qobject_cast<QComboBox*>(editor);
        if (!box) return QStyledItemDelegate::setEditorData(editor, index);
        const auto value = index.data(Qt::EditRole).toString();
        const auto at = box->findData(value);
        if (at >= 0) box->setCurrentIndex(at);
        else box->setEditText(value);
    }
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        auto* box = qobject_cast<QComboBox*>(editor);
        if (!box) return QStyledItemDelegate::setModelData(editor, model, index);
        const auto words = box->currentText();
        const auto at = box->findText(words);
        model->setData(index, at >= 0 ? box->itemData(at) : QVariant(words.section(QStringLiteral(" — "), 0, 0).trimmed()),
                       Qt::EditRole);
    }

private:
    PracticeModel* model_;
};

// The column heads, each with the marks the schema gives its keys: the golden
// key and PK, FK beside it for a column that is both, and UQ for a unique one.
class KeyHeader final : public QHeaderView {
public:
    KeyHeader(PracticeModel* model, QWidget* parent) : QHeaderView(Qt::Horizontal, parent), model_(model) {
        setSectionsClickable(false);
        setHighlightSections(false);
        setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        setMinimumSectionSize(64);
    }
    Theme theme{};

protected:
    void paintSection(QPainter* painter, const QRect& rect, int logical) const override {
        painter->save();
        // The column heads stand out from the rows under them (Zain,
        // 2026-10-23): a middle shade of the theme's accent, neither the
        // rows' near-white nor a dark bar, so it suits every theme.
        painter->fillRect(rect, mix(theme.base, theme.accent, 0.24));
        painter->setPen(mix(theme.base, theme.accent, 0.42));
        painter->drawLine(rect.bottomLeft(), rect.bottomRight());
        painter->drawLine(rect.topRight(), rect.bottomRight());
        if (logical >= 0 && logical < model_->columnCount()) {
            const auto& column = model_->column(logical);
            auto font = this->font();
            font.setWeight(QFont::DemiBold);
            painter->setFont(font);
            painter->setPen(theme.text);
            const auto name = text(column.name);
            const auto area = rect.adjusted(8, 0, -4, 0);
            painter->drawText(area, Qt::AlignVCenter | Qt::AlignLeft, name);
            double x = area.left() + QFontMetrics(font).horizontalAdvance(name) + 6;
            for (const auto& [word, ink] : marks(column)) {
                if (word == QLatin1String("PK")) {
                    const auto key = primary_key_mark(13, devicePixelRatioF(), colourless(theme.id));
                    painter->drawPixmap(QPointF(x, rect.center().y() - 6.5), key);
                    x += 14;
                }
                x += badge(*painter, QPointF(x, rect.center().y()), word, ink) + 4;
            }
        }
        painter->restore();
    }
    QSize sectionSizeFromContents(int logical) const override {
        auto size = QHeaderView::sectionSizeFromContents(logical);
        if (logical < 0 || logical >= model_->columnCount()) return size;
        auto font = this->font();
        font.setWeight(QFont::DemiBold);
        int width = 8 + QFontMetrics(font).horizontalAdvance(text(model_->column(logical).name)) + 10;
        for (const auto& [word, ink] : marks(model_->column(logical))) {
            (void)ink;
            width += badge_width(word) + 4 + (word == QLatin1String("PK") ? 14 : 0);
        }
        size.setWidth(width);
        size.setHeight(30);
        return size;
    }

private:
    [[nodiscard]] std::vector<std::pair<QString, QColor>> marks(const PreviewColumn& column) const {
        std::vector<std::pair<QString, QColor>> result;
        if (column.primary_key) result.emplace_back("PK", theme.warning);
        if (column.foreign_key) result.emplace_back("FK", theme.valid);
        if (column.unique && !column.primary_key) result.emplace_back("UQ", theme.accent);
        return result;
    }
    [[nodiscard]] QFont badge_font() const {
        auto font = this->font();
        font.setPointSizeF(std::max(7.0, font.pointSizeF() * 0.78));
        font.setWeight(QFont::Bold);
        return font;
    }
    [[nodiscard]] int badge_width(const QString& word) const {
        return QFontMetrics(badge_font()).horizontalAdvance(word) + 8;
    }
    double badge(QPainter& painter, QPointF left_middle, const QString& word, const QColor& ink) const {
        const auto font = badge_font();
        const double width = badge_width(word);
        const double height = QFontMetrics(font).height() + 2;
        const QRectF area(left_middle.x(), left_middle.y() - height / 2, width, height);
        painter.setPen(QPen(mix(theme.base, ink, 0.55), 1));
        painter.setBrush(mix(theme.base, ink, 0.16));
        painter.drawRoundedRect(area, 4, 4);
        painter.setFont(font);
        painter.setPen(legible_on(ink, mix(theme.base, ink, 0.16)));
        painter.drawText(area, Qt::AlignCenter, word);
        return width;
    }
    PracticeModel* model_;
};

// A table's name, whole wherever the card has room for it, and shortened with
// its whole self on hover only where it has not.
class TableName final : public QLabel {
public:
    using QLabel::QLabel;
    void name(const QString& whole) {
        whole_ = whole;
        setText(whole);
        updateGeometry();
        fit();
    }
    [[nodiscard]] QSize minimumSizeHint() const override { return {40, QLabel::minimumSizeHint().height()}; }
    [[nodiscard]] QSize sizeHint() const override {
        return {fontMetrics().horizontalAdvance(whole_) + 2, QLabel::sizeHint().height()};
    }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QLabel::resizeEvent(event);
        fit();
    }

private:
    void fit() {
        const auto shown = fontMetrics().elidedText(whole_, Qt::ElideRight, width());
        if (shown != text()) setText(shown);
        setToolTip(shown == whole_ ? QString() : whole_);
    }
    QString whole_;
};

// A table of rows that says whether a cell is being typed in, which is when
// Delete belongs to the cell rather than to the rows.
class RowsView final : public QTableView {
public:
    using QTableView::QTableView;
    [[nodiscard]] bool typing() const { return state() == QAbstractItemView::EditingState; }
    // The size that shows every column and every row (Zain, 2026-10-23): what
    // a card is made at until it is pulled to another.
    QSize whole;
    QSize least;
    [[nodiscard]] QSize sizeHint() const override { return whole.isValid() ? whole : QTableView::sizeHint(); }
    [[nodiscard]] QSize minimumSizeHint() const override { return least.isValid() ? least : QTableView::minimumSizeHint(); }
};

// One table: its name, how many rows it has, whether they keep every rule,
// what to do with them, and the rows themselves.
class PracticeCard final : public QFrame {
public:
    PracticeCard(application::Editor& editor, domain::RelationId id, QWidget* parent)
        : QFrame(parent), editor_(editor), id_(id) {
        setObjectName("practiceCard");
        setFrameShape(QFrame::NoFrame);
        // Taken by an edge or a corner, a card is pulled to another size.
        setMouseTracking(true);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(14, 12, 14, 12);
        layout->setSpacing(8);
        auto* top = new QHBoxLayout;
        top->setSpacing(8);
        auto* naming = new QVBoxLayout;
        naming->setSpacing(1);
        // The name has the line to itself (Zain, 2026-10-23): a table whose
        // rows keep every rule says nothing more, and one whose rows do not
        // says so beside its row count, below.
        name_ = new TableName(this);
        name_->setObjectName("practiceCardName");
        naming->addWidget(name_);
        auto* count_line = new QHBoxLayout;
        count_line->setSpacing(6);
        count_ = new QLabel(this);
        count_->setObjectName("practiceRowCount");
        // A card is taken hold of by its heading, as a table on the schema is
        // by its title, and put down in front of another (Zain, 2026-10-23).
        for (QWidget* handle : {static_cast<QWidget*>(name_), static_cast<QWidget*>(count_)})
            handle->setCursor(Qt::OpenHandCursor);
        count_line->addWidget(count_);
        state_ = new QLabel(this);
        state_->setObjectName("practiceTrouble");
        state_->hide();
        count_line->addWidget(state_);
        count_line->addStretch();
        naming->addLayout(count_line);
        top->addLayout(naming, 1);
        add_ = new QPushButton("Add Row", this);
        add_->setObjectName("practiceAddRow");
        add_->setToolTip("Add a row to this table. Type its values, then Save Row.");
        connect(add_, &QPushButton::clicked, this, [this] { start_draft(); });
        top->addWidget(add_, 0, Qt::AlignTop);
        reset_ = new QPushButton("Reset", this);
        reset_->setObjectName("practiceReset");
        reset_->setToolTip("Put back the rows this table started with. The table, its columns and its rules are "
                           "not changed. Undo brings back what it replaced.");
        connect(reset_, &QPushButton::clicked, this, [this] { apply(editor_.reset_practice_table(id_)); });
        top->addWidget(reset_, 0, Qt::AlignTop);
        more_ = new QToolButton(this);
        more_->setObjectName("practiceMore");
        more_->setText(QStringLiteral("⋯"));
        more_->setToolTip("More for this table");
        more_->setPopupMode(QToolButton::InstantPopup);
        auto* menu = new QMenu(more_);
        auto* remove = menu->addAction("Delete Selected Rows", this, [this] { delete_selected(); });
        remove->setObjectName("practiceDeleteRows");
        auto* nothing = menu->addAction("Set Cell to NULL", this, [this] { set_null(); });
        nothing->setObjectName("practiceSetNull");
        menu->addSeparator();
        auto* clear = menu->addAction("Clear Table", this, [this] { apply(editor_.clear_practice_table(id_)); });
        clear->setObjectName("practiceClear");
        clear->setToolTip("Take every row out of this table. Its columns, keys and rules stay as they are.");
        more_->setMenu(menu);
        top->addWidget(more_, 0, Qt::AlignTop);
        layout->addLayout(top);

        message_ = new QLabel(this);
        message_->setObjectName("practiceMessage");
        message_->setWordWrap(true);
        message_->setTextFormat(Qt::RichText);
        message_->hide();
        layout->addWidget(message_);

        model_ = new PracticeModel;
        model_->setParent(this);
        model_->on_edit = [this](std::size_t row, domain::ColumnIdentity column, PracticeValue value) {
            // Made once the cell has finished being edited, so the table is
            // not refilled under the editor that is still handing its value in.
            QTimer::singleShot(0, this, [this, row, column, value] {
                apply(editor_.set_practice_value(id_, row, column, value));
            });
        };
        view_ = new RowsView(this);
        view_->setObjectName("practiceTable");
        header_ = new KeyHeader(model_, view_);
        view_->setHorizontalHeader(header_);
        view_->setModel(model_);
        view_->setItemDelegate(new PracticeDelegate(model_, view_));
        view_->verticalHeader()->hide();
        view_->verticalHeader()->setDefaultSectionSize(row_height);
        view_->setWordWrap(false);
        view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
        view_->setSelectionBehavior(QAbstractItemView::SelectItems);
        view_->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        view_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        view_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed
                               | QAbstractItemView::AnyKeyPressed | QAbstractItemView::SelectedClicked);
        view_->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(view_, &QWidget::customContextMenuRequested, this, [this](const QPoint& at) {
            QMenu menu(this);
            const auto index = view_->indexAt(at);
            if (index.isValid() && model_->flags(index).testFlag(Qt::ItemIsEditable))
                menu.addAction("Set to NULL", this, [this] { set_null(); });
            if (!view_->selectionModel()->selectedIndexes().isEmpty())
                menu.addAction("Delete Row", this, [this] { delete_selected(); });
            if (!menu.isEmpty()) menu.exec(view_->viewport()->mapToGlobal(at));
        });
        view_->installEventFilter(this);
        layout->addWidget(view_, 1);
        empty_ = new QLabel("No rows yet. Add Row puts one in.", this);
        empty_->setObjectName("practiceEmpty");
        empty_->hide();
        layout->addWidget(empty_);

        auto* drafting = new QHBoxLayout;
        drafting->addStretch();
        cancel_ = new QPushButton("Cancel", this);
        cancel_->setObjectName("practiceCancelRow");
        connect(cancel_, &QPushButton::clicked, this, [this] { drop_draft(); });
        drafting->addWidget(cancel_);
        save_ = new QPushButton("Save Row", this);
        save_->setObjectName("practiceSaveRow");
        save_->setToolTip("Add the row, if it keeps every rule.");
        connect(save_, &QPushButton::clicked, this, [this] { save_draft(); });
        drafting->addWidget(save_);
        layout->addLayout(drafting);
        cancel_->hide();
        save_->hide();
    }

    [[nodiscard]] domain::RelationId id() const { return id_; }
    [[nodiscard]] QString name() const { return text(model_->shape.name); }
    [[nodiscard]] std::size_t trouble() const { return troubled_rows_; }
    std::function<void(const application::EditResult&)> changed;

    void wear(const Theme& theme, IconMode mode) {
        theme_ = theme;
        mode_ = mode;
        model_->theme = theme;
        header_->theme = theme;
        add_->setIcon(QIcon(outline_pixmap("insert", theme.selected_text, 16)));
        reset_->setIcon(QIcon(outline_pixmap("undo", theme.text, 16)));
        model_->refill();
        show_state();
    }

    void update_from(const domain::SchemaPreview& preview, std::size_t t, const domain::PracticeData& data) {
        const auto& table = preview.tables[t];
        model_->shape = table;
        model_->columns = domain::practice_columns(table);
        const auto held = data.tables.find(table.id);
        model_->rows = held == data.tables.end() ? std::vector<PracticeRow>{} : held->second.rows;
        model_->enforced = data.enforced;
        model_->problems.clear();
        std::set<std::size_t> troubled;
        for (auto& problem : domain::practice_problems(preview, data, t)) {
            troubled.insert(problem.row);
            model_->problems.emplace(std::pair{problem.row, problem.column}, std::move(problem));
        }
        troubled_rows_ = troubled.size();
        model_->options.clear();
        for (const auto c : model_->columns) {
            const auto& column = table.columns[c];
            if (!column.foreign_key || !column.references || *column.references >= preview.tables.size()) continue;
            const auto& target = preview.tables[*column.references];
            if (column.references_column >= target.columns.size()) continue;
            // A row of the table pointed at is named by its first column of
            // words that is not a key, where it has one.
            std::optional<std::size_t> naming;
            for (const auto n : domain::practice_columns(target)) {
                const auto& candidate = target.columns[n];
                if (!candidate.primary_key && !candidate.foreign_key && written_as_text(candidate)) {
                    naming = n;
                    break;
                }
            }
            auto& choices = model_->options[c];
            const auto there = data.tables.find(target.id);
            if (there == data.tables.end()) continue;
            for (const auto& row : there->second.rows) {
                const auto key = domain::practice_value(row, target.columns[column.references_column]);
                if (!key) continue;
                QString label = text(*key);
                if (naming)
                    if (const auto said = domain::practice_value(row, target.columns[*naming]); said && !said->empty())
                        label += QStringLiteral(" — ") + text(*said);
                choices.push_back({text(*key), label});
            }
        }
        model_->refill();
        name_->name(name());
        const auto rows = model_->rows.size();
        count_->setText(rows == 0 ? QStringLiteral("No rows") : rows == 1 ? QStringLiteral("1 row")
                                                                          : QString("%1 rows").arg(rows));
        // Whatever this card said is about a change that has now been made or
        // taken back, unless the change it was about is the one just refused.
        if (!keep_message_) quiet();
        keep_message_ = false;
        show_state();
        fit_columns();
        fit_height();
    }

    // How big the table is whole -- every column, every row -- which is how
    // big the card is made until somebody pulls it to another size.
    void fit_height() {
        const int rows = std::max(model_->rowCount(), 1);
        const int frame = 2 * view_->frameWidth();
        const int head = header_->sizeHint().height();
        view_->whole = QSize(whole_columns_ + frame + 2, head + rows * row_height + frame + 2);
        view_->least = QSize(120, head + row_height + frame);
        view_->updateGeometry();
        empty_->setVisible(model_->rowCount() == 0);
        if (relayout) relayout();
    }

    // The card's own size with its whole table in it, and how tall it is at a
    // narrower width, where its table scrolls sideways.
    [[nodiscard]] QSize whole_size() const {
        // The card's own edge counts too, or the table comes out a pixel or
        // two short and scrolls for nothing.
        const auto wanted = sizeHint();
        return {std::max(wanted.width(), least_width()), wanted.height()};
    }
    [[nodiscard]] int height_at(int width) const {
        const auto whole = whole_size();
        return whole.height() + (width < whole.width() ? view_->horizontalScrollBar()->sizeHint().height() : 0);
    }
    [[nodiscard]] int least_width() const { return std::max(300, minimumSizeHint().width()); }
    [[nodiscard]] int least_height() const { return minimumSizeHint().height(); }
    // Told when the card's whole size changes, so the page can stand the cards
    // again; and when a hand has pulled it to another size.
    std::function<void()> relayout;
    std::function<void(QRect)> pulled;

protected:
    // Which of the card's edges a point is on: any of four, two at a corner.
    enum Edge { Left = 1, Right = 2, Top = 4, Bottom = 8 };
    [[nodiscard]] int edges_at(QPoint at) const {
        constexpr int reach = 6;
        int edges = 0;
        if (at.x() < reach) edges |= Left;
        if (at.x() >= width() - reach) edges |= Right;
        if (at.y() < reach) edges |= Top;
        if (at.y() >= height() - reach) edges |= Bottom;
        return edges;
    }
    void wear_edge_cursor(int edges) {
        if (edges == (Left | Top) || edges == (Right | Bottom)) setCursor(Qt::SizeFDiagCursor);
        else if (edges == (Right | Top) || edges == (Left | Bottom)) setCursor(Qt::SizeBDiagCursor);
        else if (edges & (Left | Right)) setCursor(Qt::SizeHorCursor);
        else if (edges & (Top | Bottom)) setCursor(Qt::SizeVerCursor);
        else unsetCursor();
    }
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            const auto at = event->position().toPoint();
            if (const auto edges = edges_at(at)) {
                pulling_ = edges;
                pulled_from_ = geometry();
                pull_start_ = event->globalPosition().toPoint();
                raise();
                return;
            }
            held_at_ = at;
        }
        QFrame::mousePressEvent(event);
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if (pulling_) {
            const auto moved = event->globalPosition().toPoint() - pull_start_;
            auto box = pulled_from_;
            const int narrowest = least_width();
            const int shortest = least_height();
            if (pulling_ & Left) box.setLeft(std::clamp(box.left() + moved.x(), 0, box.right() - narrowest));
            if (pulling_ & Right) box.setRight(std::max(box.left() + narrowest, box.right() + moved.x()));
            if (pulling_ & Top) box.setTop(std::clamp(box.top() + moved.y(), 0, box.bottom() - shortest));
            if (pulling_ & Bottom) box.setBottom(std::max(box.top() + shortest, box.bottom() + moved.y()));
            setGeometry(box);
            return;
        }
        if (!(event->buttons() & Qt::LeftButton)) {
            wear_edge_cursor(edges_at(event->position().toPoint()));
            return QFrame::mouseMoveEvent(event);
        }
        if (!held_at_ || !(event->buttons() & Qt::LeftButton)
            || (event->position().toPoint() - *held_at_).manhattanLength() < QApplication::startDragDistance())
            return QFrame::mouseMoveEvent(event);
        const auto held = *held_at_;
        held_at_.reset();
        auto* carried = new QMimeData;
        const auto& bytes = id_.value.bytes;
        carried->setData(moved_table, QByteArray(reinterpret_cast<const char*>(bytes.data()),
                                                 static_cast<qsizetype>(bytes.size())));
        const auto hold = held;
        carried->setData(moved_hold, QByteArray::number(hold.x()) + ',' + QByteArray::number(hold.y()));
        auto* drag = new QDrag(this);
        drag->setMimeData(carried);
        // What is carried is the card itself, held where it was taken.
        auto picture = grab();
        drag->setPixmap(picture);
        drag->setHotSpot(hold);
        drag->exec(Qt::MoveAction);
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        held_at_.reset();
        if (pulling_) {
            pulling_ = 0;
            if (geometry() != pulled_from_ && pulled) pulled(geometry());
            return;
        }
        QFrame::mouseReleaseEvent(event);
    }
    void leaveEvent(QEvent* event) override {
        if (!pulling_) unsetCursor();
        QFrame::leaveEvent(event);
    }
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == view_ && event->type() == QEvent::KeyPress
            && !view_->typing()) {
            const auto* key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace) {
                delete_selected();
                return true;
            }
            // Ctrl+0 sets a cell to NULL, as database tools have it.
            if (key->key() == Qt::Key_0 && key->modifiers().testFlag(Qt::ControlModifier)) {
                set_null();
                return true;
            }
        }
        return QFrame::eventFilter(watched, event);
    }

private:
    void show_state() {
        state_->setVisible(troubled_rows_ > 0);
        state_->setText(troubled_rows_ == 1 ? QStringLiteral("· 1 breaks a rule")
                                            : QString("· %1 break a rule").arg(troubled_rows_));
        state_->setToolTip("Point at a marked cell to read which rule it breaks.");
        state_->setStyleSheet(QString("QLabel#practiceTrouble { color: %1; font-size: 12px; font-weight: 600; }")
                                  .arg(legible_on(theme_.error, theme_.base).name()));
    }

    void fit_columns() {
        header_->setStretchLastSection(false);
        view_->resizeColumnsToContents();
        whole_columns_ = 0;
        for (int section = 0; section < model_->columnCount(); ++section) {
            header_->resizeSection(section, std::clamp(header_->sectionSize(section) + 12, 64, 260));
            whole_columns_ += header_->sectionSize(section);
        }
        header_->setStretchLastSection(true);
    }

    void apply(const application::EditResult& result) {
        if (result) quiet();
        else say(result.error);
        keep_message_ = !result;
        if (changed) changed(result);
        keep_message_ = false;
    }
    void say(const std::string& error) {
        const auto words = text(error);
        const auto split = words.indexOf('\n');
        const auto title = split < 0 ? QString() : words.left(split);
        const auto body = split < 0 ? words : words.mid(split + 1);
        message_->setText((title.isEmpty() ? QString() : "<b>" + title.toHtmlEscaped() + "</b><br>")
                          + body.toHtmlEscaped());
        message_->setStyleSheet(QString("QLabel#practiceMessage { color: %1; background: %2; border: 1px solid %3; "
                                        "border-radius: 6px; padding: 6px 9px; }")
                                    .arg(theme_.text.name(), mix(theme_.base, theme_.error, 0.08).name(),
                                         mix(theme_.base, theme_.error, 0.45).name()));
        message_->show();
    }
    void quiet() {
        message_->clear();
        message_->hide();
    }

    void start_draft() {
        if (!model_->draft) {
            model_->draft = PracticeRow{};
            model_->refill();
            save_->show();
            cancel_->show();
            fit_height();
        }
        const int row = model_->rowCount() - 1;
        for (int section = 0; section < model_->columnCount(); ++section) {
            const auto index = model_->index(row, section);
            if (model_->flags(index).testFlag(Qt::ItemIsEditable)) {
                view_->scrollTo(index);
                view_->setCurrentIndex(index);
                view_->edit(index);
                break;
            }
        }
    }
    void drop_draft() {
        model_->draft.reset();
        model_->refill();
        save_->hide();
        cancel_->hide();
        quiet();
        fit_height();
    }
    void save_draft() {
        if (!model_->draft) return;
        // A cell still open hands its value in first.
        if (auto* open = view_->focusWidget(); open && open != view_) view_->setFocus();
        const auto row = *model_->draft;
        const auto result = editor_.add_practice_row(id_, row);
        if (result) {
            model_->draft.reset();
            save_->hide();
            cancel_->hide();
        }
        apply(result);
    }
    void delete_selected() {
        std::set<std::size_t> rows;
        bool draft = false;
        for (const auto& index : view_->selectionModel()->selectedIndexes()) {
            if (model_->is_draft(index.row())) draft = true;
            else rows.insert(static_cast<std::size_t>(index.row()));
        }
        if (draft) drop_draft();
        if (rows.empty()) return;
        apply(editor_.delete_practice_rows(id_, {rows.begin(), rows.end()}));
    }
    void set_null() {
        const auto index = view_->currentIndex();
        if (!index.isValid() || !model_->flags(index).testFlag(Qt::ItemIsEditable)) return;
        model_->setData(index, QVariant(), Qt::EditRole);
    }

    application::Editor& editor_;
    domain::RelationId id_;
    Theme theme_{};
    IconMode mode_ = IconMode::Outline;
    TableName* name_ = nullptr;
    QLabel* state_ = nullptr;
    QLabel* count_ = nullptr;
    QLabel* message_ = nullptr;
    QLabel* empty_ = nullptr;
    QPushButton* add_ = nullptr;
    QPushButton* reset_ = nullptr;
    QPushButton* save_ = nullptr;
    QPushButton* cancel_ = nullptr;
    QToolButton* more_ = nullptr;
    RowsView* view_ = nullptr;
    KeyHeader* header_ = nullptr;
    PracticeModel* model_ = nullptr;
    std::size_t troubled_rows_ = 0;
    bool keep_message_ = false;
    std::optional<QPoint> held_at_;
    int pulling_ = 0;
    QRect pulled_from_;
    QPoint pull_start_;
    int whole_columns_ = 0;
};

// Where the cards stand, and where one being moved is put down: exactly where
// the hand lets it go (Zain, 2026-10-23), held as it was taken. An outline
// shows where it will land while it is carried, and the page scrolls when it is
// carried to an edge. Nothing else moves: the first card put down puts every
// card where it stood, and a gap left behind stays a gap.
class CardHost final : public QWidget {
public:
    explicit CardHost(QWidget* parent) : QWidget(parent) {
        setAcceptDrops(true);
        mark_ = new QFrame(this);
        mark_->setObjectName("practiceDropMark");
        mark_->setAttribute(Qt::WA_TransparentForMouseEvents);
        mark_->hide();
    }
    // The cards, shown or not.
    std::function<std::vector<PracticeCard*>()> cards;
    // Where the cards were put by hand, as the project keeps it.
    std::function<std::map<domain::RelationId, domain::Rect>()> places;
    // A card put down: which, and where every card now stands.
    std::function<void(domain::RelationId, std::map<domain::RelationId, domain::Rect>)> on_moved;
    QScrollArea* scroll = nullptr;
    void wear(const Theme& theme) {
        mark_->setStyleSheet(QString("QFrame#practiceDropMark { background: transparent; border: 2px dashed %1; "
                                     "border-radius: 10px; }").arg(theme.accent.name()));
    }

protected:
    void dragEnterEvent(QDragEnterEvent* event) override {
        if (carried(event->mimeData())) event->acceptProposedAction();
        else event->ignore();
    }
    void dragMoveEvent(QDragMoveEvent* event) override {
        const auto moving = carried(event->mimeData());
        auto* card = moving ? card_of(*moving) : nullptr;
        if (!card) return event->ignore();
        event->acceptProposedAction();
        const auto at = event->position().toPoint();
        mark_->setGeometry(QRect(landing(at, event->mimeData()), card->size()));
        mark_->show();
        mark_->raise();
        if (scroll) {
            const auto inside = scroll->viewport()->mapFrom(this, at);
            auto* down = scroll->verticalScrollBar();
            auto* across = scroll->horizontalScrollBar();
            if (inside.y() < 48) down->setValue(down->value() - 18);
            else if (inside.y() > scroll->viewport()->height() - 48) down->setValue(down->value() + 18);
            if (inside.x() < 48) across->setValue(across->value() - 18);
            else if (inside.x() > scroll->viewport()->width() - 48) across->setValue(across->value() + 18);
        }
    }
    void dragLeaveEvent(QDragLeaveEvent*) override { mark_->hide(); }
    void dropEvent(QDropEvent* event) override {
        mark_->hide();
        const auto moving = carried(event->mimeData());
        auto* card = moving ? card_of(*moving) : nullptr;
        if (!card || !on_moved) return event->ignore();
        event->acceptProposedAction();
        auto placed = pinned();
        const auto to = landing(event->position().toPoint(), event->mimeData());
        auto& place = placed[*moving];
        place.x = to.x();
        place.y = to.y();
        place.width = card->width();
        on_moved(*moving, std::move(placed));
    }

public:
    // Every card shown, where it stands now, so none of them moves when one is
    // put down or pulled; a card never pulled keeps a height of nought, as
    // tall as its rows.
    [[nodiscard]] std::map<domain::RelationId, domain::Rect> pinned() const {
        auto placed = places ? places() : std::map<domain::RelationId, domain::Rect>{};
        for (auto* each : cards())
            if (each->isVisible() && !placed.contains(each->id())) {
                const auto box = each->geometry();
                placed[each->id()] = domain::Rect{double(box.x()), double(box.y()), double(box.width()), 0};
            }
        return placed;
    }

protected:

private:
    [[nodiscard]] std::optional<domain::RelationId> carried(const QMimeData* data) const {
        if (!data || !data->hasFormat(moved_table)) return std::nullopt;
        const auto bytes = data->data(moved_table);
        domain::RelationId id;
        if (bytes.size() != static_cast<qsizetype>(id.value.bytes.size())) return std::nullopt;
        std::copy(bytes.begin(), bytes.end(), id.value.bytes.begin());
        return id;
    }
    [[nodiscard]] PracticeCard* card_of(domain::RelationId id) const {
        for (auto* card : cards())
            if (card->id() == id) return card;
        return nullptr;
    }
    // The card's top-left corner when let go here: under the hand where it was
    // held, and never off the top or the left of the page.
    [[nodiscard]] static QPoint landing(QPoint at, const QMimeData* data) {
        const auto hold = data->data(moved_hold).split(',');
        const QPoint held = hold.size() == 2 ? QPoint(hold[0].toInt(), hold[1].toInt()) : QPoint();
        const auto corner = at - held;
        return {std::max(0, corner.x()), std::max(0, corner.y())};
    }
    QFrame* mark_ = nullptr;
};

} // namespace

struct PracticeDataView::Impl {
    application::Editor& editor;
    PracticeDataView* self = nullptr;
    Theme theme{};
    IconMode mode = IconMode::Outline;
    QLabel* mark = nullptr;
    QPushButton* show_schema = nullptr;
    QFrame* status = nullptr;
    QLabel* status_mark = nullptr;
    QLabel* status_count = nullptr;
    QLabel* status_words = nullptr;
    PracticeSwitch* constraints = nullptr;
    QLabel* constraints_words = nullptr;
    QToolButton* grid_button = nullptr;
    QToolButton* list_button = nullptr;
    QLineEdit* search = nullptr;
    QAction* search_mark = nullptr;
    QScrollArea* scroll = nullptr;
    CardHost* host = nullptr;
    QGridLayout* grid = nullptr;
    QLabel* nothing = nullptr;
    std::map<domain::RelationId, PracticeCard*> cards;
    std::vector<PracticeCard*> order;
    bool listed = false;
    bool synchronizing = false;
    int across = 1;

    explicit Impl(application::Editor& owner) : editor(owner) {}

    // Where every card stands. Until a hand has put one somewhere, each card
    // is as big as its whole table and they stand side by side, the next one
    // beside the last wherever it fits and on a line below where it does not
    // (Zain, 2026-10-23); once one has been put or pulled by hand, every card
    // stands where it was put, at the size it was given, and nothing else
    // moves. The list stands them one to a line, as wide as the page.
    void reflow() {
        while (grid->count() > 0) delete grid->takeAt(0);
        const int width = std::max(scroll->viewport()->width() - 4, 300);
        const auto looking = search->text().trimmed();
        const auto& places = editor.project().practice.places;
        const bool by_hand = !listed && !places.empty();
        int shown_count = 0;
        QRect reach;
        std::vector<PracticeCard*> flowing;
        for (auto* card : order) {
            const bool shown = looking.isEmpty() || card->name().contains(looking, Qt::CaseInsensitive);
            card->setVisible(shown);
            if (!shown) continue;
            ++shown_count;
            const auto found = by_hand ? places.find(card->id()) : places.end();
            if (found == places.end()) {
                flowing.push_back(card);
                continue;
            }
            const auto& at = found->second;
            const int wide = std::max(card->least_width(), static_cast<int>(at.width));
            const int tall = at.height > 0 ? std::max(card->least_height(), static_cast<int>(at.height))
                                           : card->height_at(wide);
            card->setGeometry(static_cast<int>(at.x), static_cast<int>(at.y), wide, tall);
            card->raise();
            reach = reach.united(card->geometry());
        }
        // The rest flow: below what stands by hand, beside each other while
        // they fit.
        int x = 0;
        int y = reach.isNull() ? 0 : reach.bottom() + card_gap;
        int row = 0;
        across = 0;
        bool first_line = true;
        for (auto* card : flowing) {
            const int wide = listed ? width : std::min(card->whole_size().width(), width);
            if (x > 0 && x + wide > width) {
                x = 0;
                y += row + card_gap;
                row = 0;
                first_line = false;
            }
            const int tall = card->height_at(wide);
            card->setGeometry(x, y, wide, tall);
            reach = reach.united(card->geometry());
            row = std::max(row, tall);
            x += wide + card_gap;
            if (first_line) ++across;
        }
        if (listed) across = 1;
        host->setMinimumSize(reach.isNull() ? QSize(0, 0) : QSize(reach.right() + 4, reach.bottom() + 18));
        nothing->setVisible(shown_count == 0);
        nothing->setText(order.empty() ? "The schema has no tables yet."
                                       : "No table here is called that. Clear the search to see them all.");
    }

    void restyle() {
        const auto& t = theme;
        const auto soft = [&](const QColor& ink, double share) { return mix(t.base, ink, share).name(); };
        self->setStyleSheet(QString(R"(
#practiceData { background: %1; }
#practiceData QLabel { color: %2; background: transparent; }
#practiceData QLabel#practiceTitle { font-size: 22px; font-weight: 700; }
#practiceData QLabel#practiceSubtitle, #practiceData QLabel#practiceRowCount, #practiceData QLabel#practiceEmpty,
#practiceData QLabel#practiceNothing, #practiceData QLabel#practiceStatusWords { color: %3; }
#practiceData QLabel#practiceStatusCount { font-weight: 700; }
#practiceData QLabel#practiceCardName { font-size: 15px; font-weight: 700; }
#practiceData QFrame#practiceControls { background: %4; border: 1px solid %5; border-radius: 10px; }
#practiceData QFrame#practiceCard { background: %11; border: 1px solid %12; border-radius: 10px; }
#practiceData QFrame#practiceStatus { background: %6; border: 1px solid %7; border-radius: 10px; }
#practiceData QWidget#practiceCards, #practiceData QScrollArea#practiceScroll { background: %1; border: none; }
#practiceData QPushButton { background: %4; color: %2; border: 1px solid %5; border-radius: 6px; padding: 5px 11px; }
#practiceData QPushButton:hover { border-color: %8; }
#practiceData QPushButton#practiceAddRow, #practiceData QPushButton#practiceSaveRow { background: %8; color: %9; border-color: %8; font-weight: 600; }
#practiceData QToolButton#practiceGrid, #practiceData QToolButton#practiceList { background: %4; border: 1px solid %5; border-radius: 6px; padding: 4px; }
#practiceData QToolButton#practiceGrid:checked, #practiceData QToolButton#practiceList:checked { background: %8; border-color: %8; }
#practiceData QToolButton#practiceMore { background: transparent; color: %3; border: none; padding: 2px 6px; font-size: 16px; font-weight: 700; }
#practiceData QToolButton#practiceMore::menu-indicator { image: none; width: 0px; }
#practiceData QLineEdit#practiceSearch { background: %4; color: %2; border: 1px solid %5; border-radius: 7px; padding: 5px 8px; }
#practiceData QTableView#practiceTable { background: %4; color: %2; gridline-color: %5; border: 1px solid %5; border-radius: 6px; selection-background-color: %10; selection-color: %2; }
)")
                                .arg(t.window.name(), t.text.name(), t.muted.name(), t.base.name(), t.border.name(),
                                     soft(t.valid, 0.1), soft(t.valid, 0.4), t.accent.name(), t.selected_text.name(),
                                     hover_surface(t).name(),
                                     // Each card a little apart from the page it stands on (Zain,
                                     // 2026-10-23): lightly tinted with the theme's accent, and edged
                                     // in a deeper shade of it, so cards side by side read as cards.
                                     soft(t.accent, 0.09), soft(t.accent, 0.5)));
        const auto ink_for_mark = colourless(t.id) ? t.text : t.accent;
        mark->setPixmap(outline_pixmap("database", ink_for_mark, 34));
        show_schema->setIcon(QIcon(outline_pixmap("view", t.text, 16)));
        grid_button->setIcon(two_state(outline_pixmap("grid", t.text, 16), outline_pixmap("grid", t.selected_text, 16)));
        list_button->setIcon(two_state(list_mark(t.text, 16), list_mark(t.selected_text, 16)));
        search_mark->setIcon(QIcon(outline_pixmap("search", t.muted, 16)));
        constraints->wear(t);
        host->wear(t);
        for (auto* card : order) card->wear(t, mode);
        show_status();
    }

    void show_status() {
        std::size_t troubled = 0;
        for (auto* card : order)
            if (card->trouble() > 0) ++troubled;
        const auto tables = order.size();
        status_count->setText(tables == 1 ? QStringLiteral("1 table converted") : QString("%1 tables converted").arg(tables));
        status_words->setText(troubled == 0 ? QStringLiteral("Practice tables ready")
                              : troubled == 1 ? QStringLiteral("1 table has rows that break a rule")
                                              : QString("%1 tables have rows that break a rule").arg(troubled));
        status_mark->setPixmap(outline_pixmap("validate", troubled == 0 ? theme.valid : theme.error, 22));
        const auto ink = troubled == 0 ? theme.valid : theme.error;
        status->setStyleSheet(QString("QFrame#practiceStatus { background: %1; border: 1px solid %2; border-radius: 10px; }")
                                  .arg(mix(theme.base, ink, 0.1).name(), mix(theme.base, ink, 0.4).name()));
    }

    void show_constraints(bool enforced) {
        const QSignalBlocker quiet(constraints);
        constraints->setChecked(enforced);
        constraints_words->setText(enforced ? "Constraints active" : "Constraints off");
        constraints->setToolTip(enforced
            ? "A change that breaks a key or a rule is refused, as a database refuses it."
            : "A change that breaks a key or a rule is kept, and the cells that break it are marked.");
        constraints_words->setToolTip(constraints->toolTip());
    }
};

PracticeDataView::PracticeDataView(application::Editor& editor, QWidget* parent)
    : QWidget(parent), impl_(std::make_unique<Impl>(editor)) {
    auto& d = *impl_;
    d.self = this;
    setObjectName("practiceData");
    setAttribute(Qt::WA_StyledBackground, true);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 18, 22, 0);
    layout->setSpacing(14);

    // What this is, and the way back to the schema.
    auto* header = new QHBoxLayout;
    header->setSpacing(12);
    d.mark = new QLabel(this);
    d.mark->setObjectName("practiceMark");
    header->addWidget(d.mark, 0, Qt::AlignTop);
    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    auto* title = new QLabel("Practice Data", this);
    title->setObjectName("practiceTitle");
    titles->addWidget(title);
    auto* subtitle = new QLabel("Your schema has been converted into practice tables. Add, edit, and test data "
                                "without writing SQL.", this);
    subtitle->setObjectName("practiceSubtitle");
    subtitle->setWordWrap(true);
    titles->addWidget(subtitle);
    header->addLayout(titles, 1);
    d.show_schema = new QPushButton("Show Schema", this);
    d.show_schema->setObjectName("practiceShowSchema");
    d.show_schema->setToolTip("Back to the schema, exactly as it was. The practice rows are kept.");
    connect(d.show_schema, &QPushButton::clicked, this, [this] {
        if (on_show_schema) on_show_schema();
    });
    header->addWidget(d.show_schema, 0, Qt::AlignVCenter);
    d.status = new QFrame(this);
    d.status->setObjectName("practiceStatus");
    auto* status_layout = new QHBoxLayout(d.status);
    status_layout->setContentsMargins(12, 8, 16, 8);
    status_layout->setSpacing(10);
    d.status_mark = new QLabel(d.status);
    status_layout->addWidget(d.status_mark);
    auto* status_words = new QVBoxLayout;
    status_words->setSpacing(0);
    d.status_count = new QLabel(d.status);
    d.status_count->setObjectName("practiceStatusCount");
    status_words->addWidget(d.status_count);
    d.status_words = new QLabel(d.status);
    d.status_words->setObjectName("practiceStatusWords");
    status_words->addWidget(d.status_words);
    status_layout->addLayout(status_words);
    header->addWidget(d.status, 0, Qt::AlignVCenter);
    layout->addLayout(header);

    // Finding a table among many, whether the rules are held to, and how the
    // tables are laid out. No grouping of tables into kinds: every table is in
    // the one grid, found by its name.
    auto* controls = new QFrame(this);
    controls->setObjectName("practiceControls");
    auto* bar = new QHBoxLayout(controls);
    bar->setContentsMargins(10, 8, 10, 8);
    bar->setSpacing(6);
    d.search = new QLineEdit(controls);
    d.search->setObjectName("practiceSearch");
    d.search->setPlaceholderText("Search tables…");
    d.search->setClearButtonEnabled(true);
    d.search->setFixedWidth(260);
    d.search_mark = d.search->addAction(QIcon(), QLineEdit::LeadingPosition);
    connect(d.search, &QLineEdit::textChanged, this, [this] { impl_->reflow(); });
    bar->addWidget(d.search);
    bar->addStretch(1);
    d.constraints = new PracticeSwitch(controls);
    d.constraints->setObjectName("practiceConstraints");
    connect(d.constraints, &QCheckBox::toggled, this, [this](bool on) {
        const auto result = impl_->editor.enforce_practice_constraints(on);
        if (on_changed) on_changed(result);
    });
    bar->addWidget(d.constraints);
    d.constraints_words = new QLabel(controls);
    d.constraints_words->setObjectName("practiceConstraintsWords");
    bar->addWidget(d.constraints_words);
    bar->addSpacing(10);
    auto* layouts = new QButtonGroup(this);
    d.grid_button = new QToolButton(controls);
    d.grid_button->setObjectName("practiceGrid");
    d.grid_button->setToolTip("Grid: several tables side by side");
    d.grid_button->setCheckable(true);
    d.grid_button->setChecked(true);
    d.list_button = new QToolButton(controls);
    d.list_button->setObjectName("practiceList");
    d.list_button->setToolTip("List: one table to a line, as wide as the window");
    d.list_button->setCheckable(true);
    layouts->addButton(d.grid_button);
    layouts->addButton(d.list_button);
    connect(layouts, &QButtonGroup::buttonClicked, this, [this](QAbstractButton* button) {
        impl_->listed = button == impl_->list_button;
        impl_->reflow();
    });
    bar->addWidget(d.grid_button);
    bar->addWidget(d.list_button);
    layout->addWidget(controls);

    // The cards. The page scrolls, as any long page does; a card's own table
    // scrolls sideways where it has more columns than room.
    d.scroll = new QScrollArea(this);
    d.scroll->setObjectName("practiceScroll");
    d.scroll->setWidgetResizable(true);
    d.scroll->setFrameShape(QFrame::NoFrame);
    d.scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    d.host = new CardHost(d.scroll);
    d.host->scroll = d.scroll;
    d.host->cards = [this] { return impl_->order; };
    d.host->places = [this] { return impl_->editor.project().practice.places; };
    d.host->on_moved = [this](domain::RelationId table, std::map<domain::RelationId, domain::Rect> places) {
        const auto result = impl_->editor.move_practice_table(table, std::move(places));
        if (on_changed) on_changed(result);
    };
    d.host->setObjectName("practiceCards");
    auto* host_layout = new QVBoxLayout(d.host);
    host_layout->setContentsMargins(0, 0, 0, 18);
    host_layout->setSpacing(0);
    d.nothing = new QLabel(d.host);
    d.nothing->setObjectName("practiceNothing");
    d.nothing->setAlignment(Qt::AlignCenter);
    d.nothing->hide();
    host_layout->addWidget(d.nothing);
    d.grid = new QGridLayout;
    d.grid->setHorizontalSpacing(card_gap);
    d.grid->setVerticalSpacing(card_gap);
    host_layout->addLayout(d.grid, 1);
    d.scroll->setWidget(d.host);
    layout->addWidget(d.scroll, 1);
}

PracticeDataView::~PracticeDataView() = default;

void PracticeDataView::set_theme(const Theme& theme, IconMode mode) {
    impl_->theme = theme;
    impl_->mode = mode;
    impl_->restyle();
}

void PracticeDataView::synchronize() {
    auto& d = *impl_;
    const auto& project = d.editor.project();
    const auto preview = domain::schema_preview(project);
    d.synchronizing = true;
    std::vector<PracticeCard*> order;
    std::set<domain::RelationId> present;
    for (std::size_t t = 0; t < preview.tables.size(); ++t) {
        const auto id = preview.tables[t].id;
        present.insert(id);
        auto& card = d.cards[id];
        if (!card) {
            card = new PracticeCard(d.editor, id, d.host);
            card->wear(d.theme, d.mode);
            card->changed = [this](const application::EditResult& result) {
                if (on_changed) on_changed(result);
            };
            card->relayout = [this] {
                if (!impl_->synchronizing) impl_->reflow();
            };
            card->pulled = [this, id](QRect box) {
                auto places = impl_->host->pinned();
                places[id] = domain::Rect{double(box.x()), double(box.y()), double(box.width()), double(box.height())};
                const auto result = impl_->editor.resize_practice_table(id, std::move(places));
                if (on_changed) on_changed(result);
                impl_->reflow();
            };
        }
        card->update_from(preview, t, project.practice);
        order.push_back(card);
    }
    for (auto at = d.cards.begin(); at != d.cards.end();) {
        if (present.contains(at->first)) {
            ++at;
            continue;
        }
        at->second->deleteLater();
        at = d.cards.erase(at);
    }
    d.order = std::move(order);
    d.synchronizing = false;
    d.show_constraints(project.practice.enforced);
    d.show_status();
    d.reflow();
}

int PracticeDataView::columns_across() const { return impl_->across; }

void PracticeDataView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    // The scroll area learns its new width once laid out, so the cards are
    // laid out again after it.
    QTimer::singleShot(0, this, [this] { impl_->reflow(); });
}

void PracticeDataView::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    impl_->reflow();
}

} // namespace erdflow::desktop
