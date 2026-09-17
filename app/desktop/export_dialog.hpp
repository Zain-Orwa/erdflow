#pragma once

#include "picture_export.hpp"

#include <QDialog>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;

namespace erdflow::desktop {

class DiagramView;

// What to ask before a picture is written. The options are not a refinement to
// be added later: a picture exported at the wrong size, cropped to the wrong
// part of the diagram or standing on the wrong background is not a usable
// picture, so they are settled here, in one place, before any file is named.
//
// The dialog offers only what the build can actually do and only what the
// chosen format can actually hold: a format the plugin for is missing is not
// listed, a selection that does not exist cannot be chosen, and the project
// can only be carried by the two formats that have somewhere to put it.
class ExportDialog final : public QDialog {
public:
    ExportDialog(DiagramView& view, QWidget* parent = nullptr);
    [[nodiscard]] PictureOptions options() const { return options_; }
    // Opens on the options last used in this session, so exporting a second
    // picture the same way takes one press rather than four.
    void set_options(const PictureOptions& options);

private:
    DiagramView& view_;
    PictureOptions options_;
    QComboBox* format_ = nullptr;
    QComboBox* extent_ = nullptr;
    QComboBox* background_ = nullptr;
    QDoubleSpinBox* scale_ = nullptr;
    QSpinBox* resolution_ = nullptr;
    QDoubleSpinBox* margin_ = nullptr;
    QCheckBox* carry_ = nullptr;
    QLabel* caution_ = nullptr;
    QLabel* size_ = nullptr;
    // Keeps the dialog saying what pressing Export will produce: the size in
    // pixels or on the page, which of the fields apply to the chosen format,
    // and what that format will and will not carry.
    void refresh();
    // Reads the controls into the options, without touching what is shown.
    void collect();
    bool refreshing_ = false;
};

} // namespace erdflow::desktop
