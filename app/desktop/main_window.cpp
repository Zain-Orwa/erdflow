// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "main_window.hpp"
#include "export_dialog.hpp"
#include "ribbon.hpp"
#include "search_bar.hpp"
#include "symbol_picker.hpp"
#include "symbols.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QActionGroup>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QApplication>
#include <QBuffer>
#include <QCloseEvent>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QAbstractSpinBox>
#include <QDoubleSpinBox>
#include <QSpinBox>
#include <QClipboard>
#include <QFileDialog>
#include <QDir>
#include <QFileInfo>
#include <QFocusEvent>
#include <QFontMetricsF>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QIcon>
#include <QImageReader>
#include <QLabel>
#include <QFrame>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QIntValidator>
#include <QListWidget>
#include <QScreen>
#include <QMenuBar>
#include <QMenu>
#include <QColorDialog>
#include <QMessageBox>
#include <QMimeData>
#include <QPaintEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QPushButton>
#include <QScopeGuard>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QSignalBlocker>
#include <QSlider>
#include <QStandardItemModel>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStatusBar>
#include <QResizeEvent>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QTreeWidget>
#include <QDateTime>
#include <QVariantAnimation>
#include <QEasingCurve>
#include <QVBoxLayout>
#include <QWidgetAction>

#include <algorithm>
#include <cmath>
#include <array>

namespace erdflow::desktop {
namespace {
using namespace domain;
QString text(const std::string& value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }
std::string bytes(const QString& value) { return value.toUtf8().toStdString(); }
QString key(ElementRef ref) {
    const auto id = uuid(ref);
    return QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(id.bytes.data()), 16).toHex());
}
QString kind_label(ElementRef ref);
// A plain note is a symbol rather than a note, and the panel has to call it
// what it is: a card and a character drawn bare are not the same thing to
// anyone looking at them, whatever they share underneath.
QString kind_label(const Project& project, ElementRef ref) {
    if (const auto* note_id = std::get_if<NoteId>(&ref)) {
        const auto found = project.notes.find(*note_id);
        if (found != project.notes.end() && found->second.plain) return QStringLiteral("Symbol");
    }
    return kind_label(ref);
}
// What an element actually is, said in full: not "Attribute" but "Derived
// attribute", not "Entity" but "Weak entity". The Explorer draws each element
// as the shape the canvas draws it as, and this is the same thing in words, for
// anyone pointing at the row rather than reading the shape.
QString kind_description(const Project& project, ElementRef ref) {
    if (const auto* id = std::get_if<EntityId>(&ref)) {
        const auto found = project.entities.find(*id);
        return found != project.entities.end() && found->second.weak ? QStringLiteral("Weak entity")
                                                                     : QStringLiteral("Entity");
    }
    if (const auto* id = std::get_if<AttributeId>(&ref)) {
        const auto found = project.attributes.find(*id);
        if (found == project.attributes.end()) return QStringLiteral("Attribute");
        const auto& attribute = found->second;
        // A key on a weak entity identifies an instance only once the owner is
        // known, so it is a partial key and must not be called a key.
        const bool weak_owner = attribute.owner && std::holds_alternative<EntityId>(*attribute.owner)
            && [&] {
                   const auto owner = project.entities.find(std::get<EntityId>(*attribute.owner));
                   return owner != project.entities.end() && owner->second.weak;
               }();
        switch (attribute.kind) {
            case AttributeKind::Key: return weak_owner ? QStringLiteral("Partial key") : QStringLiteral("Key attribute");
            case AttributeKind::Composite: return QStringLiteral("Composite attribute");
            case AttributeKind::Multivalued: return QStringLiteral("Multivalued attribute");
            case AttributeKind::Derived: return QStringLiteral("Derived attribute");
            case AttributeKind::Normal: break;
        }
        return QStringLiteral("Attribute");
    }
    if (const auto* id = std::get_if<RelationshipId>(&ref)) {
        const auto found = project.relationships.find(*id);
        if (found == project.relationships.end()) return QStringLiteral("Relationship");
        switch (relationship_kind(found->second)) {
            case RelationshipKind::Identifying: return QStringLiteral("Identifying relationship");
            case RelationshipKind::Associative: return QStringLiteral("Associative relationship");
            case RelationshipKind::Regular: break;
        }
        return QStringLiteral("Relationship");
    }
    if (const auto* id = std::get_if<SpecializationId>(&ref)) {
        const auto found = project.specializations.find(*id);
        return found != project.specializations.end() && found->second.direction == Inheritance::Generalization
            ? QStringLiteral("Generalization") : QStringLiteral("Specialization");
    }
    return kind_label(project, ref);
}

QString kind_label(ElementRef ref) {
    if (std::holds_alternative<EntityId>(ref)) return QStringLiteral("Entity");
    if (std::holds_alternative<AttributeId>(ref)) return QStringLiteral("Attribute");
    if (std::holds_alternative<SpecializationId>(ref)) return QStringLiteral("Specialization");
    if (std::holds_alternative<PictureId>(ref)) return QStringLiteral("Picture");
    if (std::holds_alternative<NoteId>(ref)) return QStringLiteral("Note");
    return QStringLiteral("Relationship");
}
// The colour an element is actually drawn with on the canvas: the one it was
// given if it has one, and otherwise whatever the theme gives its kind. The
// panel reads this so that it shows the element's own colour rather than only
// the colours a user happened to choose by hand.
// The comment written for the database, for the three things that carry one.
std::string schema_comment_of(const Project& project, const ElementRef& ref) {
    if (const auto* id = std::get_if<EntityId>(&ref)) {
        const auto found = project.entities.find(*id);
        return found == project.entities.end() ? std::string{} : found->second.comment;
    }
    if (const auto* id = std::get_if<AttributeId>(&ref)) {
        const auto found = project.attributes.find(*id);
        return found == project.attributes.end() ? std::string{} : found->second.comment;
    }
    if (const auto* id = std::get_if<RelationshipId>(&ref)) {
        const auto found = project.relationships.find(*id);
        return found == project.relationships.end() ? std::string{} : found->second.comment;
    }
    return {};
}

QColor surface_of(const Project& project, const Theme& colors, ElementRef ref) {
    // A see-through surface shows the canvas through it, so what the panel
    // has to match is the blend the eye sees rather than the paint on its own.
    const auto faded = project.transparency.find(ref);
    const auto seen = [&](QColor paint) {
        if (faded != project.transparency.end()) paint.setAlphaF(static_cast<float>(1.0 - faded->second / 100.0));
        return over(colors.canvas, paint);
    };
    if (const auto chosen = project.colours.find(ref); chosen != project.colours.end())
        return seen(QColor(chosen->second.red, chosen->second.green, chosen->second.blue));
    if (std::holds_alternative<AttributeId>(ref)) return seen(colors.attribute_fill);
    if (std::holds_alternative<EntityId>(ref)) return seen(colors.entity_fill);
    if (std::holds_alternative<SpecializationId>(ref)) return seen(colors.isa_fill);
    if (std::holds_alternative<PictureId>(ref)) return seen(colors.panel);
    if (std::holds_alternative<NoteId>(ref)) return seen(note_surface(colors));
    // An associative relationship converts to a relation of its own and wears
    // the entity palette on the canvas, so it wears it here too.
    const auto& relationship = project.relationships.at(std::get<RelationshipId>(ref));
    return seen(relationship.associative ? colors.entity_fill : colors.relationship_fill);
}

// The relationships that join one entity to itself: the ones naming it as a
// participant more than once. A recursive entity is one that has any.
std::vector<ElementRef> recursions_of(const Project& project, EntityId id) {
    std::vector<ElementRef> found;
    for (const auto& [relationship_id, relationship] : project.relationships) {
        std::size_t touches = 0;
        for (const auto& participant : relationship.participants)
            if (participant.target == ParticipantTarget{id}) ++touches;
        if (touches > 1) found.emplace_back(relationship_id);
    }
    return found;
}

// The name a background style goes by, for the actions that choose it.
QString background_key(domain::BackgroundStyle style) {
    switch (style) {
    case domain::BackgroundStyle::Theme: return QStringLiteral("Theme");
    case domain::BackgroundStyle::Squares: return QStringLiteral("Squares");
    case domain::BackgroundStyle::Lines: return QStringLiteral("Lines");
    case domain::BackgroundStyle::Dots: return QStringLiteral("Dots");
    case domain::BackgroundStyle::Image: return QStringLiteral("Image");
    }
    return QStringLiteral("Theme");
}

QString display_name(const Project& project, ElementRef ref) {
    const auto value = text(name(project, ref));
    return value.isEmpty() ? QStringLiteral("(unnamed)") : value;
}
// The name at the top of a card. A side means nothing on its own: it is that
// element as it takes part in this one, so the card says both, with an arrow
// between them. Each wears the colour it is drawn in on the diagram, and each
// ink is chosen against its own colour rather than taken from the theme, since
// either colour may be one the user picked.
// How much of a shape's width its outline takes before the name can start. A
// diamond or a triangle narrows away from its middle and needs far more of it
// than a rectangle or an oval does.
double inset_of(bool pointed) { return pointed ? 0.22 : 0.09; }

// The room an element's shape would like: its own proportions on the diagram,
// and no less than its name takes to read in full. It is a wish rather than a
// rule -- the panel gives what it has, and the name is elided when that is
// less -- so nothing ever runs out of the panel whatever its width.
int wanted_width(double aspect, const QString& name, bool pointed, int tall, double points) {
    QFont font;
    font.setPointSizeF(points);
    font.setWeight(QFont::DemiBold);
    const auto text = QFontMetricsF(font).horizontalAdvance(name);
    const auto for_the_name = text / std::max(1.0 - 2 * inset_of(pointed), 0.2) + 14;
    return static_cast<int>(std::lround(std::max<double>(tall * std::max(aspect, 0.2), for_the_name)));
}

// The element's own shape with its name written inside it. The name is the
// thing the user gave it, so it is edited on the shape it belongs to rather
// than in a box beside a picture of one. The shape is redrawn whenever the
// room changes, so it stays crisp at any width.
class ShapedName final : public QWidget {
public:
    ShapedName(std::function<QPixmap(QSize)> draw, double aspect, const QString& name, bool pointed, QWidget* parent)
        : QWidget(parent), draw_(std::move(draw)), pointed_(pointed) {
        setObjectName("elementShape");
        setFixedHeight(56);
        wanted_ = wanted_width(aspect, name, pointed, 56, 13.0);
        // Asked for as much as the name wants, given as much as the panel has.
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }
    [[nodiscard]] QSize sizeHint() const override { return {wanted_, height()}; }
    [[nodiscard]] QSize minimumSizeHint() const override { return {56, height()}; }
    void hold(QLineEdit* field) {
        field_ = field;
        place();
    }
protected:
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        shape_ = draw_(size());
        place();
    }
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.drawPixmap(0, 0, shape_);
    }
private:
    void place() {
        if (!field_ || width() < 2) return;
        // A diamond or a triangle narrows away from its middle, so the name
        // is given only the room its shape actually has there.
        const auto inset = width() * inset_of(pointed_);
        const auto tall = std::min(30, height() - 10);
        field_->setGeometry(static_cast<int>(inset), (height() - tall) / 2,
                            std::max(24, static_cast<int>(width() - inset * 2)), tall);
    }
    std::function<QPixmap(QSize)> draw_;
    bool pointed_;
    int wanted_ = 56;
    QPixmap shape_;
    QLineEdit* field_ = nullptr;
};

// An element's shape with its name written inside it, for the places that only
// show a name rather than letting it be edited. The name is drawn rather than
// laid out, so it can be held inside an outline that narrows away from its
// middle, which no ordinary label would respect.
class ShapedTag final : public QWidget {
public:
    ShapedTag(std::function<QPixmap(QSize)> draw, QString name, QColor ink, double aspect, bool pointed,
              const char* named, QWidget* parent)
        : QWidget(parent), draw_(std::move(draw)), name_(std::move(name)), ink_(std::move(ink)), pointed_(pointed) {
        setObjectName(QString::fromLatin1(named));
        // Read out and hovered as the name it stands for, since the text is
        // painted rather than held by a label of its own.
        setAccessibleName(name_);
        setToolTip(name_);
        font_ = font();
        font_.setPointSizeF(10.5);
        font_.setWeight(QFont::DemiBold);
        setFixedHeight(30);
        wanted_ = wanted_width(aspect, name_, pointed, 30, 10.5);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }
    [[nodiscard]] QSize sizeHint() const override { return {wanted_, height()}; }
    // Never squeezed so far that its name is only a mark: an end that cannot
    // show a few letters says nothing at all.
    [[nodiscard]] QSize minimumSizeHint() const override { return {54, height()}; }
protected:
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        shape_ = draw_(size());
    }
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.drawPixmap(0, 0, shape_);
        painter.setFont(font_);
        painter.setPen(ink_);
        const auto inset = width() * inset_of(pointed_);
        const QRectF room(inset, 0, width() - inset * 2, height());
        painter.drawText(room, Qt::AlignCenter,
                         QFontMetricsF(font_).elidedText(name_, Qt::ElideRight, room.width()));
    }
private:
    std::function<QPixmap(QSize)> draw_;
    QString name_;
    QColor ink_;
    bool pointed_;
    int wanted_ = 44;
    QFont font_;
    QPixmap shape_;
};

// The name at the top of a card. A side means nothing on its own: it is that
// element as it takes part in this one, so the card says both, with an arrow
// between them, each written inside the shape and colour it is drawn with.
QWidget* card_title(QWidget* side, QWidget* toward, QWidget* parent) {
    auto* row = new QWidget(parent);
    row->setObjectName("cardTitle");
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 2, 0, 4);
    layout->setSpacing(7);
    side->setParent(row);
    toward->setParent(row);
    layout->addWidget(side, 0);
    auto* arrow = new QLabel(QStringLiteral("\u2192"), row);
    arrow->setObjectName("cardArrow");
    arrow->setStyleSheet(QStringLiteral("QLabel#cardArrow { font-size: 13px; font-weight: 800; }"));
    layout->addWidget(arrow, 0);
    layout->addWidget(toward, 0);
    layout->addStretch();
    return row;
}
// A field's name inside a card: bold, and in a hue of its own, so the several
// things asked about one side are told apart at a glance rather than read in
// order. The hues come from the theme and change with it.
QLabel* field_label(const QString& text, const QColor& ink, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName("fieldLabel");
    label->setStyleSheet(QStringLiteral("QLabel#fieldLabel { color: %1; font-weight: 700; }").arg(ink.name()));
    return label;
}
// The bytes of an image file, ready to be kept in a project. They are the
// file's own when it is already a PNG or JPEG of modest size, so nothing is
// lost; anything else is re-encoded, and scaled down first when it is large,
// since a project file has room for only so much picture.
std::optional<std::vector<std::uint8_t>> encoded_image(const QString& path, QString& why_not,
                                                      int longest = 1024, qint64 keep_below = 1024 * 1024) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const auto format = reader.format().toLower();
    const auto image = reader.read();
    if (image.isNull()) {
        why_not = reader.errorString().isEmpty() ? QStringLiteral("The file is not an image ERDFlow can read.")
                                                 : reader.errorString();
        return std::nullopt;
    }
    QByteArray encoded;
    QFile file(path);
    if ((format == "png" || format == "jpeg" || format == "jpg") && file.size() <= keep_below
        && file.open(QIODevice::ReadOnly)) {
        encoded = file.readAll();
    } else {
        // The best of the sizes that fits, rather than one size and a refusal:
        // a picture is worth keeping as large as the file has room for.
        for (auto side = longest; side >= 512; side = side * 3 / 4) {
            auto fitted = image;
            if (fitted.width() > side || fitted.height() > side)
                fitted = fitted.scaled(side, side, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            encoded.clear();
            QBuffer buffer(&encoded);
            buffer.open(QIODevice::WriteOnly);
            // A photograph is far smaller as a JPEG; anything transparent has to stay a PNG.
            if (fitted.hasAlphaChannel()) fitted.save(&buffer, "PNG");
            else fitted.save(&buffer, "JPEG", 90);
            if (static_cast<std::size_t>(encoded.size()) <= domain::max_image_bytes) break;
        }
    }
    if (encoded.isEmpty() || static_cast<std::size_t>(encoded.size()) > domain::max_image_bytes) {
        why_not = QStringLiteral("The picture is too large to keep in a project file.");
        return std::nullopt;
    }
    return std::vector<std::uint8_t>(encoded.begin(), encoded.end());
}

// The type catalogue as it is offered: grouped by family, and led by the ones
// a designer reaches for constantly, so the common answer is never more than a
// glance away while the whole list is still there.
struct TypeFamily { const char* name; std::vector<domain::LogicalType> types; };

const std::vector<TypeFamily>& type_families() {
    using T = domain::LogicalType;
    static const std::vector<TypeFamily> families{
        {"Most used", {T::Int, T::Varchar, T::NVarchar, T::Bit, T::Date, T::DateTime2,
                       T::Decimal, T::BigInt, T::UniqueIdentifier}},
        {"Exact numerics", {T::Int, T::BigInt, T::SmallInt, T::TinyInt, T::Bit, T::Decimal,
                            T::Numeric, T::Money, T::SmallMoney}},
        {"Approximate numerics", {T::Float, T::Real}},
        {"Character strings", {T::Varchar, T::VarcharMax, T::Char, T::Text}},
        {"Unicode character strings", {T::NVarchar, T::NVarcharMax, T::NChar, T::NText}},
        {"Binary strings", {T::Varbinary, T::VarbinaryMax, T::Binary, T::Image}},
        {"Date and time", {T::Date, T::Time, T::DateTime2, T::DateTimeOffset,
                           T::DateTime, T::SmallDateTime}},
        {"Others", {T::UniqueIdentifier, T::Xml, T::RowVersion, T::HierarchyId,
                    T::SqlVariant, T::Cursor, T::Table, T::Geometry, T::Geography}}};
    return families;
}

// Every type there is, in one run from the one reached for constantly to the
// one hardly reached for at all. Not grouped: a reader looking for a type is
// looking for a name, and families only put headings between them and it. The
// order is the list's whole argument, so it is written out rather than sorted
// from anything.
const std::vector<domain::LogicalType>& types_by_use() {
    using T = domain::LogicalType;
    static const std::vector<domain::LogicalType> order{
        T::Int, T::Varchar, T::NVarchar, T::Bit, T::DateTime2, T::Date, T::Decimal,
        T::BigInt, T::UniqueIdentifier, T::NVarcharMax, T::VarcharMax, T::Char,
        T::NChar, T::SmallInt, T::Money, T::Float, T::TinyInt, T::Time,
        T::DateTimeOffset, T::DateTime, T::Numeric, T::Varbinary, T::VarbinaryMax,
        T::Real, T::SmallDateTime, T::SmallMoney, T::Binary, T::Xml, T::RowVersion,
        T::Text, T::NText, T::Image, T::HierarchyId, T::SqlVariant, T::Geography,
        T::Geometry, T::Cursor, T::Table};
    return order;
}

// What a measured type is usually measured in. A length means different
// things to different types -- characters to a varchar, fractional-second
// digits to a datetime2, mantissa bits to a float -- so what is offered
// follows the type rather than being one list for all of them.
struct SizeAdvice {
    QString what;
    QString hint;
    std::vector<int> common;
};

SizeAdvice size_advice(domain::LogicalType type) {
    using T = domain::LogicalType;
    switch (type) {
    case T::Char: case T::Varchar: case T::NChar: case T::NVarchar:
        return {"Characters", "How many characters it holds.",
                {1, 2, 3, 5, 10, 16, 20, 25, 30, 32, 50, 64, 100, 128, 200, 255, 256,
                 500, 512, 1000, 2000, 4000}};
    case T::Binary: case T::Varbinary:
        return {"Bytes", "How many bytes it holds.",
                {1, 8, 16, 20, 32, 50, 64, 100, 128, 256, 512, 1000, 2000, 4000, 8000}};
    case T::Time: case T::DateTime2: case T::DateTimeOffset:
        return {"Fractional seconds", "Digits after the second, 0 to 7.", {0, 1, 2, 3, 4, 5, 6, 7}};
    case T::Float:
        return {"Mantissa bits", "1 to 24 stores a real; 25 to 53 stores a float.", {24, 53}};
    case T::Decimal: case T::Numeric:
        return {"Precision", "Digits in all, and how many of them sit after the point.",
                {5, 9, 10, 15, 18, 19, 28, 38}};
    default:
        return {};
    }
}

QString type_label(domain::LogicalType type) {
    using T = domain::LogicalType;
    switch (type) {
    case T::Unset: return "Not chosen yet";
    case T::Int: return "int";
    case T::BigInt: return "bigint";
    case T::SmallInt: return "smallint";
    case T::TinyInt: return "tinyint";
    case T::Bit: return "bit";
    case T::Decimal: return "decimal";
    case T::Numeric: return "numeric";
    case T::Money: return "money";
    case T::SmallMoney: return "smallmoney";
    case T::Float: return "float";
    case T::Real: return "real";
    case T::Char: return "char";
    case T::Varchar: return "varchar";
    case T::VarcharMax: return "varchar(max)";
    case T::Text: return "text";
    case T::NChar: return "nchar";
    case T::NVarchar: return "nvarchar";
    case T::NVarcharMax: return "nvarchar(max)";
    case T::NText: return "ntext";
    case T::Binary: return "binary";
    case T::Varbinary: return "varbinary";
    case T::VarbinaryMax: return "varbinary(max)";
    case T::Image: return "image";
    case T::Date: return "date";
    case T::Time: return "time";
    case T::DateTime: return "datetime";
    case T::DateTime2: return "datetime2";
    case T::DateTimeOffset: return "datetimeoffset";
    case T::SmallDateTime: return "smalldatetime";
    case T::UniqueIdentifier: return "uniqueidentifier";
    case T::Xml: return "xml";
    case T::RowVersion: return "rowversion";
    case T::HierarchyId: return "hierarchyid";
    case T::SqlVariant: return "sql_variant";
    case T::Cursor: return "cursor";
    case T::Table: return "table";
    case T::Geometry: return "geometry";
    case T::Geography: return "geography";
    }
    return "Not chosen yet";
}

// The bar a panel is resized by. It draws a short handle, takes the vertical
// resize cursor, and reports every movement while it is held: the panel must
// follow the hand exactly, so nothing here is eased or stepped.
class ResizeGrip final : public QWidget {
public:
    explicit ResizeGrip(QWidget* parent) : QWidget(parent) {
        setObjectName("schemaGrip");
        setCursor(Qt::SizeVerCursor);
        setFixedHeight(11);
        setFocusPolicy(Qt::StrongFocus);
        setAccessibleName("Resize Relational Design");
        setToolTip("Drag to make Relational Design taller or shorter. Double-click for half or full.");
    }
    // Told how far the pointer has moved since the drag began, in pixels.
    std::function<void(int)> dragged;
    std::function<void()> began;
    std::function<void()> toggled;
    // Told which way an arrow key was pressed, so the panel is not mouse-only.
    std::function<void(int)> nudged;

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().color(held_ || underMouse() ? QPalette::Highlight : QPalette::Mid));
        const auto bar = QRectF((width() - 46) / 2.0, (height() - 4) / 2.0, 46, 4);
        painter.drawRoundedRect(bar, 2, 2);
    }
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton) return;
        held_ = true;
        from_ = event->globalPosition().y();
        if (began) began();
        update();
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if (!held_ || !dragged) return;
        // Up is taller, which is the direction the panel opened in.
        dragged(static_cast<int>(from_ - event->globalPosition().y()));
    }
    void mouseReleaseEvent(QMouseEvent*) override { held_ = false; update(); }
    void mouseDoubleClickEvent(QMouseEvent*) override { if (toggled) toggled(); }
    void enterEvent(QEnterEvent*) override { update(); }
    void leaveEvent(QEvent*) override { update(); }
    void keyPressEvent(QKeyEvent* event) override {
        if (!nudged) { QWidget::keyPressEvent(event); return; }
        if (event->key() == Qt::Key_Up) nudged(1);
        else if (event->key() == Qt::Key_Down) nudged(-1);
        else QWidget::keyPressEvent(event);
    }

private:
    bool held_ = false;
    double from_ = 0;
};

QLabel* hint(const QString& value, QWidget* parent) {
    auto* label = new QLabel(value, parent);
    label->setWordWrap(true);
    label->setObjectName("hint");
    return label;
}
// A panel section that folds away, opened and closed by the same mark the
// Explorer folds its groups by. The mark is drawn by the style rather than by
// hand, so it is the one the rest of the window already uses, it follows
// whatever theme is on, and it stays a mark on the platforms where a stylesheet
// arrow does not survive.
//
// What is inside is part of the model and is kept whether it is on screen or
// not: folding a section away hides questions, never answers. Whether it is on
// screen is a matter of who is looking -- a diagram being taught wants it shut,
// a schema being prepared wants it open -- so the state is the user's own and
// never travels in the document.
class FoldingSection final : public QWidget {
public:
    FoldingSection(const QString& title, const QColor& ink, bool open, QWidget* parent)
        : QWidget(parent), header_(new Header(title, ink, this)), body_(new QWidget(this)) {
        auto* column = new QVBoxLayout(this);
        // Room above the rule the header draws, so the section reads as a
        // boundary rather than as the next field down.
        column->setContentsMargins(0, 6, 0, 0);
        column->setSpacing(8);
        column->addWidget(header_);
        column->addWidget(body_);
        // Set before anything is listening, so opening the panel is not itself
        // reported as the user having folded something.
        header_->setChecked(open);
        body_->setVisible(open);
        QObject::connect(header_, &QAbstractButton::toggled, body_, [this](bool shown) {
            body_->setVisible(shown);
            if (folded) folded(shown);
        });
    }
    // Where the fields go: the caller gives this a layout and fills it.
    [[nodiscard]] QWidget* body() const { return body_; }
    // Told the new state whenever the user folds or unfolds the section.
    std::function<void(bool)> folded;

private:
    class Header final : public QAbstractButton {
    public:
        Header(const QString& title, const QColor& ink, QWidget* parent)
            : QAbstractButton(parent), ink_(ink) {
            setText(title);
            setCheckable(true);
            setObjectName("sectionHeader");
            setCursor(Qt::PointingHandCursor);
            // Reachable by keyboard, and operated by the space bar like any
            // other button, so the fields inside are not mouse-only.
            setFocusPolicy(Qt::StrongFocus);
            setToolTip("Fold this section away, or open it again. What is inside is kept either way.");
        }
        [[nodiscard]] QSize sizeHint() const override {
            const QFontMetrics metrics(heading());
            return {mark + gap + metrics.horizontalAdvance(text()),
                    std::max(mark, metrics.height()) + rule + 10};
        }

    protected:
        void paintEvent(QPaintEvent*) override {
            QPainter painter(this);
            // A hairline across the top, in the theme's own divider colour, so
            // the section is seen to begin somewhere. Taken from the palette
            // rather than from a stylesheet, so it follows every theme and
            // cannot be dropped by a stricter parser.
            painter.setPen(palette().color(QPalette::Mid));
            painter.drawLine(0, 0, width(), 0);
            const auto middle = (height() + rule) / 2;
            QStyleOptionViewItem branch;
            branch.initFrom(this);
            branch.rect = QRect(0, middle - mark / 2, mark, mark);
            branch.state |= QStyle::State_Children;
            if (isChecked()) branch.state |= QStyle::State_Open;
            else branch.state &= ~QStyle::State_Open;
            style()->drawPrimitive(QStyle::PE_IndicatorBranch, &branch, &painter, this);
            painter.setFont(heading());
            painter.setPen(ink_);
            painter.drawText(QRect(mark + gap, rule, width() - mark - gap, height() - rule),
                             Qt::AlignLeft | Qt::AlignVCenter, text());
        }

    private:
        static constexpr int mark = 14;
        static constexpr int gap = 4;
        // The height the hairline and the space beneath it take.
        static constexpr int rule = 7;
        // Heavier and a size larger than a field label, because this names a
        // group of fields rather than one of them, and the difference has to be
        // visible at a glance or the fold reads as a stray mark beside a label.
        [[nodiscard]] QFont heading() const {
            auto weighted = font();
            weighted.setBold(true);
            weighted.setPointSizeF(weighted.pointSizeF() + 1.0);
            return weighted;
        }
        QColor ink_;
    };
    Header* header_;
    QWidget* body_;
};
// Descriptions commit through the same command path on focus loss.
// The Explorer's fold marks sit against its right-hand edge rather than in
// front of each row.
//
// The Explorer is on the left of the window and the diagram fills the middle,
// so the hand comes back from the canvas to the panel's near edge. A mark in
// front of a row is the far edge: it means crossing the whole width of the
// panel to open a group and crossing back to carry on. Against the right edge
// it is the first thing reached rather than the last, and every group opens
// from the same column whatever depth it sits at.
//
// The indentation is left exactly as it was, because that is what says what
// belongs to what. Only the mark moves.
class ExplorerTree final : public QTreeView {
public:
    using QTreeView::QTreeView;
    // The width of the strip the marks live in, kept clear of the text so a
    // long name is elided before it reaches the mark rather than under it.
    static constexpr int fold_strip = 20;

    // Where a row's mark is drawn: against the viewport's right edge, so the
    // marks stay in one column as the panel is resized and, when the tree is
    // scrolled sideways, stay where the hand expects rather than sliding away.
    [[nodiscard]] QRect fold_rect(const QRect& row) const {
        constexpr int mark = 14;
        return QRect(viewport()->width() - fold_strip + (fold_strip - mark) / 2,
                     row.top() + (row.height() - mark) / 2, mark, mark);
    }

protected:
    // Nothing is drawn in the branch column, so no mark appears in front of a
    // row. The column is still there and still indents.
    void drawBranches(QPainter*, const QRect&, const QModelIndex&) const override {}

    void drawRow(QPainter* painter, const QStyleOptionViewItem& options, const QModelIndex& index) const override {
        QTreeView::drawRow(painter, options, index);
        if (!model() || !model()->hasChildren(index)) return;
        // Drawn by the style itself rather than by hand, so it is the same
        // mark the rest of the window uses and follows whatever theme is on.
        QStyleOptionViewItem mark = options;
        mark.rect = fold_rect(options.rect);
        mark.state |= QStyle::State_Children;
        if (isExpanded(index)) mark.state |= QStyle::State_Open;
        else mark.state &= ~QStyle::State_Open;
        style()->drawPrimitive(QStyle::PE_IndicatorBranch, &mark, painter, this);
    }

    void mousePressEvent(QMouseEvent* event) override {
        // Pressing the mark opens or closes the group, and does nothing else:
        // it must not also change what is selected, or reaching for a fold
        // would throw away the selection the user was working with.
        const auto index = indexAt(event->pos());
        if (index.isValid() && model() && model()->hasChildren(index)
            && fold_rect(visualRect(index)).contains(event->pos())) {
            setExpanded(index, !isExpanded(index));
            event->accept();
            return;
        }
        QTreeView::mousePressEvent(event);
    }
};

// How many attributes belong to a row, when any do. It is painted at the end of
// the row rather than written into the name, so a name stays a name: a number
// inside it would read as part of what the element is called.
constexpr int owned_count_role = Qt::UserRole + 1;

// The size the Explorer draws an element at. Wider than it is tall, because
// that is the shape most of them are, and large enough that a dashed outline
// and a doubled one can be told apart at a glance -- which is the whole reason
// for drawing the element rather than a badge for its kind.
constexpr QSize explorer_shape{28, 20};

// The strip the raft of view controls is taken hold of by. Every button on the
// raft does something when it is pressed, so the raft needs somewhere to be
// picked up that is not one of them.
class RaftGrip final : public QWidget {
public:
    explicit RaftGrip(QWidget* parent) : QWidget(parent) {
        setFixedHeight(12);
        setCursor(Qt::OpenHandCursor);
        setToolTip("Drag to move these controls. Right-click them to put them away.");
    }
    std::function<void(QPoint)> dragged;
    std::function<QColor()> ink;

protected:
    void paintEvent(QPaintEvent*) override {
        // Two rows of dots, which is what a thing that can be dragged looks
        // like everywhere else, so nobody has to be told.
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(ink ? ink() : palette().color(QPalette::Mid));
        const auto middle = width() / 2.0;
        for (const auto row : {-2.0, 2.0})
            for (const auto column : {-5.0, 0.0, 5.0})
                painter.drawEllipse(QPointF(middle + column, height() / 2.0 + row), 1.1, 1.1);
    }
    void mousePressEvent(QMouseEvent* event) override {
        holding_ = event->globalPosition().toPoint();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
    }
    void mouseMoveEvent(QMouseEvent* event) override {
        if (holding_.isNull()) return;
        const auto now = event->globalPosition().toPoint();
        if (dragged) dragged(now - holding_);
        holding_ = now;
        event->accept();
    }
    void mouseReleaseEvent(QMouseEvent* event) override {
        holding_ = {};
        setCursor(Qt::OpenHandCursor);
        event->accept();
    }

private:
    QPoint holding_;
};

// Draws a dropped-down row's sample in an ink that reads on the surface the row
// is actually being painted on.
//
// Asking QIcon for a second pixmap and letting it choose between them is not
// enough here. A list decides an icon's mode from its own idea of what is
// selected, which under a stylesheet is not always the row that is drawn
// highlighted, so the two disagree and the sample comes out white on white.
// The state handed to paint is the one the row is really being drawn in, so
// the ink is chosen from that and cannot disagree with anything.
class SampleRows final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    std::function<QPixmap(int row, bool lit)> sample;
protected:
    void paint(QPainter* painter, const QStyleOptionViewItem& given, const QModelIndex& index) const override {
        QStyleOptionViewItem option = given;
        initStyleOption(&option, index);
        if (sample) {
            const bool lit = option.state.testFlag(QStyle::State_Selected)
                          || option.state.testFlag(QStyle::State_MouseOver);
            option.icon = QIcon(sample(index.row(), lit));
        }
        // Drawn by the style itself, so the row keeps the padding, the
        // highlight and the lettering the stylesheet gives every other row.
        const auto* widget = option.widget;
        auto* style = widget ? widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &option, painter, widget);
    }
};

