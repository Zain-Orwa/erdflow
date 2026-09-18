#include "export_dialog.hpp"

#include "diagram_view.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <cmath>
#include <initializer_list>

namespace erdflow::desktop {
namespace {

// A document format and a picture format are both chosen from the one list, so
// each carries a code that says which it is. Documents are offset rather than
// interleaved, so no picture can ever be mistaken for a document.
constexpr int document_offset = 100;
[[nodiscard]] int code_of(DocumentFormat format) { return document_offset + static_cast<int>(format); }
[[nodiscard]] int code_of(PictureFormat format) { return static_cast<int>(format); }
[[nodiscard]] bool is_document(int code) { return code >= document_offset; }

struct Choice { int value; const char* label; const char* explanation; };

const std::vector<Choice>& extents() {
    static const std::vector<Choice> list{
        {static_cast<int>(PictureExtent::WholeDiagram), "Whole diagram", "Everything that has been drawn."},
        {static_cast<int>(PictureExtent::Selection), "Selection", "Only what is selected on the canvas."},
        {static_cast<int>(PictureExtent::CurrentView), "Current view", "Exactly what the canvas is showing now."},
    };
    return list;
}

const std::vector<Choice>& backgrounds() {
    static const std::vector<Choice> list{
        {static_cast<int>(PictureBackground::Transparent), "Transparent", "Nothing behind the diagram, so it sits on whatever it is placed on."},
        {static_cast<int>(PictureBackground::ThemeColour), "Theme colour", "The canvas colour the editor is showing."},
        {static_cast<int>(PictureBackground::White), "White", "Paper, for a destination that assumes it."},
    };
    return list;
}

} // namespace

ExportDialog::ExportDialog(DiagramView& view, const domain::Project& project, QWidget* parent)
    : QDialog(parent), view_(view), project_(project) {
    setObjectName("exportDialog");
    setWindowTitle("Export");

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);
    auto* form = new QFormLayout;
    form->setSpacing(8);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    format_ = new QComboBox(this);
    format_->setObjectName("exportFormat");
    // Documents first: a person handing this work on is choosing between a
    // report and a picture before they are choosing between PNG and SVG.
    for (const auto& info : document_formats()) {
        format_->addItem(QString::fromUtf8(info.label), code_of(info.format));
        format_->setItemData(format_->count() - 1, QString::fromUtf8(info.caution), Qt::ToolTipRole);
    }
    format_->insertSeparator(format_->count());
    for (const auto& info : picture_formats()) {
        // A format this build has no writer for is left out rather than
        // offered and then failed.
        if (!picture_format_available(info.format)) continue;
        format_->addItem(QString::fromUtf8(info.label), code_of(info.format));
        if (*info.caution)
            format_->setItemData(format_->count() - 1, QString::fromUtf8(info.caution), Qt::ToolTipRole);
    }
    form->addRow("Format", format_);

    extent_ = new QComboBox(this);
    extent_->setObjectName("exportExtent");
    for (const auto& choice : extents()) {
        extent_->addItem(QString::fromUtf8(choice.label), choice.value);
        extent_->setItemData(extent_->count() - 1, QString::fromUtf8(choice.explanation), Qt::ToolTipRole);
    }
    form->addRow("Extent", extent_);

    background_ = new QComboBox(this);
    background_->setObjectName("exportBackground");
    for (const auto& choice : backgrounds()) {
        background_->addItem(QString::fromUtf8(choice.label), choice.value);
        background_->setItemData(background_->count() - 1, QString::fromUtf8(choice.explanation), Qt::ToolTipRole);
    }
    form->addRow("Background", background_);

    scale_ = new QDoubleSpinBox(this);
    scale_->setObjectName("exportScale");
    scale_->setRange(0.05, 40);
    scale_->setSingleStep(0.5);
    scale_->setDecimals(2);
    scale_->setSuffix(" ×");
    scale_->setToolTip("How large the picture is drawn, as a multiple of the diagram's own size.");
    form->addRow("Scale", scale_);

