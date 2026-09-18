#pragma once

#include "document_export.hpp"
#include "picture_export.hpp"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;

namespace erdflow::desktop {

class DiagramView;

// What an export settled on: a picture, with the options that decide whether
// it is usable, or a document, which has none to decide.
struct ExportChoice {
    bool document = false;
    DocumentFormat as_document = DocumentFormat::Pdf;
    PictureOptions as_picture;
};

// What to ask before anything is written. One list holds every format ERDFlow
// writes, documents above pictures, because a person deciding how to hand this
// work on is choosing between a report and a picture before they are choosing
// between PNG and SVG.
//
// The options below the list are the picture's. A document has no extent, no
// background and no scale to choose, so those fields go quiet rather than
// disappearing and changing the size of the window under the pointer. The
// dialog offers only what the build can do and only what the chosen format can
// hold: a format whose writer is missing is not listed, an extent with nothing
// in it cannot be chosen, and the project can only be carried by the two
// formats with somewhere to put it.
class ExportDialog final : public QDialog {
public:
    ExportDialog(DiagramView& view, const domain::Project& project, QWidget* parent = nullptr);
    [[nodiscard]] ExportChoice choice() const { return choice_; }
    // Opens on what was last settled on, so a second export of the same work
    // takes one press rather than four.
    void set_choice(const ExportChoice& choice);

private:
    DiagramView& view_;
    const domain::Project& project_;
    ExportChoice choice_;
    QComboBox* format_ = nullptr;
    QComboBox* extent_ = nullptr;
    QComboBox* background_ = nullptr;
    QDoubleSpinBox* scale_ = nullptr;
    QSpinBox* resolution_ = nullptr;
    QDoubleSpinBox* margin_ = nullptr;
    QCheckBox* carry_ = nullptr;
    QLabel* caution_ = nullptr;
    QLabel* size_ = nullptr;
    // Keeps the dialog saying what pressing Export will produce, which of the
    // fields apply to the chosen format, and what that format will not carry.
    void refresh();
    void collect();
    bool refreshing_ = false;
};

} // namespace erdflow::desktop