// Keeps every row clear of the strip the fold marks stand in, so the two never
// overlap and a row's highlight ends in the same place whether or not it has
// anything to fold -- and writes the count of what belongs to a row at the end
// of it, quietly, in the same column for every row that has one.
class FoldOnTheRight final : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    // The ink a count is written in: quiet on an ordinary row, and readable on
    // a row that is lit up, which is a different colour entirely.
    std::function<QColor(bool lit)> ink;

    [[nodiscard]] static QString counted(const QModelIndex& index) {
        const auto value = index.data(owned_count_role);
        return value.isValid() ? QString::number(value.toInt()) : QString();
    }

protected:
    void initStyleOption(QStyleOptionViewItem* option, const QModelIndex& index) const override {
        QStyledItemDelegate::initStyleOption(option, index);
        auto inset = ExplorerTree::fold_strip;
        // The name gives up exactly the room the count needs, so a long name is
        // elided before it reaches the number rather than running under it.
        if (const auto shown = counted(index); !shown.isEmpty())
            inset += option->fontMetrics.horizontalAdvance(shown) + 10;
        option->rect.adjust(0, 0, -inset, 0);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& given, const QModelIndex& index) const override {
        QStyledItemDelegate::paint(painter, given, index);
        const auto shown = counted(index);
        if (shown.isEmpty()) return;
        painter->save();
        const bool lit = given.state.testFlag(QStyle::State_Selected);
        painter->setPen(ink ? ink(lit) : given.palette.color(QPalette::Disabled, QPalette::Text));
        painter->drawText(given.rect.adjusted(0, 0, -ExplorerTree::fold_strip - 2, 0),
                          Qt::AlignRight | Qt::AlignVCenter, shown);
        painter->restore();
    }
};

class DescriptionEdit final : public QPlainTextEdit {
public:
    using QPlainTextEdit::QPlainTextEdit;
    std::function<void()> commit;
protected:
    void focusOutEvent(QFocusEvent* event) override {
        QPlainTextEdit::focusOutEvent(event);
        if (commit) commit();
    }
};
// A choice or a number must not change because a finger brushed the trackpad
// while the pointer was over it: a value is changed by pressing the control
// and choosing, or by typing, and not by passing across it. The wheel is
// handed to whatever scrolls behind instead, so the panel moves under the
// pointer as it was meant to, and a control with nothing behind it simply
// lets the turn go by.
class WheelGuard final : public QObject {
public:
    using QObject::QObject;
protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() != QEvent::Wheel) return QObject::eventFilter(watched, event);
        auto* widget = qobject_cast<QWidget*>(watched);
        if (!widget) return QObject::eventFilter(watched, event);
        for (auto* ancestor = widget->parentWidget(); ancestor; ancestor = ancestor->parentWidget())
            if (auto* area = qobject_cast<QAbstractScrollArea*>(ancestor)) {
                QApplication::sendEvent(area->viewport(), event);
                break;
            }
        return true;
    }
};

// Tells the window about every press made in it, wherever it lands. Watched
// on the application rather than widget by widget, because a press anywhere
// outside the diagram counts, and the window has no list of every widget in
// it. A press that nothing under the pointer took is passed on up to each
// parent in turn, so only the widget first pressed is reported: the one with
// no child of its own under the pointer.
class PressWatch final : public QObject {
public:
    PressWatch(QWidget* window, std::function<void(QWidget*)> pressed)
        : QObject(window), window_(window), pressed_(std::move(pressed)) {}
protected:
    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() != QEvent::MouseButtonPress) return QObject::eventFilter(watched, event);
        auto* widget = qobject_cast<QWidget*>(watched);
        if (!widget || widget->window() != window_) return QObject::eventFilter(watched, event);
        if (widget->childAt(static_cast<QMouseEvent*>(event)->position().toPoint()))
            return QObject::eventFilter(watched, event);
        pressed_(widget);
        return QObject::eventFilter(watched, event);
    }
private:
    QWidget* window_;
    std::function<void(QWidget*)> pressed_;
};

void finish_field_edit() {
    auto* widget = QApplication::focusWidget();
    if (!qobject_cast<QLineEdit*>(widget) && !qobject_cast<QPlainTextEdit*>(widget)) return;
    // The search box is not one of the model's fields. Nothing it holds needs
    // committing, and taking the caret out of it would end the word somebody is
    // in the middle of writing -- which is what happened: the first letter
    // filtered the diagram, the caret left, and the second could not be typed.
    for (const auto* ancestor = widget; ancestor; ancestor = ancestor->parentWidget())
        if (ancestor->objectName() == QLatin1String("searchBar")) return;
    widget->clearFocus();
}
}

MainWindow::MainWindow(application::Editor& editor, application::ProjectStore& store,
                       application::IdGenerator& ids, QWidget* parent)
    : QMainWindow(parent), ids_(ids), editor_(editor), store_(store) {
    setObjectName("mainWindow");
    // Shut until somebody opens it, which is how the diagram was drawn before
    // the fields had a section of their own: someone learning the notation is
    // not asked what a Student becomes until they go looking.
    schema_section_open_ = QSettings().value("schemaSectionOpen", false).toBool();
    wheel_guard_ = new WheelGuard(this);
    resize(1440, 920);
    // Small enough to be useful on a narrow screen. What the window cannot do
    // is stay this size and keep everything at full width, so the toolbar gives
    // up its labels before the window gives up its tools.
    setMinimumSize(560, 460);
    build_shell();
    build_actions();
    build_history();
    // The tabs go on once every action and menu they are built from exists.
    ribbon_ = new Ribbon(*this);
    wire_home();
    // Which field a picked character goes into is decided by where the caret
    // was, so the last text field written in is remembered as focus moves.
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget* was, QWidget* now) {
        remember_caret(was);
        remember_text_target(now);
    });
    canvas_->on_edit = [this](const auto& result) { show_result(result); };
    canvas_->on_selection = [this](const auto& selected) { selection_changed(selected); };
    canvas_->on_tool = [this](Tool tool) {
        tool_actions_.at(tool)->setChecked(true);
        refresh_tool_labels();
    };
    canvas_->on_attribute_owner = [this] { refresh_properties(); };
    qApp->installEventFilter(new PressWatch(this, [this](QWidget* pressed) { pressed_outside_canvas(pressed); }));
    canvas_->on_status = [this](const QString& message) { statusBar()->showMessage(message, 7000); };
    canvas_->on_zoom = [this](double factor) {
        zoom_label_->setText(QString::number(qRound(factor * 100)) + "%");
    };
    // The application opens on the home screen, with the work's own
    // furniture put away behind it until somebody chooses something to do.
    show_home(true);
    refresh();
}

MainWindow::~MainWindow() {
    // QWidget owns the projections; retire callbacks while Editor still lives.
    refreshing_ = true;
    canvas_->on_edit = {};
    canvas_->on_selection = {};
    canvas_->on_tool = {};
    canvas_->on_attribute_owner = {};
    canvas_->on_status = {};
    canvas_->on_zoom = {};
    // Some of what the window listens to speaks up while the window is being
    // torn down: the application says focus has left, a dock that was showing
    // says it is no longer visible, and a view says its selection has emptied.
    // All three arrive from QWidget's own destructor, which runs after every
    // member of this class has already been destroyed, because a base class is
    // destroyed last. A slot reached then would read a map that no longer
    // exists, so the window stops listening before any of it can happen.
    // Signals that only a person can cause are left alone: nobody presses a
    // button on a window that is going away.
    qApp->disconnect(this);
    if (validation_dock_) validation_dock_->disconnect(this);
    if (explorer_ && explorer_->selectionModel()) explorer_->selectionModel()->disconnect(this);
    if (issues_) issues_->disconnect(this);
    delete properties_->takeWidget();
    delete takeCentralWidget();
}

namespace {
// Defined further down with the rest of the toolbar's furniture; the schema
// panel is built before it and offers the same four notations, which have to
// be worded the same way in both places or they would be two lists.
const std::array<std::pair<Notation, QString>, 4>& notation_styles();
// A heading inside a menu, likewise defined below and wanted here. A style
// section is not used, because some styles draw one as a bare line and throw
// the words away -- which is what left the schema's own menus as two runs of
// unlabelled choices.
QAction* menu_heading(QMenu* menu, const QString& words, const char* named);
} // namespace

// How both pickers are dressed. They are their own windows, so they inherit
// nothing and are told; and they are the same idea twice, so they are told
// the same thing.
QString picker_sheet(const Theme& colors) {
    const auto rgba = [](QColor colour, double alpha) {
        return QString("rgba(%1,%2,%3,%4)").arg(colour.red()).arg(colour.green())
                   .arg(colour.blue()).arg(alpha, 0, 'f', 3);
    };
    const auto solid = [](QColor colour) { return colour.name(QColor::HexRgb); };
    return QString(R"(
        #typePicker, #sizePicker { background: %1; border: 1px solid %2; border-radius: 10px; }
        #typePickerSearch, #sizePickerLength, #sizePickerScale {
            background: %3; border: 1px solid %2; border-radius: 8px;
            padding: 7px 10px; color: %4; selection-background-color: %5;
        }
        #typePickerSearch:focus, #sizePickerLength:focus, #sizePickerScale:focus {
            border: 1px solid %5;
        }
        #sizePickerWhat { color: %7; padding: 1px 2px; }
        #typePickerList, #sizePickerCommon {
            background: transparent; border: none; outline: none; color: %4;
        }
        #typePickerList::item, #sizePickerCommon::item {
            padding: 4px 10px; border-radius: 6px; margin: 1px 0px; border: none;
        }
        #typePickerList::item:selected, #sizePickerCommon::item:selected {
            background: %5; color: %6;
        }
        #typePickerList::item:disabled { color: %7; background: transparent; }
        QScrollBar:vertical { background: transparent; width: 9px; margin: 2px 0px; }
        QScrollBar::handle:vertical { background: %8; border-radius: 4px; min-height: 28px; }
        QScrollBar::handle:vertical:hover { background: %9; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
    )").arg(solid(colors.panel), solid(colors.border), solid(colors.base), solid(colors.text),
            solid(colors.accent), solid(readable_on(colors.accent)), solid(colors.muted),
            rgba(colors.muted, 0.35), rgba(colors.muted, 0.6));
}

// What opens on a column's size: a field to type the number into, the common
// answers for that particular type under it, and a second field where the type
// carries a scale as well. It is dressed and behaves like the type list, so
// the two read as one idea.
class SizePicker final : public QFrame {
public:
    explicit SizePicker(QWidget* parent) : QFrame(parent, Qt::Popup) {
        setObjectName("sizePicker");
        setFrameShape(QFrame::NoFrame);
        setAttribute(Qt::WA_StyledBackground, true);
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(6);
        what_ = new QLabel(this);
        what_->setObjectName("sizePickerWhat");
        what_->setWordWrap(true);
        layout->addWidget(what_);
        auto* fields = new QWidget(this);
        auto* across = new QHBoxLayout(fields);
        across->setContentsMargins(0, 0, 0, 0);
        across->setSpacing(6);
        length_ = new QLineEdit(fields);
        length_->setObjectName("sizePickerLength");
        length_->setValidator(new QIntValidator(0, static_cast<int>(domain::max_logical_length), this));
        across->addWidget(length_, 1);
        scale_ = new QLineEdit(fields);
        scale_->setObjectName("sizePickerScale");
        scale_->setPlaceholderText("scale");
        scale_->setValidator(new QIntValidator(0, 38, this));
        across->addWidget(scale_, 1);
        layout->addWidget(fields);
        common_ = new QListWidget(this);
        common_->setObjectName("sizePickerCommon");
        common_->setFrameShape(QFrame::NoFrame);
        common_->setUniformItemSizes(true);
        common_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        common_->setMouseTracking(true);
        common_->viewport()->setMouseTracking(true);
        common_->viewport()->installEventFilter(this);
        layout->addWidget(common_);
        setMinimumWidth(248);
        connect(common_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) { take(item); });
        connect(common_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) { take(item); });
        length_->installEventFilter(this);
        scale_->installEventFilter(this);
    }

    std::function<void(std::uint32_t length, std::uint32_t scale)> chose;
    std::function<void()> closed;

    void wear(const Theme& colors, const QString& sheet) {
        auto ground = palette();
        ground.setColor(QPalette::Window, colors.panel);
        setPalette(ground);
        setAutoFillBackground(true);
        setStyleSheet(sheet);
    }

    void open_at(QPoint at, const domain::PreviewColumn& column) {
        const auto advice = size_advice(column.type);
        const auto precise = domain::size_of(column.type) == domain::TypeSize::Precision;
        what_->setText(advice.hint);
        length_->setPlaceholderText(advice.what.toLower());
        length_->setText(column.length ? QString::number(column.length) : QString());
        scale_->setVisible(precise);
        scale_->setText(precise && column.scale ? QString::number(column.scale) : QString());
        common_->clear();
        for (const auto value : advice.common) {
            auto* item = new QListWidgetItem(QString::number(value), common_);
            item->setData(Qt::UserRole, value);
        }
        common_->setVisible(!advice.common.empty());
        common_->setFixedHeight(advice.common.empty() ? 0 : 180);
        adjustSize();
        const auto space = screen() ? screen()->availableGeometry() : QRect(0, 0, 1920, 1080);
        auto where = at;
        if (where.x() + width() > space.right()) where.setX(space.right() - width());
        if (where.y() + height() > space.bottom()) where.setY(at.y() - height() - 24);
        move(where);
        opened_with_ = length_->text();
        opened_scale_ = scale_->text();
        taken_ = false;
        show();
        // A popup is not an active window, so on some platforms nothing in it
        // receives a keystroke until it is asked for. Without this the field
        // can be looked at and not typed into, and the list becomes the only
        // way to answer -- which is not what a field is for.
        raise();
        activateWindow();
        length_->setFocus(Qt::OtherFocusReason);
        length_->selectAll();
    }

protected:
    // Typed and then clicked away from still counts. A number written into
    // the field is an answer whether or not it was finished with Return.
    void hideEvent(QHideEvent* event) override {
        QFrame::hideEvent(event);
        if (closed) closed();
        if (taken_) return;
        if (length_->text() == opened_with_ && scale_->text() == opened_scale_) return;
        apply(length_->text().toUInt(), scale_->text().toUInt());
    }

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == common_->viewport() && event->type() == QEvent::MouseMove) {
            if (auto* under = common_->itemAt(static_cast<QMouseEvent*>(event)->pos()))
                common_->setCurrentItem(under);
        }
        if ((watched == length_ || watched == scale_) && event->type() == QEvent::KeyPress) {
            const auto* key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
                apply(length_->text().toUInt(), scale_->text().toUInt());
                return true;
            }
        }
        return QFrame::eventFilter(watched, event);
    }

private:
    void take(QListWidgetItem* item) {
        if (!item || !item->data(Qt::UserRole).isValid()) return;
        // Chosen from the common answers, the scale is whatever is in the
        // field beside it -- picking a precision should not quietly undo it.
        apply(item->data(Qt::UserRole).toUInt(), scale_->text().toUInt());
    }

    void apply(std::uint32_t length, std::uint32_t scale) {
        taken_ = true;
        hide();
        if (chose) chose(length, scale);
    }

    QString opened_with_;
    QString opened_scale_;
    bool taken_ = false;
    QLabel* what_ = nullptr;
    QLineEdit* length_ = nullptr;
    QLineEdit* scale_ = nullptr;
    QListWidget* common_ = nullptr;
};

// The list that opens on a column waiting for a type: a line to search by and
// every type under it, in the order above. It is a popup, so it closes when
// the pointer goes elsewhere and needs no dismissing.
class TypePicker final : public QFrame {
public:
    explicit TypePicker(QWidget* parent) : QFrame(parent, Qt::Popup) {
        setObjectName("typePicker");
        setFrameShape(QFrame::NoFrame);
        setAttribute(Qt::WA_StyledBackground, true);
        // The corners are rounded by the stylesheet, which leaves the pixels
        // outside the curve unpainted. A see-through window would be the tidy
        // answer, but text drawn onto one loses its crispness, and a list of
        // type names has to be read. So the window keeps its own ground in the
        // same colour instead, and the corners are rounded against a match.
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(8, 8, 8, 8);
        layout->setSpacing(6);
        looking_ = new QLineEdit(this);
        looking_->setObjectName("typePickerSearch");
        looking_->setPlaceholderText("Search types");
        looking_->setClearButtonEnabled(true);
        layout->addWidget(looking_);
        list_ = new QListWidget(this);
        list_->setObjectName("typePickerList");
        list_->setUniformItemSizes(true);
        list_->setFrameShape(QFrame::NoFrame);
        list_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list_->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        // The highlight follows the pointer rather than staying where the
        // keyboard left it. One highlight serves both, so what a click will
        // take and what Return will take are never two different things.
        list_->setMouseTracking(true);
        list_->viewport()->setMouseTracking(true);
        list_->viewport()->installEventFilter(this);
        layout->addWidget(list_);
        setMinimumWidth(248);
        // Tall enough to read a dozen at a glance. A list that shows seven of
        // thirty-eight is a list that has to be scrolled before it can be
        // judged, and the order is the whole point of it.
        list_->setMinimumHeight(330);
        connect(looking_, &QLineEdit::textChanged, this, [this](const QString& text) { narrow(text); });
        connect(list_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) { take(item); });
        connect(list_, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) { take(item); });
        // Typing runs the search whatever has focus, and the arrows walk the
        // list from the search line, so a type can be chosen without the hand
        // leaving the keyboard.
        looking_->installEventFilter(this);
    }

    std::function<void(domain::LogicalType)> chose;
    // Said when it goes away, however it goes away, so whatever was drawn as
    // waiting for it can stop waiting.
    std::function<void()> closed;

    // Dressed in the theme the window is wearing. The picker is its own
    // window, so it inherits nothing and has to be told.
    void wear(const Theme& colors) {
        auto ground = palette();
        ground.setColor(QPalette::Window, colors.panel);
        setPalette(ground);
        setAutoFillBackground(true);
        setStyleSheet(picker_sheet(colors));
        if (!searching_with_) {
            searching_with_ = looking_->addAction(glyph_icon(Glyph::Search, colors, 14),
                                                  QLineEdit::LeadingPosition);
            searching_with_->setEnabled(false);   // a mark, not a button
        } else {
            searching_with_->setIcon(glyph_icon(Glyph::Search, colors, 14));
        }
    }

    void open_at(QPoint at, std::optional<domain::LogicalType> current) {
        current_ = current;
        looking_->clear();
        narrow({});
        // Nudged back on screen where the row it came from is near an edge.
        const auto space = screen() ? screen()->availableGeometry() : QRect(0, 0, 1920, 1080);
        auto where = at;
        if (where.x() + width() > space.right()) where.setX(space.right() - width());
        if (where.y() + height() > space.bottom()) where.setY(at.y() - height() - 24);
        move(where);
        show();
        raise();
        activateWindow();
        looking_->setFocus(Qt::OtherFocusReason);
    }

protected:
    void hideEvent(QHideEvent* event) override {
        QFrame::hideEvent(event);
        if (closed) closed();
    }

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == list_->viewport() && event->type() == QEvent::MouseMove) {
            if (auto* under = list_->itemAt(static_cast<QMouseEvent*>(event)->pos());
                under && under->flags() & Qt::ItemIsEnabled)
                list_->setCurrentItem(under);
        }
        if (watched == looking_ && event->type() == QEvent::KeyPress) {
            const auto* key = static_cast<QKeyEvent*>(event);
            if (key->key() == Qt::Key_Down || key->key() == Qt::Key_Up) {
                const auto step = key->key() == Qt::Key_Down ? 1 : -1;
                const auto next = std::clamp(list_->currentRow() + step, 0, list_->count() - 1);
                list_->setCurrentRow(next);
                return true;
            }
            if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
                take(list_->currentItem());
                return true;
            }
        }
        return QFrame::eventFilter(watched, event);
    }

private:
    void narrow(const QString& looking_for) {
        list_->clear();
        const auto wanted = looking_for.trimmed();
        for (const auto type : types_by_use()) {
            const auto label = type_label(type);
            if (!wanted.isEmpty() && !label.contains(wanted, Qt::CaseInsensitive)) continue;
            auto* item = new QListWidgetItem(label, list_);
            item->setData(Qt::UserRole, static_cast<int>(type));
            if (domain::deprecated_type(type))
                item->setToolTip(QString("%1 is being removed from SQL Server. Prefer the (max) form.")
                                     .arg(label));
            if (current_ && *current_ == type) item->setSelected(true);
        }
        // Nothing found is said, rather than leaving an empty box that looks
        // as though the list failed to load.
        if (list_->count() == 0) {
            auto* nothing = new QListWidgetItem("No type of that name", list_);
            nothing->setFlags(Qt::NoItemFlags);
        }
        if (list_->count() > 0) list_->setCurrentRow(0);
    }

    void take(QListWidgetItem* item) {
        if (!item || !item->data(Qt::UserRole).isValid()) return;
        const auto type = static_cast<domain::LogicalType>(item->data(Qt::UserRole).toInt());
        hide();
        if (chose) chose(type);
    }

    QLineEdit* looking_ = nullptr;
    QListWidget* list_ = nullptr;
    QAction* searching_with_ = nullptr;
    std::optional<domain::LogicalType> current_;
};