    resolution_ = new QSpinBox(this);
    resolution_->setObjectName("exportResolution");
    resolution_->setRange(72, 1200);
    resolution_->setSingleStep(50);
    resolution_->setSuffix(" dpi");
    resolution_->setToolTip("The detail a PDF page is written at. A page has a physical size, so it is measured in dots per inch rather than pixels.");
    form->addRow("Resolution", resolution_);

    margin_ = new QDoubleSpinBox(this);
    margin_->setObjectName("exportMargin");
    margin_->setRange(0, 400);
    margin_->setDecimals(0);
    margin_->setSingleStep(8);
    margin_->setToolTip("Room left around the diagram, so nothing is drawn hard against the edge.");
    form->addRow("Margin", margin_);
    layout->addLayout(form);

    carry_ = new QCheckBox("Carry the project inside the picture", this);
    carry_->setObjectName("exportCarryProject");
    carry_->setToolTip("The picture is also the project: anyone can open it as a picture, and ERDFlow reopens it as the diagram it was.");
    layout->addWidget(carry_);

    caution_ = new QLabel(this);
    caution_->setObjectName("exportCaution");
    caution_->setWordWrap(true);
    layout->addWidget(caution_);

    size_ = new QLabel(this);
    size_->setObjectName("exportSize");
    size_->setWordWrap(true);
    layout->addWidget(size_);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto* accept = buttons->addButton("Export…", QDialogButtonBox::AcceptRole);
    accept->setObjectName("exportAccept");
    accept->setDefault(true);
    buttons->button(QDialogButtonBox::Cancel)->setObjectName("exportCancel");
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    for (auto* box : {format_, extent_, background_})
        connect(box, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(scale_, &QDoubleSpinBox::valueChanged, this, [this] { refresh(); });
    connect(margin_, &QDoubleSpinBox::valueChanged, this, [this] { refresh(); });
    connect(resolution_, &QSpinBox::valueChanged, this, [this] { refresh(); });
    connect(carry_, &QCheckBox::toggled, this, [this] { refresh(); });

    set_choice(choice_);
}

void ExportDialog::set_choice(const ExportChoice& choice) {
    choice_ = choice;
    refreshing_ = true;
    const auto code = choice.document ? code_of(choice.as_document) : code_of(choice.as_picture.format);
    if (const auto index = format_->findData(code); index >= 0) format_->setCurrentIndex(index);
    extent_->setCurrentIndex(extent_->findData(static_cast<int>(choice.as_picture.extent)));
    background_->setCurrentIndex(background_->findData(static_cast<int>(choice.as_picture.background)));
    scale_->setValue(choice.as_picture.scale);
    resolution_->setValue(choice.as_picture.resolution);
    margin_->setValue(choice.as_picture.margin);
    carry_->setChecked(choice.as_picture.carry_project);
    refreshing_ = false;
    refresh();
}

void ExportDialog::collect() {
    const auto code = format_->currentData().toInt();
    choice_.document = is_document(code);
    if (choice_.document) choice_.as_document = static_cast<DocumentFormat>(code - document_offset);
    else choice_.as_picture.format = static_cast<PictureFormat>(code);
    choice_.as_picture.extent = static_cast<PictureExtent>(extent_->currentData().toInt());
    choice_.as_picture.background = static_cast<PictureBackground>(background_->currentData().toInt());
    choice_.as_picture.scale = scale_->value();
    choice_.as_picture.resolution = resolution_->value();
    choice_.as_picture.margin = margin_->value();
    choice_.as_picture.carry_project = carry_->isChecked();
}

void ExportDialog::refresh() {
    if (refreshing_) return;
    refreshing_ = true;
    collect();

    // A document is a listing of the whole project. It has no extent, no
    // background, no scale and nowhere to carry the project, so those fields go
    // quiet rather than vanishing and resizing the window under the pointer.
    if (choice_.document) {
        const auto& info = document_format(choice_.as_document);
        for (QWidget* field : std::initializer_list<QWidget*>{extent_, background_, scale_, resolution_, margin_})
            field->setEnabled(false);
        carry_->setEnabled(false);
        caution_->setText(QString::fromUtf8(info.caution));
        caution_->setVisible(true);
        const auto entities = project_.entities.size();
        const auto attributes = project_.attributes.size();
        const auto relationships = project_.relationships.size();
        size_->setText((entities || attributes || relationships)
            ? QString("A listing of %1 entities, %2 attributes and %3 relationships%4.")
                  .arg(entities).arg(attributes).arg(relationships)
                  .arg(info.carries_diagram ? ", with the diagram above it" : ", without the diagram")
            : QString("There is nothing to list yet."));
        refreshing_ = false;
        return;
    }

    const auto& info = picture_format(choice_.as_picture.format);
    // An extent with nothing in it cannot be chosen, and one that empties while
    // it is chosen falls back to the whole diagram rather than to a refusal at
    // the moment of writing.
    for (int row = 0; row < extent_->count(); ++row) {
        const auto extent = static_cast<PictureExtent>(extent_->itemData(row).toInt());
        const auto usable = !picture_extent(view_, extent).isEmpty();
        auto* model = qobject_cast<QStandardItemModel*>(extent_->model());
        if (model && model->item(row)) model->item(row)->setEnabled(usable);
        if (!usable && extent_->currentIndex() == row) {
            extent_->setCurrentIndex(extent_->findData(static_cast<int>(PictureExtent::WholeDiagram)));
            choice_.as_picture.extent = PictureExtent::WholeDiagram;
        }
    }
    extent_->setEnabled(true);
    background_->setEnabled(true);
    margin_->setEnabled(true);
    // A scale belongs to a picture measured in pixels; a page is measured in
    // dots per inch instead.
    scale_->setEnabled(true);
    resolution_->setEnabled(choice_.as_picture.format == PictureFormat::Pdf);
    carry_->setEnabled(info.carries_project);
    if (!info.carries_project) carry_->setChecked(false);

    QString caution = QString::fromUtf8(info.caution);
    if (choice_.as_picture.background == PictureBackground::Transparent && !info.keeps_transparency)
        caution += QString(caution.isEmpty() ? "" : " ")
                 + QString("%1 cannot keep transparency, so the picture will stand on white.")
                       .arg(QString::fromUtf8(info.label));
    caution_->setText(caution);
    caution_->setVisible(!caution.isEmpty());

    // What pressing Export will actually produce, in the units the format is
    // measured in, worked out the way the writer works it out so the two cannot
    // disagree about the size of the file.
    auto source = picture_extent(view_, choice_.as_picture.extent);
    if (source.isEmpty()) {
        size_->setText("There is nothing to export yet.");
    } else {
        const auto margin = choice_.as_picture.margin;
        source = source.adjusted(-margin, -margin, margin, margin);
        const auto scale = choice_.as_picture.scale;
        const auto wide = std::llround(source.width() * scale);
        const auto high = std::llround(source.height() * scale);
        if (choice_.as_picture.format == PictureFormat::Pdf)
            size_->setText(QString("A page %1 by %2 mm, at %3 dpi.")
                               .arg(source.width() * scale * 25.4 / 96.0, 0, 'f', 0)
                               .arg(source.height() * scale * 25.4 / 96.0, 0, 'f', 0)
                               .arg(choice_.as_picture.resolution));
        else if (info.raster)
            size_->setText(QString("%1 by %2 pixels.").arg(wide).arg(high));
        else
            size_->setText(QString("%1 by %2, and readable at any size.").arg(wide).arg(high));
    }
    refreshing_ = false;
}

} // namespace erdflow::desktop