void MainWindow::build_shell() {
    auto* workspace = new QWidget(this);
    auto* layout = new QVBoxLayout(workspace);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    auto* header = new QWidget(workspace);
    header->setObjectName("workspaceHeader");
    auto* header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(22, 14, 22, 14);
    // The way back to Home, which is the door every project is come in by
    // (Zain, 2026-09-26): first in the header, where a way back is looked
    // for, on the diagram and on the schema, whatever the project and however
    // it was opened, so a change of mind can always go back and choose
    // another card. What is open stays open behind Home, as the Home command
    // leaves it.
    back_to_home_ = new QPushButton(QStringLiteral("← Back to Home"), header);
    back_to_home_->setObjectName("backToHome");
    back_to_home_->setToolTip("Return to the Home screen. The project stays open.");
    connect(back_to_home_, &QPushButton::clicked, this, [this] { show_home(true); });
    header_layout->addWidget(back_to_home_);
    auto* badge = new QLabel("CONCEPTUAL", header);
    badge->setObjectName("workspaceBadge");
    header_layout->addWidget(badge);
    document_label_ = new QLabel(header);
    document_label_->setObjectName("documentTitle");
    header_layout->addWidget(document_label_, 1);
    // Search has a button of its own, not only an entry in a menu and a key.
    // The bar it opens takes no room until it is asked for, which is only worth
    // doing if there is something on screen to ask with: without a button there
    // is nothing to say the search is there at all, and nothing to reach for
    // once the bar has been closed. It sits here rather than among the drawing
    // tools, because it is about looking at the document rather than adding to
    // it, and because that row is already tight enough to start dropping the
    // names its tools are known by.
    // What the model becomes, beside the badge that says what workspace this
    // is. It belongs here rather than among the drawing tools: it is about
    // what is being looked at, not something to draw with.
    auto* preview = new QPushButton("Relational Design", header);
    preview->setObjectName("previewSchema");
    preview->setCheckable(true);
    preview->setToolTip("The Relational Design this diagram becomes, raised over the lower half of "
                        "the canvas. It is a preview: nothing is converted, and nothing is written.");
    connect(preview, &QPushButton::clicked, this, [this] { show_schema(!schema_open_); });
    header_layout->addWidget(preview);
    search_button_ = new QToolButton(header);
    search_button_->setObjectName("searchButton");
    search_button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    header_layout->addWidget(search_button_);
    // Undo, a search of the schema, and the theme. While the schema has the
    // whole window the drawing tools are put away with the diagram they draw
    // on, and these three are what is still worth having: two of them undo
    // work that has just been done on the schema, and the other two are about
    // looking at it. Hidden until then, because the toolbar already has them.
    schema_header_tools_ = new QWidget(header);
    schema_header_tools_->setObjectName("schemaHeaderTools");
    auto* header_tools = new QHBoxLayout(schema_header_tools_);
    header_tools->setContentsMargins(0, 0, 0, 0);
    header_tools->setSpacing(6);
    // The actions themselves do not exist yet -- the shell is built before
    // them -- so the buttons are made here and given their actions at the end
    // of build_actions, where there is something to give them.
    for (const auto* named : {"schemaUndo", "schemaRedo"}) {
        auto* button = new QToolButton(schema_header_tools_);
        button->setObjectName(QLatin1String(named));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        header_tools->addWidget(button);
    }
    schema_search_ = new QLineEdit(schema_header_tools_);
    schema_search_->setObjectName("schemaSearch");
    schema_search_->setPlaceholderText("Search Relational Design");
    schema_search_->setClearButtonEnabled(true);
    schema_search_->setFixedWidth(190);
    schema_search_->setToolTip("Pick out the tables and columns whose names contain this.");
    connect(schema_search_, &QLineEdit::textChanged, this, [this](const QString& looking_for) {
        if (schema_) schema_->set_looking_for(looking_for);
    });
    header_tools->addWidget(schema_search_);
    schema_theme_ = new QToolButton(schema_header_tools_);
    schema_theme_->setObjectName("schemaTheme");
    schema_theme_->setText("Theme");
    schema_theme_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    schema_theme_->setPopupMode(QToolButton::InstantPopup);
    header_tools->addWidget(schema_theme_);
    schema_header_tools_->hide();
    header_layout->addWidget(schema_header_tools_);

    auto* example = new QPushButton("Open example", header);
    example->setObjectName("openExample");
    connect(example, &QPushButton::clicked, this, &MainWindow::load_example);
    header_layout->addWidget(example);
    layout->addWidget(header);
    // The search sits directly above the thing it filters, and takes no room
    // at all until it is asked for.
    search_bar_ = new SearchBar(workspace);
    search_bar_->hide();
    layout->addWidget(search_bar_);
    // The canvas and the schema share one area: the schema rises over the
    // lower part of the diagram rather than replacing it or opening a window
    // of its own, so the model and what it becomes are read together.
    auto* stage = new QWidget(workspace);
    stage->setObjectName("workspaceStage");
    auto* stage_layout = new QVBoxLayout(stage);
    stage_layout->setContentsMargins(0, 0, 0, 0);
    stage_layout->setSpacing(0);
    canvas_ = new DiagramView(editor_, stage);
    canvas_->setObjectName("diagramCanvas");
    canvas_->setAccessibleName("Conceptual ERD canvas");
    stage_layout->addWidget(canvas_, 1);
    layout->addWidget(stage, 1);

    // The curtain. It is a child of the stage rather than a row in the layout,
    // so it can be raised over the diagram and animated into place.
    schema_panel_ = new QWidget(stage);
    schema_panel_->setObjectName("schemaPanel");
    schema_panel_->setAutoFillBackground(true);
    auto* panel_layout = new QVBoxLayout(schema_panel_);
    panel_layout->setContentsMargins(0, 0, 0, 0);
    panel_layout->setSpacing(0);
    auto* grip = new ResizeGrip(schema_panel_);
    panel_layout->addWidget(grip);
    auto* schema_bar = new QWidget(schema_panel_);
    schema_bar->setObjectName("schemaBar");
    auto* bar_layout = new QHBoxLayout(schema_bar);
    bar_layout->setContentsMargins(12, 7, 12, 7);
    bar_layout->setSpacing(9);
    auto* schema_title = new QLabel("Relational Design", schema_bar);
    schema_title->setObjectName("schemaTitle");
    bar_layout->addWidget(schema_title);
    schema_state_ = new QLabel(schema_bar);
    schema_state_->setObjectName("schemaState");
    bar_layout->addWidget(schema_state_);
    bar_layout->addStretch();
    // Everything that shapes the whole schema, in two menus rather than a
    // row of eight controls. They are grouped by the question they answer:
    // Arrange is about where things are put, Appearance is about how they are
    // written. A row that names every setting at once is a row nobody reads.
    auto* arrange = new QToolButton(schema_bar);
    arrange->setObjectName("schemaArrange");
    arrange->setText("Arrange");
    arrange->setPopupMode(QToolButton::InstantPopup);
    arrange->setToolButtonStyle(Qt::ToolButtonTextOnly);
    auto* arranging = new QMenu(arrange);
    arranging->setObjectName("schemaArrangeMenu");
    menu_heading(arranging, "Where the tables sit", "schemaTablesHeading");
    auto* align_tables = arranging->addAction("Align to the grid");
    align_tables->setObjectName("schemaAlign");
    align_tables->setToolTip("Round every table's position to the grid, once.");
    connect(align_tables, &QAction::triggered, this, [this] { schema_->align(); });
    menu_heading(arranging, "How the lines run", "schemaLinesHeading");
    schema_lines_ = new QActionGroup(this);
    static const std::array<std::pair<SchemaRouting, const char*>, 2> routings{{
        {SchemaRouting::AroundTables, "Around the tables"},
        {SchemaRouting::Straight, "Straight there"}}};
    for (const auto& [which, label] : routings) {
        auto* choice = arranging->addAction(QLatin1String(label));
        choice->setObjectName(which == SchemaRouting::Straight ? "schemaLinesStraight"
                                                               : "schemaLinesAround");
        choice->setCheckable(true);
        choice->setChecked(which == SchemaRouting::AroundTables);
        choice->setActionGroup(schema_lines_);
        connect(choice, &QAction::triggered, this, [this, which] {
            if (schema_) schema_->set_routing(which);
        });
    }
    // Whether a table may be pulled about by its edges. Its own heading,
    // because it is a question about the tables rather than about the lines
    // above it.
    menu_heading(arranging, "Table size", "schemaWidthHeading");
    schema_sizing_ = new QActionGroup(this);
    static const std::array<std::pair<bool, const char*>, 2> sizing{{
        {true, "Resizable"}, {false, "Fixed"}}};
    const auto resizable_now = QSettings().value("schemaTablesResizable", true).toBool();
    for (const auto& [on, label] : sizing) {
        auto* choice = arranging->addAction(QLatin1String(label));
        choice->setObjectName(on ? "schemaResizable" : "schemaFixed");
        choice->setCheckable(true);
        choice->setChecked(on == resizable_now);
        choice->setActionGroup(schema_sizing_);
        choice->setToolTip(on ? "Pull any edge or corner of a table to resize it. The side you pull "
                                "moves and the opposite one stays where it is."
                              : "Leave every table at the size it has, so an edge cannot be "
                                "caught while moving one.");
        connect(choice, &QAction::triggered, this, [this, on] {
            QSettings().setValue("schemaTablesResizable", on);
            if (schema_) schema_->set_tables_resizable(on);
        });
    }

    menu_heading(arranging, "When a table lands on a line", "schemaGiveWayHeading");
    auto* give_way = arranging->addAction("Move out of the way of tables");
    give_way->setObjectName("schemaGiveWay");
    give_way->setCheckable(true);
    give_way->setToolTip("When a table is moved onto a line that was shaped by hand, hand that "
                         "line back to the router so it goes around. Off, the line stays where "
                         "it was put.");
    // Off unless it has been asked for. A shape somebody made is theirs, and a
    // line that undid itself because a table drifted over it would be undoing
    // their work for a reason they never asked about. Remembered with the
    // application rather than in the project: it is how somebody likes to
    // work, not something the document says.
    give_way->setChecked(QSettings().value("schemaLinesGiveWay", false).toBool());
    connect(give_way, &QAction::toggled, this, [this](bool on) {
        QSettings().setValue("schemaLinesGiveWay", on);
        if (schema_) schema_->set_lines_give_way(on);
    });

    // Putting back what was moved by hand is its own kind of thing, and the
    // two entries differ in how much they undo: one gives the lines back and
    // leaves the tables, the other gives back both.
    menu_heading(arranging, "Put back", "schemaPutBackHeading");
    auto* release = arranging->addAction("Release the lines");
    release->setObjectName("schemaRelease");
    release->setToolTip("Give every line back to the router, leaving the tables where they are.");
    connect(release, &QAction::triggered, this, [this] { schema_->release_lines(); });
    auto* tidy_tables = arranging->addAction("Tidy everything");
    tidy_tables->setObjectName("schemaTidy");
    tidy_tables->setToolTip("Put every table and every line back to the automatic arrangement.");
    connect(tidy_tables, &QAction::triggered, this, [this] { schema_->tidy(); });
    arrange->setMenu(arranging);
    bar_layout->addWidget(arrange);

    auto* appearance = new QToolButton(schema_bar);
    appearance->setObjectName("schemaAppearance");
    appearance->setText("Appearance");
    appearance->setPopupMode(QToolButton::InstantPopup);
    appearance->setToolButtonStyle(Qt::ToolButtonTextOnly);
    auto* appearing = new QMenu(appearance);
    appearing->setObjectName("schemaAppearanceMenu");
    menu_heading(appearing, "Notation", "schemaNotationHeading");
    schema_notation_ = new QActionGroup(this);
    for (const auto& [style, label] : notation_styles()) {
        auto* choice = appearing->addAction(label);
        choice->setObjectName("schemaNotation" + QString(label).remove(QRegularExpression("[^A-Za-z]")));
        choice->setCheckable(true);
        choice->setActionGroup(schema_notation_);
        choice->setData(static_cast<int>(style));
        connect(choice, &QAction::triggered, this, [this, style] {
            if (refreshing_) return;
            choose_notation(style);
        });
    }
    menu_heading(appearing, "Table names", "schemaNamesHeading");
    schema_names_ = new QActionGroup(this);
    static const std::array<std::pair<domain::TableNaming, const char*>, 2> namings{{
        {domain::TableNaming::Plural, "Plural"},
        {domain::TableNaming::AsDrawn, "As the diagram draws them"}}};
    for (const auto& [which, label] : namings) {
        auto* choice = appearing->addAction(QLatin1String(label));
        choice->setObjectName(which == domain::TableNaming::Plural ? "schemaNamesPlural"
                                                                   : "schemaNamesAsDrawn");
        choice->setCheckable(true);
        choice->setActionGroup(schema_names_);
        choice->setData(static_cast<int>(which));
        connect(choice, &QAction::triggered, this, [this, which] {
            if (refreshing_) return;
            show_result(editor_.set_table_naming(which), false);
        });
    }
    appearance->setMenu(appearing);
    bar_layout->addWidget(appearance);

    auto* full_schema = new QPushButton("Full", schema_bar);
    full_schema->setObjectName("schemaFull");
    full_schema->setCheckable(true);
    full_schema->setToolTip("Give the whole window to Relational Design: the panels go away and the "
                            "diagram behind it is covered. Press again to bring everything back.");
    bar_layout->addWidget(full_schema);
    auto* close_schema = new QPushButton("Close", schema_bar);
    close_schema->setObjectName("schemaClose");
    bar_layout->addWidget(close_schema);
    panel_layout->addWidget(schema_bar);

    // Which tables the schema is being asked about, by where they came from.
    // The same four questions the diagram's own search asks, so a reader who
    // has learned one has learned the other.
    auto* narrowing = new QWidget(schema_panel_);
    narrowing->setObjectName("schemaNarrowing");
    auto* narrow_layout = new QHBoxLayout(narrowing);
    narrow_layout->setContentsMargins(10, 2, 10, 4);
    narrow_layout->setSpacing(6);
    static const std::array<std::pair<SchemaShowing, const char*>, 4> chips{{
        {SchemaShowing::Everything, "Everything"},
        {SchemaShowing::FromEntities, "From entities"},
        {SchemaShowing::FromRelationships, "From relationships"},
        {SchemaShowing::FromAttributes, "From attributes"}}};
    for (const auto& [which, label] : chips) {
        auto* chip = new QPushButton(QLatin1String(label), narrowing);
        chip->setObjectName(QString("schemaShow") + QString(label).remove(' '));
        chip->setCheckable(true);
        chip->setChecked(which == SchemaShowing::Everything);
        chip->setProperty("chip", true);
        schema_chips_.push_back(chip);
        narrow_layout->addWidget(chip);
        connect(chip, &QPushButton::clicked, this, [this, which] {
            if (schema_) schema_->set_showing(which);
            for (std::size_t i = 0; i < schema_chips_.size(); ++i)
                schema_chips_[i]->setChecked(static_cast<SchemaShowing>(i) == which);
            // Asking about a kind of table is a different question from asking
            // about one table, so it puts any selection down first.
            if (schema_) schema_->select(std::nullopt);
        });
    }
    narrow_layout->addStretch(1);
    panel_layout->addWidget(narrowing);

    schema_scroll_ = new QScrollArea(schema_panel_);
    schema_scroll_->setObjectName("schemaScroll");
    schema_scroll_->setWidgetResizable(true);
    schema_scroll_->setFrameShape(QFrame::NoFrame);
    // A scroll area that resizes its widget asks for as much height as that
    // widget's minimum, and the schema's minimum is however tall the tables
    // happen to reach. Left at that, the panel cannot give room to anything
    // below the scroll area and pushes it off the bottom of the window. The
    // area is told it may be small; what it holds can still be any size, which
    // is what the scrollbars are for.
    schema_scroll_->setMinimumHeight(60);
    schema_ = new SchemaView(editor_, schema_scroll_);
    // Shaping a line is not an edit, so it does not pass through the Editor and
    // nothing else hears about it. The panel still has to be told, because the
    // count of ends left hanging is part of what it reports.
    schema_->shaped = [this] { refresh_schema_state(); };
    // Arranging the schema is an edit like any other, so its result goes the
    // same way every other edit's does: reported if it failed, and the window
    // refreshed either way, which is what makes undo show up here.
    schema_->arranged = [this](const application::EditResult& result) { show_result(result, false); };
    schema_->set_lines_give_way(QSettings().value("schemaLinesGiveWay", false).toBool());
    schema_->set_tables_resizable(QSettings().value("schemaTablesResizable", true).toBool());
    schema_->asked = [this](const SchemaView::Spot& spot) { offer_schema_actions(spot); };
    schema_->rules_asked = [this](const SchemaView::Constrained& hit, QPoint at) {
        offer_schema_rules(hit, at);
    };
    schema_->renamed = [this](const SchemaView::Spot& spot, const QString& typed) {
        rename_from_schema(spot, typed);
    };
    // What is wrong with where a line's end was put. The end stays there: it is
    // reported, not refused, because a line that sprang back would be arguing
    // with the person drawing it. Said for long enough to be read, since it
    // explains something rather than confirming it.
    schema_->warned = [this](const QString& warning, QPoint at) {
        if (warning.isEmpty()) return;
        // Both: the status bar keeps it after the notice has gone, and the
        // notice is what gets read, being put where the hand already is.
        statusBar()->showMessage(warning, 12000);
        if (notice_) notice_->say(warning, mapFromGlobal(at));
    };
    schema_->add_column = [this](std::size_t which) {
        if (!schema_ || which >= schema_->preview().tables.size()) return;
        const auto& table = schema_->preview().tables[which];
        if (table.origin) add_schema_column(*table.origin);
    };
    schema_->decided = [this](const domain::OpenDecision& decision, std::size_t choice) {
        answer_decision(decision, choice);
    };
    schema_->chose = [this] { refresh_schema_state(); };
    schema_->asked_type = [this](const domain::PreviewColumn& column, QPoint at) {
        ask_column_type(column, at);
    };
    schema_->asked_size = [this](const domain::PreviewColumn& column, QPoint at) {
        ask_column_size(column, at);
    };
    schema_scroll_->setWidget(schema_);
    panel_layout->addWidget(schema_scroll_, 1);
    // Columns that share a name, gathered up. Seventeen types open is almost
    // never seventeen questions: it is ID asked six times, Name asked five,
    // and a few of their own. Answering the repeats once is the difference
    // between a schema that can be finished and one that cannot.
    shared_names_ = new QWidget(schema_panel_);
    shared_names_->setObjectName("schemaSharedNames");
    auto* shared_layout = new QVBoxLayout(shared_names_);
    shared_layout->setContentsMargins(0, 0, 0, 0);
    shared_layout->setSpacing(0);
    shared_names_head_ = new QPushButton(shared_names_);
    shared_names_head_->setObjectName("schemaSharedNamesHead");
    shared_names_head_->setFlat(true);
    shared_names_head_->setCursor(Qt::PointingHandCursor);
    shared_layout->addWidget(shared_names_head_);
    shared_names_body_ = new QWidget(shared_names_);
    shared_names_body_->setObjectName("schemaSharedNamesBody");
    auto* body_layout = new QVBoxLayout(shared_names_body_);
    body_layout->setContentsMargins(14, 2, 14, 8);
    body_layout->setSpacing(4);
    shared_layout->addWidget(shared_names_body_);
    shared_names_body_->hide();
    connect(shared_names_head_, &QPushButton::clicked, this, [this] {
        shared_names_open_ = !shared_names_open_;
        shared_names_body_->setVisible(shared_names_open_);
        refresh_shared_names();
    });
    panel_layout->addWidget(shared_names_);

    schema_panel_->hide();

    // Resizing. The share is remembered, so the panel opens again at whatever
    // height it was left at, and it is clamped so a panel can never be pulled
    // out of reach: a sliver still shows its bar, and full still leaves the
    // diagram's own header above it.
    grip->began = [this] { schema_share_at_grab_ = schema_share_; };
    grip->dragged = [this](int moved) {
        auto* stage = schema_panel_->parentWidget();
        if (!stage || stage->height() <= 0) return;
        schema_share_ = std::clamp(schema_share_at_grab_
                                       + static_cast<double>(moved) / stage->height(), 0.12, 1.0);
        lay_out_schema();
    };
    grip->nudged = [this](int direction) {
        schema_share_ = std::clamp(schema_share_ + direction * 0.06, 0.12, 1.0);
        lay_out_schema();
    };
    // Half and full are the two heights anybody actually wants, so the grip
    // swaps between them without having to be aimed.
    grip->toggled = [this] {
        schema_share_ = schema_share_ > 0.85 ? 0.5 : 1.0;
        lay_out_schema();
    };

    connect(full_schema, &QPushButton::toggled, this, [this](bool on) { set_schema_full(on); });
    connect(close_schema, &QPushButton::clicked, this, [this] { show_schema(false); });
    search_bar_->on_changed = [this] { search_diagram(search_bar_->search()); };
    search_bar_->on_closed = [this] { close_search(); };
    auto* instructions = hint("Choose a shape, then click the canvas. Connect links an attribute to its owner, or a relationship to an entity.", workspace);
    instructions->setObjectName("canvasInstructions");
    instructions->setContentsMargins(18, 10, 18, 10);
    layout->addWidget(instructions);
    // The home screen and the workspace both exist from the start, stacked,
    // so moving between them is a change of which is in front rather than a
    // teardown and a rebuild.
    home_ = new HomePage(this);
    // A card's + Create takes its route. A route that cannot yet be taken has
    // its button disabled, so there is no case here for one that leads
    // nowhere. Where the sidebar's rows go is said in wire_home, once the
    // menus some of them open exist.
    home_->route_chosen = [this](StartRoute route) {
        switch (route) {
        case StartRoute::Conceptual:
            // A new conceptual project, untitled, as New Project makes one.
            // Its name and where it is kept are asked elsewhere, not on Home
            // (ADR-022 section 9.19).
            if (begin_new_project()) show_home(false);
            return;
        case StartRoute::RelationalDesign:
        case StartRoute::Sql:
            // Neither route is enabled, so neither can be chosen. Named here
            // so that enabling one later is a compiler error until somebody
            // says what it should do.
            return;
        }
    };
    pages_ = new QStackedWidget(this);
    pages_->setObjectName("pages");
    pages_->addWidget(home_);
    pages_->addWidget(workspace);
    setCentralWidget(pages_);

    auto* explorer_dock = new QDockWidget("Explorer", this);
    explorer_dock->setObjectName("explorerDock");
    explorer_ = new ExplorerTree(explorer_dock);
    explorer_->setObjectName("explorer");
    auto* rows = new FoldOnTheRight(explorer_);
    rows->ink = [this](bool lit) {
        const auto& colors = theme(theme_);
        return lit ? readable_on(colors.accent) : colors.muted;
    };
    explorer_->setItemDelegate(rows);
    // Room for the element shapes, which say more than a badge for the kind
    // can: a dashed outline is a derived attribute and a doubled one is
    // multivalued, and neither reads at the size a plain list icon is drawn at.
    explorer_->setIconSize(explorer_shape);
    explorer_->setAccessibleName("Project elements");
    explorer_->setHeaderHidden(true);
    explorer_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    explorer_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    explorer_->setMinimumWidth(120);
    explorer_->setUniformRowHeights(true);
    explorer_model_ = new QStandardItemModel(this);
    explorer_->setModel(explorer_model_);
    explorer_dock->setWidget(explorer_);
    addDockWidget(Qt::LeftDockWidgetArea, explorer_dock);
    connect(explorer_->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
        if (refreshing_) return;
        std::vector<ElementRef> selected;
        for (const auto& index : explorer_->selectionModel()->selectedRows()) {
            const auto found = references_.find(index.data(Qt::UserRole).toString());
            // One attribute may be selected through either of its rows; it is
            // still one attribute.
            if (found != references_.end() && std::find(selected.begin(), selected.end(), found->second) == selected.end())
                selected.push_back(found->second);
        }
        canvas_->select_elements(selected, true);
        selection_changed(selected);
    });
    connect(explorer_, &QTreeView::doubleClicked, this, [this](const QModelIndex& index) {
        if (index.data(Qt::UserRole).toString() == "project") {
            bool accepted = false;
            const auto value = QInputDialog::getText(this, "Project name", "Name", QLineEdit::Normal,
                                                   text(editor_.project().name), &accepted);
            if (accepted) show_result(editor_.rename_project(bytes(value)));
        } else if (references_.contains(index.data(Qt::UserRole).toString())) rename_selection();
    });

    auto* properties_dock = new QDockWidget("Properties", this);
    properties_dock->setObjectName("propertiesDock");
    properties_ = new QScrollArea(properties_dock);
    properties_->setWidgetResizable(true);
    properties_->setMinimumWidth(180);
    properties_->setFrameShape(QFrame::NoFrame);
    properties_dock->setWidget(properties_);
    addDockWidget(Qt::RightDockWidgetArea, properties_dock);

    validation_dock_ = new QDockWidget("Model checks", this);
    validation_dock_->setObjectName("validationDock");
    issues_ = new QTreeView(validation_dock_);
    issues_->setObjectName("modelIssues");
    issues_->setRootIsDecorated(false);
    issues_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    issue_model_ = new QStandardItemModel(this);
    issues_->setModel(issue_model_);
    validation_dock_->setWidget(issues_);
    addDockWidget(Qt::BottomDockWidgetArea, validation_dock_);
    validation_dock_->hide();
    connect(issues_, &QTreeView::clicked, this, [this](const QModelIndex& index) {
        const auto found = references_.find(index.siblingAtColumn(0).data(Qt::UserRole).toString());
        if (found != references_.end()) canvas_->select_elements({found->second}, true);
    });
    auto* view_menu = menuBar()->addMenu("&View");
    view_menu->setObjectName("viewMenu");
    view_menu->addAction(explorer_dock->toggleViewAction());
    view_menu->addAction(properties_dock->toggleViewAction());
    view_menu->addAction(validation_dock_->toggleViewAction());
    count_label_ = new QLabel(this);
    // Laid over the whole window rather than over one view, so it is in the
    // same place whatever is being worked on and never scrolls away with the
    // thing it is about.
    notice_ = new Notice(this);
    notice_->wear(theme(theme_));
    readiness_label_ = new QLabel(this);
    zoom_label_ = new QLabel("100%", this);
    statusBar()->addWidget(count_label_);
    statusBar()->addPermanentWidget(readiness_label_);
    statusBar()->addPermanentWidget(zoom_label_);
    statusBar()->setSizeGripEnabled(true);
    resizeDocks({explorer_dock, properties_dock}, {230, 310}, Qt::Horizontal);
}

namespace {
// Both pickers show a sample of a line. The sizes live here rather than at each
// call site, which is what keeps the icon a widget asks for the same size as the
// one it is later redrawn at when the theme changes.
constexpr QSize line_style_sample{48, 24};
constexpr QSize notation_sample{58, 22};

QString isa_label(Tool mode) {
    return mode == Tool::Generalization ? QStringLiteral("Generalization") : QStringLiteral("Specialization");
}
// The notations offered, in the order they appear everywhere in the UI.
const std::array<std::pair<Notation, QString>, 4>& notation_styles() {
    static const std::array<std::pair<Notation, QString>, 4> styles{{
        {Notation::Chen, "Chen"}, {Notation::MinMax, "Min–max"},
        {Notation::CrowsFoot, "Crow's foot"}, {Notation::Bachman, "Bachman"}
    }};
    return styles;
}
} // namespace

namespace {
// A heading inside a menu. Written as an entry that cannot be chosen rather
// than as a style section, because some styles draw a section as a bare line
// with the words thrown away -- which leaves the groups beneath it looking like
// one undivided list, or worse, like alternatives.
QAction* menu_heading(QMenu* menu, const QString& words, const char* named) {
    if (!menu->isEmpty()) menu->addSeparator();
    auto* heading = menu->addAction(words);
    heading->setObjectName(QString::fromLatin1(named));
    heading->setEnabled(false);
    return heading;
}
} // namespace

void MainWindow::build_actions() {
    // Find, where a document application keeps it, and on the key it keeps it
    // on. It narrows the diagram to what is asked for rather than only walking
    // from one match to the next, which is what makes it worth having on a
    // drawing rather than in a list. It is made here because it goes in two
    // places -- the Edit menu and the tool row -- and both must be the same
    // action, or one of them would go stale.
    find_action_ = new QAction("Search…", this);
    find_action_->setObjectName("searchDiagram");
    find_action_->setShortcut(QKeySequence::Find);
    find_action_->setToolTip("Narrow the diagram to what you are looking for.");
    connect(find_action_, &QAction::triggered, this, [this] { open_search(); });
    action_glyphs_[find_action_] = Glyph::Search;

    auto* file = new QMenu("&File", this);
    file->setObjectName("fileMenu");
    menuBar()->insertMenu(menuBar()->actions().front(), file);
    auto* action_new = file->addAction("&New project", QKeySequence::New, this, &MainWindow::new_project);
    action_new->setObjectName("newProject");
    action_glyphs_[action_new] = Glyph::New;
    auto* action_open = file->addAction("&Open…", QKeySequence::Open, this,
                                        &MainWindow::open_dialog);
    action_open->setObjectName("openProject");
    action_glyphs_[action_open] = Glyph::Open;
    auto* action_save = file->addAction("&Save", QKeySequence::Save, this, [this] { save(); });
    action_save->setObjectName("saveProject");
    action_glyphs_[action_save] = Glyph::Save;
    file->addAction("Save &as…", QKeySequence::SaveAs, this, [this] { save(true); });
    file->addSeparator();

    // Export is how work leaves ERDFlow. Its own menu, so the ribbon can put
    // a row over it the way Insert and Design are put over theirs, and a copy
    // of it under File, which is where a document application keeps it.
    //
    // Documents come first. Someone handing this work on is choosing between a
    // report and a picture before they are choosing between PNG and SVG.
    auto* export_menu = new QMenu("Export", this);
    export_menu->setObjectName("exportMenu");
    // The project itself leads, because it is the only one of these that loses
    // nothing. Saving writes the project you are working on; this writes a copy
    // of it somewhere else and leaves the one you are working on alone.
    menu_heading(export_menu, "Project", "exportProjectHeading");
    auto* export_project = export_menu->addAction("ERDFlow project…", this, [this] { export_project_file(); });
    export_project->setObjectName("exportProject");
    export_project->setToolTip("Write a copy of the project, losing nothing. Saving keeps working on this one; "
                               "this leaves it where it is.");
    menu_heading(export_menu, "Documents", "exportDocumentsHeading");
    struct DocumentEntry { DocumentFormat format; const char* name; };
    for (const auto& entry : {DocumentEntry{DocumentFormat::Pdf, "exportPdfDocument"},
                              DocumentEntry{DocumentFormat::Markdown, "exportMarkdown"},
                              DocumentEntry{DocumentFormat::Html, "exportHtml"},
                              DocumentEntry{DocumentFormat::Csv, "exportCsv"}}) {
        const auto& info = document_format(entry.format);
        auto* item = export_menu->addAction(QString::fromUtf8(info.label) + "…", this,
                                              [this, format = entry.format] { export_document(format); });
        item->setObjectName(QString::fromLatin1(entry.name));
        item->setToolTip(QString::fromUtf8(info.caution));
    }

    // Then the pictures a person reaches for without thinking about options.
    // SVG leads because it is the default picture to hand out: it reads at any size and it
    // is one of the two that carry the project home again.
    menu_heading(export_menu, "Pictures", "exportPicturesHeading");
    struct PictureEntry { PictureFormat format; const char* name; bool common; };
    QMenu* more_pictures = nullptr;
    for (const auto& entry : {PictureEntry{PictureFormat::Svg, "exportSvg", true},
                              PictureEntry{PictureFormat::Png, "exportPng", true},
                              PictureEntry{PictureFormat::Pdf, "exportPdfPage", true},
                              PictureEntry{PictureFormat::Jpeg, "exportJpeg", false},
                              PictureEntry{PictureFormat::WebP, "exportWebp", false},
                              PictureEntry{PictureFormat::Tiff, "exportTiff", false}}) {
        // A format this build has no writer for is left out rather than offered
        // and then failed.
        if (!picture_format_available(entry.format)) continue;
        const auto& info = picture_format(entry.format);
        // The three anyone wants sit on the menu; the rest are gathered behind
        // one entry, so a common choice is never hunted for among rare ones.
        if (!entry.common && !more_pictures) {
            more_pictures = export_menu->addMenu("Other picture formats");
            more_pictures->setObjectName("exportMorePictures");
        }
        auto* into = entry.common ? export_menu : more_pictures;
        auto* item = into->addAction(QString::fromUtf8(info.label) + "…", this, [this, format = entry.format] {
            auto options = export_choice_.as_picture;
            options.format = format;
            export_picture(options);
        });
        item->setObjectName(QString::fromLatin1(entry.name));
        if (*info.caution) item->setToolTip(QString::fromUtf8(info.caution));
    }

    export_menu->addSeparator();
    auto* export_options = export_menu->addAction("Export with options…", QKeySequence("Ctrl+Shift+E"),
                                                      this, &MainWindow::export_dialog);
    export_options->setObjectName("exportWithOptions");
    export_options->setToolTip("Choose the format, and for a picture its size, extent and background.");
    action_glyphs_[export_options] = Glyph::Export;
    auto* copy_action = export_menu->addAction("Copy as picture", QKeySequence("Ctrl+Shift+C"),
                                                 this, [this] { copy_picture(); });
    copy_action->setObjectName("copyAsPicture");
    copy_action->setToolTip("Put a picture of the selection, or of the whole diagram, on the clipboard.");
    // Gathered so they can be turned off together while there is nothing drawn.
    // A submenu's own entries are collected too, since the submenu itself only
    // names them.
    for (auto* action : export_menu->actions()) {
        if (action->isSeparator()) continue;
        // A heading is not a command, so it is not one of the things turned on
        // when there is something to export: turning it on would make it look
        // like something that could be pressed.
        if (action->objectName().endsWith(QLatin1String("Heading"))) continue;
        if (auto* submenu = action->menu()) {
            for (auto* nested : submenu->actions()) export_actions_.push_back(nested);
            export_actions_.push_back(action);
            continue;
        }
        export_actions_.push_back(action);
    }
    file->addMenu(export_menu);

    // Import sits next to Export, because that is its pair. It reads what
    // ERDFlow itself writes: the project, and the two pictures that carry one.
    // Reading what other tools write is a later thing, and the entry that says
    // so is left in place rather than the absence being silent.
    auto* import_menu = new QMenu("Import", this);
    import_menu->setObjectName("importMenu");
    auto* import_project = import_menu->addAction("ERDFlow project…", QKeySequence("Ctrl+Shift+I"),
                                                  this, [this] { import_dialog(false); });
    import_project->setObjectName("importProject");
    import_project->setToolTip("Bring another project's contents into this one. Everything arrives with "
                               "identities of its own, so nothing collides, and it all undoes in one step.");
    auto* import_picture = import_menu->addAction("Picture carrying a project…", this,
                                                  [this] { import_dialog(true); });
    import_picture->setObjectName("importPicture");
    import_picture->setToolTip("An SVG or PNG that ERDFlow wrote carries the whole project inside it.");
    import_menu->addSeparator();
    auto* import_later = import_menu->addAction("From another tool…");
    import_later->setObjectName("importFromOtherTools");
    import_later->setEnabled(false);
    import_later->setToolTip("SQL, CSV and JSON arrive with the Relational Design workspace: they describe "
                             "tables rather than a conceptual diagram, so there is nowhere yet to put them.");
    for (auto* action : import_menu->actions())
        if (!action->isSeparator() && action->isEnabled()) import_actions_.push_back(action);
    file->addMenu(import_menu);
    file->addSeparator();
    file->addAction("Open example", this, &MainWindow::load_example);
    file->addSeparator();
    file->addAction("&Quit", QKeySequence::Quit, this, &QWidget::close);
    auto* edit = new QMenu("&Edit", this);
    // Named like the window's other menus, so it can be found by name.
    edit->setObjectName("editMenu");
    menuBar()->insertMenu(findChild<QMenu*>("viewMenu")->menuAction(), edit);

    // Home, between File and Edit: where somebody goes to start something
    // rather than to do something to what is already open. Edit keeps Undo,
    // Redo and the rest, which is where anybody would look for them.
    auto* home_menu = new QMenu("&Home", this);
    home_menu->setObjectName("homeMenu");
    menuBar()->insertMenu(edit->menuAction(), home_menu);
    auto* go_home = home_menu->addAction("Home", QKeySequence("Ctrl+Shift+H"),
                                         this, [this] { show_home(true); });
    go_home->setObjectName("goHome");
    go_home->setToolTip("The screen ERDFlow opens on, where a project is started or found.");
    home_menu->addSeparator();
    home_menu->addAction(findChild<QAction*>("newProject"));
    home_menu->addAction(findChild<QAction*>("openProject"));
    home_menu->addSeparator();
    recent_menu_ = home_menu->addMenu("Recent");
    recent_menu_->setObjectName("recentMenu");
    connect(recent_menu_, &QMenu::aboutToShow, this, [this] { refresh_recent_menu(); });
    refresh_recent_menu();
    home_menu->addSeparator();
    auto* examples = home_menu->addAction("Open example", this, [this] { load_example(); });
    examples->setObjectName("homeExamples");
    // A template is a project left untitled and unsaved (ADR-016). It is the
    // general starting frame, not the University example (Zain, 2026-09-26).
    auto* templates = home_menu->addAction("New from template", this, [this] { load_template(); });
    templates->setObjectName("homeTemplates");

    // Design: what is done to the model as a whole rather than to one thing in
    // it. Arrange and Appearance already exist on the schema's own header; the
    // menu is where they are reachable from the keyboard and from a workspace
    // that has no header of its own.
    auto* design_menu = new QMenu("&Design", this);
    design_menu->setObjectName("designMenu");
    menuBar()->insertMenu(findChild<QMenu*>("viewMenu")->menuAction(), design_menu);
    // File · Home · Edit · Insert · Design · View · Help. Insert is built
    // later than this and lands ahead of Design, so Design is moved behind it
    // once both are there.
    if (auto* insert_menu = findChild<QMenu*>("insertMenu")) {
        menuBar()->removeAction(design_menu->menuAction());
        const auto& order = menuBar()->actions();
        const auto at = std::find(order.begin(), order.end(), insert_menu->menuAction());
        menuBar()->insertMenu(at + 1 == order.end() ? nullptr : *(at + 1),
                              design_menu);
    }
    auto* to_schema = design_menu->addAction("Relational Design", QKeySequence("Ctrl+R"),
                                             this, [this] { show_schema(true); });
    to_schema->setObjectName("designRelational");
    design_menu->addSeparator();
    auto* tidy = design_menu->addAction("Arrange the relational design", this, [this] {
        if (schema_) schema_->tidy();
    });
    tidy->setObjectName("designArrange");
    tidy->setToolTip("Lay the tables out again from scratch, undoing any arrangement by hand.");
    undo_ = edit->addAction("Undo", QKeySequence::Undo, this, [this] {
        // A remark never outlives the thing it remarks on: taking back the
        // misplaced end takes back what was said about it, rather than leaving
        // a warning standing over work that has been put right.
        if (notice_) notice_->put_away();
        finish_field_edit(); canvas_->cancel_interaction(); show_result(editor_.undo());
    });
    undo_->setObjectName("undoCommand");
    redo_ = edit->addAction("Redo", QKeySequence::Redo, this, [this] {
        if (notice_) notice_->put_away();
        finish_field_edit(); canvas_->cancel_interaction(); show_result(editor_.redo());
    });
    redo_->setObjectName("redoCommand");
    // The menu entry names the edit that will be reversed, which makes the
    // label change width on every command. A toolbar button must not resize
    // as you work, so it keeps a fixed icon text and explains itself by tooltip.
    undo_->setIconText("Undo");
    redo_->setIconText("Redo");
    action_glyphs_[undo_] = Glyph::Undo;
    action_glyphs_[redo_] = Glyph::Redo;
    edit->addSeparator();
    rename_ = edit->addAction("Rename…", this, &MainWindow::rename_selection);
    action_glyphs_[rename_] = Glyph::Rename;
    duplicate_ = edit->addAction("Duplicate", QKeySequence("Ctrl+D"), this, [this] {
        finish_field_edit(); show_result(editor_.duplicate(selection_));
    });
    duplicate_->setObjectName("duplicateElements");
    action_glyphs_[duplicate_] = Glyph::Duplicate;
    action_glyphs_[edit->addAction("Delete selection", this, [this] { finish_field_edit(); canvas_->delete_selection(); })] = Glyph::Delete;
    edit->addSeparator();
    edit->addAction(find_action_);
    // The header's button and the menu entry are one action, so they cannot
    // disagree about what searching is called or whether it can be done.
    if (search_button_) search_button_->setDefaultAction(find_action_);
    edit->addSeparator();
    // A symbol is drawn as its character filling its box, so making the box
    // bigger is what makes the character bigger. The view's own zoom already
    // owns Ctrl+= and Ctrl+-, and it means something else -- how close the
    // whole diagram is looked at, not how big one thing on it is -- so these
    // take the same keys with Shift, the way a document editor's font size does.
    enlarge_ = edit->addAction("Enlarge symbol", QKeySequence("Ctrl+Shift+="), this,
                               [this] { finish_field_edit(); canvas_->resize_symbols(symbol_step); });
    enlarge_->setObjectName("enlargeSymbol");
    enlarge_->setToolTip("Make the selected symbol bigger. Its corners do the same thing by hand.");
    shrink_ = edit->addAction("Shrink symbol", QKeySequence("Ctrl+Shift+-"), this,
                              [this] { finish_field_edit(); canvas_->resize_symbols(1 / symbol_step); });
    shrink_->setObjectName("shrinkSymbol");
    shrink_->setToolTip("Make the selected symbol smaller.");
    // Delete/Backspace and the letter tool shortcuts belong to the canvas, so
    // typing inside property fields never deletes model elements.
    auto* toolbar = addToolBar("Model tools");
    toolbar->setObjectName("modelTools");
    toolbar->setMovable(false);
    // Icon beside text, the way an office application labels its toolbar: the
    // glyph carries recognition, the word removes any doubt.
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    // Big enough that the drawing in an icon can be read rather than guessed
    // at. The toolbar runs out of width before it runs out of room in height,
    // so anything past this pushes buttons into the overflow.
    toolbar->setIconSize(QSize(34, 34));
    toolbar->addAction(action_save);
    toolbar->addSeparator();
    toolbar->addAction(undo_);
    toolbar->addAction(redo_);
    toolbar->addSeparator();
    auto* group = new QActionGroup(this);
    const std::array<std::pair<Tool, QString>, 4> tools{{
        {Tool::Select, "Select"}, {Tool::Entity, "Entity"}, {Tool::Attribute, "Attribute"},
        {Tool::Relationship, "Relationship"}
    }};
    for (const auto& [tool, label] : tools) {
        auto* action = toolbar->addAction(label);
        action->setCheckable(true);
        action->setData(label);
        action->setObjectName("tool" + label);
        action->setActionGroup(group);
        action->setChecked(tool == Tool::Select);
        tool_actions_[tool] = action;
        action_glyphs_[action] = tool == Tool::Select ? Glyph::Select
            : tool == Tool::Entity ? Glyph::Entity
            : tool == Tool::Attribute ? Glyph::Attribute : Glyph::Relationship;
        connect(action, &QAction::triggered, this, [this, tool] { choose_tool(tool, false); });
        // A double click locks the tool. The button itself has to report it,
        // because the click that locks is also an ordinary click that selects.
        if (auto* button = toolbar->widgetForAction(action)) button->installEventFilter(this);
    }
    // ISA covers two directions onto the same structure, so one entry carries
    // both: the arrow chooses the direction, the button uses the chosen one and
    // locks on a double click exactly like every other tool.
    isa_action_ = new QAction(isa_label(isa_mode_), this);
    isa_action_->setCheckable(true);
    isa_action_->setData(isa_label(isa_mode_));
    isa_action_->setObjectName("toolIsa");
    isa_action_->setActionGroup(group);
    action_glyphs_[isa_action_] = Glyph::Isa;
    connect(isa_action_, &QAction::triggered, this, [this] { choose_tool(isa_mode_, false); });
    auto* isa_menu = new QMenu(this);
    for (const auto mode : {Tool::Specialization, Tool::Generalization}) {
        auto* entry = isa_menu->addAction(isa_label(mode));
        entry->setObjectName("isa" + isa_label(mode));
        entry->setToolTip(mode == Tool::Specialization
            ? "Top-down: click the entity to specialise, then connect its subtypes."
            : "Bottom-up: select the subtypes, then click the entity that generalises them.");
        connect(entry, &QAction::triggered, this, [this, mode] {
            isa_mode_ = mode;
            isa_action_->setData(isa_label(mode));
            choose_tool(mode, false);
        });
    }
    auto* isa_button = new QToolButton(toolbar);
    isa_button->setObjectName("isaButton");
    isa_button->setDefaultAction(isa_action_);
    isa_button->setMenu(isa_menu);
    isa_button->setPopupMode(QToolButton::MenuButtonPopup);
    // These two are plain widgets on the toolbar rather than actions, so they
    // inherit none of its presentation and have to be told to match it.
    isa_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    isa_button->setIconSize(toolbar->iconSize());
    toolbar->addWidget(isa_button);
    tool_actions_[Tool::Specialization] = isa_action_;
    tool_actions_[Tool::Generalization] = isa_action_;
    isa_button->installEventFilter(this);

    // How connectors are drawn belongs with the tool that draws them, so the
    // Connect button carries the choice on its own arrow rather than in a
    // separate control the eye has to find.
    auto* connect_action = new QAction("Connect", this);
    connect_action->setCheckable(true);
    connect_action->setData("Connect");
    connect_action->setObjectName("toolConnect");
    connect_action->setActionGroup(group);
    action_glyphs_[connect_action] = Glyph::Connect;
    connect(connect_action, &QAction::triggered, this, [this] { choose_tool(Tool::Connect, false); });
    auto* line_menu = new QMenu(this);
    // Right angles first, since that is how a line between two points chosen
    // by hand is drawn, and what the canvas draws unless told otherwise.
    for (const auto style : {LineStyle::Elbow, LineStyle::Curved, LineStyle::Straight}) {
        const QString label = style == LineStyle::Straight ? "Straight lines"
            : style == LineStyle::Elbow ? "Right-angle lines" : "Curved lines";
        auto* entry = line_menu->addAction(line_style_icon(style), label);
        entry->setCheckable(true);
        entry->setChecked(style == canvas_->line_style());
        entry->setObjectName(style == LineStyle::Straight ? "lineStraight"
            : style == LineStyle::Elbow ? "lineElbow" : "lineCurved");
        line_actions_[style] = entry;
        connect(entry, &QAction::triggered, this, [this, style] { choose_line_style(style); });
    }
    // A line Connect draws is never pinned to where it was clicked (Zain,
    // 2026-09-26). "Join where I click" pinned both ends there, and was the
    // default: a join pinned on a side facing away from its other end hooked
    // round or ran across the shape. It is no longer offered, and so neither
    // is the choice it was one half of, whatever was remembered from before.
    // Every new line starts unlocked; one that is wanted fixed is locked by
    // hand, and either end of a selected line can still be dragged to a point.
    canvas_->set_join_mode(JoinMode::Automatic);
    auto* connect_button = new QToolButton(toolbar);
    connect_button->setObjectName("connectButton");
    connect_button->setDefaultAction(connect_action);
    connect_button->setMenu(line_menu);
    connect_button->setPopupMode(QToolButton::MenuButtonPopup);
    connect_button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect_button->setIconSize(toolbar->iconSize());
    toolbar->addWidget(connect_button);
    tool_actions_[Tool::Connect] = connect_action;
    connect_button->installEventFilter(this);

    // Pictures and notes are the visual aids. A picture comes from a file, so
    // it is an action and lives on Insert; a note is put down by a click, so
    // it is a tool like the elements, sits with them after Connect, and locks
    // like them.
    auto* picture = new QAction("Picture…", this);
    picture->setObjectName("insertPicture");
    picture->setToolTip("Insert a picture from a file.");
    action_glyphs_[picture] = Glyph::Picture;
    connect(picture, &QAction::triggered, this, [this] { insert_picture_dialog(); });
    auto* note_action = new QAction("Note", this);
    note_action->setCheckable(true);
    note_action->setData("Note");
    note_action->setObjectName("toolNote");
    note_action->setActionGroup(group);
    action_glyphs_[note_action] = Glyph::Note;
    tool_actions_[Tool::Note] = note_action;
    connect(note_action, &QAction::triggered, this, [this] { choose_tool(Tool::Note, false); });
    toolbar->addAction(note_action);
    if (auto* button = toolbar->widgetForAction(note_action)) button->installEventFilter(this);
    // The picture in the menu bar too, for the keyboard and for anyone who
    // looks there first. The ribbon's Insert row is built from this menu, and
    // the canvas's right-click menu offers both a picture and a note.
    auto* insert_menu = new QMenu("&Insert", this);
    insert_menu->setObjectName("insertMenu");
    menuBar()->insertMenu(findChild<QMenu*>("viewMenu")->menuAction(), insert_menu);
    // Design was made before Insert existed, so put it back after Insert now
    // that both actions can be ordered. This is the native application menu;
    // the ribbon below it remains a separate workspace tool system.
    if (auto* designing = findChild<QMenu*>("designMenu")) {
        menuBar()->removeAction(designing->menuAction());
        menuBar()->insertMenu(findChild<QMenu*>("viewMenu")->menuAction(), designing);
    }
    insert_menu->addAction(picture);
    // Characters that cannot be typed but are wanted constantly in this of all
    // editors: the relational algebra signs, the set and logic signs, arrows,
    // Greek, and the marks and emoji a note is annotated with. They go into a
    // name, a role or a description, so this is one gallery reached two ways
    // rather than two galleries. The ribbon's Insert row is built from this
    // menu, so the submenu appears there as a button of its own.
    // One entry rather than a submenu: a submenu's button carries a dropdown
    // arrow, and on the ribbon's Insert row the style puts that arrow beneath
    // the label, which makes the row taller than Home and breaks the tabs'
    // one promise, that every row is the same height. The gallery lists its
    // groups down its own side, so emoji are one click in rather than one
    // click out.
    auto* symbols = insert_menu->addAction("Symbols…");
    symbols->setObjectName("insertSymbols");
    symbols->setToolTip("Relational algebra, logic, arrows, Greek, marks, emoji and people, for names and notes.");
    action_glyphs_[symbols] = Glyph::Symbols;
    connect(symbols, &QAction::triggered, this, [this] { show_symbols(QStringLiteral("Relational algebra")); });
    canvas_->on_insert_picture = [this](QPointF at) { insert_picture_dialog(at); };
    canvas_->on_comment = [this](std::vector<domain::CommentTarget> targets) { add_comment(std::move(targets)); };
    // The way back. Offered only while the raft is away, because an entry that
    // puts back something already there says nothing worth reading.
    canvas_->on_canvas_menu = [this](QMenu& menu) {
        if (!canvas_controls_ || canvas_controls_->isVisible()) return;
        menu.addSeparator();
        auto* back = menu.addAction("Show the view controls");
        back->setObjectName("showCanvasControls");
        connect(back, &QAction::triggered, this, [this] { show_canvas_controls(true); });
    };

    // Notation follows Connect: it decides how the lines Connect draws are read.
    // The picker draws each option, so the cardinality symbols can be recognised
    // rather than remembered from a name.
    notation_separator_ = toolbar->addSeparator();
    // Named, so a reader who does not yet know the symbols knows what the
    // picker is for. The label goes with the picker whenever the bar has to
    // give it up.
    auto* notation_label = new QLabel(" Notation ", toolbar);
    notation_label->setObjectName("notationLabel");
    notation_label_action_ = toolbar->addWidget(notation_label);
    notation_box_ = new QComboBox(toolbar);
    notation_box_->setObjectName("notationPicker");
    notation_box_->setIconSize(notation_sample);
    notation_box_->setToolTip("How each participant's minimum and maximum are drawn.");
    // Held to the width of its drawings and a little text. The sample is what
    // the choice is made on; the name is only there to confirm it, and letting
    // it run to full length costs more of the toolbar than it is worth.
    notation_box_->setMaximumWidth(notation_sample.width() + 62);
    notation_box_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    notation_box_->setMinimumContentsLength(4);
    notation_box_->installEventFilter(wheel_guard_);
    for (const auto& [style, label] : notation_styles())
        notation_box_->addItem(notation_icon(style), label, QVariant::fromValue(static_cast<int>(style)));
    // The rows of the dropped-down list ink their samples from the surface each
    // is painted on, so a sample is never drawn in the colour it stands on.
    auto* notation_rows = new SampleRows(notation_box_);
    notation_rows->sample = [this](int row, bool lit) {
        return canvas_->notation_preview(static_cast<Notation>(row), notation_sample,
                                         lit ? std::optional<QColor>(readable_on(theme(theme_).accent))
                                             : std::optional<QColor>{});
    };
    notation_box_->view()->setItemDelegate(notation_rows);
    notation_box_->setCurrentIndex(static_cast<int>(canvas_->notation()));
    connect(notation_box_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (refreshing_ || index < 0) return;
        choose_notation(static_cast<Notation>(index));
    });
    notation_action_ = toolbar->addWidget(notation_box_);


    // Panning and framing are about looking rather than modelling, so they sit
    // on the canvas by what they act on instead of in the row of drawing tools.
    auto* pan_action = new QAction("Pan", this);
    pan_action->setCheckable(true);
    pan_action->setData("Pan");
    pan_action->setObjectName("toolPan");
    pan_action->setActionGroup(group);
    tool_actions_[Tool::Pan] = pan_action;
    action_glyphs_[pan_action] = Glyph::Pan;
    connect(pan_action, &QAction::triggered, this, [this] { choose_tool(Tool::Pan, false); });

    auto* fit = new QAction("Fit", this);
    fit->setObjectName("viewFit");
    fit->setShortcut(QKeySequence("Ctrl+0"));
    fit->setToolTip("Fit the whole diagram in the view.");
    connect(fit, &QAction::triggered, canvas_, &DiagramView::fit_diagram);
    action_glyphs_[fit] = Glyph::Fit;

    // Putting the panels away is about looking rather than modelling, so it
    // joins the controls on the canvas. It is written out in the View menu,
    // and named by its tooltip on the raft, where there is only room for a
    // picture.
    full_view_ = new QAction("Full view", this);
    full_view_->setObjectName("viewFullView");
    full_view_->setCheckable(true);
    action_glyphs_[full_view_] = Glyph::FullView;
    connect(full_view_, &QAction::toggled, this, [this](bool on) { set_full_view(on); });

    // What follows lives in the right-hand corner rather than in the row of
    // tools: checking the model and choosing a theme are about the whole
    // diagram, not about the next thing drawn on it.
    auto* stretch = new QWidget(toolbar);
    stretch->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolbar->addWidget(stretch);
    toolbar->addSeparator();
    // One button for both halves of looking at the findings: it opens them,
    // and once they are open it puts them away again. The button says which
    // it will do by the mark it wears, and follows the panel however the panel
    // was opened or closed.
    check_ = toolbar->addAction("Check model", this, [this] {
        finish_field_edit();
        if (validation_dock_->isVisible()) {
            validation_dock_->hide();
            return;
        }
        refresh_validation();
        validation_dock_->show();
    });
    check_->setObjectName("checkModel");
    check_->setCheckable(true);
    action_glyphs_[check_] = Glyph::Check;
    connect(validation_dock_, &QDockWidget::visibilityChanged, this, [this] { refresh_check_action(); });


    // A small raft of view controls over the bottom-right of the canvas, where
    // a diagram is framed and zoomed rather than across the window from it.
    canvas_controls_ = new QWidget(canvas_);
    canvas_controls_->setObjectName("canvasControls");
    // Right-clicking the raft offers to put it away. The buttons do not answer
    // a right-click themselves, so the press reaches the raft beneath them and
    // the offer is the same wherever on it the pointer was.
    canvas_controls_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(canvas_controls_, &QWidget::customContextMenuRequested, this, [this](const QPoint& at) {
        QMenu menu(canvas_controls_);
        auto* away = menu.addAction("Hide these controls");
        away->setObjectName("hideCanvasControls");
        away->setToolTip("Put the raft away. Right-click the diagram to bring it back.");
        if (menu.exec(canvas_controls_->mapToGlobal(at)) == away) show_canvas_controls(false);
    });
    auto* stack = new QVBoxLayout(canvas_controls_);
    stack->setContentsMargins(4, 4, 4, 4);
    stack->setSpacing(2);
    auto* grip = new RaftGrip(canvas_controls_);
    grip->setObjectName("canvasControlsGrip");
    grip->ink = [this] { return theme(theme_).muted; };
    grip->dragged = [this](QPoint by) { move_canvas_controls(by); };
    stack->addWidget(grip);
    // Every button on the raft is the same size and sits on the same centre
    // line, so the column reads as one control rather than as icons that
    // happen to be near some signs.
    stack->setAlignment(Qt::AlignHCenter);
    const auto raft_button = [&](QAction* action, const char* named) {
        auto* button = new QToolButton(canvas_controls_);
        button->setObjectName(named);
        button->setDefaultAction(action);
        button->setToolButtonStyle(Qt::ToolButtonIconOnly);
        button->setAutoRaise(true);
        button->setIconSize(QSize(18, 18));
        button->setFixedSize(26, 24);
        stack->addWidget(button, 0, Qt::AlignHCenter);
        return button;
    };
    raft_button(full_view_, "canvasFullView");
    raft_button(fit, "canvasFit");
    // Pan locks on a double-click, exactly as the tools on the toolbar do, so
    // a long look around the diagram does not need the button pressed again
    // after every drag.
    raft_button(pan_action, "canvasPan")->installEventFilter(this);
    // Zooming has no glyph of its own in either set, and a pair of signs says
    // what it does more plainly than a picture would at this size.
    // The name the platform gives the key that zooms: the Command symbol on a
    // Mac, the word Control elsewhere. Taken from Qt rather than written out
    // twice, so it can never be right on one platform and wrong on the other.
    const auto zoom_key = QKeySequence(QKeySequence::ZoomIn)
                              .toString(QKeySequence::NativeText).section(QChar('+'), 0, 0);
    for (const auto& [text, name, step] : std::initializer_list<std::tuple<const char*, const char*, int>>{
             {"+", "canvasZoomIn", 1}, {"\u2212", "canvasZoomOut", -1}}) {
        auto* button = new QToolButton(canvas_controls_);
        button->setObjectName(name);
        button->setText(QString::fromUtf8(text));
        button->setToolTip(QString("%1. Or hold %2 and scroll, which zooms about the pointer.")
                               .arg(step > 0 ? "Zoom in" : "Zoom out", zoom_key));
        button->setAutoRaise(true);
        button->setFixedSize(26, 24);
        connect(button, &QToolButton::clicked, this,
                [this, step] { if (step > 0) canvas_->zoom_in(); else canvas_->zoom_out(); });
        stack->addWidget(button, 0, Qt::AlignHCenter);
    }
    canvas_->installEventFilter(this);
    place_canvas_controls();

    auto* view = findChild<QMenu*>("viewMenu");
    view->addAction(full_view_);
    view->addSeparator();
    view->addAction(fit);
    view->addAction("Actual size", QKeySequence("Ctrl+1"), canvas_, &DiagramView::actual_size);
    view->addAction("Zoom in", QKeySequence::ZoomIn, canvas_, &DiagramView::zoom_in);
    view->addAction("Zoom out", QKeySequence::ZoomOut, canvas_, &DiagramView::zoom_out);
    // Dragging follows the pointer continuously by default. Aligning to the
    // grid rounds movement to the grid step, which reads as stuttering rather
    // than as help, so it stays available but off until it is asked for.
    for (bool align : {false, true}) {
        auto* action = view->addAction(align ? "Align to grid" : "Show grid");
        action->setCheckable(true);
        action->setChecked(!align);
        action->setObjectName(align ? "viewAlignToGrid" : "viewShowGrid");
        if (align) action->setToolTip("Line elements up on the grid as you move or place them.");
        connect(action, &QAction::toggled, this, [this, align](bool checked) {
            if (align) canvas_->set_align_to_grid(checked); else canvas_->set_grid_visible(checked);
        });
    }
    // Remarks left on the diagram. The switch is how the diagram is being
    // looked at rather than part of it, so it sits with the grid and is not
    // saved with the document: it quiets every remark at once, for reading the
    // model or taking a picture of it. The mark on a commented element stays
    // either way, because a remark nobody can see is a remark nobody can find.
    // The raft of view controls, so there is a way back to it that does not
    // depend on knowing to right-click the diagram.
    auto* raft_entry = view->addAction("View controls on the diagram");
    raft_entry->setCheckable(true);
    raft_entry->setChecked(true);
    raft_entry->setObjectName("viewCanvasControls");
    raft_entry->setToolTip("The small raft on the diagram: full view, fit, pan and zoom. "
                           "Drag it by its grip, and right-click it to put it away.");
    connect(raft_entry, &QAction::toggled, this, [this](bool checked) { show_canvas_controls(checked); });
    auto* show_comments = view->addAction("Show comments");
    show_comments->setCheckable(true);
    show_comments->setChecked(true);
    show_comments->setObjectName("viewShowComments");
    show_comments->setShortcut(QKeySequence("Ctrl+Shift+M"));
    show_comments->setToolTip("Show what has been said about the diagram when you point at it. "
                              "The mark on a commented element stays either way.");
    connect(show_comments, &QAction::toggled, this, [this](bool checked) { canvas_->set_comments_visible(checked); });
    canvas_->set_align_to_grid(false);
    canvas_->set_grid_visible(true);
    canvas_->set_comments_visible(true);
    // The paper the diagram is drawn on. It sits with the other choices about
    // how the diagram looks, and unlike them it travels with the document: a
    // diagram drawn on graph paper should open on graph paper.
    auto* backgrounds = view->addMenu("Background");
    backgrounds->setObjectName("backgroundMenu");
    auto* background_group = new QActionGroup(this);
    const std::array<std::pair<domain::BackgroundStyle, const char*>, 4> papers{{
        {domain::BackgroundStyle::Theme, "None"}, {domain::BackgroundStyle::Squares, "Squares"},
        {domain::BackgroundStyle::Lines, "Lines"}, {domain::BackgroundStyle::Dots, "Dots"}}};
    for (const auto& [style, label] : papers) {
        auto* action = backgrounds->addAction(QString::fromLatin1(label));
        action->setCheckable(true);
        action->setActionGroup(background_group);
        action->setObjectName(QString("background") + background_key(style));
        background_actions_[style] = action;
        connect(action, &QAction::triggered, this, [this, style] { choose_background(style); });
    }
    auto* own_picture = backgrounds->addAction("Picture…");
    own_picture->setCheckable(true);
    own_picture->setActionGroup(background_group);
    own_picture->setObjectName("backgroundImage");
    background_actions_[domain::BackgroundStyle::Image] = own_picture;
    connect(own_picture, &QAction::triggered, this, &MainWindow::choose_background_image);
    // How strongly a picture shows through, which is what keeps it behind the
    // diagram rather than in front of it. A ruling needs no such thing: it is
    // drawn as the ruling it is, so the bar is offered only for a picture.
    backgrounds->addSeparator();
    auto* strength_row = new QWidget(backgrounds);
    auto* strength_layout = new QHBoxLayout(strength_row);
    strength_layout->setContentsMargins(22, 4, 12, 4);
    strength_layout->setSpacing(8);
    strength_layout->addWidget(new QLabel("Strength", strength_row));
    auto* strength = new QSlider(Qt::Horizontal, strength_row);
    strength->setObjectName("backgroundStrength");
    strength->setRange(0, 100);
    strength->setMinimumWidth(120);
    strength->installEventFilter(wheel_guard_);
    auto* reading = new QLabel(strength_row);
    reading->setMinimumWidth(36);
    strength_layout->addWidget(strength, 1);
    strength_layout->addWidget(reading);
    auto* strength_action = new QWidgetAction(backgrounds);
    strength_action->setObjectName("backgroundStrengthRow");
    strength_action->setDefaultWidget(strength_row);
    backgrounds->addAction(strength_action);
    connect(strength, &QSlider::valueChanged, this, [this, reading](int value) {
        reading->setText(QString("%1%").arg(value));
        if (refreshing_) return;
        auto paper = editor_.project().background;
        if (paper.strength == value) return;
        paper.strength = static_cast<std::uint8_t>(value);
        show_result(editor_.set_background(std::move(paper)));
    });
    connect(backgrounds, &QMenu::aboutToShow, this, [this] { refresh_background_menu(); });

    // The same participants can be read in several notations. This is a display
    // choice, so it lives with the other view settings rather than in the file.
    auto* themes = view->addMenu("Theme");
    themes->setObjectName("themeMenu");
    auto* theme_group = new QActionGroup(this);
    for (const auto& entry : erdflow::desktop::themes()) {
        auto* action = themes->addAction(entry.label);
        action->setCheckable(true);
        action->setChecked(entry.id == theme_);
        action->setObjectName("theme" + QString(entry.key).remove('-'));
        action->setActionGroup(theme_group);
        theme_actions_[entry.id] = action;
        connect(action, &QAction::triggered, this, [this, id = entry.id] { set_theme(id); });
        // Hovering a name shows the theme on the whole window, which is the only
        // way to judge one: a palette is about how the diagram reads, not about
        // what it is called.
        connect(action, &QAction::hovered, this, [this, id = entry.id] { preview_theme_soon(id); });
    }
    // Leaving the menu without choosing puts back what was chosen before.
    connect(themes, &QMenu::aboutToHide, this, [this] {
        // A look that has not happened yet must not happen after the menu has
        // gone, or it would put a theme on the window nobody is still pointing at.
        if (theme_preview_timer_) theme_preview_timer_->stop();
        if (theme_ != committed_theme_) apply_appearance(committed_theme_);
    });
    // Appearance is tried repeatedly rather than set once, so the same list is
    // put on the toolbar behind a button. It is the menu itself, not a copy, so
    // the two can never disagree about which theme is the current one.
    theme_button_ = new QToolButton(toolbar);
    theme_button_->setObjectName("themeButton");
    theme_button_->setText("Theme");
    theme_button_->setToolTip("Change the appearance of the window and the diagram.");
    theme_button_->setMenu(themes);
    theme_button_->setPopupMode(QToolButton::InstantPopup);
    theme_button_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    theme_button_->setIconSize(toolbar->iconSize());
    // The icon set is a choice about appearance like the theme is, so it sits
    // in the same menu rather than somewhere of its own.
    auto* icon_menu = view->addMenu("Icons");
    icon_menu->setObjectName("iconMenu");
    auto* icon_group = new QActionGroup(this);
    for (const auto mode : {IconMode::Outline, IconMode::Normal, IconMode::Modern}) {
        auto* action = icon_menu->addAction(mode == IconMode::Modern ? "Modern"
                                          : mode == IconMode::Outline ? "Outline" : "Painted");
        action->setCheckable(true);
        action->setChecked(mode == icon_mode_);
        action->setObjectName("icons" + icon_mode_key(mode));
        action->setActionGroup(icon_group);
        icon_mode_actions_[mode] = action;
        connect(action, &QAction::triggered, this, [this, mode] { set_icon_mode(mode); });
    }

    // Outermost on the right: the corner is where a choice about the whole
    // window is looked for.
    toolbar->addSeparator();
    toolbar->addWidget(theme_button_);
    auto* notations = view->addMenu("Notation");
    notations->setObjectName("notationMenu");
    auto* notation_group = new QActionGroup(this);
    for (const auto& [style, label] : notation_styles()) {
        auto* action = notations->addAction(label);
        action->setCheckable(true);
        action->setChecked(style == canvas_->notation());
        action->setObjectName("notation" + QString(label).remove(QRegularExpression("[^A-Za-z]")));
        action->setActionGroup(notation_group);
        notation_actions_[style] = action;
        connect(action, &QAction::triggered, this, [this, style] { choose_notation(style); });
    }
    full_view_->setToolTip("Full view — put the panels away and give the whole window to the diagram.");
    // Every action now exists, so give them their first icons. The window must
    // look right on its own, not only once a theme is chosen from outside it.
    refresh_icons();
    auto* help = menuBar()->addMenu("&Help");
    help->setObjectName("helpMenu");
    help->addAction("Quick guide", this, [this] { show_quick_guide(); })->setObjectName("quickGuide");
    help->addAction("About ERDFlow", this, [this] {
        QMessageBox::about(this, "ERDFlow", "ERDFlow 0.1 · Conceptual editor foundation\n\n"
                           "Draw once, progressively refine.\nC++20 · Qt 6 · Local project files");
    });

    // The header's undo and redo were built with the shell, before any of
    // these actions existed. They are the same two actions, so they are
    // handed over here rather than being made again.
    if (auto* button = findChild<QToolButton*>("schemaUndo")) button->setDefaultAction(undo_);
    if (auto* button = findChild<QToolButton*>("schemaRedo")) button->setDefaultAction(redo_);
}

void MainWindow::refresh() {
    if (refreshing_) return;
    refreshing_ = true;
    canvas_->synchronize();
    selection_ = canvas_->selected_elements();
    refresh_explorer();
    refresh_properties();
    refresh_validation();
    refresh_history();
    undo_->setEnabled(editor_.can_undo());
    redo_->setEnabled(editor_.can_redo());
    undo_->setText(editor_.can_undo() ? "Undo " + text(editor_.undo_label()) : "Undo");
    redo_->setText(editor_.can_redo() ? "Redo " + text(editor_.redo_label()) : "Redo");
    // An explicitly set icon text survives setText, so the toolbar keeps its
    // fixed wording and only the tooltip follows the named edit.
    undo_->setToolTip(undo_->text() + "\t" + undo_->shortcut().toString(QKeySequence::NativeText));
    redo_->setToolTip(redo_->text() + "\t" + redo_->shortcut().toString(QKeySequence::NativeText));
    refresh_selection_commands();
    refresh_export_actions();
    refresh_schema();
    auto title = text(editor_.project().name);
    document_label_->setText(title + (editor_.dirty() ? " · Unsaved" : ""));
    setWindowTitle(title + "[*] — ERDFlow");
    setWindowModified(editor_.dirty());
    const auto& project = editor_.project();
    count_label_->setText(QString("  %1 entities  ·  %2 attributes  ·  %3 relationships  ")
        .arg(project.entities.size()).arg(project.attributes.size()).arg(project.relationships.size()));
    refreshing_ = false;
}

void MainWindow::refresh_explorer() {
    // The tree is rebuilt on every change, so what the user had opened is
    // noted first and reopened after, or each edit would fold it all shut.
    std::set<QString> opened;
    const auto identity = [](const QModelIndex& index) {
        const auto id = index.data(Qt::UserRole).toString();
        return id.isEmpty() ? index.data(Qt::DisplayRole).toString() : id;
    };
    const auto remember = [&](auto&& self, const QModelIndex& parent) -> void {
        for (int row = 0; row < explorer_model_->rowCount(parent); ++row) {
            const auto index = explorer_model_->index(row, 0, parent);
            if (explorer_->isExpanded(index)) opened.insert(identity(index));
            self(self, index);
        }
    };
    const bool first_build = explorer_model_->rowCount() == 0;
    remember(remember, QModelIndex());

    explorer_model_->clear();
    references_.clear();
    auto* project = new QStandardItem(text(editor_.project().name));
    project->setData("project", Qt::UserRole);
    project->setEditable(false);
    explorer_model_->appendRow(project);
    // Each group and every row under it wears the same glyph the toolbar uses to
    // place that kind of element, so the tree reads as the diagram does. They
    // are built from the active theme and icon set, which is why the tree is
    // rebuilt when either changes.
    const auto& colors = theme(theme_);
    const auto& attributes = editor_.project().attributes;
    // Every row is drawn as the element itself rather than as a badge for its
    // kind, using the canvas's own drawing, so a derived attribute is dashed
    // here as it is there and a weak entity wears its second border. The badge
    // stands in only if the canvas has not projected the element yet, which
    // would otherwise leave the row blank.
    const auto shaped = [&](const ElementRef& ref, Glyph fallback) {
        const auto drawn = canvas_->element_preview(ref, explorer_shape);
        return drawn.isNull() ? glyph_icon(fallback, colors, explorer_shape.height(), icon_mode_) : QIcon(drawn);
    };
    const auto owned_by = [&](const ElementRef& owner) {
        std::vector<AttributeId> owned;
        for (const auto& [id, attribute] : attributes)
            if (attribute.owner && *attribute.owner == owner) owned.push_back(id);
        return owned;
    };
    // An element's own attributes are listed beneath it, one level in, so an
    // entity can be opened to see what belongs to it. A composite attribute
    // lists its parts the same way. These are the same attributes the group
    // below counts; here they are shown by what they belong to.
    const auto nest = [&](auto&& self, QStandardItem* under, const ElementRef& owner) -> void {
        const auto owned = owned_by(owner);
        // What belongs to this row, counted at the end of it. A composite
        // attribute carries one as much as an entity does: the parts hanging
        // off it are what belongs to it.
        if (!owned.empty()) under->setData(static_cast<int>(owned.size()), owned_count_role);
        for (const auto id : owned) {
            const ElementRef ref{id};
            auto* row = new QStandardItem(shaped(ref, Glyph::Attribute), display_name(editor_.project(), ref));
            row->setData(key(ref), Qt::UserRole);
            row->setToolTip(kind_description(editor_.project(), ref) + " · " + key(ref));
            under->appendRow(row);
            references_.emplace(key(ref), ref);
            self(self, row, ref);
        }
    };
    const auto append = [&](const QString& label, Glyph glyph, const auto& collection, bool with_owned) {
        const auto badge = glyph_icon(glyph, colors, explorer_shape.height(), icon_mode_);
        // A group keeps its badge, being a kind rather than an element, and its
        // count goes at the end of the row like every other count, so the tree
        // says how many in one place and one way.
        auto* group = new QStandardItem(badge, label);
        group->setData(static_cast<int>(collection.size()), owned_count_role);
        group->setSelectable(false);
        // The group is known by a key of its own rather than by its text, whose
        // count changes with every element added: an open group that changed
        // its number must still be the same open group.
        group->setData("group:" + label, Qt::UserRole);
        project->appendRow(group);
        for (const auto& [id, item] : collection) {
            (void)item;
            const ElementRef ref{id};
            auto* row = new QStandardItem(shaped(ref, glyph), display_name(editor_.project(), ref));
            row->setData(key(ref), Qt::UserRole);
            row->setToolTip(kind_description(editor_.project(), ref) + " · " + key(ref));
            group->appendRow(row);
            references_.emplace(key(ref), ref);
            if (with_owned) nest(nest, row, ref);
        }
    };
    append("Entities", Glyph::Entity, editor_.project().entities, true);
    append("Attributes", Glyph::Attribute, attributes, false);
    append("Relationships", Glyph::Relationship, editor_.project().relationships, true);
    // Visual aids are listed only when there are any, so a diagram without
    // them is not shown two empty groups.
    if (!editor_.project().pictures.empty()) append("Pictures", Glyph::Picture, editor_.project().pictures, false);
    if (!editor_.project().notes.empty()) append("Notes", Glyph::Note, editor_.project().notes, false);

    // The groups start open and the elements under them folded, so the tree
    // shows what there is without spilling every attribute twice. After that
    // it keeps whatever the user has opened.
    if (first_build) {
        explorer_->expandToDepth(1);
    } else {
        const auto reopen = [&](auto&& self, const QModelIndex& parent) -> void {
            for (int row = 0; row < explorer_model_->rowCount(parent); ++row) {
                const auto index = explorer_model_->index(row, 0, parent);
                if (opened.contains(identity(index))) explorer_->expand(index);
                self(self, index);
            }
        };
        reopen(reopen, QModelIndex());
    }
    highlight_explorer();
}

void MainWindow::highlight_explorer() {
    const QSignalBlocker blocker(explorer_->selectionModel());
    explorer_->selectionModel()->clearSelection();
    for (const auto ref : selection_) {
        // An attribute is listed both under its owner and in the group of all
        // attributes, and both rows should light up for it.
        const auto matches = explorer_model_->match(explorer_model_->index(0, 0), Qt::UserRole,
                                                    key(ref), -1, Qt::MatchExactly | Qt::MatchRecursive);
        for (const auto& match : matches)
            explorer_->selectionModel()->select(match, QItemSelectionModel::Select | QItemSelectionModel::Rows);
    }
}

void MainWindow::refresh_properties() {
    if (auto* old = properties_->takeWidget()) { old->hide(); old->deleteLater(); }
    auto* panel = new QWidget(properties_);
    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    if (selection_.size() != 1) {
        layout->addWidget(hint(selection_.empty()
            ? "Select an object to edit its name, description, and conceptual properties."
            : QString("%1 objects selected. Drag to move together, or use Edit to duplicate or delete.").arg(selection_.size()), panel));
        layout->addStretch();
        properties_->setWidget(panel);
        return;
    }
    const auto ref = selection_.front();
    const auto& project = editor_.project();
    const auto& colors = theme(theme_);
    // Every field label is written the same way: bold, in the theme's own ink.
    // The rows are already told apart by their words and by the controls
    // beside them, so colouring each one would be decoration, and it would
    // compete with the one colour here that carries meaning -- the colour an
    // element is drawn in on the diagram, worn by its heading and its name.
    const auto label_tone = colors.text;
    // A choice whose words are long must not force the panel wider than the
    // window can spare: the closed box shortens to what it is given and the
    // list still reads in full when it is opened.
    const auto narrowable = [this](QComboBox* box) {
        box->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        box->setMinimumContentsLength(8);
        box->installEventFilter(wheel_guard_);
        return box;
    };
    // One element written inside its own shape, for a card.
    // The proportions an element is drawn with on the diagram.
    const auto aspect_of = [&project](const ElementRef& element) {
        const auto found = project.layout.find(element);
        if (found == project.layout.end() || found->second.height <= 0) return 2.0;
        return found->second.width / found->second.height;
    };
    const auto shaped_tag = [this, &project, &colors, aspect_of](const ElementRef& element, const char* named, QWidget* parent) {
        const auto pointed = std::holds_alternative<RelationshipId>(element)
                          || std::holds_alternative<SpecializationId>(element);
        return static_cast<QWidget*>(new ShapedTag(
            [this, element](QSize size) { return canvas_->element_preview(element, size, true); },
            display_name(project, element), readable_on(surface_of(project, colors, element)),
            aspect_of(element), pointed, named, parent));
    };
    auto* heading = new QLabel(kind_label(project, ref), panel);
    heading->setObjectName("propertyHeading");
    // The heading says what kind of thing this is, so it stays a title in the
    // theme's own accent. The element's colour is carried by the name field
    // below it and by the cards; wearing it here as well would say the same
    // thing twice and leave the panel shouting.
    const auto surface = surface_of(project, colors, ref);
    const auto ink = readable_on(surface);
    layout->addWidget(heading);
    auto* form = new QFormLayout;
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    // The name is written inside the shape the element is drawn as, in the ink
    // that reads on its colour, so what is being named is plain.
    const bool pointed = std::holds_alternative<RelationshipId>(ref)
                      || std::holds_alternative<SpecializationId>(ref);
    auto* shaped = new ShapedName([this, ref](QSize size) { return canvas_->element_preview(ref, size, true); },
                                  aspect_of(ref), text(name(project, ref)), pointed, panel);
    auto* name_edit = new QLineEdit(text(name(project, ref)), shaped);
    name_edit->setObjectName("elementName");
    name_edit->setAlignment(Qt::AlignCenter);
    shaped->hold(name_edit);
    // The name field carries the same colour. Its border is darkened from the
    // fill rather than taken from the theme, which would otherwise draw a line
    // the element's colour knows nothing about around it.
    // The focus ring has to be restated: a style set on the widget wins over the
    // application's, so the theme's focus rule no longer reaches this field, and
    // it is drawn in the ink rather than the accent, which is the one colour
    // already known to contrast with whatever fill the element carries.
    name_edit->setStyleSheet(QStringLiteral(
        "QLineEdit#elementName { background: transparent; color: %1; border: 1px solid transparent;"
        " border-radius: 2px; font-size: 13px; font-weight: 700; }"
        "QLineEdit#elementName:focus { border: 1px solid %1; }")
        .arg(ink.name()));
    name_edit->setMaxLength(512);
    name_edit->setToolTip(text(name(project, ref)));
    // A name longer than its shape is read from its beginning.
    name_edit->setCursorPosition(0);
    // Right-clicking a chosen word offers to leave a remark on that word alone,
    // beneath the cut-and-paste entries the field already has: a comment on a
    // name is often about one part of it rather than the whole of it.
    offer_text_comment(name_edit);
    form->addRow(field_label("Name", label_tone, panel), shaped);
    connect(name_edit, &QLineEdit::editingFinished, this, [this, ref, name_edit] {
        if (refreshing_ || !exists(editor_.project(), ref)) return;
        const auto value = bytes(name_edit->text());
        if (value != name(editor_.project(), ref)) show_result(editor_.rename(ref, value));
    });
    layout->addLayout(form);
    // The same lock the element's right-click menu offers (Zain, 2026-09-26):
    // while it is on, every attribute placed is attached to this. Kept near
    // the top, where it is seen, since a relationship's panel runs long.
    if (canvas_->can_own_attributes(ref)) {
        const bool holding = canvas_->attribute_owner() == ref;
        auto* owner_lock = new QPushButton(holding ? "Unlock attribute owner" : "Lock as attribute owner", panel);
        owner_lock->setObjectName("attributeOwnerLock");
        owner_lock->setCheckable(true);
        owner_lock->setChecked(holding);
        owner_lock->setToolTip(holding
            ? "Stop attaching attributes to this. Each one placed then stands on its own."
            : "Attach every attribute placed from now on to this, with its line drawn.");
        connect(owner_lock, &QPushButton::clicked, this, [this, ref](bool on) {
            canvas_->set_attribute_owner(on ? std::optional<ElementRef>{ref} : std::nullopt);
        });
        layout->addWidget(owner_lock);
    }
    if (const auto* entity_id = std::get_if<EntityId>(&ref)) {
        // Regular or weak. A weak entity is identified through an identifying
        // relationship rather than by a key of its own.
        const auto& entity = project.entities.at(*entity_id);
        auto* kind = narrowable(new QComboBox(panel));
        kind->setObjectName("entityKind");
        kind->addItem("Regular — identified by its own key");
        kind->addItem("Weak — identified through a relationship");
        kind->setCurrentIndex(entity.weak ? 1 : 0);
        connect(kind, &QComboBox::activated, this, [this, id = *entity_id](int index) {
            if (refreshing_) return;
            show_result(editor_.set_entity_weak(id, index == 1));
        });
        auto* kind_form = new QFormLayout;
        kind_form->setRowWrapPolicy(QFormLayout::WrapAllRows);
        kind_form->addRow(field_label("Kind", label_tone, panel), kind);
        layout->addLayout(kind_form);
        if (entity.weak)
            layout->addWidget(hint("Drawn with a double border. Connect it to an identifying relationship, usually with total "
                                   "participation; its key attribute is a partial key, underlined in dashes.", panel));
        // An entity that relates to itself, as an employee who manages
        // employees does. Ticking it draws the relationship that says so;
        // clearing it takes those relationships away again.
        auto* recursive = new QCheckBox("Recursive — relates to itself", panel);
        recursive->setObjectName("entityRecursive");
        recursive->setChecked(!recursions_of(project, *entity_id).empty());
        recursive->setToolTip("A relationship from this entity back to itself. Give each side a role to say which is which.");
        connect(recursive, &QCheckBox::clicked, this, [this, id = *entity_id](bool on) {
            if (refreshing_) return;
            const auto& current = editor_.project();
            const auto existing = recursions_of(current, id);
            if (!on) {
                if (!existing.empty()) show_result(editor_.erase(existing));
                return;
            }
            if (!existing.empty()) return;
            // Placed off the entity's side, which is where the loop is drawn
            // from, so the shape is right the moment it appears.
            const auto found = current.layout.find(ElementRef{id});
            const auto box = found == current.layout.end() ? domain::Rect{} : found->second;
            const domain::Rect body{box.x + box.width + 130,
                                    box.y + box.height / 2 - relationship_body.height / 2,
                                    relationship_body.width, relationship_body.height};
            const auto result = editor_.relate(id, id, body, "Relationship");
            show_result(result);
            if (result && result.created) canvas_->select_elements({*result.created}, true);
        });
        layout->addWidget(recursive);
        if (!recursions_of(project, *entity_id).empty())
            layout->addWidget(hint("The second side loops back around the entity. Drag the line to shape it by hand, "
                                   "or double-click it to hand it back.", panel));
    }
    if (const auto* picture_id = std::get_if<PictureId>(&ref)) {
        // The picture itself, small, so the panel says which picture this is.
        const auto& picture = project.pictures.at(*picture_id);
        QPixmap pixmap;
        pixmap.loadFromData(picture.image.data(), static_cast<uint>(picture.image.size()));
        auto* preview = new QLabel(panel);
        preview->setObjectName("picturePreview");
        preview->setAlignment(Qt::AlignCenter);
        if (!pixmap.isNull()) preview->setPixmap(pixmap.scaled(240, 160, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        layout->addWidget(preview);
        layout->addWidget(hint(QString("%1 × %2 pixels · %3 KB").arg(pixmap.width()).arg(pixmap.height())
                                   .arg((picture.image.size() + 1023) / 1024), panel));
        layout->addWidget(hint("A picture is a visual aid: it is not part of the model and nothing connects to it. "
                               "Set its size below; it keeps its proportions.", panel));
    }
    if (const auto* note_id = std::get_if<NoteId>(&ref)) {
        const bool symbol = project.notes.at(*note_id).plain;
        layout->addWidget(hint(symbol
                                   ? "The name is the character that is drawn. It is drawn on its own, with nothing "
                                     "behind it, and fills whatever size it is given."
                                   : "The name is the note's title, drawn bold; the text below is drawn beneath it.",
                               panel));
        if (symbol) {
            // One number rather than the width and height below, because a
            // symbol is drawn to the smaller of the two and so is square in
            // practice. The two fields are still there for anyone who wants a
            // box that is not; this row keeps the common case to one figure.
            const auto box = project.layout.at(ref);
            auto* size_form = new QFormLayout;
            size_form->setRowWrapPolicy(QFormLayout::WrapAllRows);
            auto* size = new QSpinBox(panel);
            size->setObjectName("symbolSize");
            size->setRange(static_cast<int>(min_symbol_size), static_cast<int>(max_symbol_size));
            size->setValue(static_cast<int>(std::round(std::min(box.width, box.height))));
            size->setSingleStep(8);
            size->setSuffix(" units");
            size->setKeyboardTracking(false);
            size->setToolTip("How big the character is drawn. Its corners on the diagram do the same thing by hand.");
            size->installEventFilter(wheel_guard_);
            connect(size, &QSpinBox::valueChanged, this, [this, ref](int value) {
                if (refreshing_ || !exists(editor_.project(), ref)) return;
                const auto current = editor_.project().layout.at(ref);
                const double side = value;
                // Grown about its centre, so a symbol stays where it was put
                // instead of walking down and to the right as it is enlarged.
                show_result(editor_.resize_symbols({{ref, {current.x + (current.width - side) / 2,
                                                           current.y + (current.height - side) / 2, side, side}}}),
                            false);
            });
            size_form->addRow(field_label("Size", label_tone, panel), size);
            layout->addLayout(size_form);
        }
    }
    if (const auto* id = std::get_if<AttributeId>(&ref)) {
        const auto attribute_id = *id;
        const auto attribute = project.attributes.at(attribute_id);
        auto* attr_form = new QFormLayout;
        attr_form->setRowWrapPolicy(QFormLayout::WrapAllRows);
        auto* kind = narrowable(new QComboBox(panel));
        kind->setObjectName("attributeKind");
        kind->addItems({"Normal", "Key", "Composite", "Multivalued", "Derived"});
        kind->setCurrentIndex(static_cast<int>(attribute.kind));
        attr_form->addRow(field_label("Attribute kind", label_tone, panel), kind);
        connect(kind, &QComboBox::activated, this, [this, attribute_id](int index) {
            show_result(editor_.set_attribute_kind(attribute_id, static_cast<AttributeKind>(index)));
        });
        auto* owner = narrowable(new QComboBox(panel));
        owner->setObjectName("attributeOwner");
        owner->addItem("Unassigned");
        std::vector<std::optional<AttributeOwner>> owners{{}};
        for (const auto& [candidate_key, candidate] : references_) {
            (void)candidate_key;
            if (candidate == ref || is_figure(candidate)) continue;
            if (const auto* attr = std::get_if<AttributeId>(&candidate);
                attr && project.attributes.at(*attr).kind != AttributeKind::Composite) continue;
            if (attribute.kind == AttributeKind::Key && std::holds_alternative<RelationshipId>(candidate)) continue;
            owners.push_back(candidate);
            owner->addItem(kind_label(candidate) + ": " + display_name(project, candidate));
            if (attribute.owner == owners.back()) owner->setCurrentIndex(owner->count() - 1);
        }
        attr_form->addRow(field_label("Owner", label_tone, panel), owner);
        connect(owner, &QComboBox::activated, this, [this, attribute_id, owners](int index) {
            show_result(editor_.set_attribute_owner(attribute_id, owners.at(static_cast<std::size_t>(index))));
        });
        layout->addLayout(attr_form);
        layout->addWidget(hint("Composite attributes can own other attributes. Key attributes belong to entities.", panel));
    }
    if (const auto* specialization_id = std::get_if<SpecializationId>(&ref)) {
        const auto& specialization = project.specializations.at(*specialization_id);
        layout->addWidget(hint("An ISA triangle. Connect the entity it generalises first; every entity connected "
                               "after that becomes a subtype. The two rules below decide how it converts to relations.", panel));
        // The triangle points the way the hierarchy is read, so the direction is
        // an editable property rather than only a choice made at creation.
        auto* direction = narrowable(new QComboBox(panel));
        direction->setObjectName("specializationDirection");
        direction->addItem("Specialization — points down at the subtypes");
        direction->addItem("Generalization — points up at the supertype");
        direction->setCurrentIndex(specialization.direction == Inheritance::Generalization ? 1 : 0);
        connect(direction, &QComboBox::activated, this, [this, id = *specialization_id, direction](int index) {
            if (refreshing_) return;
            (void)direction;
            show_result(editor_.set_inheritance_direction(id,
                index == 1 ? Inheritance::Generalization : Inheritance::Specialization));
        });
        auto* direction_form = new QFormLayout;
        direction_form->setRowWrapPolicy(QFormLayout::WrapAllRows);
        direction_form->addRow(field_label("Direction", label_tone, panel), direction);
        layout->addLayout(direction_form);
        auto* super = new QLabel(specialization.supertype
            ? "Supertype: " + display_name(project, ElementRef{*specialization.supertype})
            : QStringLiteral("Supertype: not connected yet"), panel);
        super->setObjectName("specializationSupertype");
        super->setStyleSheet(QStringLiteral("QLabel#specializationSupertype { color: %1; font-weight: 700; }")
                                 .arg(label_tone.name()));
        layout->addWidget(super);
        if (specialization.supertype) {
            auto* detach_super = new QPushButton("Detach supertype", panel);
            detach_super->setObjectName("detachSupertype");
            connect(detach_super, &QPushButton::clicked, this, [this, id = *specialization_id] {
                show_result(editor_.set_supertype(id, {}));
            });
            layout->addWidget(detach_super);
        }
        // Disjoint or overlapping, and total or partial, are exactly the inputs
        // a later Conceptual to Relational conversion needs to choose a mapping.
        auto* constraint = narrowable(new QComboBox(panel));
        constraint->setObjectName("specializationConstraint");
        constraint->addItem("Disjoint — at most one subtype");
        constraint->addItem("Overlapping — may be several subtypes");
        constraint->setCurrentIndex(specialization.constraint == Disjointness::Overlapping ? 1 : 0);
        auto* completeness = narrowable(new QComboBox(panel));
        completeness->setObjectName("specializationCompleteness");
        completeness->addItem("Partial — need not be any subtype");
        completeness->addItem("Total — must be some subtype");
        completeness->setCurrentIndex(specialization.completeness == Completeness::Total ? 1 : 0);
        const auto apply_rules = [this, id = *specialization_id, constraint, completeness] {
            if (refreshing_) return;
            show_result(editor_.set_specialization_rules(id,
                constraint->currentIndex() == 1 ? Disjointness::Overlapping : Disjointness::Disjoint,
                completeness->currentIndex() == 1 ? Completeness::Total : Completeness::Partial));
        };
        connect(constraint, &QComboBox::activated, this, [apply_rules](int) { apply_rules(); });
        connect(completeness, &QComboBox::activated, this, [apply_rules](int) { apply_rules(); });
        auto* rules = new QFormLayout;
        rules->setRowWrapPolicy(QFormLayout::WrapAllRows);
        rules->addRow(field_label("Constraint", label_tone, panel), constraint);
        rules->addRow(field_label("Completeness", label_tone, panel), completeness);
        layout->addLayout(rules);
        for (const auto& subtype : specialization.subtypes) {
            auto* card = new QWidget(panel);
            card->setObjectName("participantCard");
            auto* row = new QFormLayout(card);
            row->setRowWrapPolicy(QFormLayout::WrapAllRows);
            row->addRow(card_title(shaped_tag(ElementRef{subtype}, "cardShape", card),
                                   shaped_tag(ref, "cardTowardShape", card), card));
            auto* detach = new QPushButton("Detach subtype", card);
            connect(detach, &QPushButton::clicked, this, [this, id = *specialization_id, subtype] {
                show_result(editor_.detach_subtype(id, subtype));
            });
            row->addRow(detach);
            layout->addWidget(card);
        }
    }
    if (const auto* id = std::get_if<RelationshipId>(&ref)) {
        const auto relationship_id = *id;
        const auto& relationship = project.relationships.at(*id);
        // Regular, identifying or associative. An identifying relationship is
        // the one a weak entity is identified through; an associative one
        // keeps its own identity, so it can take part in further relationships
        // exactly as an entity does.
        auto* kind = narrowable(new QComboBox(panel));
        kind->setObjectName("relationshipKind");
        kind->addItem("Regular");
        kind->addItem("Identifying — identifies a weak entity");
        kind->addItem("Associative — has an identity of its own");
        kind->setCurrentIndex(static_cast<int>(relationship_kind(relationship)));
        kind->setToolTip("An identifying relationship is drawn as a double diamond; an associative one as a diamond in a box.");
        connect(kind, &QComboBox::activated, this, [this, id = relationship.id](int index) {
            if (refreshing_) return;
            const auto found_relationship = editor_.project().relationships.find(id);
            if (found_relationship == editor_.project().relationships.end()) return;
            const auto chosen = static_cast<RelationshipKind>(index);
            // An associative entity takes the entity body size, since that is
            // what it behaves as. Keep it centred so the diagram does not shift.
            std::optional<domain::Rect> body;
            const bool was = found_relationship->second.associative;
            const bool becomes = chosen == RelationshipKind::Associative;
            const auto found = editor_.project().layout.find(domain::ElementRef{id});
            if (was != becomes && found != editor_.project().layout.end()) {
                const auto& size = becomes ? entity_body : relationship_body;
                const auto& current = found->second;
                body = domain::Rect{current.x + current.width / 2 - size.width / 2,
                                    current.y + current.height / 2 - size.height / 2,
                                    size.width, size.height};
            }
            show_result(editor_.set_relationship_kind(id, chosen, body));
        });
        auto* kind_form = new QFormLayout;
        kind_form->setRowWrapPolicy(QFormLayout::WrapAllRows);
        kind_form->addRow(field_label("Kind", label_tone, panel), kind);
        layout->addLayout(kind_form);
        // The ratio is the two maximums read together. Setting it here writes
        // both participants at once, so every notation redraws consistently.
        auto* ratio = narrowable(new QComboBox(panel));
        ratio->setObjectName("relationshipRatio");
        const std::array<std::pair<Cardinality, Cardinality>, 4> ratios{{
            {Cardinality::One, Cardinality::One}, {Cardinality::One, Cardinality::Many},
            {Cardinality::Many, Cardinality::One}, {Cardinality::Many, Cardinality::Many}
        }};
        for (const auto& [first, second] : ratios)
            ratio->addItem(QString("%1:%2").arg(first == Cardinality::One ? "1" : "M",
                                                second == Cardinality::One ? "1" : "M"));
        const bool binary = relationship.participants.size() == 2;
        ratio->setEnabled(binary);
        if (binary) {
            const auto current = std::make_pair(relationship.participants[0].maximum,
                                                relationship.participants[1].maximum);
            ratio->setCurrentIndex(static_cast<int>(
                std::find(ratios.begin(), ratios.end(), current) - ratios.begin()));
        }
        connect(ratio, &QComboBox::activated, this, [this, id = relationship.id, ratios](int index) {
            if (refreshing_ || index < 0) return;
            show_result(editor_.set_ratio(id, ratios[static_cast<std::size_t>(index)].first,
                                              ratios[static_cast<std::size_t>(index)].second));
        });
        auto* reverse = new QPushButton("Reverse sides", panel);
        reverse->setObjectName("reverseRelationship");
        reverse->setEnabled(binary);
        reverse->setToolTip("Swap the two sides' constraints, turning 1:M into M:1.");
        connect(reverse, &QPushButton::clicked, this, [this, id = relationship.id] {
            show_result(editor_.reverse_participants(id));
        });
        auto* ratio_form = new QFormLayout;
        ratio_form->setRowWrapPolicy(QFormLayout::WrapAllRows);
        ratio_form->addRow(field_label("Ratio", label_tone, panel), ratio);
        layout->addLayout(ratio_form);
        layout->addWidget(reverse);
        layout->addWidget(hint(binary
            ? "The ratio reads left to right in the order the sides are listed below, and matches whichever notation the toolbar is showing."
            : "A ratio describes exactly two sides. Set each side's own constraints below.", panel));
        layout->addWidget(hint("Connect this relationship to entities, or to another relationship when this one is associative. Each connection has its own role and constraints.", panel));
        for (const auto& participant : relationship.participants) {
            auto* card = new QWidget(panel);
            card->setObjectName("participantCard");
            auto* participant_form = new QFormLayout(card);
            participant_form->setRowWrapPolicy(QFormLayout::WrapAllRows);
            const auto side = target_ref(participant.target);
            participant_form->addRow(card_title(shaped_tag(side, "cardShape", card),
                                                shaped_tag(ref, "cardTowardShape", card), card));
            auto* maximum = narrowable(new QComboBox(card));
            maximum->addItems({"1 — One", "M — Many"});
            maximum->setCurrentIndex(participant.maximum == Cardinality::One ? 0 : 1);
            participant_form->addRow(field_label("Maximum cardinality", label_tone, card), maximum);
            auto* participation = narrowable(new QComboBox(card));
            participation->addItems({"Partial — optional", "Total — required"});
            participation->setCurrentIndex(participant.participation == Participation::Total ? 1 : 0);
            participant_form->addRow(field_label("Participation", label_tone, card), participation);
            auto* role = new QLineEdit(text(participant.role), card);
            role->setPlaceholderText("Role (especially for recursive links)");
            role->setMaxLength(512);
            participant_form->addRow(field_label("Role", label_tone, card), role);
            const auto apply = [this, relationship_id, participant, maximum, participation, role] {
                if (refreshing_ || !editor_.project().relationships.contains(relationship_id)) return;
                const auto max = maximum->currentIndex() == 0 ? Cardinality::One : Cardinality::Many;
                const auto part = participation->currentIndex() == 0 ? Participation::Partial : Participation::Total;
                const auto role_value = bytes(role->text());
                if (max != participant.maximum || part != participant.participation || role_value != participant.role)
                    show_result(editor_.update_participant(relationship_id, participant.id, max, part, role_value));
            };
            connect(maximum, &QComboBox::activated, this, [apply](int) { apply(); });
            connect(participation, &QComboBox::activated, this, [apply](int) { apply(); });
            connect(role, &QLineEdit::editingFinished, this, apply);
            auto* disconnect = new QPushButton("Disconnect", card);
            connect(disconnect, &QPushButton::clicked, this, [this, relationship_id, participant] {
                show_result(editor_.disconnect(relationship_id, participant.id));
            });
            participant_form->addRow(disconnect);
            layout->addWidget(card);
        }
    }
    // A card note draws its text beneath its title; a symbol draws neither, so
    // for a symbol the field is a description like any other element's.
    bool note = false;
    if (const auto* note_id = std::get_if<NoteId>(&ref)) note = !project.notes.at(*note_id).plain;
    layout->addWidget(field_label(note ? "Text" : "Description", label_tone, panel));
    auto* description_edit = new DescriptionEdit(panel);
    description_edit->setObjectName("elementDescription");
    description_edit->setPlainText(text(description(project, ref)));
    description_edit->setPlaceholderText(note ? "Write the note’s text…" : "Explain this object’s meaning…");
    description_edit->setFixedHeight(100);
    offer_text_comment(description_edit);
    description_edit->commit = [this, ref, description_edit] {
        if (refreshing_ || !exists(editor_.project(), ref)) return;
        const auto value = bytes(description_edit->toPlainText());
        if (value != description(editor_.project(), ref)) show_result(editor_.describe(ref, value));
    };
    layout->addWidget(description_edit);
    // What this becomes in the schema, for the three things that become tables
    // and columns: everything conversion will need, gathered in one place and
    // behind one fold.
    //
    // It sits beneath the description because a description and a comment are
    // easily confused, and the difference is worth stating where both are
    // written: a description says what this means to a reader, a comment is
    // written for the database.
    //
    // The section folds so that a diagram being drawn and taught is not
    // crowded by questions it is not yet asking, while a model being prepared
    // for conversion has the answers together. Nothing here is conditional on
    // that fold: the fields are part of the model, they are always stored, and
    // validation, readiness and conversion all see them whether the section is
    // open or shut.
    const bool schema_bound = std::holds_alternative<EntityId>(ref)
        || std::holds_alternative<AttributeId>(ref) || std::holds_alternative<RelationshipId>(ref);
    if (schema_bound) {
        auto* section = new FoldingSection("For Relational Design", label_tone, schema_section_open_, panel);
        section->setObjectName("schemaSection");
        // Remembered for the next element looked at and the next time the
        // application is opened. It is the user's preference, so it is kept
        // with the application's settings and never in the project.
        section->folded = [this](bool open) {
            schema_section_open_ = open;
            QSettings().setValue("schemaSectionOpen", open);
        };
        auto* inside = section->body();
        inside->setObjectName("schemaSectionBody");
        auto* stacked = new QVBoxLayout(inside);
        stacked->setContentsMargins(0, 0, 0, 0);
        stacked->setSpacing(12);
        if (const auto* schema_attribute = std::get_if<AttributeId>(&ref)) {
            const auto attribute_id = *schema_attribute;
            const auto attribute = project.attributes.at(attribute_id);
            auto* schema_form = new QFormLayout;
            schema_form->setRowWrapPolicy(QFormLayout::WrapAllRows);
            auto* type = narrowable(new QComboBox(inside));
            type->setObjectName("attributeLogicalType");
            // The whole catalogue, grouped by family. A flat list of forty
            // names is a list nobody reads; the families are how the types are
            // actually thought about, and the ones reached for most come first.
            type->addItem(type_label(domain::LogicalType::Unset),
                          QVariant::fromValue(static_cast<int>(domain::LogicalType::Unset)));
            for (const auto& family : type_families())
                for (const auto entry : family.types) {
                    // The family's name heads its first entry, so the list
                    // reads as groups rather than as one long run of words.
                    if (entry == family.types.front())
                        type->addItem(QString("— %1 —").arg(QString::fromLatin1(family.name)),
                                      QVariant());
                    type->addItem(type_label(entry), QVariant::fromValue(static_cast<int>(entry)));
                    if (domain::deprecated_type(entry))
                        type->setItemData(type->count() - 1,
                                          QString("%1 is being removed from SQL Server. Prefer the (max) form.")
                                              .arg(type_label(entry)), Qt::ToolTipRole);
                }
            for (int row = 0; row < type->count(); ++row) {
                if (!type->itemData(row).isValid()) {
                    // A family heading is a label, not a choice.
                    if (auto* model = qobject_cast<QStandardItemModel*>(type->model()))
                        if (auto* item = model->item(row)) item->setFlags(Qt::NoItemFlags);
                    continue;
                }
                if (type->itemData(row).toInt() == static_cast<int>(attribute.logical_type))
                    type->setCurrentIndex(row);
            }
            type->setToolTip("What this column is. The SQL type catalogue, grouped the way SQL Server "
                             "groups it, with the ones reached for most at the top.");
            schema_form->addRow(field_label("Logical type", label_tone, inside), type);

            // Only the two types that are measured offer a number, so nobody is
            // asked how long a Boolean is.
            auto* length = new QSpinBox(inside);
            length->setObjectName("attributeLength");
            length->setRange(0, static_cast<int>(domain::max_logical_length));
            length->setSpecialValueText("Unspecified");
            length->setValue(static_cast<int>(attribute.length));
            length->installEventFilter(wheel_guard_);
            length->setEnabled(domain::size_of(attribute.logical_type) != domain::TypeSize::None);
            schema_form->addRow(field_label("Length", label_tone, inside), length);
            connect(type, &QComboBox::activated, this, [this, attribute_id, type, length](int index) {
                const auto chosen = type->itemData(index);
                if (!chosen.isValid()) return;   // a family heading, not a type
                show_result(editor_.set_logical_type(attribute_id,
                                                     static_cast<domain::LogicalType>(chosen.toInt()),
                                                     static_cast<std::uint32_t>(length->value())));
            });
            connect(length, &QSpinBox::editingFinished, this, [this, attribute_id, type, length] {
                if (refreshing_) return;
                const auto chosen = type->itemData(type->currentIndex());
                if (!chosen.isValid()) return;
                show_result(editor_.set_logical_type(attribute_id,
                                                     static_cast<domain::LogicalType>(chosen.toInt()),
                                                     static_cast<std::uint32_t>(length->value())));
            });

            // What the table will enforce, kept apart from the Chen kind above:
            // the oval says how the diagram draws it, these say what the
            // database will insist on.
            // Stacked rather than in a row: three of these do not fit across a
            // narrow panel, and a docked panel is narrow whenever the window is.
            // A column fits whatever width it is given, so none is ever clipped.
            auto* rules = new QWidget(inside);
            auto* rules_column = new QVBoxLayout(rules);
            rules_column->setContentsMargins(0, 0, 0, 0);
            rules_column->setSpacing(2);
            struct Rule { const char* label; const char* name; bool set; const char* tip; };
            std::vector<QCheckBox*> boxes;
            for (const auto& rule : {
                     Rule{"Identifier", "attributeIdentifier", attribute.identifier,
                          "Part of what identifies a row."},
                     Rule{"Required", "attributeRequired", attribute.required,
                          "Must be filled in: NOT NULL."},
                     Rule{"Unique", "attributeUnique", attribute.unique,
                          "No two rows may share it."}}) {
                auto* box = new QCheckBox(QString::fromLatin1(rule.label), rules);
                box->setObjectName(QString::fromLatin1(rule.name));
                box->setChecked(rule.set);
                box->setToolTip(QString::fromUtf8(rule.tip));
                rules_column->addWidget(box);
                boxes.push_back(box);
            }
            schema_form->addRow(field_label("Rules", label_tone, inside), rules);
            for (auto* box : boxes)
                connect(box, &QCheckBox::toggled, this, [this, attribute_id, boxes] {
                    if (refreshing_) return;
                    show_result(editor_.set_attribute_rules(attribute_id, boxes[0]->isChecked(),
                                                            boxes[1]->isChecked(), boxes[2]->isChecked()));
                });
            stacked->addLayout(schema_form);
        }
        // The section already says these are for the schema, so the field is
        // called what it is rather than repeating where it is.
        stacked->addWidget(field_label("Comment", label_tone, inside));
        auto* schema_edit = new DescriptionEdit(inside);
        schema_edit->setObjectName("elementSchemaComment");
        schema_edit->setPlainText(text(schema_comment_of(project, ref)));
        schema_edit->setPlaceholderText("What the generated table or column should say about itself…");
        schema_edit->setFixedHeight(64);
        offer_text_comment(schema_edit);
        schema_edit->commit = [this, ref, schema_edit] {
            if (refreshing_ || !exists(editor_.project(), ref)) return;
            const auto value = bytes(schema_edit->toPlainText());
            if (value != schema_comment_of(editor_.project(), ref))
                show_result(editor_.set_schema_comment(ref, value));
        };
        stacked->addWidget(schema_edit);
        layout->addWidget(section);
    }
    layout->addWidget(hint("Text changes apply when you leave the field. Every applied change can be undone.", panel));
    auto* geometry = new QFormLayout;
    const auto rect = project.layout.at(ref);
    std::array<QDoubleSpinBox*, 4> fields{};
    const std::array<double, 4> values{rect.x, rect.y, rect.width, rect.height};
    const std::array<QString, 4> names{"X", "Y", "Width", "Height"};
    for (std::size_t i = 0; i < fields.size(); ++i) {
        fields[i] = new QDoubleSpinBox(panel);
        fields[i]->setDecimals(0);
        fields[i]->setRange(i < 2 ? -max_coordinate : 24, max_coordinate);
        fields[i]->setValue(values[i]);
        fields[i]->setKeyboardTracking(false);
        fields[i]->setObjectName("geometry" + names[i]);
        fields[i]->installEventFilter(wheel_guard_);
        geometry->addRow(field_label(names[i], label_tone, panel), fields[i]);
    }
    layout->addLayout(geometry);
    auto* apply_geometry = new QPushButton("Apply position and size", panel);
    apply_geometry->setObjectName("applyGeometry");
    connect(apply_geometry, &QPushButton::clicked, this, [this, ref, fields] {
        show_result(editor_.move({{ref, {fields[0]->value(), fields[1]->value(), fields[2]->value(), fields[3]->value()}}}));
    });
    layout->addWidget(apply_geometry);
    build_comment_section(panel, layout);
    layout->addStretch();
    properties_->setWidget(panel);
}

void MainWindow::build_comment_section(QWidget* panel, QVBoxLayout* layout) {
    if (selection_.size() != 1) return;
    const auto ref = selection_.front();
    const auto& project = editor_.project();
    const auto pinned = comments_on(project, ref);
    if (pinned.empty()) return;
    const auto& colors = theme(theme_);

    auto* heading = new QLabel(pinned.size() == 1 ? "Comment" : QString("Comments (%1)").arg(pinned.size()), panel);
    heading->setObjectName("commentHeading");
    heading->setStyleSheet(QString("font-weight: 700; color: %1;").arg(colors.text.name()));
    layout->addWidget(heading);

    for (const auto& id : pinned) {
        const auto& comment = project.comments.at(id);
        auto* row = new QWidget(panel);
        row->setObjectName("commentRow");
        auto* rows = new QVBoxLayout(row);
        rows->setContentsMargins(10, 8, 10, 8);
        rows->setSpacing(6);
        // A remark stands on the theme's warning colour, faintly, so it reads
        // as something said about the diagram rather than as part of it -- the
        // same hue the mark on the canvas wears, so the two are plainly one
        // thing seen in two places.
        row->setStyleSheet(QString("QWidget#commentRow { background: %1; border: 1px solid %2;"
                                   " border-radius: 4px; }")
                               .arg(over(colors.panel, QColor(colors.warning.red(), colors.warning.green(),
                                                              colors.warning.blue(), 28)).name(),
                                    colors.border.name()));

        auto* said = new QLabel(text(comment.text), row);
        said->setObjectName("commentSaid");
        said->setWordWrap(true);
        said->setTextInteractionFlags(Qt::TextSelectableByMouse);
        // A remark that has been put away is still readable here, faded, so it
        // can be brought back; putting away quiets the diagram, not the panel.
        if (comment.hidden) said->setStyleSheet(QString("color: %1; font-style: italic;").arg(colors.muted.name()));
        rows->addWidget(said);

        // Where else this one remark is pinned, since a remark covering several
        // things is read differently from one about this alone.
        if (comment.targets.size() > 1) {
            const auto others = comment.targets.size() - 1;
            auto* elsewhere = new QLabel(others == 1 ? QString("Also on 1 other thing.")
                                                     : QString("Also on %1 other things.").arg(others), row);
            elsewhere->setObjectName("commentElsewhere");
            elsewhere->setStyleSheet(QString("color: %1; font-size: 11px;").arg(colors.muted.name()));
            rows->addWidget(elsewhere);
        }

        auto* buttons = new QHBoxLayout;
        buttons->setSpacing(6);
        auto* edit = new QPushButton("Edit…", row);
        edit->setObjectName("commentEdit");
        connect(edit, &QPushButton::clicked, this, [this, id] {
            const auto found = editor_.project().comments.find(id);
            if (found == editor_.project().comments.end()) return;
            const auto words = ask_for_comment(text(found->second.text), {});
            if (!words.isEmpty()) show_result(editor_.set_comment_text(id, bytes(words)), false);
        });
        auto* put_away = new QPushButton(comment.hidden ? "Show" : "Hide", row);
        put_away->setObjectName("commentHide");
        put_away->setToolTip(comment.hidden
            ? "Bring this remark back, so pointing at what it is pinned to shows it again."
            : "Put this remark away without deleting it. Its mark stays on the diagram.");
        connect(put_away, &QPushButton::clicked, this, [this, id, was = comment.hidden] {
            show_result(editor_.set_comment_hidden(id, !was), false);
        });
        auto* remove = new QPushButton("Delete", row);
        remove->setObjectName("commentDelete");
        connect(remove, &QPushButton::clicked, this, [this, id] {
            show_result(editor_.erase_comment(id), false);
        });
        buttons->addWidget(edit);
        buttons->addWidget(put_away);
        buttons->addWidget(remove);
        buttons->addStretch();
        rows->addLayout(buttons);
        layout->addWidget(row);
    }
}

// The History, as a panel of its own beside the others (Zain, 2026-09-26):
// every step Undo can take back, oldest first, said in words, with the time it
// was made. Pressing one goes back or forward to just after it, by undoing or
// redoing; pressing Start goes back to before the oldest step kept. Steps
// that have been undone stay, fainter, until a new edit takes their place.
// It is closed until opened, and its entry goes at the end of View, after
// everything already there, so nothing there moves to make room for it.
void MainWindow::build_history() {
    history_dock_ = new QDockWidget("History", this);
    history_dock_->setObjectName("historyDock");
    history_list_ = new QTreeWidget(history_dock_);
    history_list_->setObjectName("historyList");
    history_list_->setAccessibleName("History");
    history_list_->setColumnCount(2);
    history_list_->setHeaderHidden(true);
    history_list_->setRootIsDecorated(false);
    history_list_->setUniformRowHeights(true);
    history_list_->setSelectionMode(QAbstractItemView::SingleSelection);
    history_list_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    history_list_->header()->setStretchLastSection(false);
    history_list_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    history_list_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    history_dock_->setWidget(history_list_);
    addDockWidget(Qt::RightDockWidgetArea, history_dock_);
    history_dock_->hide();
    // A press and an activation can both arrive for one click; the second
    // finds the history already where it was asked to be, and does nothing.
    const auto go_to = [this](QTreeWidgetItem* item) {
        if (!item) return;
        const auto row = history_list_->indexOfTopLevelItem(item);
        if (row < 0 || static_cast<std::size_t>(row) == editor_.history_position()) return;
        if (notice_) notice_->put_away();
        finish_field_edit();
        canvas_->cancel_interaction();
        show_result(editor_.go_to(static_cast<std::size_t>(row)));
    };
    connect(history_list_, &QTreeWidget::itemClicked, this, [go_to](QTreeWidgetItem* item) { go_to(item); });
    connect(history_list_, &QTreeWidget::itemActivated, this, [go_to](QTreeWidgetItem* item) { go_to(item); });
    // Kept up to date only while it can be seen, and brought up to date the
    // moment it can.
    connect(history_dock_, &QDockWidget::visibilityChanged, this, [this](bool shown) {
        if (shown) refresh_history(true);
    });
    if (auto* view = findChild<QMenu*>("viewMenu")) {
        view->addSeparator();
        auto* toggle = history_dock_->toggleViewAction();
        toggle->setObjectName("viewHistory");
        toggle->setToolTip("Every change made, in order. Press one to go back to it.");
        view->addAction(toggle);
    }
}

void MainWindow::refresh_history(bool again) {
    if (!history_list_ || !history_dock_->isVisible()) return;
    if (!again && history_shown_ == editor_.revision()) return;
    history_shown_ = editor_.revision();
    const auto entries = editor_.history();
    const auto& colors = theme(theme_);
    const QSignalBlocker quiet(history_list_);
    history_list_->clear();
    auto* start = new QTreeWidgetItem(history_list_, QStringList{"Start", QString{}});
    start->setToolTip(0, "The project as it was before the oldest change kept here: as it was opened or created.");
    for (const auto& entry : entries) {
        const auto when = QDateTime::fromMSecsSinceEpoch(
            std::chrono::duration_cast<std::chrono::milliseconds>(entry.when.time_since_epoch()).count());
        auto* item = new QTreeWidgetItem(history_list_,
                                         QStringList{text(entry.description), when.toString("HH:mm:ss")});
        item->setToolTip(0, text(entry.description) + "\n" + text(entry.label) + " · "
                                + QLocale().toString(when, QLocale::ShortFormat)
                                + (entry.in_effect ? QString{} : QStringLiteral("\nUndone. Press it to redo it.")));
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        item->setForeground(1, colors.muted);
        if (!entry.in_effect) {
            auto font = item->font(0);
            font.setItalic(true);
            item->setFont(0, font);
            item->setForeground(0, colors.muted);
        }
    }
    auto* current = history_list_->topLevelItem(static_cast<int>(editor_.history_position()));
    history_list_->setCurrentItem(current);
    history_list_->scrollToItem(current);
}

void MainWindow::refresh_validation() {
    issue_model_->clear();
    issue_model_->setHorizontalHeaderLabels({"Level", "Object", "Message"});
    const auto findings = validate(editor_.project());
    for (const auto& issue : findings) {
        const auto level = issue.severity == Severity::Error ? "Error" : issue.severity == Severity::Warning ? "Warning" : "Info";
        auto* severity = new QStandardItem(level);
        if (issue.element) severity->setData(key(*issue.element), Qt::UserRole);
        issue_model_->appendRow({severity,
            new QStandardItem(issue.element ? display_name(editor_.project(), *issue.element) : "Project"),
            new QStandardItem(text(issue.message))});
    }
    if (findings.empty()) issue_model_->appendRow({new QStandardItem("OK"), new QStandardItem("Project"),
        new QStandardItem("No issues in the supported conceptual checks.")});
    issues_->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    issues_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    issues_->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    readiness_label_->setText(findings.empty() ? "No model issues   " : QString("%1 model checks   ").arg(findings.size()));
}

void MainWindow::refresh_selection_commands() {
    duplicate_->setEnabled(!selection_.empty());
    rename_->setEnabled(selection_.size() == 1);
    // Every one of them, not merely one: a command that acted on the
    // symbols in a mixed selection and silently left the rest alone would
    // be doing something other than what its name says.
    const auto sizeable = !selection_.empty() && canvas_->selected_symbols().size() == selection_.size();
    enlarge_->setEnabled(sizeable);
    shrink_->setEnabled(sizeable);
}

void MainWindow::selection_changed(const std::vector<ElementRef>& selection) {
    if (refreshing_ || selection_ == selection) return;
    refreshing_ = true;
    selection_ = selection;
    highlight_explorer();
    refresh_properties();
    refresh_selection_commands();
    refreshing_ = false;
}

void MainWindow::show_result(const application::EditResult& result, bool choose_what_was_made) {
    refresh();
    if (!result) {
        statusBar()->showMessage(text(result.error), 12000);
        QMessageBox::warning(this, "Change could not be applied", text(result.error));
    } else if (result.created && choose_what_was_made) canvas_->select_elements({*result.created});
}

// One entry point, so the menu and the toolbar picker cannot disagree about
// which notation is in use.
void MainWindow::choose_notation(Notation notation) {
    canvas_->set_notation(notation);
    // The schema draws its line ends in the same notation as the diagram, so it
    // is told here rather than only when a theme changes. Without this the
    // panel keeps whichever notation it was opened with, and a schema asked for
    // crow's feet goes on drawing Chen's letters.
    if (schema_) schema_->set_notation(notation);
    if (schema_notation_)
        for (auto* choice : schema_notation_->actions())
            choice->setChecked(choice->data().toInt() == static_cast<int>(notation));
    const auto previous = refreshing_;
    refreshing_ = true;
    if (const auto found = notation_actions_.find(notation); found != notation_actions_.end())
        found->second->setChecked(true);
    if (notation_box_) notation_box_->setCurrentIndex(static_cast<int>(notation));
    refreshing_ = previous;
}

// Choosing a tool and reporting the choice happen in one place, so the button
// label can never disagree with what the canvas will actually do.
void MainWindow::choose_line_style(LineStyle style) {
    canvas_->set_line_style(style);
    for (const auto& [candidate, action] : line_actions_) action->setChecked(candidate == style);
}

void MainWindow::choose_tool(Tool tool, bool locked) {
    finish_field_edit();
    canvas_->set_tool(tool, locked);
    canvas_->setFocus();
    refresh_tool_labels();
}

// A click anywhere in the window outside the diagram puts down the tool in
// hand, locked or not, and takes up Select (Zain, 2026-09-26). Inside the
// diagram a click does what the tool does, placing what it places. The
// diagram's own zoom controls and scrollbars are part of it, since they are
// how the place for the next element is found. A button that chooses a tool
// is left to choose it.
//
// Handed back once the click has been dealt with rather than as it lands,
// so a name being typed on the diagram is kept by the click that moves away
// from it, as it always has been, instead of being dropped by the tool going.
void MainWindow::pressed_outside_canvas(QWidget* pressed) {
    if (!canvas_ || canvas_->tool() == Tool::Select) return;
    if (pressed == canvas_ || canvas_->isAncestorOf(pressed)) return;
    if (const auto* button = qobject_cast<QToolButton*>(pressed); button && button->defaultAction())
        for (const auto& [tool, action] : tool_actions_)
            if (button->defaultAction() == action) return;
    QTimer::singleShot(0, this, [this] {
        if (canvas_ && canvas_->tool() != Tool::Select) canvas_->set_tool(Tool::Select);
    });
}

void MainWindow::place_canvas_controls() {
    if (!canvas_controls_) return;
    canvas_controls_->adjustSize();
    const auto size = canvas_controls_->size();
    QPoint at;
    if (canvas_controls_place_) {
        // Kept where it was put as a fraction of the view, so a raft dragged
        // to the middle stays in the middle when the window is resized rather
        // than drifting towards a corner.
        at = QPoint(qRound(canvas_controls_place_->x() * canvas_->width()),
                    qRound(canvas_controls_place_->y() * canvas_->height()));
    } else {
        // Measured from the view's own edge and inset by a scrollbar's thickness
        // whether or not one is showing, so fitting the diagram — which brings
        // scrollbars in or takes them out — never moves the raft.
        const auto bar = canvas_->style()->pixelMetric(QStyle::PM_ScrollBarExtent, nullptr, canvas_);
        at = QPoint(canvas_->width() - size.width() - bar - 12,
                    canvas_->height() - size.height() - bar - 12);
    }
    // Held inside the view: a raft dragged to an edge and then met with a
    // smaller window must not end up off the side where it cannot be reached.
    at.setX(std::clamp(at.x(), 4, std::max(4, canvas_->width() - size.width() - 4)));
    at.setY(std::clamp(at.y(), 4, std::max(4, canvas_->height() - size.height() - 4)));
    canvas_controls_->move(at);
    canvas_controls_->raise();
}

void MainWindow::move_canvas_controls(QPoint by) {
    if (!canvas_controls_ || canvas_->width() <= 0 || canvas_->height() <= 0) return;
    const auto at = canvas_controls_->pos() + by;
    canvas_controls_place_ = QPointF(static_cast<double>(at.x()) / canvas_->width(),
                                     static_cast<double>(at.y()) / canvas_->height());
    place_canvas_controls();
}

void MainWindow::show_canvas_controls(bool shown) {
    if (!canvas_controls_) return;
    canvas_controls_->setVisible(shown);
    if (shown) place_canvas_controls();
    if (auto* entry = findChild<QAction*>("viewCanvasControls"); entry && entry->isChecked() != shown) {
        const QSignalBlocker quiet(entry);
        entry->setChecked(shown);
    }
    statusBar()->showMessage(shown ? "The view controls are back."
                                   : "View controls put away. Right-click the diagram to bring them back.", 7000);
}

int MainWindow::icon_pixels() const {
    auto* toolbar = findChild<QToolBar*>("modelTools");
    return toolbar ? toolbar->iconSize().width() : 34;
}

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    fit_toolbar();
    lay_out_schema();
    // A notice stands where it was put, so it is put there again whenever the
    // window it is laid over changes shape under it.
    if (notice_) notice_->settle_again();
}

// A tool that has fallen off the end of the toolbar may as well not exist, so
// the toolbar sheds what it can spare before it sheds a tool, and it sheds
// the cheapest thing first: some of the icons' size, then the words on the
// corner controls, whose check mark and half disc are read at a glance, then
// the notation picker, which the View menu also offers, and the names only
// when the window has been made genuinely small.
//
// Which of those is needed is measured rather than guessed from the window's
// width. What fits depends on how many tools there are and how long their names
// read, and a threshold picked by hand goes wrong the moment either changes.
void MainWindow::fit_toolbar() {
    auto* toolbar = findChild<QToolBar*>("modelTools");
    if (!toolbar || fitting_) return;
    fitting_ = true;
    struct Step {
        Qt::ToolButtonStyle style;
        int icon;
        bool notation;
        bool notation_named;
        bool corner_named;
    };
    // Names stay for as long as they possibly can: a tool's lock mark hangs on
    // its name, and a bar of bare icons is the state for a window that has
    // been made small, not for one at an ordinary size. The icons give up
    // size first, then the picker its word, then the corner controls theirs,
    // then the picker, and only then the names.
    static constexpr std::array<Step, 9> steps{{
        {Qt::ToolButtonTextBesideIcon, 34, true, true, true},
        {Qt::ToolButtonTextBesideIcon, 28, true, true, true},
        {Qt::ToolButtonTextBesideIcon, 24, true, true, true},
        {Qt::ToolButtonTextBesideIcon, 24, true, false, true},
        {Qt::ToolButtonTextBesideIcon, 24, true, false, false},
        {Qt::ToolButtonTextBesideIcon, 24, false, false, false},
        {Qt::ToolButtonIconOnly, 28, true, false, false},
        {Qt::ToolButtonIconOnly, 24, false, false, false},
        {Qt::ToolButtonIconOnly, 20, false, false, false},
    }};
    for (std::size_t index = 0; index < steps.size(); ++index) {
        const auto& step = steps[index];
        toolbar->setToolButtonStyle(step.style);
        toolbar->setIconSize(QSize(step.icon, step.icon));
        for (const char* named : {"isaButton", "connectButton"})
            if (auto* button = findChild<QToolButton*>(named)) {
                button->setToolButtonStyle(step.style);
                button->setIconSize(toolbar->iconSize());
            }
        // The corner controls are set after the bar, since the bar hands its
        // own style to the buttons it made and the corner's may differ.
        const auto corner_style = step.corner_named ? step.style : Qt::ToolButtonIconOnly;
        if (auto* check = findChild<QAction*>("checkModel"))
            if (auto* button = qobject_cast<QToolButton*>(toolbar->widgetForAction(check)))
                button->setToolButtonStyle(corner_style);
        if (theme_button_) {
            theme_button_->setToolButtonStyle(corner_style);
            theme_button_->setIconSize(toolbar->iconSize());
        }
        // Hiding the widget would leave its room behind in the toolbar's layout;
        // it is the action holding it that has to go.
        for (auto* hidden : {notation_separator_, notation_action_})
            if (hidden) hidden->setVisible(step.notation);
        if (notation_label_action_) notation_label_action_->setVisible(step.notation && step.notation_named);
        toolbar->adjustSize();
        if (toolbar->sizeHint().width() <= width() || index + 1 == steps.size()) break;
    }
    fitting_ = false;
    refresh_icons();
}

void MainWindow::set_icon_mode(IconMode mode) {
    icon_mode_ = mode;
    QSettings().setValue("iconMode", icon_mode_key(mode));
    for (const auto& [candidate, action] : icon_mode_actions_) action->setChecked(candidate == mode);
    refresh_icons();
    refresh_explorer();
}

// Everything that has to change for the window to be wearing a theme. Choosing
// one and merely looking at one do the same work; only what is remembered and
// what is ticked differ between them.
void MainWindow::apply_appearance(ThemeId id) {
    // Wearing a theme is the most expensive thing the window does on a whim:
    // the application's whole stylesheet is rebuilt and every widget in it
    // re-polished, every icon is redrawn, and the Explorer and the panel are
    // rebuilt in the new colours. Qt repeats `hovered` while the pointer moves
    // within one entry, so without this the same theme is put on again and
    // again for no change at all -- which measured slower than changing to a
    // different one, since nothing about the window was already right.
    if (appearance_applied_ && id == theme_) return;
    appearance_applied_ = true;
    theme_ = id;
    if (auto* application = qobject_cast<QApplication*>(QCoreApplication::instance()))
        apply_theme(*application, id);
    canvas_->set_theme(id);
    // The schema is drawn in the same palette as the diagram, so a table wears
    // the colours of the thing it came from.
    if (schema_) {
        schema_->set_theme(theme(id));
        schema_->set_notation(canvas_->notation());
    }
    if (notice_) notice_->wear(theme(id));
    // The Home screen asks the resolved tokens of the same theme, so it
    // follows the window rather than staying in the one it was built in.
    if (home_) home_->wear(id);
    refresh_icons();
    refresh_explorer();
    // The panel's labels are written in the theme's own hues, so they are
    // rebuilt with it rather than keeping the colours of the theme just left.
    const auto was_refreshing = refreshing_;
    refreshing_ = true;
    refresh_properties();
    refreshing_ = was_refreshing;
    // So are the History's fainter steps.
    refresh_history(true);
}

void MainWindow::preview_theme(ThemeId id) {
    if (theme_preview_timer_) theme_preview_timer_->stop();
    apply_appearance(id);
}

// Hovering asks for a theme; it does not ask for it this instant. A pointer
// travelling to the bottom of the menu crosses every entry above it, and
// showing each one costs the whole window, so what is asked for is remembered
// and shown once the pointer has settled. Resting on an entry still shows it
// promptly -- the wait is shorter than a deliberate pause -- while sliding
// past a dozen now costs one theme rather than a dozen.
void MainWindow::preview_theme_soon(ThemeId id) {
    if (id == theme_) {
        if (theme_preview_timer_) theme_preview_timer_->stop();
        return;
    }
    pending_preview_ = id;
    if (!theme_preview_timer_) {
        theme_preview_timer_ = new QTimer(this);
        theme_preview_timer_->setSingleShot(true);
        theme_preview_timer_->setInterval(45);
        connect(theme_preview_timer_, &QTimer::timeout, this,
                [this] { apply_appearance(pending_preview_); });
    }
    theme_preview_timer_->start();
}

void MainWindow::set_theme(ThemeId id) {
    if (theme_preview_timer_) theme_preview_timer_->stop();
    committed_theme_ = id;
    apply_appearance(id);
    QSettings().setValue("theme", theme(id).key);
    for (const auto& [candidate, action] : theme_actions_) action->setChecked(candidate == id);
}

// A sample drawn twice: once for an ordinary row and once for a highlighted
// one. A highlighted row is painted in the theme's accent, and a sample drawn
// in that same accent would vanish into it, so the second is inked in whatever
// reads on the accent. Qt asks for the second by itself, as QIcon::Selected,
// whenever the row carrying it is highlighted or the entry is under the
// pointer, so nothing downstream has to know which state it is in.
QIcon MainWindow::two_tone(const std::function<QPixmap(std::optional<QColor>)>& draw) const {
    // A menu highlights the entry under the pointer and asks for its icon in
    // Active; a list asks for Selected. Both are the theme's accent, and a
    // sample drawn in that accent would vanish into it, so both get the
    // version inked in whatever reads on the accent instead.
    const auto lit = readable_on(theme(theme_).accent);
    QIcon icon(draw({}));
    icon.addPixmap(draw(lit), QIcon::Active);
    icon.addPixmap(draw(lit), QIcon::Selected);
    return icon;
}

// The closed picker shows this one, on the toolbar, where nothing is
// highlighted; the rows of the dropped-down list are drawn by SampleRows,
// which knows what each row is standing on.
QIcon MainWindow::notation_icon(Notation notation) const {
    return QIcon(canvas_->notation_preview(notation, notation_sample));
}

QIcon MainWindow::line_style_icon(LineStyle style) const {
    return two_tone([this, style](std::optional<QColor> ink) {
        return canvas_->line_style_preview(style, line_style_sample, ink);
    });
}

// Icons are drawn from the theme, so they are rebuilt whenever it changes.
void MainWindow::refresh_icons() {
    const auto& colors = theme(theme_);
    // The schema marks a primary key with a key from the same set, so it
    // follows the choice with everything else rather than keeping its own.
    if (schema_) schema_->set_icon_mode(icon_mode_);
    for (const auto& [action, glyph] : action_glyphs_)
        action->setIcon(glyph_icon(glyph, colors, icon_pixels(), icon_mode_));
    if (theme_button_) theme_button_->setIcon(glyph_icon(Glyph::Theme, colors, icon_pixels(), icon_mode_));
    if (notation_box_) {
        for (int index = 0; index < notation_box_->count(); ++index)
            notation_box_->setItemIcon(index, notation_icon(static_cast<Notation>(index)));
    }
    for (const auto& [style, action] : line_actions_)
        action->setIcon(line_style_icon(style));
    // The raft's hand carries a lock mark the action's own icon does not, so
    // redrawing the icons has to redraw that too.
    refresh_tool_labels();
    // And Check model wears whichever of its two marks the panel calls for.
    refresh_check_action();
}

// A small padlock in the corner of an icon, for a button that has no name to
// hang the lock mark on.
QIcon with_lock_badge(const QIcon& base, const Theme& colors, int size) {
    QPixmap pixmap = base.pixmap(QSize(size, size) * 2);
    pixmap.setDevicePixelRatio(2);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal s = size;
    const QRectF body(s * 0.58, s * 0.66, s * 0.36, s * 0.28);
    const QRectF shackle(body.left() + body.width() * 0.2, body.top() - body.height() * 0.6,
                         body.width() * 0.6, body.height() * 0.9);
    painter.setPen(QPen(colors.accent, s * 0.08));
    painter.setBrush(Qt::NoBrush);
    painter.drawArc(shackle, 0, 180 * 16);
    painter.setPen(QPen(colors.selected_text, s * 0.04));
    painter.setBrush(colors.accent);
    painter.drawRoundedRect(body, s * 0.05, s * 0.05);
    return QIcon(pixmap);
}

void MainWindow::refresh_tool_labels() {
    const auto active = canvas_->tool();
    const auto locked = canvas_->tool_locked();
    if (auto* hand = findChild<QToolButton*>("canvasPan")) {
        const auto plain = glyph_icon(Glyph::Pan, theme(theme_), 18, icon_mode_);
        hand->setIcon(active == Tool::Pan && locked ? with_lock_badge(plain, theme(theme_), 18) : plain);
        hand->setToolTip(active == Tool::Pan && locked
            ? "Pan is locked. Drag as much as you like; choose another tool or press Escape to stop."
            : "Pan. Double-click to lock it for a longer look around.");
    }
    for (const auto& [tool, action] : tool_actions_) {
        // Generalization and specialization share one action, so only the mode
        // its button is actually set to should drive the label.
        if (action == isa_action_ && tool != isa_mode_) continue;
        const auto plain = action->data().toString();
        // A locked tool is marked on the button, since nothing else on screen
        // would explain why placing does not stop after the first element.
        action->setText(tool == active && locked ? plain + " 🔒" : plain);
        action->setToolTip(tool == active && locked
            ? plain + " is locked. Keep placing; choose another tool or press Escape to stop."
            : "Double-click to lock " + plain.toLower() + " for placing several.");
    }
}

QWidget* MainWindow::toolbar_widget(QAction* action) const {
    auto* toolbar = findChild<QToolBar*>("modelTools");
    return toolbar ? toolbar->widgetForAction(action) : nullptr;
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    // The controls float over the view rather than in a layout, so they are put
    // back in the corner whenever the view changes size under them.
    if (canvas_ && watched == canvas_
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show
            || event->type() == QEvent::LayoutRequest)) {
        place_canvas_controls();
        return false;
    }
    if (event->type() == QEvent::MouseButtonDblClick) {
        if (watched == static_cast<QObject*>(findChild<QToolButton*>("canvasPan"))) {
            choose_tool(Tool::Pan, true);
            return true;
        }
        if (watched == static_cast<QObject*>(findChild<QToolButton*>("isaButton"))) {
            choose_tool(isa_mode_, true);
            return true;
        }
        if (watched == static_cast<QObject*>(findChild<QToolButton*>("connectButton"))) {
            choose_tool(Tool::Connect, true);
            return true;
        }
        for (const auto& [tool, action] : tool_actions_)
            if (action != isa_action_ && watched == static_cast<QObject*>(toolbar_widget(action))) {
                choose_tool(tool, true);
                return true;
            }
        // A button on another row that carries one of the tool actions, as the
        // ribbon's Insert row does, locks the tool exactly as Home's does. ISA
        // is one action for two tools, so it locks the direction it is set to.
        if (auto* button = qobject_cast<QToolButton*>(watched); button && button->defaultAction())
            for (const auto& [tool, action] : tool_actions_)
                if (button->defaultAction() == action && (action != isa_action_ || tool == isa_mode_)) {
                    choose_tool(tool, true);
                    return true;
                }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::rename_selection() {
    finish_field_edit();
    if (selection_.size() != 1) return;
    const auto ref = selection_.front();
    bool accepted = false;
    const auto value = QInputDialog::getText(this, "Rename " + kind_label(ref).toLower(), "Name",
                                           QLineEdit::Normal, text(name(editor_.project(), ref)), &accepted);
    if (accepted) show_result(editor_.rename(ref, bytes(value)));
}

bool MainWindow::confirm_discard() {
    finish_field_edit();
    canvas_->cancel_interaction();
    if (!editor_.dirty()) return true;
    const auto answer = QMessageBox::warning(this, "Save your changes?",
        "The project has unsaved changes.", QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (answer == QMessageBox::Save) return save();
    return answer == QMessageBox::Discard;
}

bool MainWindow::save(bool choose_path) {
    finish_field_edit();
    canvas_->cancel_interaction();
    auto location = path_;
    if (choose_path || location.isEmpty()) {
        location = QFileDialog::getSaveFileName(this, "Save ERDFlow project", location.isEmpty() ? "Untitled.erdx" : location,
                                               "ERDFlow project (*.erdx)");
        if (location.isEmpty()) return false;
        if (!location.endsWith(".erdx", Qt::CaseInsensitive)) {
            location += ".erdx";
            if (QFileInfo::exists(location) && QMessageBox::question(this, "Replace existing project?",
                "A file already exists at " + location + ". Replace it?", QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No) != QMessageBox::Yes) return false;
        }
    }
    const auto result = application::save_project(editor_, store_, bytes(location));
    if (!result) {
        QMessageBox::warning(this, "Project could not be saved", text(result.error));
        return false;
    }
    path_ = location;
    remember_recent(path_);
    refresh();
    statusBar()->showMessage("Saved " + QFileInfo(path_).fileName(), 7000);
    return true;
}

void MainWindow::show_home(bool on) {
    if (!pages_) return;
    // The docks and the tool bar belong to the work, not to the home screen,
    // so they go away with it. Which ones come back is remembered rather than
    // assumed: a panel somebody had closed must not be reopened just because
    // they visited the home screen and left it again.
    if (on) {
        if (!home_chrome_hidden_) {
            home_chrome_hidden_ = true;
            for (auto* dock : findChildren<QDockWidget*>())
                // Asked as "not explicitly hidden" rather than "visible",
                // because at the moment the window is built nothing is
                // visible yet -- the window itself has not been shown -- and
                // a panel that was going to appear would otherwise be missed
                // and left standing over the home screen.
                if (!dock->isHidden()) { hidden_for_home_.push_back(dock); dock->hide(); }

            // The ribbon is the workspace's tool system, and Home has nothing
            // for it to act on, so its rows give way to Home's own slim bar.
            // The native menu bar and the status line stay: Home is never
            // left without its menus (ADR-022 section 9.14). Exactly the rows
            // that were out are remembered, so the one that was in front
            // comes back rather than an assumed Home row.
            for (auto* bar : findChildren<QToolBar*>())
                if (!bar->isHidden()) {
                    hidden_chrome_for_home_.push_back(bar);
                    bar->hide();
                }
        }
    } else {
        for (const auto& dock : hidden_for_home_) if (dock) dock->show();
        hidden_for_home_.clear();
        for (const auto& chrome : hidden_chrome_for_home_) if (chrome) chrome->show();
        hidden_chrome_for_home_.clear();
        home_chrome_hidden_ = false;
        workspace_seen_ = true;
    }
    // Home offers the way back into the workspace it was come to from, named
    // for whichever was in front: the schema if it had the whole window, the
    // conceptual diagram otherwise (Zain, 2026-09-27). Returning is leaving
    // Home, which puts everything back exactly as it was.
    if (on && home_)
        home_->top_bar()->set_return_to(!workspace_seen_ ? QString{}
                                        : schema_full_ ? QStringLiteral("Relational Design")
                                                       : QStringLiteral("Conceptual Design"));
    pages_->setCurrentIndex(on ? 0 : 1);
}

bool MainWindow::showing_home() const {
    return pages_ != nullptr && pages_->currentIndex() == 0;
}

bool MainWindow::begin_new_project() {
    if (!confirm_discard()) return false;
    editor_.new_project();
    path_.clear();
    refresh();
    canvas_->actual_size();
    canvas_->centerOn(0, 0);
    return true;
}

void MainWindow::new_project() {
    (void)begin_new_project();
}

namespace {
constexpr int recent_limit = 10;
const char* const recent_key = "recentProjects";
} // namespace

void MainWindow::remember_recent(const QString& path) {
    if (path.isEmpty()) return;
    const auto absolute = QFileInfo(path).absoluteFilePath();
    auto recent = QSettings().value(recent_key).toStringList();
    recent.removeAll(absolute);
    recent.prepend(absolute);
    while (recent.size() > recent_limit) recent.removeLast();
    QSettings().setValue(recent_key, recent);
}

void MainWindow::refresh_recent_menu() {
    if (!recent_menu_) return;
    recent_menu_->clear();
    const auto recent = QSettings().value(recent_key).toStringList();
    if (recent.isEmpty()) {
        // Said rather than left as an empty menu, which reads as broken.
        recent_menu_->addAction("No recent projects yet")->setEnabled(false);
        return;
    }
    for (const auto& path : recent) {
        const QFileInfo file(path);
        const bool there = file.exists();
        auto* entry = recent_menu_->addAction(there ? file.completeBaseName()
                                                    : file.completeBaseName() + " (moved or deleted)");
        entry->setToolTip(QDir::toNativeSeparators(path));
        entry->setStatusTip(QDir::toNativeSeparators(path));
        connect(entry, &QAction::triggered, this, [this, path, there] {
            if (there) { open_path(path); return; }
            // A project can be moved or deleted outside ERDFlow. Its entry says
            // so and offers to be forgotten, rather than failing to open (ADR-016).
            if (QMessageBox::question(this, "Project not found",
                    QDir::toNativeSeparators(path) + " is no longer there. Forget it?",
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes) == QMessageBox::Yes) {
                auto kept = QSettings().value(recent_key).toStringList();
                kept.removeAll(path);
                QSettings().setValue(recent_key, kept);
            }
        });
    }
    recent_menu_->addSeparator();
    recent_menu_->addAction("Clear recent projects", this, [] {
        QSettings().remove(recent_key);
    })->setObjectName("clearRecent");
}

void MainWindow::show_quick_guide() {
    QMessageBox::information(this, "Drawing a conceptual ERD",
        "1. Choose Entity, Attribute, or Relationship and click the canvas.\n"
        "2. Use Connect, then click the two objects to link them.\n"
        "3. Select an object to edit its Properties. Field edits apply on focus loss.\n"
        "4. Select a relationship to set each participant's cardinality, participation, and role.\n"
        "5. Save your work as an .erdx project.\n\n"
        "Drag to move; Shift-click for multiple selection. Scroll to zoom.\n"
        "Escape cancels the current gesture. Delete removes the canvas selection.\n"
        "Unfinished diagrams can be saved; Model checks explain missing information.");
}

void MainWindow::wire_home() {
    if (!home_) return;
    // Settings on the Home screen holds the choices that belong to the whole
    // application. They are the View menu's own submenus, added here as well
    // rather than copied, so the two can never disagree.
    settings_menu_ = new QMenu("Settings", this);
    settings_menu_->setObjectName("settingsMenu");
    for (const char* name : {"themeMenu", "iconMenu", "notationMenu"})
        if (auto* menu = findChild<QMenu*>(name)) settings_menu_->addMenu(menu);
    home_->top_bar()->attach_theme_menu(findChild<QMenu*>("themeMenu"));
    connect(home_->top_bar()->return_button(), &QPushButton::clicked, this, [this] { show_home(false); });

    auto* rail = home_->sidebar();
    // A menu opened from a row stands beside it, as a submenu would.
    const auto beside = [rail](HomeSection section, QMenu* menu) {
        return [rail, section, menu] {
            if (!menu) return;
            auto* row = rail->button(section);
            menu->popup(row->mapToGlobal(QPoint(row->width() + 6, 0)));
        };
    };
    rail->set_callback(HomeSection::OpenProject, [this] { open_dialog(); });
    rail->set_callback(HomeSection::Recent, beside(HomeSection::Recent, recent_menu_));
    rail->set_callback(HomeSection::Examples, [this] { load_example(); });
    // Templates are projects (ADR-016). The one there is is the general
    // starting frame, not the example (Zain, 2026-09-26), opened untitled and
    // unsaved, as a template should be.
    rail->set_callback(HomeSection::Templates, [this] { load_template(); });
    // Bringing in work that already exists. Today that is an ERDFlow project
    // or a picture carrying one; SQL and database sources join it when there
    // is a Relational Design to read them into.
    rail->set_callback(HomeSection::Import, [this] { open_dialog(); });
    rail->set_callback(HomeSection::Settings, beside(HomeSection::Settings, settings_menu_));
    rail->set_callback(HomeSection::Help, beside(HomeSection::Help, findChild<QMenu*>("helpMenu")));

    auto* learning = home_->learning();
    // There are no tutorials beyond the quick guide yet, so that is what this
    // opens rather than nothing.
    learning->set_callback(HomeLearningLink::ViewTutorials, [this] { show_quick_guide(); });
}

namespace {
// A spin box is a number field with a line edit inside it, and that line edit
// answers to everything a name field answers to. A picked character must never
// land in one: the panel's size, position and transparency fields take numbers,
// and a symbol typed into one would be silently thrown away. They are refused
// here rather than each of them being named, so a field added later is covered
// by being the kind of field it is.
[[nodiscard]] bool number_field(const QWidget* widget) {
    return qobject_cast<const QAbstractSpinBox*>(widget) != nullptr
        || qobject_cast<const QAbstractSpinBox*>(widget->parentWidget()) != nullptr;
}
} // namespace

void MainWindow::remember_text_target(QWidget* widget) {
    // Only somewhere a character could actually go, and never the picker's own
    // search box: searching for a symbol must not make the search box the
    // place the symbol lands.
    if (!widget) return;
    if (symbols_ && symbols_->isAncestorOf(widget)) return;
    if (!qobject_cast<QLineEdit*>(widget) && !qobject_cast<QPlainTextEdit*>(widget)) return;
    if (number_field(widget)) return;
    if (!isAncestorOf(widget)) return;
    text_target_ = widget;
    text_target_name_ = widget->objectName();
    refresh_symbol_destination();
}

void MainWindow::remember_caret(QWidget* widget) {
    if (!widget || widget != text_target_) return;
    if (auto* line = qobject_cast<QLineEdit*>(widget)) text_target_caret_ = line->cursorPosition();
    else if (auto* text = qobject_cast<QPlainTextEdit*>(widget)) text_target_caret_ = text->textCursor().position();
    // Which field that caret belongs to, so a rebuilt one can be told from the
    // one the writing actually stopped in.
    caret_owner_ = widget;
}

QWidget* MainWindow::text_target() {
    // Where the keyboard is, asked of this window rather than of the desktop,
    // so it is still the answer while the gallery is the window the desktop
    // calls active. A character belongs wherever the next typed letter would
    // go; if that is nowhere, it belongs on the diagram.
    auto* focused = focusWidget();
    if (!focused || !focused->isVisible()) return nullptr;
    if (symbols_ && symbols_->isAncestorOf(focused)) return nullptr;
    if (!qobject_cast<QLineEdit*>(focused) && !qobject_cast<QPlainTextEdit*>(focused)) return nullptr;
    if (number_field(focused)) return nullptr;
    // Committing a name rebuilds the properties panel and hands the keyboard
    // to the field that replaced the one being written in. The replacement
    // reads from its beginning, so the caret goes back to where the writing
    // stopped; otherwise a character would land in front of the name.
    // The field the caret was remembered in, not merely the last field focused:
    // committing an edit rebuilds the panel and destroys that field, and the
    // keyboard can reach its replacement before the character does. Asking
    // whether this is the same widget the caret came from answers that in both
    // orders, because a destroyed field leaves nothing to be the same as.
    if (focused != caret_owner_ && focused->objectName() == text_target_name_ && text_target_caret_ >= 0) {
        if (auto* line = qobject_cast<QLineEdit*>(focused))
            line->setCursorPosition(std::min(text_target_caret_, static_cast<int>(line->text().size())));
        else if (auto* text = qobject_cast<QPlainTextEdit*>(focused)) {
            auto cursor = text->textCursor();
            cursor.setPosition(std::min(text_target_caret_, static_cast<int>(text->toPlainText().size())));
            text->setTextCursor(cursor);
        }
    }
    text_target_ = focused;
    return focused;
}

bool MainWindow::place_symbol(const QString& character) {
    // Nothing is being written in, so the character goes on the diagram
    // itself, drawn bare: no card, no border, no title, the way an emoji sits
    // in a line of chat. It is a note underneath, so it is moved, coloured,
    // copied, deleted and undone like anything else on the diagram.
    // Square, because the character is drawn to fill the room it is given and
    // the characters worth placing are about as wide as they are tall. It is a
    // starting size rather than the size: a placed symbol is enlarged and
    // shrunk by its corners, by Edit's two commands, or in the panel.
    constexpr double width = symbol_body.width, height = symbol_body.height;
    // Put down where the user was working, which is where the pointer last
    // was over the diagram. The pointer is over the gallery at the moment a
    // character is picked, so its place on the canvas is the one remembered
    // from before that. With the pointer never yet over the canvas, the middle
    // of the view is the only sensible answer.
    auto centre = canvas_->pointer_place().value_or(canvas_->mapToScene(canvas_->viewport()->rect().center()));
    // Several characters picked one after another would otherwise land on the
    // same spot and hide one another, so each takes a step down and across
    // until it finds room, the way a duplicated element does.
    const auto& layout = editor_.project().layout;
    const auto taken = [&](const QPointF& at) {
        return std::any_of(layout.begin(), layout.end(), [&](const auto& entry) {
            return std::abs(entry.second.x - (at.x() - width / 2)) < 2
                && std::abs(entry.second.y - (at.y() - height / 2)) < 2;
        });
    };
    for (int step = 0; step < 64 && taken(centre); ++step) centre += QPointF(24, 24);
    const domain::Rect rect{centre.x() - width / 2, centre.y() - height / 2, width, height};
    // Left unchosen on purpose. Picking a character is putting one down, not
    // choosing something to work on, and choosing it would swap the properties
    // panel over to it every time one was placed.
    const auto result = editor_.create_symbol(bytes(character), rect);
    show_result(result, false);
    return bool(result);
}

void MainWindow::refresh_symbol_destination() {
    if (!symbols_) return;
    auto* target = text_target();
    if (!target) { symbols_->set_destination({}); return; }
    // The field's own label is what the user sees next to it, so that is what
    // the picker calls the destination rather than an internal name.
    QString field = target->accessibleName();
    if (field.isEmpty()) {
        if (target->objectName() == QStringLiteral("elementName")) field = QStringLiteral("Name");
        else if (target->objectName() == QStringLiteral("elementDescription")) field = QStringLiteral("Description");
        else if (target->objectName() == QStringLiteral("projectName")) field = QStringLiteral("Project name");
        else field = QStringLiteral("the field being written in");
    }
    symbols_->set_destination(field);
}

void MainWindow::show_symbols(const QString& group) {
    if (!symbols_) {
        symbols_ = new SymbolPicker(this);
        symbols_->on_chosen = [this](const QString& character) { insert_symbol(character); };
    }
    symbols_->set_theme(theme(theme_));
    if (!group.isEmpty()) symbols_->show_group(group);
    refresh_symbol_destination();
    symbols_->show();
    symbols_->raise();
}

bool MainWindow::insert_symbol(const QString& character) {
    auto* target = text_target();
    if (auto* line = qobject_cast<QLineEdit*>(target)) {
        line->insert(character);
        // The caret goes back where the character landed, so the next one is
        // typed or picked in the same place rather than at the start again.
        line->setFocus(Qt::OtherFocusReason);
        return true;
    }
    if (auto* text = qobject_cast<QPlainTextEdit*>(target)) {
        text->insertPlainText(character);
        text->setFocus(Qt::OtherFocusReason);
        return true;
    }
    const auto placed = place_symbol(character);
    if (placed)
        statusBar()->showMessage("Put on the diagram where the pointer last was. To put one in a name instead, "
                                 "click into the name first, then pick the character.", 7000);
    refresh_symbol_destination();
    return placed;
}

void MainWindow::insert_picture_dialog(std::optional<QPointF> at) {
    const auto location = QFileDialog::getOpenFileName(this, "Insert picture", {},
        "Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tif *.tiff);;All files (*)");
    if (!location.isEmpty()) insert_picture(location, at);
}

bool MainWindow::insert_picture(const QString& path, std::optional<QPointF> at) {
    finish_field_edit();
    QString why_not;
    const auto bytes_of_image = encoded_image(path, why_not);
    if (!bytes_of_image) {
        QMessageBox::warning(this, "Picture could not be inserted", why_not);
        return false;
    }
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const auto image = reader.read();
    // Placed where it was asked for, or else in the middle of what is on
    // screen, at a size that shows it without filling the view; a small image
    // keeps its own size.
    const auto centre = at.value_or(canvas_->mapToScene(canvas_->viewport()->rect().center()));
    auto size = QSizeF(image.size());
    if (size.width() > 320 || size.height() > 320) size = size.scaled(QSizeF(320, 320), Qt::KeepAspectRatio);
    size = size.expandedTo(QSizeF(24, 24));
    const domain::Rect rect{centre.x() - size.width() / 2, centre.y() - size.height() / 2, size.width(), size.height()};
    const auto result = editor_.create_picture(bytes(QFileInfo(path).completeBaseName()), rect, *bytes_of_image);
    show_result(result);
    if (result && result.created) canvas_->select_elements({*result.created}, true);
    return bool(result);
}

void MainWindow::refresh_check_action() {
    if (!check_ || !validation_dock_) return;
    const bool open = validation_dock_->isVisible();
    check_->setChecked(open);
    action_glyphs_[check_] = open ? Glyph::Dismiss : Glyph::Check;
    check_->setIcon(glyph_icon(action_glyphs_[check_], theme(theme_), icon_pixels(), icon_mode_));
    check_->setToolTip(open ? "Put the model checks away."
                            : "Look the model over and list what is missing.");
}

void MainWindow::refresh_background_menu() {
    const auto& paper = editor_.project().background;
    for (const auto& [style, action] : background_actions_) action->setChecked(style == paper.style);
    if (auto* strength = findChild<QSlider*>("backgroundStrength")) {
        const auto was = refreshing_;
        refreshing_ = true;
        strength->setValue(paper.strength);
        refreshing_ = was;
    }
    if (auto* row = findChild<QWidgetAction*>("backgroundStrengthRow"))
        row->setVisible(paper.style == domain::BackgroundStyle::Image);
}

void MainWindow::choose_background(domain::BackgroundStyle style) {
    auto paper = editor_.project().background;
    paper.style = style;
    show_result(editor_.set_background(std::move(paper)));
    refresh_background_menu();
}

void MainWindow::choose_background_image() {
    const auto location = QFileDialog::getOpenFileName(this, "Background picture", {},
        "Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp *.tif *.tiff);;All files (*)");
    if (location.isEmpty()) { refresh_background_menu(); return; }
    QString why_not;
    // A background is looked at rather than glanced at, so it is kept as large
    // as a project file will carry.
    const auto picture = encoded_image(location, why_not, 2560, domain::max_image_bytes);
    if (!picture) {
        QMessageBox::warning(this, "Background could not be set", why_not);
        refresh_background_menu();
        return;
    }
    auto paper = editor_.project().background;
    paper.style = domain::BackgroundStyle::Image;
    paper.image = *picture;
    show_result(editor_.set_background(std::move(paper)));
    refresh_background_menu();
}

void MainWindow::set_full_view(bool on) {
    if (on) {
        hidden_panels_.clear();
        for (auto* dock : findChildren<QDockWidget*>())
            if (dock->isVisible()) {
                hidden_panels_.push_back(dock);
                dock->hide();
            }
    } else {
        for (auto* dock : hidden_panels_) dock->show();
        hidden_panels_.clear();
    }
    full_view_->setToolTip(on ? "Full view. Press again to bring the panels back."
                              : "Full view — put the panels away and give the whole window to the diagram.");
    statusBar()->showMessage(on ? "Full view. Press it again to bring the panels back." : "Panels restored.", 5000);
    place_canvas_controls();
}

QByteArray MainWindow::project_payload(QString& note) {
    const auto encoded = store_.project_bytes(editor_.project());
    if (encoded) return QByteArray(encoded.bytes.data(), static_cast<qsizetype>(encoded.bytes.size()));
    // Too large to travel inside a picture. The picture is still worth having,
    // so it is written without the project and the reason is said plainly,
    // rather than the export failing over something the picture does not need.
    note = "The project was too large to travel inside it: " + text(encoded.error);
    return {};
}

application::LoadResult MainWindow::read_project(const QString& path) {
    // A picture ERDFlow wrote is a project as much as a .erdx is, so the two
    // are opened by the same command and differ only in where the bytes were
    // found. A picture from anywhere else is an ordinary picture, and saying
    // so is more useful than reporting it as a damaged project.
    if (!may_carry_project(path)) return store_.load(bytes(path));
    const auto payload = payload_of_picture_file(path);
    if (payload.isEmpty())
        return {{}, "This picture does not carry an ERDFlow project inside it. "
                    "Use Insert → Picture to place it on the diagram instead."};
    return store_.project_from_bytes(std::string(payload.constData(), static_cast<std::size_t>(payload.size())));
}

bool MainWindow::open_dialog() {
    const auto location = QFileDialog::getOpenFileName(this, "Open ERDFlow project", path_,
        "ERDFlow project or picture (*.erdx *.svg *.png);;ERDFlow project (*.erdx);;"
        "Picture carrying a project (*.svg *.png)");
    return !location.isEmpty() && open_path(location);
}

bool MainWindow::open_path(const QString& path) {
    // Validate the complete candidate before asking to replace the open work.
    auto candidate = read_project(path);
    if (!candidate) {
        QMessageBox::warning(this, "Project could not be opened", text(candidate.error));
        return false;
    }
    if (!confirm_discard()) return false;
    // Save in the discard prompt may have updated this very file. Re-read it
    // before installing so the pre-prompt candidate cannot restore old data.
    candidate = read_project(path);
    if (!candidate) {
        QMessageBox::warning(this, "Project could not be opened", text(candidate.error));
        return false;
    }
    const auto result = editor_.replace_project(std::move(*candidate.project));
    if (!result) { show_result(result); return false; }
    // A project is open, so the home screen has done its job.
    show_home(false);
    // A project opened out of a picture has no project file of its own yet.
    // Leaving the picture as the save location would overwrite it with project
    // bytes and destroy the picture, so the next save asks where it should go.
    path_ = may_carry_project(path) ? QString() : path;
    remember_recent(path_);
    refresh();
    canvas_->fit_diagram();
    if (path_.isEmpty())
        statusBar()->showMessage("Opened the project carried inside " + QFileInfo(path).fileName()
                                 + ". Save it to give it a project file of its own.", 9000);
    return true;
}

void MainWindow::export_dialog() {
    finish_field_edit();
    canvas_->cancel_interaction();
    ExportDialog dialog(*canvas_, editor_.project(), this);
    dialog.set_choice(export_choice_);
    if (dialog.exec() != QDialog::Accepted) return;
    export_choice_ = dialog.choice();
    if (export_choice_.document) export_document(export_choice_.as_document);
    else export_picture(export_choice_.as_picture);
}

// Where an exported file is suggested to go: named after the project, beside
// it when it has a file of its own, so what leaves lands where the work lives.
QString MainWindow::export_location(const QString& suffix, const QString& label) {
    const auto stem = path_.isEmpty() ? QString("Untitled") : QFileInfo(path_).completeBaseName();
    const auto suggested = path_.isEmpty() ? stem + "." + suffix
                                           : QFileInfo(path_).dir().filePath(stem + "." + suffix);
    auto location = QFileDialog::getSaveFileName(this, "Export", suggested,
                                                 QString("%1 (*.%2)").arg(label, suffix));
    if (location.isEmpty()) return {};
    if (!location.endsWith("." + suffix, Qt::CaseInsensitive)) {
        location += "." + suffix;
        if (QFileInfo::exists(location) && QMessageBox::question(this, "Replace existing file?",
            "A file already exists at " + location + ". Replace it?", QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No) != QMessageBox::Yes) return {};
    }
    return location;
}

bool MainWindow::export_document(DocumentFormat format, const QString& location_given) {
    finish_field_edit();
    canvas_->cancel_interaction();
    const auto& info = document_format(format);
    auto location = location_given;
    if (location.isEmpty())
        location = export_location(QString::fromLatin1(info.suffix), QString::fromUtf8(info.label));
    if (location.isEmpty()) return false;
    const auto result = write_document(*canvas_, editor_.project(), format, location);
    if (!result) {
        QMessageBox::warning(this, "Document could not be written", result.error);
        return false;
    }
    // A listing is not the project, and someone who exports one and expects
    // to reopen it should be told so once rather than discover it later.
    statusBar()->showMessage("Exported " + QFileInfo(location).fileName()
                             + ". It is a listing of the model, not the project itself.", 9000);
    return true;
}

bool MainWindow::export_picture(const PictureOptions& options, const QString& location_given) {
    finish_field_edit();
    canvas_->cancel_interaction();
    const auto& info = picture_format(options.format);
    const auto suffix = QString::fromLatin1(info.suffix);
    auto location = location_given;
    if (location.isEmpty()) location = export_location(suffix, QString::fromUtf8(info.label));
    if (location.isEmpty()) return false;
    QString note;
    const auto payload = options.carry_project && info.carries_project ? project_payload(note) : QByteArray();
    const auto result = write_picture(*canvas_, options, payload, location);
    if (!result) {
        QMessageBox::warning(this, "Picture could not be written", result.error);
        return false;
    }
    // What was written, and where the project ended up, since a recipient who
    // expects to reopen the picture needs to know whether it can be.
    auto said = "Exported " + QFileInfo(location).fileName();
    if (result.carried_project) said += ", with the project inside it";
    said += ".";
    if (!note.isEmpty()) said += " " + note;
    else if (!result.carried_note.isEmpty() && options.carry_project && info.carries_project)
        said += " " + result.carried_note;
    statusBar()->showMessage(said, 9000);
    return true;
}

bool MainWindow::copy_picture() {
    finish_field_edit();
    canvas_->cancel_interaction();
    // A copy is of what is selected, and of the whole diagram when nothing is,
    // which is what every drawing application does with the same command.
    auto options = export_choice_.as_picture;
    options.extent = canvas_->selection_bounds().isEmpty() ? PictureExtent::WholeDiagram : PictureExtent::Selection;
    QString note;
    const auto payload = options.carry_project ? project_payload(note) : QByteArray();

    options.format = PictureFormat::Png;
    QByteArray png;
    const auto raster = draw_picture(*canvas_, options, payload, png);
    if (!raster) {
        statusBar()->showMessage(raster.error, 9000);
        return false;
    }
    options.format = PictureFormat::Svg;
    QByteArray svg;
    const auto vector = draw_picture(*canvas_, options, payload, svg);

    // Both pictures go on at once and the destination takes whichever it
    // prefers: a word processor usually takes the vector, a chat window the
    // raster, and neither has to be chosen in advance.
    auto* data = new QMimeData;
    QImage image;
    if (image.loadFromData(png, "png")) data->setImageData(image);
    data->setData("image/png", png);
    if (vector) data->setData("image/svg+xml", svg);
    QApplication::clipboard()->setMimeData(data);
    statusBar()->showMessage(options.extent == PictureExtent::Selection
                                 ? "Copied the selection as a picture."
                                 : "Copied the diagram as a picture.", 7000);
    return true;
}

QString MainWindow::ask_for_comment(const QString& said, const QString& about) {
    // A remark is prose, often a sentence or two, so it is written in a box
    // that takes several lines rather than in a single-line field.
    QDialog dialog(this);
    dialog.setObjectName("commentDialog");
    dialog.setWindowTitle(said.isEmpty() ? "Add comment" : "Edit comment");
    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(8);
    auto* about_label = new QLabel(about, &dialog);
    about_label->setObjectName("commentAbout");
    about_label->setWordWrap(true);
    layout->addWidget(about_label);
    auto* editor = new QPlainTextEdit(said, &dialog);
    editor->setObjectName("commentText");
    editor->setPlaceholderText("What should whoever reads this diagram know?");
    editor->setMinimumSize(360, 120);
    layout->addWidget(editor);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setObjectName("commentAccept");
    buttons->button(QDialogButtonBox::Cancel)->setObjectName("commentCancel");
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    editor->setFocus();
    if (dialog.exec() != QDialog::Accepted) return {};
    return editor->toPlainText().trimmed();
}

namespace {
// What a remark is being left on, said plainly, so the box asking for the words
// says what they will be pinned to.
QString about_targets(const domain::Project& project, const std::vector<domain::CommentTarget>& targets) {
    if (targets.size() > 1) return QString("On %1 things at once.").arg(targets.size());
    if (targets.empty()) return {};
    if (const auto* element = std::get_if<domain::ElementRef>(&targets.front()))
        return "On " + kind_label(project, *element).toLower() + " " + display_name(project, *element) + ".";
    if (std::holds_alternative<domain::ConnectorRef>(targets.front())) return "On this line.";
    const auto& anchor = std::get<domain::TextAnchor>(targets.front());
    return QString("On the %1 of %2.")
        .arg(anchor.field == domain::TextField::Name ? "name" : "description",
             display_name(project, anchor.owner));
}
} // namespace

bool MainWindow::add_comment(std::vector<domain::CommentTarget> targets, const QString& said) {
    if (targets.empty()) return false;
    finish_field_edit();
    auto words = said;
    if (words.isEmpty()) words = ask_for_comment({}, about_targets(editor_.project(), targets));
    if (words.isEmpty()) return false;
    const auto result = editor_.create_comment(bytes(words), std::move(targets));
    show_result(result, false);
    return bool(result);
}

void MainWindow::offer_text_comment(QWidget* field) {
    field->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(field, &QWidget::customContextMenuRequested, this, [this, field](const QPoint& at) {
        // The standard entries stay: taking cut, copy and paste away from a
        // text field to add one entry of our own would be a poor trade.
        auto* line = qobject_cast<QLineEdit*>(field);
        auto* block = qobject_cast<QPlainTextEdit*>(field);
        std::unique_ptr<QMenu> menu(line ? line->createStandardContextMenu()
                                         : block ? block->createStandardContextMenu() : nullptr);
        if (!menu) return;
        const bool chosen = line ? line->hasSelectedText() : block->textCursor().hasSelection();
        menu->addSeparator();
        auto* comment = menu->addAction("Comment on selection…");
        comment->setObjectName("fieldComment");
        comment->setEnabled(chosen);
        comment->setToolTip(chosen ? "Pin a remark to the words you have chosen."
                                   : "Choose some words first, and the remark is pinned to those.");
        const auto name = field->objectName();
        connect(comment, &QAction::triggered, this, [this, name] { comment_on_selected_text(name); });
        menu->exec(field->mapToGlobal(at));
    });
}

namespace {
// How many characters stand before a position that Qt counts in UTF-16 code
// units. A comment's range is counted in characters so that it means the same
// thing in the file, in the domain and in the panel, none of which index text
// the way Qt does.
std::uint32_t characters_before(const QString& text, int units) {
    const auto clamped = std::clamp(units, 0, static_cast<int>(text.size()));
    return static_cast<std::uint32_t>(text.left(clamped).toUcs4().size());
}
} // namespace

bool MainWindow::comment_on_selected_text(const QString& field_name, const QString& said) {
    if (selection_.size() != 1) return false;
    const auto ref = selection_.front();
    domain::TextAnchor anchor;
    anchor.owner = ref;
    QString whole;
    int begin_units = 0;
    int end_units = 0;
    if (auto* line = properties_->findChild<QLineEdit*>(field_name)) {
        if (!line->hasSelectedText()) return false;
        anchor.field = domain::TextField::Name;
        whole = line->text();
        begin_units = line->selectionStart();
        end_units = begin_units + static_cast<int>(line->selectedText().size());
    } else if (auto* block = properties_->findChild<QPlainTextEdit*>(field_name)) {
        const auto cursor = block->textCursor();
        if (!cursor.hasSelection()) return false;
        anchor.field = domain::TextField::Description;
        whole = block->toPlainText();
        begin_units = cursor.selectionStart();
        end_units = cursor.selectionEnd();
    } else {
        return false;
    }
    // The field may hold text that has not been committed yet. A remark is
    // pinned into what the model actually holds, so the writing is committed
    // first and the range is measured against the result.
    finish_field_edit();
    const auto stored = text(anchor.field == domain::TextField::Name ? name(editor_.project(), ref)
                                                                     : description(editor_.project(), ref));
    if (stored != whole) whole = stored;
    anchor.begin = characters_before(whole, begin_units);
    const auto end = characters_before(whole, end_units);
    if (end <= anchor.begin) return false;
    anchor.length = end - anchor.begin;
    return add_comment({domain::CommentTarget{anchor}}, said);
}

void MainWindow::search_diagram(const DiagramSearch& search) {
    // Nothing is committed here on purpose. A search changes nothing in the
    // model, so it has no reason to end an edit anybody has in progress.
    canvas_->set_search(search);
    const auto found = canvas_->found_elements();
    if (search_bar_) search_bar_->report(static_cast<int>(found.size()));
    // What was asked for is brought to the reader rather than left for them to
    // go looking for, which is the whole point of having asked.
    if (!found.empty()) canvas_->frame_found();
    statusBar()->showMessage(!search.looking() ? QString()
                             : found.empty() ? QString("Nothing on the diagram matches.")
                             : QString("%1 of the diagram shown.").arg(found.size() == 1
                                   ? QString("1 element") : QString("%1 elements").arg(found.size())), 6000);
}

void MainWindow::open_search(const QString& looking_for) {
    if (!search_bar_) return;
    if (looking_for.isEmpty()) search_bar_->open();
    else search_bar_->look_for(looking_for);
    search_diagram(search_bar_->search());
}

void MainWindow::close_search() {
    if (!search_bar_) return;
    search_bar_->hide();
    // Closing puts the whole diagram back: a filter left on behind a closed bar
    // would be a diagram missing pieces for no visible reason.
    canvas_->set_search({});
    canvas_->setFocus();
}

bool MainWindow::export_project_file(const QString& location_given) {
    finish_field_edit();
    canvas_->cancel_interaction();
    auto location = location_given;
    if (location.isEmpty()) location = export_location(QStringLiteral("erdx"), QStringLiteral("ERDFlow project"));
    if (location.isEmpty()) return false;
    // Written through the store directly rather than through the save use case:
    // this is a copy put somewhere, so the project being worked on keeps its own
    // file and its own unsaved state.
    const auto result = store_.save(bytes(location), editor_.project());
    if (!result) {
        QMessageBox::warning(this, "Project could not be written", text(result.error));
        return false;
    }
    statusBar()->showMessage("Exported a copy to " + QFileInfo(location).fileName()
                             + ". You are still working on this one.", 9000);
    return true;
}

void MainWindow::import_dialog(bool pictures) {
    const auto location = QFileDialog::getOpenFileName(this, pictures ? "Import from a picture" : "Import a project",
        path_, pictures ? "Picture carrying a project (*.svg *.png)" : "ERDFlow project (*.erdx)");
    if (!location.isEmpty()) import_project(location);
}

bool MainWindow::import_project(const QString& path) {
    finish_field_edit();
    canvas_->cancel_interaction();
    auto incoming = read_project(path);
    if (!incoming) {
        QMessageBox::warning(this, "Nothing could be imported", text(incoming.error));
        return false;
    }
    // Put down to the right of everything already drawn, with a gap, so an
    // import never lands on top of the work it is joining. The incoming
    // project has coordinates of its own, so the gap is measured from its own
    // left edge rather than from nothing.
    double offset_x = 0;
    const auto drawn = canvas_->diagram_bounds();
    if (!drawn.isEmpty() && !incoming.project->layout.empty()) {
        auto leftmost = incoming.project->layout.begin()->second.x;
        for (const auto& [ref, rect] : incoming.project->layout) {
            (void)ref;
            leftmost = std::min(leftmost, rect.x);
        }
        offset_x = drawn.right() + 140 - leftmost;
    }
    const auto before = editor_.project();
    const auto result = editor_.merge_project(*incoming.project, offset_x, 0);
    if (!result) { show_result(result); return false; }
    refresh();
    // What arrived is chosen and brought into view, so the reader can see what
    // they just imported rather than having to go looking for it.
    std::vector<domain::ElementRef> arrived;
    for (const auto& [ref, rect] : editor_.project().layout) {
        (void)rect;
        if (!before.layout.contains(ref)) arrived.push_back(ref);
    }
    canvas_->select_elements(arrived, true);
    statusBar()->showMessage(QString("Imported %1 from %2. One undo takes it all back out again.")
                                 .arg(arrived.size() == 1 ? QString("1 element")
                                                          : QString("%1 elements").arg(arrived.size()),
                                      QFileInfo(path).fileName()), 9000);
    return true;
}

// The panel takes the lower part of the stage and slides up into it. It is a
// child of the stage rather than a row in the layout, so the diagram keeps its
// full height behind it and nothing is re-laid-out as it moves.
void MainWindow::lay_out_schema() {
    if (!schema_panel_ || !canvas_ || laying_out_schema_) return;
    const QSignalBlocker quiet(schema_panel_);
    laying_out_schema_ = true;
    const auto done = qScopeGuard([this] { laying_out_schema_ = false; });
    auto* stage = schema_panel_->parentWidget();
    if (!stage) return;
    const auto height = std::max(120, static_cast<int>(stage->height() * schema_share_));
    const auto top = schema_open_ ? stage->height() - height : stage->height();
    schema_panel_->setGeometry(0, top, stage->width(), height);
}

void MainWindow::open_schema(bool full) {
    show_schema(true);
    // The panel rises over 280ms and is given a geometry on every frame of it.
    // Asking for full height in the middle of that would be overwritten by the
    // next frame, so it waits for the rise to land -- which is also what a
    // person does, since they cannot click Full before the panel is there.
    if (full) QTimer::singleShot(320, this, [this] { set_schema_full(true); });
}

void MainWindow::show_schema(bool shown) {
    if (!schema_panel_ || schema_open_ == shown) return;
    // A panel that is closing takes its full view with it, rather than leaving
    // the window stripped with nothing in it.
    if (!shown && schema_full_) set_schema_full(false);
    schema_open_ = shown;
    auto* stage = schema_panel_->parentWidget();
    const auto height = std::max(120, static_cast<int>(stage->height() * schema_share_));
    if (shown) {
        schema_->set_theme(theme(theme_));
        schema_->set_notation(canvas_->notation());
        refresh_schema();
        // Start it off the bottom edge at the size it will be, so the rise has
        // somewhere to rise from: a panel that has never been laid out has no
        // size at all, and animating from that moves nothing anywhere.
        schema_panel_->setGeometry(0, stage->height(), stage->width(), height);
        schema_panel_->show();
        schema_panel_->raise();
    }
    auto* rise = new QVariantAnimation(this);
    rise->setDuration(280);
    rise->setEasingCurve(QEasingCurve::OutCubic);
    rise->setStartValue(schema_panel_->y());
    rise->setEndValue(shown ? stage->height() - height : stage->height());
    connect(rise, &QVariantAnimation::valueChanged, this, [this, stage](const QVariant& at) {
        // The height is read again each frame rather than captured, so a stage
        // that changes size mid-rise is followed instead of ignored.
        const auto tall = std::max(120, static_cast<int>(stage->height() * schema_share_));
        schema_panel_->setGeometry(0, at.toInt(), stage->width(), tall);
    });
    connect(rise, &QVariantAnimation::finished, this, [this] {
        if (!schema_open_) schema_panel_->hide();
    });
    rise->start(QAbstractAnimation::DeleteWhenStopped);
    if (auto* button = findChild<QPushButton*>("previewSchema")) button->setChecked(shown);
}

// Nothing but the schema. The panel takes the whole stage and the surrounding
// panels go away, so a schema being read is not read around the edges of
// everything else. Leaving it puts back exactly what was put away -- and only
// what this put away, so a panel the user had already closed stays closed.
// The window's furniture for drawing: the ribbon of tabs and every tool row
// under it. Gathered in one place because full view and the schema both want
// to put the same things away.
std::vector<QWidget*> MainWindow::chrome_for_drawing() const {
    // The row of tabs and every row under it are toolbars, so one sweep finds
    // all of them. The Ribbon itself is the thing that arranges them and has
    // no surface of its own to hide.
    std::vector<QWidget*> furniture;
    for (auto* row : findChildren<QToolBar*>()) furniture.push_back(static_cast<QWidget*>(row));
    return furniture;
}

void MainWindow::set_workspace_in_front(bool relational) {
    if (auto* badge = findChild<QLabel*>("workspaceBadge"))
        badge->setText(relational ? "RELATIONAL DESIGN" : "CONCEPTUAL");
    if (auto* picture = findChild<QAction*>("insertPicture")) picture->setVisible(!relational);
}

void MainWindow::set_schema_full(bool full) {
    if (schema_full_ == full) return;
    schema_full_ = full;
    if (full) {
        schema_share_before_full_ = schema_share_;
        // Full view is the window's own idea of putting the panels away, so it
        // is borrowed rather than reimplemented. If it was already on, it is
        // left exactly as it was on the way out.
        schema_took_full_view_ = hidden_panels_.empty();
        if (schema_took_full_view_) set_full_view(true);
        // The diagram's own furniture goes too. A schema given the whole window
        // should not be read around a search bar and a line of instructions
        // about drawing shapes on a canvas nobody can currently see.
        hidden_for_schema_.clear();
        // The diagram's own search and the button that replaces the whole
        // document go too: neither is about the schema, and the schema has a
        // search of its own in the header.
        for (auto* also : {static_cast<QWidget*>(search_bar_),
                           static_cast<QWidget*>(findChild<QLabel*>("canvasInstructions")),
                           static_cast<QWidget*>(search_button_),
                           static_cast<QWidget*>(findChild<QPushButton*>("openExample"))})
            if (also && also->isVisible()) {
                hidden_for_schema_.push_back(also);
                also->hide();
            }
        // And the tools for drawing go with the canvas they draw on. A row of
        // shapes to place, above a diagram nobody can currently see, is a row
        // of things that cannot be done -- so the ribbon and the tool rows are
        // put away and the few things still worth reaching for come out in the
        // header instead.
        hidden_chrome_.clear();
        for (auto* furniture : chrome_for_drawing())
            if (furniture && furniture->isVisible()) {
                hidden_chrome_.push_back(furniture);
                furniture->hide();
            }
        if (schema_header_tools_) {
            if (schema_theme_ && theme_button_) {
                schema_theme_->setMenu(theme_button_->menu());
                schema_theme_->setIcon(theme_button_->icon());
            }
            schema_header_tools_->show();
        }
        // Relational Design is now the workspace in front, so the header says
        // so, and what only the diagram can take is put away with the diagram:
        // a picture is placed on the canvas, which cannot be seen. The ribbon's
        // drawing tools have already gone with the rest of its rows (ADR-022
        // section 9.12: Relational Design offers no conceptual-only tools).
        set_workspace_in_front(true);
        schema_share_ = 1.0;
    } else {
        if (schema_took_full_view_) set_full_view(false);
        schema_took_full_view_ = false;
        for (const auto& also : hidden_for_schema_) if (also) also->show();
        hidden_for_schema_.clear();
        for (const auto& furniture : hidden_chrome_) if (furniture) furniture->show();
        hidden_chrome_.clear();
        if (schema_header_tools_) schema_header_tools_->hide();
        set_workspace_in_front(false);
        schema_share_ = schema_share_before_full_;
    }
    lay_out_schema();
    QTimer::singleShot(0, this, [this] { lay_out_schema(); });
    if (auto* button = findChild<QPushButton*>("schemaFull")) {
        const QSignalBlocker quiet(button);
        button->setChecked(full);
        button->setText(full ? "Exit full" : "Full");
    }
    statusBar()->showMessage(full ? "Relational Design has the whole window. Press Full again to bring the rest back."
                                  : "Everything is back.", 5000);
}

// What can be done to the table or column that was asked about.
//
// The offer is narrow on purpose. A column the conversion invented -- a
// bridge's own key, a surrogate, a discriminator -- is there because the shape
// of the model put it there, and taking it away would be arguing with the
// conversion rather than editing the model. A foreign key is there because a
// relationship is, and it goes when that relationship does.
std::optional<domain::ParticipantId> MainWindow::carrying_side(
        const domain::PreviewColumn& column) const {
    if (!column.link || !std::holds_alternative<domain::ParticipantId>(*column.link))
        return std::nullopt;
    const auto target = std::get<domain::ParticipantId>(*column.link);
    for (const auto& [id, relationship] : editor_.project().relationships) {
        (void)id;
        const auto& sides = relationship.participants;
        if (std::none_of(sides.begin(), sides.end(),
                         [&](const auto& one) { return one.id == target; }))
            continue;
        // Only a two-sided relationship has an other side. A bridge holds
        // several keys, and no one of them is what would make it unique.
        if (sides.size() != 2) return std::nullopt;
        for (const auto& one : sides) if (one.id != target) return one.id;
    }
    return std::nullopt;
}

// The list of what a column can be made to enforce, opened under the cell
// that says what it enforces now.
//
// A list rather than three switches because several of them apply at once and
// a few of them cannot: a key is unique already, a key can never be empty, a
// counted-up column has to hold whole numbers. Nothing here is greyed out --
// every entry can be chosen, and one that cannot take says why in the status
// bar, which tells a reader learning the rules something that a dead menu
// entry does not.
void MainWindow::offer_schema_rules(const SchemaView::Constrained& hit, QPoint at) {
    if (!schema_ || hit.table >= schema_->preview().tables.size()) return;
    const auto& table = schema_->preview().tables[hit.table];
    if (hit.row >= table.columns.size()) return;
    const auto& column = table.columns[hit.row];

    QMenu menu(this);
    menu.setObjectName("schemaRulesMenu");
    const auto entry = [&](const QString& label, const char* name, bool on,
                           SchemaView::Constraint which, const QString& why) {
        auto* action = menu.addAction(label);
        action->setObjectName(name);
        action->setCheckable(true);
        action->setChecked(on);
        if (!why.isEmpty()) action->setToolTip(why);
        connect(action, &QAction::triggered, this,
                [this, hit, which] { toggle_schema_constraint(hit, which); });
    };
    // Nullability is the one that always says which way it went, so it reads
    // as the state it is in rather than as a box that happens to be empty.
    entry(column.required ? "NOT NULL" : "NULL — may be empty", "schemaRuleNotNull",
          column.required, SchemaView::Constraint::Nullability,
          column.primary_key ? "A primary key can never be empty."
              : column.foreign_key ? "This says whether the side it points at is total."
                                   : QString());
    entry("UNIQUE", "schemaRuleUnique", column.unique, SchemaView::Constraint::Unique,
          column.primary_key ? "A primary key is unique already."
              : column.foreign_key ? "This says whether the side carrying it sees one row."
                                   : QString());
    entry("IDENTITY", "schemaRuleIdentity", column.auto_increment,
          SchemaView::Constraint::AutoIncrement,
          "The database fills this in, counting up. Whole numbers only.");
    menu.exec(at);
}

// A constraint mark was pressed on the schema.
//
// Nothing here is refused by being made unpressable. Where a mark cannot
// change, it is pressed like any other and says what would have to happen
// first, in the status bar rather than in a box that has to be dismissed: a
// reader learning why a key cannot be empty is better served by being told
// than by finding the mark dead under the pointer.
void MainWindow::toggle_schema_constraint(const SchemaView::Constrained& hit,
                                          SchemaView::Constraint which) {
    if (!schema_ || hit.table >= schema_->preview().tables.size()) return;
    const auto& table = schema_->preview().tables[hit.table];
    if (hit.row >= table.columns.size()) return;
    const auto& column = table.columns[hit.row];
    const auto say = [this](const QString& why) { statusBar()->showMessage(why, 12000); };
    // A constraint that would not take reports in the status bar, like one
    // that cannot change at all. A box that has to be dismissed is too much
    // ceremony for a mark the hand is still resting on, and it would stop the
    // next press dead.
    const auto apply = [this, &say](const application::EditResult& result) {
        refresh();
        if (!result) say(text(result.error));
    };

    // What the column's rules are now, read from whatever holds them rather
    // than from the preview: the preview reports a key as required whether or
    // not the attribute says so, and writing that back would make the derived
    // value permanent.
    const auto* attribute = column.origin && editor_.project().attributes.contains(*column.origin)
        ? &editor_.project().attributes.at(*column.origin) : nullptr;
    const domain::SchemaColumn* added = nullptr;
    if (column.added)
        for (const auto& [where, columns] : editor_.project().schema.added) {
            (void)where;
            for (const auto& one : columns) if (one.id == *column.added) added = &one;
        }

    switch (which) {
    case SchemaView::Constraint::Nullability: {
        // A key is what identifies a row, so it can never be empty.
        if (column.primary_key) {
            say("A primary key can never be empty. Take the key off it first.");
            return;
        }
        // A foreign key is NOT NULL because the side it points at is total.
        // That is a fact about the relationship, so pressing this reaches the
        // diagram and changes the participation there.
        if (column.link && std::holds_alternative<domain::ParticipantId>(*column.link)) {
            apply(editor_.set_participation(
                std::get<domain::ParticipantId>(*column.link),
                column.required ? domain::Participation::Partial : domain::Participation::Total));
            return;
        }
        if (attribute) {
            apply(editor_.set_attribute_rules(*column.origin, attribute->identifier,
                                                    !attribute->required, attribute->unique));
            return;
        }
        if (added) {
            apply(editor_.set_schema_column_rules(*column.added, added->identifier,
                                                        !added->required, added->unique));
            return;
        }
        say(column.foreign_key
                ? "This key points home and must always be filled in."
                : "The conversion made this column, so there is nothing behind it to change.");
        return;
    }
    case SchemaView::Constraint::Unique: {
        if (attribute) {
            apply(editor_.set_attribute_rules(*column.origin, attribute->identifier,
                                                    attribute->required, !attribute->unique));
            return;
        }
        if (added) {
            apply(editor_.set_schema_column_rules(*column.added, added->identifier,
                                                        added->required, !added->unique));
            return;
        }
        // A key is unique by being the key. Saying so again on the same
        // column would be a constraint the database already enforces.
        if (column.primary_key) {
            say("A primary key is unique already. Nothing more to say about it.");
            return;
        }
        // A unique foreign key means the side carrying it sees one row and no
        // more, which is that side's cardinality -- so this reaches the
        // diagram, as its nullability does.
        if (const auto side = carrying_side(column)) {
            apply(editor_.set_cardinality(*side, column.unique ? domain::Cardinality::Many
                                                               : domain::Cardinality::One));
            return;
        }
        say("This key points home and is unique or not according to what it points at.");
        return;
    }
    case SchemaView::Constraint::AutoIncrement: {
        // The one constraint with nothing on the diagram behind it. A Chen ERD
        // has no way of saying a value is generated rather than recorded, so
        // this reaches the table and stops there.
        if (attribute) {
            apply(editor_.set_auto_increment(*column.origin, !attribute->auto_increment));
            return;
        }
        if (added) {
            apply(editor_.set_schema_column_auto_increment(*column.added,
                                                                 !added->auto_increment));
            return;
        }
        // A key the conversion invented is the commonest place of all to want
        // this: a table with nothing to identify it is given a surrogate, and
        // a surrogate is what a counted-up column is for. It has nothing
        // behind it, so it is remembered against the table it belongs to.
        if (column.origin_kind == domain::ColumnOrigin::Generated && table.origin) {
            apply(editor_.set_key_auto_increment(*table.origin, !column.auto_increment));
            return;
        }
        say(column.foreign_key
                ? "A foreign key takes its value from the key it points at."
                : "The conversion made this column, so there is nothing behind it to change.");
        return;
    }
    }
}

void MainWindow::offer_schema_actions(const SchemaView::Spot& spot) {
    if (!schema_ || spot.table >= schema_->preview().tables.size()) return;
    const auto& table = schema_->preview().tables[spot.table];
    if (!table.origin) return;
    QMenu menu(this);
    menu.setObjectName("schemaMenu");
    auto* add = menu.addAction("Add column");
    add->setObjectName("schemaAddColumn");
    connect(add, &QAction::triggered, this, [this, origin = *table.origin] { add_schema_column(origin); });
    // The same thing, kept off the diagram. It is the departure rather than the
    // ordinary case, so it is asked for by name here and is not what the slot
    // under the table does.
    auto* aside = menu.addAction("Add column in Relational Design only");
    aside->setObjectName("schemaAddColumnOnly");
    connect(aside, &QAction::triggered, this,
            [this, origin = *table.origin] { add_schema_column(origin, true); });
    if (spot.column && *spot.column < table.columns.size()) {
        const auto& column = table.columns[*spot.column];
        // The primary key, put on a column or taken off it from here. It is
        // one command because it is one thought, and it reaches the diagram:
        // the attribute behind the column becomes a key attribute and is
        // drawn as one. Offered only where there is an attribute behind the
        // column -- a key the conversion invented is the table's key already,
        // and a foreign key is a key somewhere else.
        if (column.origin) {
            auto* key = menu.addAction(column.primary_key
                ? QString("Take the key off \"%1\"").arg(text(column.name))
                : QString("Make \"%1\" the primary key").arg(text(column.name)));
            key->setObjectName("schemaPrimaryKey");
            key->setToolTip(column.primary_key
                ? "It stops being a key on the diagram too."
                : "It becomes a key on the diagram too, and is made required.");
            connect(key, &QAction::triggered, this,
                    [this, id = *column.origin, was = column.primary_key] {
                        show_result(editor_.set_primary_key(id, !was), false);
                    });
            menu.addSeparator();
        }
        auto* remove = menu.addAction(QString("Remove \"%1\"").arg(text(column.name)));
        remove->setObjectName("schemaRemoveColumn");
        const bool removable = column.origin || column.added;
        remove->setEnabled(removable);
        if (!removable)
            remove->setToolTip(column.foreign_key
                ? "This column is the relationship's key. It goes when the relationship does."
                : "The conversion made this column. It is not the model's to remove.");
        connect(remove, &QAction::triggered, this,
                [this, origin = *table.origin, column] { remove_schema_column(origin, column); });
    }
    // What colour the tables wear. The same palette the canvas offers, because
    // a table here and the entity it came from are one element wearing one
    // colour: colouring it on either side is the same edit, and the colour
    // already travels both ways.
    //
    // Applies to everything marked, so a band drawn round a group colours the
    // group. Where nothing is marked it applies to the table pressed.
    std::vector<domain::ElementRef> chosen = schema_->selection();
    if (chosen.empty() && table.origin) chosen.push_back(*table.origin);
    if (!chosen.empty()) {
        menu.addSeparator();
        const auto several = chosen.size() > 1;
        auto* colours = menu.addMenu(several ? QString("Colour %1 tables").arg(chosen.size())
                                             : QString("Colour"));
        colours->setObjectName("schemaColour");
        for (const auto& [name, colour] : swatches()) {
            auto* entry = colours->addAction(swatch_icon(colour), QString::fromLatin1(name));
            entry->setObjectName("schemaSwatch" + QString::fromLatin1(name));
            connect(entry, &QAction::triggered, this, [this, chosen, colour] {
                show_result(editor_.recolour(chosen, domain::Colour{
                    static_cast<std::uint8_t>(colour.red()),
                    static_cast<std::uint8_t>(colour.green()),
                    static_cast<std::uint8_t>(colour.blue())}), false);
            });
        }
        colours->addSeparator();
        auto* custom = colours->addAction("Custom colour…");
        custom->setObjectName("schemaCustomColour");
        connect(custom, &QAction::triggered, this, [this, chosen] {
            // Opened on what the first of them already wears, so the dialog
            // starts from the colour being changed rather than from nothing.
            const auto& worn = editor_.project().colours;
            const auto current = worn.find(chosen.front());
            const auto initial = current == worn.end()
                ? QColor(Qt::white)
                : QColor(current->second.red, current->second.green, current->second.blue);
            const auto picked = QColorDialog::getColor(initial, this, "Choose a surface colour");
            if (!picked.isValid()) return;
            show_result(editor_.recolour(chosen, domain::Colour{
                static_cast<std::uint8_t>(picked.red()),
                static_cast<std::uint8_t>(picked.green()),
                static_cast<std::uint8_t>(picked.blue())}), false);
        });
        auto* plain = colours->addAction("Use theme colour");
        plain->setObjectName("schemaClearColour");
        // Nothing to clear where none of them has been given a colour.
        const auto& worn = editor_.project().colours;
        plain->setEnabled(std::any_of(chosen.begin(), chosen.end(),
                                      [&](const domain::ElementRef& ref) {
                                          return worn.contains(ref);
                                      }));
        connect(plain, &QAction::triggered, this,
                [this, chosen] { show_result(editor_.recolour(chosen, {}), false); });
    }
    menu.exec(spot.at);
}

// Adding a column. The usual answer is that the diagram should gain the
// attribute too, so that is what the default button does; declining keeps the
// column on the schema alone, which ADR-010 allows and which the panel then
// reports as a difference between the two.
void MainWindow::ask_column_type(const domain::PreviewColumn& column, QPoint at) {
    if (!column.origin && !column.added) return;
    if (!type_picker_) type_picker_ = new TypePicker(this);
    // It is its own window, so it inherits none of the window's appearance
    // and is dressed each time it opens -- which also keeps it right after
    // the theme is changed behind it.
    type_picker_->wear(theme(theme_));
    const auto answering = column.origin;
    const auto added = column.added;
    type_picker_->chose = [this, answering, added](domain::LogicalType type) {
        // The same answer either way, though what holds it differs: an
        // attribute the diagram draws, or a column the schema has on its own.
        if (answering) show_result(editor_.set_logical_type(*answering, type), false);
        else if (added) show_result(editor_.set_schema_column_type(*added, type), false);
    };
    type_picker_->closed = [this] { if (schema_) schema_->set_answering({}, false); };
    if (schema_) {
        if (answering) schema_->set_answering(SchemaView::Answering{*answering}, false);
        else if (added) schema_->set_answering(SchemaView::Answering{*added}, false);
    }
    type_picker_->open_at(at, column.type == domain::LogicalType::Unset
                                  ? std::optional<domain::LogicalType>{}
                                  : column.type);
}

void MainWindow::ask_column_size(const domain::PreviewColumn& column, QPoint at) {
    if (!column.origin && !column.added) return;
    if (domain::size_of(column.type) == domain::TypeSize::None) return;
    if (!size_picker_) size_picker_ = new SizePicker(this);
    size_picker_->wear(theme(theme_), picker_sheet(theme(theme_)));
    const auto answering = column.origin;
    const auto added = column.added;
    size_picker_->chose = [this, answering, added](std::uint32_t length, std::uint32_t scale) {
        if (answering) show_result(editor_.set_type_size(*answering, length, scale), false);
        else if (added) show_result(editor_.set_schema_column_size(*added, length, scale), false);
    };
    size_picker_->closed = [this] { if (schema_) schema_->set_answering({}, true); };
    if (schema_) {
        if (answering) schema_->set_answering(SchemaView::Answering{*answering}, true);
        else if (added) schema_->set_answering(SchemaView::Answering{*added}, true);
    }
    size_picker_->open_at(at, column);
}

void MainWindow::answer_decision(const domain::OpenDecision& decision, std::size_t choice) {
    switch (decision.kind) {
    case domain::DecisionKind::IsaStrategy: {
        if (!std::holds_alternative<domain::SpecializationId>(decision.about)) return;
        static constexpr std::array<domain::IsaStrategy, 3> strategies{
            domain::IsaStrategy::PerSubclass, domain::IsaStrategy::SingleTable,
            domain::IsaStrategy::PerConcrete};
        if (choice >= strategies.size()) return;
        show_result(editor_.set_isa_strategy(std::get<domain::SpecializationId>(decision.about),
                                             strategies[choice]), false);
        return;
    }
    case domain::DecisionKind::CompositeMode: {
        if (!std::holds_alternative<domain::AttributeId>(decision.about)) return;
        static constexpr std::array<domain::CompositeMode, 3> modes{
            domain::CompositeMode::Parts, domain::CompositeMode::Whole, domain::CompositeMode::Both};
        if (choice >= modes.size()) return;
        show_result(editor_.set_composite_mode(std::get<domain::AttributeId>(decision.about),
                                               modes[choice]), false);
        return;
    }
    case domain::DecisionKind::BridgeKey: {
        if (!std::holds_alternative<domain::RelationshipId>(decision.about) || choice > 1) return;
        show_result(editor_.set_bridge_key(std::get<domain::RelationshipId>(decision.about),
                    choice == 1 ? domain::BridgeKey::Pair : domain::BridgeKey::Own), false);
        return;
    }
    case domain::DecisionKind::OneToOneKey: {
        if (!std::holds_alternative<domain::RelationshipId>(decision.about)) return;
        if (choice >= decision.sides.size()) return;
        show_result(editor_.set_one_to_one_key(std::get<domain::RelationshipId>(decision.about),
                                               decision.sides[choice]), false);
        return;
    }
    }
}

// A name typed over a table's or a column's on the schema.
//
// Which command that is depends on what the thing is made of, and this is the
// only place that knows: a table and a derived column are named by the element
// they came from, so renaming them renames that element and the diagram says
// the new name too. A column added on the schema has an identity of its own and
// is renamed by it. A key the conversion invented has nothing behind it at all,
// so it is given a name of its own, remembered against its table.
void MainWindow::rename_from_schema(const SchemaView::Spot& spot, const QString& typed) {
    if (!schema_ || spot.table >= schema_->preview().tables.size()) return;
    const auto& table = schema_->preview().tables[spot.table];
    const auto chosen = typed.trimmed().toStdString();
    if (!spot.column) {
        if (!table.origin) {
            statusBar()->showMessage(tr("This table has nothing on the diagram to rename."), 5000);
            return;
        }
        show_result(editor_.rename_table(*table.origin, chosen), false);
        return;
    }
    if (*spot.column >= table.columns.size()) return;
    const auto& column = table.columns[*spot.column];
    if (column.origin) { show_result(editor_.rename(domain::ElementRef{*column.origin}, chosen), false); return; }
    if (column.added) { show_result(editor_.rename_schema_column(*column.added, chosen), false); return; }
    if (column.origin_kind == domain::ColumnOrigin::Generated && column.primary_key && table.origin) {
        show_result(editor_.rename_schema_key(*table.origin, chosen), false);
        return;
    }
    statusBar()->showMessage(column.origin_kind == domain::ColumnOrigin::ForeignKey
        ? tr("A foreign key is named for the key it points at. Rename that key and this follows.")
        : tr("The conversion made this column. It is not the model's to rename."), 6000);
}

// Another column, made where it will be read.
//
// Nothing is asked first. A dialog wanting a name before the column exists puts
// a question in front of the thing it is about; the row is made instead, with a
// name that can be typed over the moment it appears. Reflecting is the default
// and not a question either: the schema and the diagram are two views of one
// model, so a column added here is an attribute on the model and the schema
// follows from it. A column meant for the schema alone is a departure from that
// and is asked for by name, on the menu.
void MainWindow::add_schema_column(domain::ElementRef table, bool schema_only) {
    if (schema_only) {
        const auto made = editor_.add_schema_column(table, "Column");
        show_result(made, false);
        return;
    }
    const auto made = editor_.create_attribute("Column", {}, table);
    show_result(made, false);
    if (!made || !made.created || !schema_) return;
    // The row is there now, so the name is opened for typing in it.
    if (const auto* attribute = std::get_if<domain::AttributeId>(&*made.created))
        schema_->open_column_for(*attribute);
}

// Removing a column. A column that only ever existed on the schema is simply
// removed, with nothing to ask: the diagram never had it, so there is nothing
// the diagram could be asked about.
void MainWindow::remove_schema_column(domain::ElementRef table, const domain::PreviewColumn& column) {
    (void)table;
    if (column.added) {
        show_result(editor_.erase_schema_column(*column.added), false);
        return;
    }
    if (!column.origin) return;
    QMessageBox ask(this);
    ask.setObjectName("schemaReflectRemove");
    ask.setIcon(QMessageBox::Question);
    ask.setWindowTitle("Remove it from the diagram too?");
    ask.setText(QString("Remove \"%1\" from the diagram as well?").arg(text(column.name)));
    ask.setInformativeText("Removing it from both deletes the attribute, and can be undone. "
                           "Removing it from Relational Design only leaves the attribute on the "
                           "diagram and stops Relational Design showing it, which is allowed: the "
                           "two describe different levels.");
    auto* both = ask.addButton("Remove from both", QMessageBox::DestructiveRole);
    auto* only = ask.addButton("Relational Design only", QMessageBox::AcceptRole);
    ask.addButton(QMessageBox::Cancel);
    ask.setDefaultButton(both);
    ask.exec();
    if (ask.clickedButton() == both)
        show_result(editor_.erase({domain::ElementRef{*column.origin}}, {}, {}, {}), false);
    else if (ask.clickedButton() == only)
        show_result(editor_.hide_in_schema(*column.origin, true), false);
}

void MainWindow::refresh_schema() {
    if (!schema_ || !schema_open_) return;
    schema_->refresh();
    // The naming convention is the project's, so the picker follows the model
    // rather than remembering what was last clicked: undo moves it too.
    if (schema_names_) {
        const auto previous = refreshing_;
        refreshing_ = true;
        const auto naming = static_cast<int>(editor_.project().decisions.naming);
        for (auto* choice : schema_names_->actions())
            choice->setChecked(choice->data().toInt() == naming);
        refreshing_ = previous;
    }
    refresh_schema_state();
    refresh_shared_names();
}

// What still stands between this picture and a conversion. Counted in columns
// rather than in complaints, because seventeen untyped columns are seventeen
// answers owed, not one.
//
// A line end pulled off its table is counted here too. It changes nothing about
// the model -- the foreign key is still the foreign key -- so it is not an
// error and nothing is refused for it. It is said out loud because a reader who
// has left a connection hanging should be told, rather than have the schema
// quietly put it back or quietly pretend it is joined.
void MainWindow::refresh_shared_names() {
    if (!shared_names_ || !schema_ || !shared_names_body_) return;
    const auto groups = domain::shared_names(schema_->preview());
    shared_names_->setVisible(!groups.empty());
    std::size_t repeats = 0;
    for (const auto& group : groups) repeats += group.columns;
    shared_names_head_->setText(
        QString("%1  Columns that share a name — answer once for all of them  ·  %2 name%3, %4 column%5")
            .arg(shared_names_open_ ? "▾" : "▸")
            .arg(groups.size()).arg(groups.size() == 1 ? "" : "s")
            .arg(repeats).arg(repeats == 1 ? "" : "s"));
    if (!shared_names_open_) return;

    auto* body = qobject_cast<QVBoxLayout*>(shared_names_body_->layout());
    while (auto* stale = body->takeAt(0)) {
        if (auto* widget = stale->widget()) widget->deleteLater();
        delete stale;
    }
    for (const auto& group : groups) {
        auto* row = new QWidget(shared_names_body_);
        auto* row_layout = new QHBoxLayout(row);
        row_layout->setContentsMargins(0, 0, 0, 0);
        row_layout->setSpacing(8);
        auto* named = new QLabel(text(group.name), row);
        named->setObjectName("sharedName");
        named->setMinimumWidth(150);
        row_layout->addWidget(named);
        auto* counted = new QLabel(QString("%1 columns").arg(group.columns), row);
        counted->setObjectName("sharedNameCount");
        row_layout->addWidget(counted);
        auto* type = new QComboBox(row);
        type->setObjectName("sharedNameType");
        type->addItem("Give them all a type…", QVariant());
        for (const auto& family : type_families())
            for (const auto entry : family.types) {
                if (entry == family.types.front())
                    type->addItem(QString("— %1 —").arg(QString::fromLatin1(family.name)), QVariant());
                type->addItem(type_label(entry), QVariant::fromValue(static_cast<int>(entry)));
            }
        type->installEventFilter(wheel_guard_);
        row_layout->addWidget(type, 1);
        const auto answering = group.attributes;
        connect(type, &QComboBox::currentIndexChanged, this, [this, type, answering](int index) {
            const auto chosen = type->itemData(index);
            if (!chosen.isValid()) return;
            // One edit for the lot, so one undo takes it back. Answering six
            // columns and then undoing six times would be a worse bargain than
            // answering them one at a time.
            show_result(editor_.set_logical_types(answering,
                                                  static_cast<domain::LogicalType>(chosen.toInt())), false);
        });
        body->addWidget(row);
    }
}

void MainWindow::refresh_schema_state() {
    if (!schema_ || !schema_state_) return;
    std::size_t open = 0;
    // A foreign key is not counted: its type is its key's, answered where the
    // key is, so counting it again would ask the same question twice.
    for (const auto& table : schema_->preview().tables)
        for (const auto& column : table.columns)
            if (column.type == domain::LogicalType::Unset && !column.foreign_key) ++open;
    // Where the schema has been edited away from the diagram. Reported for the
    // same reason a loose end is: a reader who has made the two levels differ
    // should be told they differ, since the whole point of allowing it is that
    // it was meant.
    std::size_t apart = editor_.project().schema.hidden.size();
    for (const auto& [table, columns] : editor_.project().schema.added) {
        (void)table;
        apart += columns.size();
    }
    QStringList said{"not converted yet"};
    if (open) said << QString("%1 type%2 open").arg(open).arg(open == 1 ? "" : "s");
    if (apart)
        said << QString("%1 change%2 not on the diagram").arg(apart).arg(apart == 1 ? "" : "s");
    if (const auto loose = schema_->loose_ends())
        said << QString("%1 end%2 not connected").arg(loose).arg(loose == 1 ? "" : "s");
    schema_state_->setText(said.join(" · "));
}

void MainWindow::refresh_export_actions() {
    // Nothing drawn is nothing to hand on. The entries stay where they are and
    // go quiet, rather than the row appearing and disappearing as work starts.
    const auto anything = !canvas_->diagram_bounds().isEmpty();
    for (auto* action : export_actions_) action->setEnabled(anything);
}

// The template is a starting frame, not a worked example (Zain, 2026-09-26):
// the general things an ERD is made of, each named for what it is -- an
// entity with an attribute, a relationship, and another entity with an
// attribute -- ready to be renamed into a model of something. The bodies are
// at their default sizes but for the diamond, and every line starts unlocked.
// It opens untitled and unsaved, as a project started from a template does
// (ADR-016).
void MainWindow::load_template() {
    if (!confirm_discard()) return;
    show_home(false);
    application::Editor starting(ids_);
    const auto body = [](double centre_x, double centre_y, const BodySize& size) {
        return domain::Rect{centre_x - size.width / 2, centre_y - size.height / 2, size.width, size.height};
    };
    const auto left = std::get<EntityId>(*starting.create_entity("Entity", body(-340, 0, entity_body)).created);
    const auto right = std::get<EntityId>(*starting.create_entity("Entity", body(340, 0, entity_body)).created);
    // The diamond is drawn wider than a new one, as the example's Enrollment
    // Date is, so the template does not open on its own word cut short.
    starting.relate(left, right, body(0, 0, BodySize{280, 120}), "Relationship");
    starting.create_attribute("Attribute", body(-340, -160, attribute_body), AttributeOwner{ElementRef{left}});
    starting.create_attribute("Attribute", body(340, -160, attribute_body), AttributeOwner{ElementRef{right}});
    show_result(editor_.replace_project(starting.project()));
    path_.clear();
    canvas_->fit_diagram();
}

void MainWindow::load_example() {
    if (!confirm_discard()) return;
    // Anything that puts a project in front of somebody leaves the home
    // screen: the home screen is for choosing what to work on, and once that
    // is chosen it is the work they want to see.
    show_home(false);
    application::Editor example(ids_);
    // This diagram was laid out when a body was half the size it is now. Rather than re-typing every coordinate in it, each rectangle is
    // taken at the scale the bodies grew by: the arrangement is kept exactly,
    // and the list below still reads as the layout it is rather than as a
    // column of unrelated numbers.
    const auto at = [](double x, double y, double wide, double tall) {
        return domain::Rect{x * diagram_scale, y * diagram_scale,
                            wide * diagram_scale, tall * diagram_scale};
    };
    example.rename_project("University · Students, courses and professors");
    // The diagram an introductory course draws: three entities, the three ways
    // they relate, and one of every kind of attribute -- a key, a composite
    // with parts of its own, one that is worked out rather than stored, and one
    // that may be held more than once. Between them those cover everything the
    // conceptual editor has to draw, which is why this is the example.
    //
    // The coordinates are the diagram's own, laid out as it is drawn on paper:
    // the view is fitted to them at the end rather than the other way round.
    auto add_entity = [&](const char* name, Rect body) {
        return std::get<EntityId>(*example.create_entity(name, body).created);
    };
    auto add_relationship = [&](const char* name, Rect body) {
        return std::get<RelationshipId>(*example.create_relationship(name, body).created);
    };
    auto add_attribute = [&](const char* name, Rect body, AttributeOwner owner,
                             AttributeKind kind = AttributeKind::Normal) {
        const auto id = std::get<AttributeId>(*example.create_attribute(name, body, owner).created);
        if (kind != AttributeKind::Normal) example.set_attribute_kind(id, kind);
        return id;
    };
    // One side of a relationship, carrying the pair the diagram labels it with:
    // total participation is the 1 of (1,1) and partial the 0, while the
    // maximum is the M or the 1 that follows it.
    auto join = [&](RelationshipId relationship, ParticipantTarget target,
                    Cardinality maximum, Participation participation) {
        const auto joined = example.connect(relationship, target);
        example.update_participant(relationship, *joined.participant, maximum, participation, "");
    };

    const auto student = add_entity("Student", at(-441, -195, 160, 80));
    const auto course = add_entity("Course", at(-441, 319, 160, 80));
    const auto professor = add_entity("Professor", at(433, 319, 160, 80));

    // A student may enroll in any number of courses and a course may hold any
    // number of students, so the pair that resolves into its own table later.
    const auto enrolled = add_relationship("Enrolled", at(-456, 33, 190, 110));
    join(enrolled, student, Cardinality::Many, Participation::Partial);
    join(enrolled, course, Cardinality::Many, Participation::Partial);
    // A course is taught by at most one professor, and a professor may be
    // between courses, so neither side is obliged to take part.
    const auto teaches = add_relationship("Teaches", at(-52, 304, 190, 110));
    join(teaches, course, Cardinality::One, Participation::Partial);
    join(teaches, professor, Cardinality::One, Participation::Partial);
    // Mentoring is the one side that is compulsory: every professor mentors,
    // while a student need not be mentored at all.
    const auto mentor = add_relationship("Mentor", at(415, -208, 190, 110));
    join(mentor, student, Cardinality::Many, Participation::Partial);
    join(mentor, professor, Cardinality::One, Participation::Total);

    // A student is identified by an ID, named by a composite whose three parts
    // hang off it, has an age nobody stores, and may be reached on more than
    // one telephone.
    add_attribute("ID", at(-727, -180, 150, 60), student, AttributeKind::Key);
    const auto student_name = add_attribute("Name", at(-544, -400, 150, 60), student, AttributeKind::Composite);
    add_attribute("First", at(-805, -515, 150, 60), student_name);
    add_attribute("Mid", at(-617, -544, 150, 60), student_name);
    add_attribute("Last", at(-418, -542, 150, 60), student_name);
    add_attribute("Gender", at(-715, -300, 150, 60), student);
    add_attribute("Birth Date", at(-367, -356, 150, 60), student);
    add_attribute("Age", at(-170, -363, 150, 60), student, AttributeKind::Derived);
    add_attribute("Phone", at(27, -360, 150, 60), student, AttributeKind::Multivalued);

    add_attribute("ID", at(-576, 506, 150, 60), course, AttributeKind::Key);
    add_attribute("Name", at(-728, 387, 150, 60), course);
    add_attribute("Credit Hours", at(-707, 263, 150, 60), course);

    add_attribute("ID", at(277, 506, 150, 60), professor, AttributeKind::Key);
    add_attribute("Name", at(489, 506, 150, 60), professor);
    add_attribute("Salary", at(661, 414, 150, 60), professor);

    // The date belongs to the enrollment rather than to the student or to the
    // course, which is the reason a relationship may carry attributes at all.
    // Its ellipse is the one that is drawn wider than the rest, because the
    // name is longer than the others and an example should not open on a
    // label that has been cut short.
    add_attribute("Enrollment Date", at(-175, 17, 200, 60), enrolled);

    show_result(editor_.replace_project(example.project()));
    path_.clear();
    canvas_->select_elements({enrolled});
    canvas_->fit_diagram();
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (confirm_discard()) event->accept(); else event->ignore();
}

} // namespace erdflow::desktop
