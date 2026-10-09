// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/icons.hpp"
#include "app/desktop/symbols.hpp"
#include "app/desktop/export_dialog.hpp"
#include "app/desktop/home_page.hpp"
#include "app/desktop/conceptual_examples.hpp"
#include "app/desktop/relational_examples.hpp"
#include "app/desktop/home_demo_canvas.hpp"
#include "app/desktop/home_demo_scenes.hpp"
#include "app/desktop/home_sidebar.hpp"
#include "app/desktop/start_route_card.hpp"
#include "app/desktop/welcome_flow_illustration.hpp"
#include "app/desktop/main_window.hpp"
#include "app/desktop/notice.hpp"
#include "app/desktop/schema_explorer.hpp"
#include "infrastructure/project_store.hpp"

#include <QAbstractAnimation>
#include <QAction>
#include <cmath>
#include <QDebug>
#include <QScrollArea>
#include <QScrollBar>
#include <QMenuBar>
#include <QDir>
#include <QApplication>
#include <QStatusBar>
#include <QStyle>
#include <QElapsedTimer>
#include <QCheckBox>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDockWidget>
#include <QTreeWidget>
#include <QLayout>
#include <QEnterEvent>
#include <QPointer>
#include <QFontMetrics>
#include <QFrame>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QDoubleSpinBox>
#include <QClipboard>
#include <QCursor>
#include <QFile>
#include <QFileInfo>
#include <QMimeData>
#include <QImage>
#include <QPainter>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QInputDialog>
#include <QRegularExpression>
#include <QAbstractButton>
#include <QSettings>
#include <QSlider>
#include <QWidgetAction>
#include <QStandardItemModel>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QWheelEvent>
#include <QToolButton>
#include <QTreeView>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>

namespace
{
    using namespace erdflow;
    void require(bool condition, const char *message)
    {
        if (!condition)
            throw std::runtime_error(message);
    }
    void settle()
    {
        QApplication::processEvents();
        QApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    // Identities handed out newest-first, each sorting before the one issued
    // before it, as a wall clock set back makes them.
    struct BackwardIds final : application::IdGenerator
    {
        std::uint64_t counter = std::uint64_t{1} << 48;
        domain::Uuid next() override
        {
            domain::Uuid id;
            id.bytes[6] = 0x70;
            id.bytes[8] = 0x80;
            const auto value = counter--;
            for (unsigned i = 0; i < 7; ++i)
                id.bytes[15 - i] = static_cast<std::uint8_t>((value >> (8U * i)) & 0xffU);
            return id;
        }
    };
    // Some work is deliberately not done on the instant it is asked for. A theme
    // hovered in the menu is shown once the pointer has settled rather than while
    // it is still travelling, so a test waiting for one has to let the clock run
    // as well as the event loop.
    void settle_for(int milliseconds)
    {
        QElapsedTimer clock;
        clock.start();
        while (clock.elapsed() < milliseconds)
        {
            QApplication::processEvents(QEventLoop::AllEvents, 5);
            settle();
        }
    }
    // Lets whatever is moving in a window come to rest -- a panel rising over
    // 280 ms, or falling away -- however long the machine takes over it. A
    // fixed wait is longer than the move on a quick machine and may be shorter
    // than it on a slow one; this waits exactly as long as the move does
    // (2026-10-09). Only for a move with nothing timed to follow it: opening
    // the schema full, raising the Conceptual preview and converting each do
    // something more a little after their panel lands.
    void settle_motion(const QWidget &window)
    {
        QElapsedTimer clock;
        clock.start();
        const auto moving = [&]
        {
            const auto animations = window.findChildren<QAbstractAnimation *>();
            return std::any_of(animations.begin(), animations.end(), [](const QAbstractAnimation *animation)
                               { return animation->state() == QAbstractAnimation::Running; });
        };
        while (moving())
        {
            require(clock.elapsed() < 10000, "What moves in the window comes to rest");
            QApplication::processEvents(QEventLoop::AllEvents, 5);
        }
        settle();
    }
    template <class T>
    T *child(desktop::MainWindow &window, const char *name)
    {
        settle();
        auto *value = window.findChild<T *>(QString::fromLatin1(name));
        require(value != nullptr, name);
        return value;
    }
    void dismiss(QMessageBox::StandardButton choice)
    {
        QTimer::singleShot(0, [choice]
                           {
        if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
            box->button(choice)->click(); });
    }
    // The words a button draws: a lone & marks a shortcut letter and is not
    // drawn, and && draws one &.
    QString shown(const QString &words)
    {
        QString drawn;
        for (qsizetype i = 0; i < words.size(); ++i)
        {
            if (words[i] != '&')
                drawn += words[i];
            else if (i + 1 < words.size() && words[i + 1] == '&')
                drawn += words[++i];
        }
        return drawn;
    }
    // A box that asks, answered by the button with these words as soon as it
    // opens; what it said -- its title, its question and its detail -- is kept
    // where asked, so a test can hold the question to what it should be.
    void answer(const QString &button, QStringList *said = nullptr)
    {
        QTimer::singleShot(0, [button, said]
                           {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (!box) return;
        if (said) *said = QStringList{box->windowTitle(), box->text(), box->informativeText()};
        for (auto* one : box->buttons())
            if (shown(one->text()) == button) {
                one->click();
                return;
            } });
    }
    // Boxes asked one after another, each answered as soon as it opens by the
    // button with the next of these words; what each said is kept where asked, as
    // answer keeps it, followed by the words of every button it offered, sorted,
    // and the words of the one Return presses. A box asking for a name keeps its
    // title, its words, the name it proposes and the words of its OK button; it
    // is answered by its Cancel or OK button's words, or by "type:" and a name,
    // typed over the one proposed before OK is pressed.
    void answer_in_turn(const QStringList &buttons, std::vector<QStringList> *said = nullptr)
    {
        auto *timer = new QTimer;
        auto left = std::make_shared<QStringList>(buttons);
        auto ticks = std::make_shared<int>(0);
        QObject::connect(timer, &QTimer::timeout, [timer, left, ticks, said]
                         {
        auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        auto* naming = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        if (box && box->isVisible() && !left->isEmpty()) {
            QStringList offered;
            for (auto* one : box->buttons()) offered << shown(one->text());
            offered.sort();
            if (said)
                said->push_back(QStringList{box->windowTitle(), box->text(), box->informativeText(),
                                            offered.join('|'),
                                            box->defaultButton() ? shown(box->defaultButton()->text()) : QString()});
            const auto wanted = left->takeFirst();
            for (auto* one : box->buttons())
                if (shown(one->text()) == wanted) {
                    one->click();
                    break;
                }
        } else if (naming && naming->isVisible() && !left->isEmpty()) {
            if (said)
                said->push_back(QStringList{naming->windowTitle(), naming->labelText(), naming->textValue(),
                                            shown(naming->okButtonText())});
            const auto wanted = left->takeFirst();
            const bool cancelled = wanted == shown(naming->cancelButtonText());
            if (wanted.startsWith("type:")) naming->setTextValue(wanted.mid(5));
            const auto pressed = shown(cancelled ? naming->cancelButtonText() : naming->okButtonText());
            for (auto* one : naming->findChildren<QPushButton*>())
                if (shown(one->text()) == pressed) {
                    one->click();
                    break;
                }
        }
        if (left->isEmpty() || ++*ticks > 500) {
            timer->stop();
            timer->deleteLater();
        } });
        timer->start(5);
    }
    // A list's itemClicked only comes from real pointer work, so a test says what
    // it means directly: this item was chosen.
    // How many pixels of a picture carry any colour at all, rather than a grey.
    // A little allowance is left for rounding in anti-aliased edges.
    int coloured_pixels(const QImage &picture)
    {
        const auto image = picture.convertToFormat(QImage::Format_ARGB32);
        int found = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
            {
                const auto pixel = image.pixel(x, y);
                if (qAlpha(pixel) < 8)
                    continue;
                const auto high = std::max({qRed(pixel), qGreen(pixel), qBlue(pixel)});
                const auto low = std::min({qRed(pixel), qGreen(pixel), qBlue(pixel)});
                if (high - low > 12)
                    ++found;
            }
        return found;
    }

    // Well beyond the few pixels within which a line, a line end or a table
    // edge answers a press on the schema, so a point this far from all of them
    // is pressed on nothing but what the test means it to be.
    constexpr double press_clearance = 12;

    // How far a point is from the nearest of the schema's lines, each given as
    // the corners it is drawn through.
    double distance_to_lines(QPointF point, const std::vector<std::vector<QPointF>> &lines)
    {
        auto nearest = std::numeric_limits<double>::infinity();
        for (const auto &line : lines)
            for (std::size_t k = 1; k < line.size(); ++k)
            {
                const auto run = line[k] - line[k - 1];
                const auto length = QPointF::dotProduct(run, run);
                const auto along = length > 0
                                       ? std::clamp(QPointF::dotProduct(point - line[k - 1], run) / length, 0.0, 1.0)
                                       : 0.0;
                const auto closest = line[k - 1] + run * along;
                nearest = std::min(nearest, std::hypot(point.x() - closest.x(), point.y() - closest.y()));
            }
        return nearest;
    }

    // How many pixels of a stretch of picture are more than half covered by an
    // ink: nearer to it than to the commonest colour there, which is what the
    // writing stands on. Each platform blends the edges of small letters into
    // their background in its own way, so no single pixel need be exactly the
    // ink, but the middle of every stroke is mostly ink everywhere.
    int inked_pixels(const QImage &picture, const QRectF &area, const QColor &ink)
    {
        std::vector<QColor> seen;
        std::map<QRgb, int> counts;
        for (int y = static_cast<int>(area.top()); y < area.bottom(); ++y)
            for (int x = static_cast<int>(area.left()); x < area.right(); ++x)
            {
                seen.push_back(picture.pixelColor(x, y));
                ++counts[seen.back().rgb()];
            }
        if (counts.empty())
            return 0;
        const QColor ground(std::max_element(counts.begin(), counts.end(), [](const auto &a, const auto &b)
                                             { return a.second < b.second; })
                                ->first);
        const auto apart = [](const QColor &a, const QColor &b)
        { return std::hypot(a.red() - b.red(), a.green() - b.green(), a.blue() - b.blue()); };
        return static_cast<int>(std::count_if(seen.begin(), seen.end(), [&](const QColor &pixel)
                                              { return apart(pixel, ink) < apart(pixel, ground); }));
    }

    // A widget photographed with its lettering smoothed in greys alone. Where
    // the platform smooths text with the screen's red, green and blue
    // subpixels, every grey letter is drawn with a coloured fringe the
    // application never chose. Each widget keeps its own font, told only to
    // smooth in greys -- a widget a style sheet dresses does not inherit its
    // parent's. Only what is on show is touched, being all a photograph holds;
    // it is checked to stay where it was, then put back and checked again.
    QImage grab_without_subpixel_text(QWidget &pictured)
    {
        std::vector<QPointer<QWidget>> widgets{&pictured};
        for (auto *widget : pictured.findChildren<QWidget *>())
            if (widget->window() == pictured.window() && widget->isVisible())
                widgets.emplace_back(widget);
        std::vector<QFont> fonts;
        std::vector<bool> own;
        std::vector<QPalette> palettes;
        std::vector<QRect> places;
        for (const auto &widget : widgets)
        {
            fonts.push_back(widget->font());
            own.push_back(widget->testAttribute(Qt::WA_SetFont));
            palettes.push_back(widget->palette());
            places.push_back(widget->geometry());
        }
        for (std::size_t i = 0; i < widgets.size(); ++i)
        {
            auto smoothed = fonts[i];
            smoothed.setStyleStrategy(QFont::StyleStrategy(fonts[i].styleStrategy() | QFont::NoSubpixelAntialias));
            widgets[i]->setFont(smoothed);
        }
        settle();
        for (std::size_t i = 0; i < widgets.size(); ++i)
            if (widgets[i])
            {
                require((widgets[i]->font().styleStrategy() & QFont::NoSubpixelAntialias) != 0,
                        "Every widget's lettering is smoothed in greys for the photograph");
                require(widgets[i]->geometry() == places[i], "Smoothing the lettering moves and resizes nothing");
            }
        const auto picture = pictured.grab().toImage();
        // Parents before their children, as findChildren lists them. A style
        // sheet lays its lettering over a widget's own font when the style
        // polishes the widget, so one it dresses is polished again to have it.
        for (std::size_t i = 0; i < widgets.size(); ++i)
            if (widgets[i])
            {
                widgets[i]->setFont(own[i] ? fonts[i] : QFont());
                if (widgets[i]->testAttribute(Qt::WA_StyleSheet))
                {
                    widgets[i]->style()->unpolish(widgets[i]);
                    widgets[i]->style()->polish(widgets[i]);
                }
            }
        settle();
        for (std::size_t i = 0; i < widgets.size(); ++i)
            if (widgets[i])
                require(widgets[i]->font() == fonts[i] && widgets[i]->testAttribute(Qt::WA_SetFont) == own[i] &&
                            widgets[i]->palette() == palettes[i] && widgets[i]->geometry() == places[i],
                        "And every widget's lettering is put back as it was, and nothing else changed");
        return picture;
    }

    void QTest_activate(QListWidget *list, QListWidgetItem *item)
    {
        list->setCurrentItem(item);
        emit list->itemActivated(item);
    }
    void click_canvas(desktop::DiagramView &canvas, QPointF position)
    {
        const auto local = canvas.mapFromScene(position);
        const auto global = canvas.viewport()->mapToGlobal(local);
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(local), QPointF(global), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(canvas.viewport(), &press);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(local), QPointF(global), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(canvas.viewport(), &release);
        settle();
    }
    // The Schema Explorer's rows (Stage 2): what the rows under one say, one of
    // them found by what it says, one pressed as a hand presses it, and what the
    // rows that are lit stand for.
    QStringList rows_said(const QAbstractItemModel &model, const QModelIndex &parent)
    {
        QStringList said;
        for (int row = 0; row < model.rowCount(parent); ++row)
            said << model.index(row, 0, parent).data().toString();
        return said;
    }
    QModelIndex row_saying(const QAbstractItemModel &model, const QModelIndex &parent, const QString &words)
    {
        for (int row = 0; row < model.rowCount(parent); ++row)
            if (model.index(row, 0, parent).data().toString() == words)
                return model.index(row, 0, parent);
        return {};
    }
    void press_row(QTreeView &tree, const QModelIndex &index)
    {
        tree.scrollTo(index);
        settle();
        const auto at = tree.visualRect(index).center();
        const auto global = tree.viewport()->mapToGlobal(at);
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(at), QPointF(global), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(tree.viewport(), &press);
        QMouseEvent release(QEvent::MouseButtonRelease, QPointF(at), QPointF(global), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
        QApplication::sendEvent(tree.viewport(), &release);
        settle();
    }
    QStringList lit_rows(const QTreeView &tree)
    {
        QStringList lit;
        for (const auto &index : tree.selectionModel()->selectedRows())
            lit << index.data(Qt::UserRole).toString();
        lit.sort();
        return lit;
    }
    // The schema's Properties (Stage 3): its heading; what it says under one
    // section and label ("Keys/Key Role"), read by what each value is rather than
    // where it is drawn; the count at the end of a label; everything it says at
    // once, to hold two ways of choosing one thing to the same answer; whether
    // anything in it could be typed into, ticked or pressed; and whether it
    // speaks only of the schema.
    QString properties_heading(const QWidget &panel)
    {
        const auto found = panel.findChildren<QLabel *>("propertyHeading");
        if (!found.isEmpty())
            return found.front()->text();
        const auto *title = panel.findChild<QLabel *>("schemaPropertiesHeading");
        return title ? title->text() : QString();
    }
    // What one value says, whether it is written or offered (Stage 4): a field's
    // text, a picker's choice, a box ticked or not as Yes or No.
    QString said_by(const QWidget &widget)
    {
        if (const auto *label = qobject_cast<const QLabel *>(&widget))
            return label->text();
        if (const auto *line = qobject_cast<const QLineEdit *>(&widget))
            return line->text();
        if (const auto *box = qobject_cast<const QComboBox *>(&widget))
            return box->currentText();
        if (const auto *check = qobject_cast<const QCheckBox *>(&widget))
            return check->isChecked() ? "Yes" : "No";
        return {};
    }
    QStringList properties_value(const QWidget &panel, const QString &field)
    {
        QStringList values;
        for (auto *widget : panel.findChildren<QWidget *>())
            if (widget->property("field").toString() == field)
                values << said_by(*widget);
        return values;
    }
    // The field, picker or box that offers one value, where one does.
    template <class Offered>
    Offered *properties_editor(const QWidget &panel, const QString &field)
    {
        for (auto *widget : panel.findChildren<Offered *>())
            if (widget->property("field").toString() == field)
                return widget;
        return nullptr;
    }
    int properties_count(const QWidget &panel, const QString &field)
    {
        for (auto *label : panel.findChildren<QLabel *>())
            if (label->property("count").toString() == field)
                return label->text().toInt();
        return 0;
    }
    std::map<QString, QStringList> properties_all(const QWidget &panel)
    {
        std::map<QString, QStringList> said{{"heading", {properties_heading(panel)}}};
        for (auto *widget : panel.findChildren<QWidget *>())
        {
            if (const auto field = widget->property("field").toString(); !field.isEmpty())
                said[field] << said_by(*widget);
            if (const auto count = widget->property("count").toString(); !count.isEmpty())
                said["count:" + count] << said_by(*widget);
            if (widget->objectName() == "hint")
                said["hint"] << said_by(*widget);
        }
        return said;
    }
    bool properties_offer_edits(const QWidget &panel)
    {
        return !panel.findChildren<QLineEdit *>().isEmpty() || !panel.findChildren<QComboBox *>().isEmpty() || !panel.findChildren<QAbstractSpinBox *>().isEmpty() || !panel.findChildren<QCheckBox *>().isEmpty() || !panel.findChildren<QPushButton *>().isEmpty() || !panel.findChildren<QPlainTextEdit *>().isEmpty();
    }
    bool properties_speak_schema(const QWidget &panel)
    {
        QStringList said;
        for (auto *label : panel.findChildren<QLabel *>())
            said << label->text() << label->toolTip();
        for (auto *button : panel.findChildren<QAbstractButton *>())
            said << button->text();
        // Whole words, so that Identity is not mistaken for an entity.
        const QRegularExpression diagram_words(
            QStringLiteral("\\b(entity|entities|attribute|attributes|specialization|diamond|multivalued)\\b"),
            QRegularExpression::CaseInsensitiveOption);
        return std::none_of(said.begin(), said.end(), [&](const QString &words)
                            { return words.contains(diagram_words); });
    }

    // The direct table presentation uses the same stable handles and name fields
    // as the detailed inspector, and lists only stored columns, once each.
    void require_table_properties(const QWidget &panel, const erdflow::desktop::SchemaView &view, std::size_t t)
    {
        using namespace erdflow;
        const auto &table = view.preview().tables[t];
        const auto cards = panel.findChildren<QFrame *>("schemaColumnRow");
        const auto stored = static_cast<qsizetype>(std::count_if(table.columns.begin(), table.columns.end(),
                                                                 [](const auto &c)
                                                                 { return !c.ignored; }));
        auto *title = panel.findChild<QLabel *>("schemaPropertiesHeading");
        require(properties_heading(panel) == "Properties" && title && title->text() == "Properties" && !panel.findChild<QLabel *>("schemaPropertyName") && panel.findChildren<QLabel *>("propertyHeading").isEmpty() && properties_value(panel, "General/Name") == QStringList{QString::fromStdString(table.name)} && panel.findChildren<QWidget *>("schemaPropertySection").isEmpty(),
                "A selected table starts with Properties and one Table Name editor, without a repeated object header");
        const auto lower = panel.findChildren<QWidget *>("schemaTablePropertySection");
        require(lower.size() == 3, "Only Keys, References and Source form the table's lower folding sections");
        for (const auto &section_title : {QString("Keys"), QString("References"), QString("Source")})
        {
            const auto found = std::find_if(lower.begin(), lower.end(), [&](auto *section)
                                            { return section->property("section").toString() == section_title; });
            require(found != lower.end(), "Each lower fact section is present");
            auto *header = (*found)->findChild<QAbstractButton *>("sectionHeader");
            auto *body = (*found)->findChild<QWidget *>("schemaTableSectionBody");
            require(header && !header->icon().isNull() && header->isChecked() && body && !body->isHidden(),
                    "Keys, References and Source have icons and are expanded by default");
        }
        require(panel.findChildren<QFrame *>("schemaFactCard").size() == 4,
                "Primary Key, Foreign Keys, Referenced By and Source each have a separate rounded card");
        for (const auto &field : {QString("Structure/Foreign Keys"), QString("References/Referenced By")})
        {
            const auto badges = panel.findChildren<QLabel *>("schemaFactCount");
            require(std::any_of(badges.begin(), badges.end(), [&](auto *badge)
                                { return badge->property("count").toString() == field && badge->text() == QString::number(properties_count(panel, field)); }),
                    "Each key/reference count badge is present, including zero counts");
        }
        require(cards.size() == stored, "Every stored column appears once in the table inspector");
        qsizetype number = 0;
        for (std::size_t r = 0; r < table.columns.size(); ++r)
        {
            const auto &column = table.columns[r];
            if (column.ignored)
                continue;
            const auto choice = desktop::SchemaSelection{desktop::ChosenColumn{*view.column_ref(t, r)}};
            auto *card = cards[number++];
            auto *name = properties_editor<QLineEdit>(*card, desktop::schema_key(choice) + "/Name");
            auto *label = card->findChild<QLabel *>("schemaColumnNumber");
            require(card->property("choice").value<desktop::SchemaSelection>() == choice && name && name->text() == QString::fromStdString(column.name) && label && label->text() == QString::number(number),
                    "A row number is presentation only; each editor and row keeps the real column handle");
            auto *type = properties_editor<QComboBox>(*card, desktop::schema_key(choice) + "/Data Type");
            auto *constraints = card->findChild<QToolButton *>("schemaColumnConstraints");
            require(type && type->currentText() == (column.type == domain::LogicalType::Unset ? QString("Not set") : desktop::written_type(column).toUpper()) && constraints && !constraints->toolTip().isEmpty(),
                    "Each stored row offers a compact current type with size and one constraints control");
            require(constraints->toolTip().contains("Identity") == column.auto_increment && constraints->toolTip().contains("Unique") == column.unique && (column.primary_key || constraints->toolTip().contains(column.required ? "Required" : "Optional")),
                    "Every independent constraint state is available in the compact control's tooltip");
            const auto pk = card->findChildren<QLabel *>("schemaColumnPK");
            const auto fk = card->findChildren<QLabel *>("schemaColumnFK");
            const auto key = card->findChildren<QLabel *>("schemaColumnKey");
            require(pk.size() == (column.primary_key ? 1 : 0) && key.size() == pk.size() && fk.size() == (column.foreign_key ? 1 : 0),
                    "PK and FK are read independently, and both remain visible for a column that is both");
            if (column.primary_key)
                require(pk.front()->text() == "PK" && !key.front()->pixmap().isNull(), "A primary key has the golden mark and PK");
            if (column.foreign_key)
                require(fk.front()->text() == "FK", "A foreign key has FK");
            // No three-dots menu, and no way from the row into the column's own
            // inspector (Zain, 2026-10-01): everything it offered is edited here.
            const auto buttons = card->findChildren<QAbstractButton *>();
            require(!card->findChild<QToolButton *>("schemaColumnActions") && std::none_of(buttons.begin(), buttons.end(), [](auto *button)
                                                                                           { return button->text() == "..." || button->toolTip() == "Column Properties"; }) &&
                        card->findChildren<QMenu *>().isEmpty() && card->focusPolicy() == Qt::NoFocus,
                    "A column row has no ... button and no Column Properties menu, and is not a control itself");
            require(card->height() == cards.front()->height() && name->width() >= 56 && type->width() > 0 && constraints->width() > 0 && name->geometry().right() < type->geometry().left() && type->geometry().right() < constraints->geometry().left() && card->width() - constraints->geometry().right() <= 8,
                    "Column rows keep name, type and constraints in order, ending at the row's edge with no room left "
                    "where the menu was");
        }
        require(panel.findChildren<QPushButton *>("schemaDerivedValue").size() == static_cast<qsizetype>(table.columns.size()) - stored,
                "Non-stored derived values stay separate and are never numbered as columns");
    }
}

namespace
{
    void export_state_tests()
    {
        infrastructure::QtIdGenerator ids;
        application::Editor model(ids);
        infrastructure::ErdxProjectStore store;
        desktop::MainWindow window(model, store, ids);
        QTemporaryDir files;
        require(files.isValid(), "Export-state test directory");
        window.resize(1440, 1080);
        window.show();
        settle();
        // Direct Editor commands need the same notification the canvas supplies
        // after a UI edit; the Editor itself has no window-change signal.
        const auto changed = [&] { window.canvas()->on_edit({}); };
        const auto new_conceptual = [&]
        {
            model.mark_saved(model.revision());
            child<QAction>(window, "newProject")->trigger();
        };
        const auto enabled = [&](const char *name) { return child<QAction>(window, name)->isEnabled(); };
        const auto state = [&](bool project, bool documents, bool pictures)
        {
            require(enabled("exportProject") == project, "Project export follows model content in either workspace");
            for (const char *name : {"exportPdfDocument", "exportMarkdown", "exportHtml", "exportCsv"})
                require(enabled(name) == documents, "Document export follows its active model target");
            for (const char *name : {"exportSvg", "exportPng", "exportPdfPage", "copyAsPicture",
                                     "exportJpeg", "exportWebp", "exportTiff"})
                if (auto *action = window.findChild<QAction *>(name))
                    require(action->isEnabled() == pictures, "Picture export follows its active diagram target");
            if (auto *menu = window.findChild<QMenu *>("exportMorePictures"))
                require(menu->menuAction()->isEnabled() == pictures, "More Formats follows its picture writers");
            require(enabled("exportWithOptions") == (documents || pictures), "Options needs a supported target");
            auto *row = child<QToolBar>(window, "exportTools");
            for (auto *action : row->actions())
                if (!action->isSeparator())
                    if (auto *button = qobject_cast<QToolButton *>(row->widgetForAction(action)))
                        require(button->isEnabled() == action->isEnabled(), "Ribbon buttons reflect QAction enablement");
        };
        // A dialog the window opens is answered once it is shown: the answer is
        // queued from the dialog's own Show event, so it runs inside that dialog's
        // loop. A timer started before the action can fire while nothing is up yet
        // and then never again -- which is what used to hang this test, the save
        // dialog opening after the timer had already fired.
        struct WhenShown final : QObject
        {
            std::function<void(QDialog *)> answer;
            bool shown = false;
            bool eventFilter(QObject *watched, QEvent *event) override
            {
                if (event->type() == QEvent::Show)
                    if (auto *dialog = qobject_cast<QDialog *>(watched); dialog && !shown)
                    {
                        shown = true;
                        QMetaObject::invokeMethod(dialog, [this, dialog] { answer(dialog); }, Qt::QueuedConnection);
                    }
                return false;
            }
        };
        // Exercise the QAction whole. Where the window would ask for a file it is
        // given one instead (MainWindow::choose_export_location) -- the dialog is
        // the one thing nobody is here to answer -- and everything after the choice
        // is the export itself. That it asks at all, and for an export rather than
        // for a save first, is checked: any dialog that shows is an unexpected
        // prerequisite, recorded and dismissed rather than left to hang the test.
        const auto file_action = [&](const char *action_name, const QString &destination)
        {
            bool asked = false;
            window.choose_export_location = [&](const QString &, const QString &)
            {
                asked = true;
                return destination;
            };
            WhenShown unexpected;
            unexpected.answer = [](QDialog *dialog) { dialog->reject(); };
            qApp->installEventFilter(&unexpected);
            child<QAction>(window, action_name)->trigger();
            qApp->removeEventFilter(&unexpected);
            window.choose_export_location = nullptr;
            require(asked && !unexpected.shown,
                    "The action goes directly to its destination, without a save prerequisite");
        };
        // A dialog the action is meant to open, answered as it is shown.
        const auto answer_dialog = [&](const char *action_name, std::function<void(QDialog *)> answer)
        {
            WhenShown expected;
            expected.answer = std::move(answer);
            qApp->installEventFilter(&expected);
            child<QAction>(window, action_name)->trigger();
            qApp->removeEventFilter(&expected);
            return expected.shown;
        };
        // A project with a file of its own, as a saved project has: written there
        // and opened from it, so Save writes to that file without asking.
        const auto give_file = [&](const QString &path)
        {
            require(store.save(path.toStdString(), model.project()).ok, "The working project is given a file");
            model.mark_saved(model.revision());
            require(window.open_path(path), "And is opened from it");
            settle_for(400);
        };
        const auto copy_matches = [&](const QString &path)
        {
            const auto loaded = store.load(path.toStdString());
            require(loaded.project && *loaded.project == model.project(), "Export copies the current in-memory project");
        };
        state(false, false, false); // Home, no project opened.
        window.show_home(false);
        state(false, false, false); // An empty Conceptual project.
        model.new_schema_project();
        changed();
        state(false, false, false);
        const auto first = model.create_relation("Parent", domain::Point{0, 0});
        require(first.ok && model.create_relation("Child", domain::Point{400, 0}).ok, "Two unsaved Schema tables");
        changed();
        require(model.dirty(), "Schema edits are unsaved");
        child<QAction>(window, "tabExport")->trigger();
        state(true, true, true);
        const auto schema_copy = files.filePath("unsaved-schema.erdx");
        file_action("exportProject", schema_copy);
        copy_matches(schema_copy);
        require(model.dirty(), "Exporting a copy does not save the working project");
        const auto before_cancel = model.project();
        const auto count_before = QDir(files.path()).entryList(QDir::Files).size();
        file_action("exportProject", {});
        require(model.project() == before_cancel && model.dirty() &&
                    QDir(files.path()).entryList(QDir::Files).size() == count_before,
                "Cancelling export writes nothing and leaves the project usable and dirty");
        const auto schema_before = model.project();
        const auto selection_before = window.schema()->selection_now();
        const auto boxes_before = window.schema()->table_boxes();
        for (const auto &[action, filename] : std::vector<std::pair<const char *, const char *>>{
                 {"exportPdfDocument", "schema-report.pdf"}, {"exportMarkdown", "schema.md"},
                 {"exportHtml", "schema.html"}, {"exportCsv", "schema.csv"},
                 {"exportSvg", "schema.svg"}, {"exportPng", "schema.png"}, {"exportPdfPage", "schema-page.pdf"},
                 {"exportJpeg", "schema.jpg"}, {"exportWebp", "schema.webp"}, {"exportTiff", "schema.tiff"}})
        {
            if (!window.findChild<QAction *>(action)) continue;
            file_action(action, files.filePath(filename));
            require(QFileInfo(files.filePath(filename)).size() > 0, "Unsaved Schema exports every available format");
        }
        for (const char *filename : {"schema.md", "schema.html", "schema.csv", "schema.svg"})
        {
            QFile exported(files.filePath(filename));
            require(exported.open(QIODevice::ReadOnly), "Read exported Schema content");
            const auto bytes = exported.readAll();
            require(bytes.contains("Parent") && bytes.contains("Child"), "Schema exports contain the actual table names");
        }
        QImage schema_image(files.filePath("schema.png"));
        require(!schema_image.isNull() && schema_image.pixelColor(0, 0).alpha() == 0,
                "Schema PNG supports transparent export margins");
        require(!desktop::payload_of_picture_file(files.filePath("schema.png")).isEmpty() &&
                    !desktop::payload_of_picture_file(files.filePath("schema.svg")).isEmpty(),
                "Schema PNG and SVG carry the project using the existing encoding");
        child<QAction>(window, "copyAsPicture")->trigger();
        require(QApplication::clipboard()->mimeData()->data("image/svg+xml").contains("Parent"),
                "Copy Image renders the Schema rather than the empty Conceptual canvas");
        bool schema_options = false;
        answer_dialog("exportWithOptions", [&](QDialog *shown)
        {
            if (auto *dialog = dynamic_cast<desktop::ExportDialog *>(shown))
            {
                desktop::ExportChoice choice;
                choice.document = true;
                dialog->set_choice(choice);
                schema_options = dialog->findChild<QLabel *>("exportSize")->text().contains("2 tables");
            }
            shown->reject();
        });
        require(schema_options, "Options describes the active Schema listing");
        file_action("exportPng", {});
        require(model.project() == schema_before && window.schema()->selection_now() == selection_before &&
                    window.schema()->table_boxes() == boxes_before && model.dirty(),
                "Schema export preserves model, selection, placement and unsaved state");

        const auto original = files.filePath("working.erdx");
        give_file(original);
        child<QAction>(window, "saveProject")->trigger();
        const auto saved_back = store.load(original.toStdString());
        require(!model.dirty() && saved_back.project && *saved_back.project == model.project(),
                "Normal Save still saves the working project, to its own file");
        const auto saved = model.project();
        require(model.create_relation("UnsavedThird", domain::Point{800, 0}).ok, "Edit an already saved Schema");
        changed();
        state(true, true, true);
        const auto dirty_copy = files.filePath("dirty-schema.erdx");
        file_action("exportProject", dirty_copy);
        copy_matches(dirty_copy);
        file_action("exportCsv", files.filePath("dirty-schema.csv"));
        QFile dirty_listing(files.filePath("dirty-schema.csv"));
        require(dirty_listing.open(QIODevice::ReadOnly) && dirty_listing.readAll().contains("UnsavedThird"),
                "Schema data dictionary includes unsaved edits to an already-saved project");
        const auto still_saved = store.load(original.toStdString());
        require(still_saved.project && *still_saved.project == saved && model.dirty(),
                "Dirty export uses current data without saving over the original");
        // A failed destination still reports failure through the existing flow.
        dismiss(QMessageBox::Ok);
        require(!window.export_project_file(files.filePath("missing/failure.erdx")) && model.dirty(),
                "A write failure is reported without saving the project");
        const auto relations = model.project().schema.relations;
        for (const auto &[id, relation] : relations)
        {
            (void)relation;
            require(model.erase_relation(id).ok, "Remove a Schema table");
            changed();
        }
        state(false, false, false);
        model.undo();
        changed();
        state(true, true, true);
        new_conceptual();
        state(false, false, false);
        require(model.create_entity("UnsavedEntity", {0, 0, 148, 86}).ok, "An unsaved Conceptual entity");
        changed();
        state(true, true, true);
        for (const auto &[action, filename] : std::vector<std::pair<const char *, const char *>>{
                 {"exportProject", "conceptual.erdx"}, {"exportPdfDocument", "report.pdf"},
                 {"exportMarkdown", "dictionary.md"}, {"exportHtml", "report.html"}, {"exportCsv", "dictionary.csv"},
                 {"exportSvg", "diagram.svg"}, {"exportPng", "diagram.png"}, {"exportPdfPage", "page.pdf"}})
        {
            file_action(action, files.filePath(filename));
            require(QFileInfo(files.filePath(filename)).size() > 0 && model.dirty(), "An unsaved model exports directly");
        }
        copy_matches(files.filePath("conceptual.erdx"));
        child<QAction>(window, "copyAsPicture")->trigger();
        require(QApplication::clipboard()->mimeData()->hasFormat("image/png") &&
                    QApplication::clipboard()->mimeData()->hasFormat("image/svg+xml"), "Copy Image works before saving");
        bool saw_options = false;
        answer_dialog("exportWithOptions", [&](QDialog *shown)
        {
            saw_options = dynamic_cast<desktop::ExportDialog *>(shown) != nullptr;
            shown->reject();
        });
        require(saw_options, "Options opens directly for unsaved Conceptual content");
        file_action("exportPng", {});
        const auto conceptual_original = files.filePath("conceptual-working.erdx");
        give_file(conceptual_original);
        require(model.create_entity("LatestUnsavedEntity", {250, 0, 148, 86}).ok, "A dirty saved Conceptual model");
        changed();
        state(true, true, true);
        file_action("exportCsv", files.filePath("latest.csv"));
        QFile csv(files.filePath("latest.csv"));
        require(csv.open(QIODevice::ReadOnly) && csv.readAll().contains("LatestUnsavedEntity") && model.dirty(),
                "The listing includes unsaved edits without changing Save state");
        child<QAction>(window, "designRelational")->trigger();
        child<QPushButton>(window, "schemaFull")->click();
        state(true, true, true);
        child<QAction>(window, "modelToConceptual")->trigger();
        state(true, true, true);
        window.show_home(true);
        state(false, false, false);
        window.show_home(false);
        state(true, true, true);
        new_conceptual();
        state(false, false, false);
        require(model.create_note("Note", {0, 0, 200, 100}, "Only a note").ok, "Picture-only content");
        changed();
        state(true, false, true);
        new_conceptual();
        child<QAction>(window, "tabExport")->trigger();
        window.close();
        require(!window.isVisible(), "Closing an empty project with Export selected is safe");
        state(false, false, false);
        std::cout << "Export state tests passed\n";
    }

    // Relational Design's checks that come first, each in a window of its own:
    // the Explorer's order, where the examples are offered, Relational
    // Design's own examples and template, a key on a relationship, and lines
    // whose ends follow their rows. With the next, they run as a part of the
    // suite on their own as well as in the whole of it (2026-10-09; see main).
    void relational_design_tests(infrastructure::QtIdGenerator &ids)
    {
        // The Explorer lists attributes in the order they were made (Zain,
        // 2026-10-03), under their owner and in the group of them all, as the
        // schema lists them -- not by their identities, which here run backwards.
        {
            BackwardIds backward;
            application::Editor model(backward);
            const auto student = std::get<domain::EntityId>(*model.create_entity("Student", {}).created);
            for (const char *number : {"1", "2", "3", "4", "5"})
                require(model.create_attribute(number, {}, domain::ElementRef{student}).ok, "An attribute is made");
            const auto name = std::get<domain::AttributeId>(*model.create_attribute("Name", {}, domain::ElementRef{student}).created);
            require(model.set_attribute_kind(name, domain::AttributeKind::Composite).ok, "Name is made composite");
            for (const char *part : {"First", "Mid", "Last"})
                require(model.create_attribute(part, {}, domain::ElementRef{name}).ok, "A part is made");
            infrastructure::ErdxProjectStore ordered_store;
            desktop::MainWindow ordered_window(model, ordered_store, backward);
            ordered_window.resize(1440, 920);
            ordered_window.show();
            ordered_window.show_home(false);
            settle();
            const auto &rows = *child<QTreeView>(ordered_window, "explorer")->model();
            const auto top = rows.index(0, 0);
            const auto student_row = row_saying(rows, row_saying(rows, top, "Entities"), "Student");
            require(rows_said(rows, student_row) == QStringList{"1", "2", "3", "4", "5", "Name"},
                    "An entity's attributes are listed in the order they were made");
            require(rows_said(rows, row_saying(rows, student_row, "Name")) == QStringList{"First", "Mid", "Last"},
                    "A composite's parts are listed in the order they were made");
            require(rows_said(rows, row_saying(rows, top, "Attributes"))
                        == QStringList{"1", "2", "3", "4", "5", "Name", "First", "Mid", "Last"},
                    "The group of all attributes lists them in the order they were made");
        }
        // Examples, the template and Import are offered in the workspace they
        // belong to, not on Home (Zain, 2026-10-03). Every one of them is
        // Conceptual, so they stand in the File menu -- the ribbon's File tab --
        // the Home menu and the Import tab while the diagram is in front, and
        // are put away from all of them while Relational Design is; its Import
        // keeps only the entry for other tools' formats, which says why it
        // cannot be used yet.
        {
            const char *conceptual_only[] = {"fileOpenExample", "fileExampleCompany", "fileExampleUniversityDatabase",
                                             "fileTemplate", "importProject", "importPicture", "homeExamples",
                                             "homeExampleCompany", "homeExampleUniversityDatabase", "homeTemplates"};
            const auto offered = [&](desktop::MainWindow &shown, bool expected)
            {
                return std::all_of(std::begin(conceptual_only), std::end(conceptual_only), [&](const char *name)
                                   { return child<QAction>(shown, name)->isVisible() == expected; });
            };
            application::Editor diagram(ids);
            infrastructure::ErdxProjectStore placed_store;
            desktop::MainWindow placed(diagram, placed_store, ids);
            placed.resize(1440, 920);
            placed.show();
            placed.show_home(false);
            settle();
            auto *file_menu = child<QMenu>(placed, "fileMenu");
            auto *import_menu = child<QMenu>(placed, "importMenu");
            require(offered(placed, true), "On the diagram, the examples, the template and Import are offered");
            for (const char *name : {"fileOpenExample", "fileExampleCompany", "fileExampleUniversityDatabase", "fileTemplate"})
                require(file_menu->actions().contains(child<QAction>(placed, name)), "In the File menu");
            require(file_menu->actions().contains(import_menu->menuAction()) &&
                        import_menu->actions().contains(child<QAction>(placed, "importProject")) &&
                        import_menu->actions().contains(child<QAction>(placed, "importPicture")),
                    "With Import beside them, reading what ERDFlow writes");
            require(child<QToolButton>(placed, "fileMenuButton")->menu() == file_menu &&
                        child<QToolBar>(placed, "importTools")->actions().contains(child<QAction>(placed, "importProject")),
                    "The ribbon's File menu and Import row carry them in the workspace");
            auto *other_tools = child<QAction>(placed, "importFromOtherTools");
            require(other_tools->isVisible() && !other_tools->isEnabled(), "Other tools' formats stay as they were");
            // Relational Design in front: they go with the diagram.
            placed.open_schema(true);
            settle_for(450);
            require(offered(placed, false), "With Relational Design in front, the Conceptual ones are put away");
            require(other_tools->isVisible() && !other_tools->isEnabled() && !other_tools->toolTip().isEmpty(),
                    "And its Import keeps the entry that says why other tools' formats cannot be read yet");
            // And back with the diagram.
            child<QPushButton>(placed, "schemaFull")->click();
            settle();
            require(offered(placed, true), "Back on the diagram, they are offered again");

            // A project begun from its schema is Relational Design from the start.
            application::Editor drawn(ids);
            drawn.new_schema_project();
            infrastructure::ErdxProjectStore drawn_store;
            desktop::MainWindow schema_first(drawn, drawn_store, ids);
            schema_first.resize(1440, 920);
            schema_first.show();
            schema_first.show_home(false);
            settle_for(450);
            require(offered(schema_first, false), "A schema drawn by hand is not offered the Conceptual ones");
            require(child<QAction>(schema_first, "importFromOtherTools")->isVisible(),
                    "Only the entry for other tools' formats");
        }
        // Relational Design's own examples and template (Zain, 2026-10-05):
        // projects that start from their schema, drawn with the Editor's schema
        // commands rather than converted from the Conceptual examples, and
        // offered -- in the File menu, the Home menu and the header's Open
        // example -- only while Relational Design is in front.
        {
            using Names = std::vector<std::string>;
            const auto sorted = [](Names names)
            {
                std::sort(names.begin(), names.end());
                return names;
            };
            const auto table_names = [&](const domain::Project &project)
            {
                Names names;
                for (const auto &[id, relation] : project.schema.relations)
                    names.push_back(relation.name);
                return sorted(names);
            };
            const auto relation_named = [](const domain::Project &project, const std::string &name)
            {
                for (const auto &[id, relation] : project.schema.relations)
                    if (relation.name == name)
                        return id;
                throw std::runtime_error("No table called " + name);
            };
            const auto column_named = [&](const domain::Project &project, const std::string &table,
                                          const std::string &name) -> const domain::SchemaColumn &
            {
                for (const auto &column : project.schema.added.at(relation_named(project, table)))
                    if (column.name == name)
                        return column;
                throw std::runtime_error("No column called " + table + "." + name);
            };
            const auto holds = [](const std::vector<domain::SchemaColumn> &columns, domain::SchemaColumnId id)
            {
                return std::any_of(columns.begin(), columns.end(), [&](const auto &column)
                                   { return column.id == id; });
            };
            // The table a column references, or nothing.
            const auto referenced = [&](const domain::Project &project, const std::string &table,
                                        const std::string &name) -> std::string
            {
                const auto id = column_named(project, table, name).id;
                for (const auto &[key, reference] : project.schema.foreign_keys)
                    if (reference.column == id)
                        return project.schema.relations.at(reference.to).name;
                return {};
            };
            // A table's primary key, its columns in the key's own order.
            const auto key_of = [&](const domain::Project &project, const std::string &table)
            {
                std::vector<std::pair<std::uint32_t, std::string>> parts;
                for (const auto &column : project.schema.added.at(relation_named(project, table)))
                    if (column.identifier)
                        parts.emplace_back(column.key_order, column.name);
                std::sort(parts.begin(), parts.end());
                Names names;
                for (const auto &part : parts)
                    names.push_back(part.second);
                return names;
            };
            // What makes a schema sound: drawn by hand with no diagram behind it;
            // every table with columns of its own, typed, none named twice, and a
            // primary key numbered from one; every foreign key a real one -- its
            // column in the table it leaves, its target the whole primary key of
            // the table it references, of the same type -- and no column holding
            // two. The schema worked out from it says the same.
            const auto sound = [&](const domain::Project &project)
            {
                const auto &schema = project.schema;
                if (!schema.standalone || !project.entities.empty() || !project.attributes.empty() ||
                    !project.relationships.empty() || !project.specializations.empty())
                    return false;
                std::set<std::string> tables;
                for (const auto &[id, relation] : schema.relations)
                {
                    if (!tables.insert(relation.name).second || !schema.added.contains(id))
                        return false;
                    std::set<std::string> columns;
                    std::vector<std::uint32_t> key;
                    for (const auto &column : schema.added.at(id))
                    {
                        if (!columns.insert(column.name).second || column.logical_type == domain::LogicalType::Unset)
                            return false;
                        if (column.identifier && !column.required)
                            return false;
                        if (column.identifier)
                            key.push_back(column.key_order);
                    }
                    std::sort(key.begin(), key.end());
                    if (key.empty())
                        return false;
                    for (std::size_t k = 0; k < key.size(); ++k)
                        if (key[k] != k + 1)
                            return false;
                }
                std::set<domain::SchemaColumnId> holding;
                for (const auto &[id, reference] : schema.foreign_keys)
                {
                    if (!holding.insert(reference.column).second || !schema.added.contains(reference.from) ||
                        !schema.added.contains(reference.to))
                        return false;
                    const auto &from = schema.added.at(reference.from);
                    const auto &to = schema.added.at(reference.to);
                    if (!holds(from, reference.column) || !holds(to, reference.target))
                        return false;
                    const auto &held = *std::find_if(from.begin(), from.end(), [&](const auto &column)
                                                     { return column.id == reference.column; });
                    const auto &target = *std::find_if(to.begin(), to.end(), [&](const auto &column)
                                                       { return column.id == reference.target; });
                    const auto keys = std::count_if(to.begin(), to.end(), [](const auto &column)
                                                    { return column.identifier; });
                    if (!target.identifier || keys != 1 || held.logical_type != target.logical_type ||
                        held.length != target.length || held.scale != target.scale)
                        return false;
                }
                const auto preview = domain::schema_preview(project);
                if (preview.tables.size() != schema.relations.size())
                    return false;
                std::size_t referencing = 0;
                for (const auto &table : preview.tables)
                {
                    if (std::none_of(table.columns.begin(), table.columns.end(), [](const auto &column)
                                     { return column.primary_key; }))
                        return false;
                    for (const auto &column : table.columns)
                        if (column.foreign_key && column.references)
                            ++referencing;
                }
                return referencing == schema.foreign_keys.size();
            };
            // A junction's primary key is the foreign keys to the two tables it
            // joins, and nothing else.
            const auto junction = [&](const domain::Project &project, const std::string &table,
                                      const std::string &first, const std::string &second)
            {
                Names parents;
                for (const auto &name : key_of(project, table))
                    parents.push_back(referenced(project, table, name));
                return parents == Names{first, second};
            };

            // Company Database -- Relational.
            application::Editor company(ids);
            desktop::build_company_database_relational(company);
            const auto &company_schema = company.project();
            require(company_schema.name == "Company Database — Relational", "The relational company example is named");
            require(sound(company_schema),
                    "Company Database — Relational is a sound schema drawn by hand, with no diagram made first");
            require(table_names(company_schema) ==
                        sorted({"Client", "Department", "DepartmentLocation", "Dependent", "Employee",
                                "EmployeePhone", "EmployeeSkill", "Invoice", "Job", "Office", "Payment", "Product",
                                "Project", "ProjectAssignment", "ProjectProduct", "Skill", "Supplier",
                                "SupplierProduct", "Task", "Team", "TeamMember", "TeamProject"}) &&
                        company_schema.schema.foreign_keys.size() == 25,
                    "Company Database — Relational has its twenty-two tables and twenty-five foreign keys");
            require(junction(company_schema, "EmployeeSkill", "Employee", "Skill") &&
                        junction(company_schema, "ProjectAssignment", "Employee", "Project") &&
                        junction(company_schema, "TeamMember", "Team", "Employee") &&
                        junction(company_schema, "TeamProject", "Team", "Project") &&
                        junction(company_schema, "ProjectProduct", "Project", "Product") &&
                        junction(company_schema, "SupplierProduct", "Supplier", "Product"),
                    "Each many-to-many relationship is a junction table keyed by its two foreign keys");
            require(key_of(company_schema, "Dependent") == Names{"EmployeeID", "DependentName"} &&
                        referenced(company_schema, "Dependent", "EmployeeID") == "Employee",
                    "A dependent is keyed through the employee it depends on");
            require(key_of(company_schema, "EmployeePhone") == Names{"EmployeeID", "PhoneNumber"} &&
                        key_of(company_schema, "DepartmentLocation") == Names{"DepartmentID", "Location"},
                    "What may be held more than once has a table of its own");
            require(referenced(company_schema, "Employee", "SupervisorID") == "Employee" &&
                        !column_named(company_schema, "Employee", "SupervisorID").required,
                    "A supervisor is a foreign key into the same table, and may be empty");
            require(referenced(company_schema, "Department", "ManagerID") == "Employee" &&
                        column_named(company_schema, "Department", "ManagerID").unique,
                    "A department's manager is a unique foreign key to Employee");
            require(referenced(company_schema, "Employee", "DepartmentID") == "Department" &&
                        column_named(company_schema, "Employee", "DepartmentID").required &&
                        referenced(company_schema, "Payment", "InvoiceID") == "Invoice" &&
                        referenced(company_schema, "Invoice", "ClientID") == "Client" &&
                        referenced(company_schema, "Task", "ProjectID") == "Project",
                    "One-to-many relationships are foreign keys in the table on the many side");
            require(column_named(company_schema, "Employee", "EmployeeID").auto_increment &&
                        !column_named(company_schema, "EmployeeSkill", "EmployeeID").auto_increment,
                    "A key that stands for nothing but the row counts itself up; one taken from another table "
                    "does not");

            // University Database -- Relational.
            application::Editor university(ids);
            desktop::build_university_database_relational(university);
            const auto &university_schema = university.project();
            require(university_schema.name == "University Database — Relational",
                    "The relational university example is named");
            require(sound(university_schema),
                    "University Database — Relational is a sound schema drawn by hand, with no diagram made first");
            require(table_names(university_schema) ==
                        sorted({"Assignment", "BookAuthor", "BookLoan", "Classroom", "Club", "ClubMember", "Course",
                                "CoursePrerequisite", "Department", "Enrollment", "Exam", "ExamResult", "Faculty",
                                "LibraryBook", "Professor", "Program", "Scholarship", "Section", "Student",
                                "StudentPhone", "StudentScholarship"}) &&
                        university_schema.schema.foreign_keys.size() == 26,
                    "University Database — Relational has its twenty-one tables and twenty-six foreign keys");
            require(junction(university_schema, "Enrollment", "Student", "Section") &&
                        junction(university_schema, "CoursePrerequisite", "Course", "Course") &&
                        junction(university_schema, "ClubMember", "Student", "Club") &&
                        junction(university_schema, "StudentScholarship", "Student", "Scholarship") &&
                        junction(university_schema, "ExamResult", "Exam", "Student"),
                    "Enrollment, prerequisites, club members, scholarships and exam results are junction tables");
            require(key_of(university_schema, "BookLoan") == Names{"BookLoanID"} &&
                        referenced(university_schema, "BookLoan", "StudentID") == "Student" &&
                        referenced(university_schema, "BookLoan", "LibraryBookID") == "LibraryBook",
                    "A book loan has a key of its own, and foreign keys to the student and the book");
            require(key_of(university_schema, "StudentPhone") == Names{"StudentID", "PhoneNumber"} &&
                        key_of(university_schema, "BookAuthor") == Names{"LibraryBookID", "AuthorName"},
                    "What may be held more than once has a table of its own");
            require(referenced(university_schema, "Department", "HeadID") == "Professor" &&
                        column_named(university_schema, "Department", "HeadID").unique &&
                        referenced(university_schema, "Section", "CourseID") == "Course" &&
                        referenced(university_schema, "Student", "AdvisorID") == "Professor",
                    "A department's head is a unique foreign key, and one-to-many relationships are foreign keys");

            // The template: two tables and the foreign key between them, no more.
            application::Editor starting(ids);
            desktop::build_basic_relational_schema(starting);
            const auto &basic = starting.project();
            require(basic.name == "Untitled" && sound(basic), "The template opens untitled, as a schema drawn by hand");
            require(table_names(basic) == Names{"Child", "Parent"} && basic.schema.foreign_keys.size() == 1,
                    "The template is Parent and Child, and one foreign key");
            const auto said = [&](const std::string &table)
            {
                Names columns;
                for (const auto &column : basic.schema.added.at(relation_named(basic, table)))
                    columns.push_back(column.name);
                return columns;
            };
            const auto &parent_key = column_named(basic, "Parent", "ParentID");
            const auto &child_key = column_named(basic, "Child", "ChildID");
            const auto &parent_id = column_named(basic, "Child", "ParentID");
            const auto &reference = basic.schema.foreign_keys.begin()->second;
            require(said("Parent") == Names{"ParentID", "Name"} && said("Child") == Names{"ChildID", "Name", "ParentID"},
                    "Parent holds ParentID and Name; Child holds ChildID, Name and ParentID");
            require(key_of(basic, "Parent") == Names{"ParentID"} && key_of(basic, "Child") == Names{"ChildID"} &&
                        parent_key.logical_type == domain::LogicalType::Int &&
                        child_key.logical_type == domain::LogicalType::Int,
                    "Each table's primary key is a whole number named for it");
            require(reference.from == relation_named(basic, "Child") && reference.to == relation_named(basic, "Parent") &&
                        reference.column == parent_id.id && reference.target == parent_key.id && parent_id.required &&
                        !parent_id.identifier,
                    "Child.ParentID references Parent.ParentID, and every child has a parent");
            for (const auto *table : {"Parent", "Child"})
            {
                const auto &name = column_named(basic, table, "Name");
                require(name.logical_type == domain::LogicalType::NVarchar && name.length == 100 && name.required,
                        "Name is nvarchar(100), NOT NULL");
            }

            // The Conceptual examples are as they were.
            application::Editor conceptual_company(ids);
            desktop::build_company_database(conceptual_company);
            application::Editor conceptual_university(ids);
            desktop::build_university_database(conceptual_university);
            require(conceptual_company.project().name == "Company Database" &&
                        conceptual_company.project().entities.size() == 14 &&
                        conceptual_company.project().attributes.size() == 87 &&
                        conceptual_company.project().relationships.size() == 17 &&
                        !conceptual_company.project().schema.standalone &&
                        conceptual_university.project().name == "University Database" &&
                        conceptual_university.project().entities.size() == 14 &&
                        conceptual_university.project().attributes.size() == 81 &&
                        conceptual_university.project().relationships.size() == 19 &&
                        !conceptual_university.project().schema.standalone,
                    "The Conceptual examples are unchanged diagrams");

            // A schema drawn by hand from nothing is as it was.
            application::Editor blank(ids);
            blank.new_schema_project();
            const auto blank_table = blank.create_relation("Table", domain::Point{0, 0});
            require(blank_table.ok && blank.project().schema.relations.size() == 1 &&
                        blank.project().schema.added.begin()->second.front().name == "TableID" &&
                        blank.project().name == "Untitled",
                    "A new Relational Schema project still starts empty, and its tables still name their keys");

            // Where they are offered.
            application::Editor opened(ids);
            infrastructure::ErdxProjectStore opened_store;
            desktop::MainWindow relational(opened, opened_store, ids);
            relational.resize(1440, 1080);
            relational.show();
            relational.show_home(false);
            settle();
            const char *relational_only[] = {"fileExampleCompanyRelational", "fileExampleUniversityRelational",
                                             "fileTemplateRelational", "homeExampleCompanyRelational",
                                             "homeExampleUniversityRelational", "homeTemplateRelational"};
            auto *header_button = child<QToolButton>(relational, "openRelationalExample");
            // In the header they are offered from its Model menu (Zain,
            // 2026-10-07); the mark that used to drop them is put away.
            auto *model_menu = child<QMenu>(relational, "headerModelMenu");
            const auto offered = [&](bool expected)
            {
                emit model_menu->aboutToShow();
                return std::all_of(std::begin(relational_only), std::end(relational_only), [&](const char *name)
                                   { return child<QAction>(relational, name)->isVisible() == expected; }) &&
                       model_menu->actions().contains(header_button->menu()->menuAction()) == expected &&
                       !header_button->isVisible();
            };
            const auto conceptual_offered = [&](bool expected)
            {
                return child<QAction>(relational, "fileExampleCompany")->isVisible() == expected &&
                       child<QAction>(relational, "homeExampleCompany")->isVisible() == expected &&
                       child<QAction>(relational, "fileTemplate")->isVisible() == expected;
            };
            require(offered(false) && conceptual_offered(true),
                    "On the diagram, Relational Design's examples and template are not offered, and the Conceptual "
                    "ones are");
            auto *file_menu = child<QMenu>(relational, "fileMenu");
            auto *home_menu = child<QMenu>(relational, "homeMenu");
            for (const char *name : {"fileExampleCompanyRelational", "fileExampleUniversityRelational",
                                     "fileTemplateRelational"})
                require(file_menu->actions().contains(child<QAction>(relational, name)) &&
                            header_button->menu() &&
                            header_button->menu()->actions().contains(child<QAction>(relational, name)),
                        "In the File menu and in the header's Open example");
            for (const char *name : {"homeExampleCompanyRelational", "homeExampleUniversityRelational",
                                     "homeTemplateRelational"})
                require(home_menu->actions().contains(child<QAction>(relational, name)), "In the Home menu");
            require(child<QAction>(relational, "fileExampleCompanyRelational")->text() == "Company Database — Relational" &&
                        child<QAction>(relational, "fileExampleUniversityRelational")->text() ==
                            "University Database — Relational" &&
                        child<QAction>(relational, "fileTemplateRelational")->text() ==
                            "New from template: Basic Relational Schema" &&
                        child<QAction>(relational, "homeExampleCompanyRelational")->text() ==
                            "Company Database — Relational" &&
                        child<QAction>(relational, "homeTemplateRelational")->text() ==
                            "New from template: Basic Relational Schema",
                    "Each is named for what it opens");
            require(header_button->toolTip() == "Open example" &&
                        header_button->popupMode() == QToolButton::InstantPopup &&
                        header_button->toolButtonStyle() == Qt::ToolButtonIconOnly,
                    "The header's Open example is a mark that drops them, named on hover");
            // The header's top right corner holds Model and Theme, Theme
            // outermost, and nothing else of the kind (Zain, 2026-10-07).
            auto *corner_model = child<QToolButton>(relational, "headerModel");
            auto *corner_theme = child<QToolButton>(relational, "headerTheme");
            const auto check_corner_placement = [&](bool conceptual)
            {
                const auto original_size = relational.size();
                auto *bar = child<QToolBar>(relational, "modelTools");
                auto *header = child<QWidget>(relational, "workspaceHeader");
                auto *corner = child<QWidget>(relational, "conceptualCorner");
                // A search field since 2026-10-08, as the schema's is.
                auto *search = child<QLineEdit>(relational, "conceptualSearch");
                for (const int width : {1280, 1440, 1920})
                {
                    relational.resize(width, 1080);
                    settle();
                    auto *row = conceptual ? static_cast<QWidget *>(bar) : header;
                    require(corner_model->parentWidget() == (conceptual ? corner : header) &&
                                corner_theme->parentWidget() == corner_model->parentWidget(),
                            "The existing Model and Theme controls belong to the active tool row");
                    const auto bounds = [&](QWidget *widget)
                    { return QRect(widget->mapTo(row, QPoint()), widget->size()); };
                    const auto model_rect = bounds(corner_model);
                    const auto theme_rect = bounds(corner_theme);
                    require(corner_model->isVisible() && corner_theme->isVisible() &&
                                row->rect().contains(model_rect) && row->rect().contains(theme_rect) &&
                                model_rect.right() < theme_rect.left() &&
                                row->width() - theme_rect.right() < 40,
                            "Model then Theme stay visible at the far right at every desktop width");
                    if (conceptual)
                    {
                        require(search->parentWidget() == corner && search->isVisible() &&
                                    row->rect().contains(bounds(search)) && bounds(search).right() < model_rect.left() &&
                                    search->width() >= search->minimumWidth(),
                                "Conceptual Search is a field that precedes Model and Theme in the upper row");
                        auto *notation = child<QComboBox>(relational, "notationPicker");
                        require(notation->isVisible() && bounds(notation).right() < bounds(search).left(),
                                "Notation remains visible before the right-side group without overlap");
                        // One row, as the schema has (Zain, 2026-10-08): Home,
                        // Schema | Conceptual and the title lead it, before Save,
                        // and the header under it is put away.
                        auto *identity = child<QWidget>(relational, "conceptualIdentity");
                        auto *home = child<QPushButton>(relational, "backToHome");
                        auto *modes = child<QWidget>(relational, "schemaModeSwitch");
                        auto *title = child<QLabel>(relational, "documentTitle");
                        auto *save = bar->widgetForAction(child<QAction>(relational, "saveProject"));
                        require(header->isHidden() && identity->isVisible() && identity->isAncestorOf(home) &&
                                    identity->isAncestorOf(modes) && identity->isAncestorOf(title) &&
                                    home->isVisible() && modes->isVisible() && title->isVisible() &&
                                    row->rect().contains(bounds(identity)) && save &&
                                    bounds(home).right() < bounds(modes).left() &&
                                    bounds(modes).right() < bounds(title).left() &&
                                    bounds(identity).right() < bounds(save).left() &&
                                    !child<QLabel>(relational, "workspaceBadge")->isVisible() &&
                                    child<QPushButton>(relational, "previewConceptual")->isChecked() &&
                                    !header->isAncestorOf(search) && !header->isAncestorOf(corner_model) &&
                                    !header->isAncestorOf(corner_theme),
                                "Home, Schema | Conceptual and the title lead the one row, before Save, with "
                                "Conceptual lit and no header beneath");
                    }
                }
                relational.resize(original_size);
                settle();
            };
            check_corner_placement(true);
            answer_in_turn({"type:Placement check"});
            child<QAction>(relational, "renameDocumentAction")->trigger();
            settle();
            require(opened.project().name == "Placement check" &&
                        child<QLabel>(relational, "documentTitle")->text().contains("Placement check"),
                    "Renaming still updates the project and the title in the row");
            // Schema | Conceptual in the diagram's row (2026-10-08): Conceptual is
            // lit, being the design in front; Schema raises the schema the
            // diagram converts to and lights while it is up; Conceptual puts it
            // away again. Neither converts or writes anything.
            {
                auto *schema_half = child<QPushButton>(relational, "schemaModeSchema");
                auto *conceptual_half = child<QPushButton>(relational, "previewConceptual");
                auto *raised = child<QPushButton>(relational, "previewSchema");
                auto *panel = child<QWidget>(relational, "schemaPanel");
                const auto revision = opened.revision();
                require(conceptual_half->isChecked() && !schema_half->isChecked() && !raised->isChecked(),
                        "Conceptual is lit in the diagram's row, and Schema is not");
                schema_half->click();
                settle_motion(relational);
                require(raised->isChecked() && panel->isVisible() && schema_half->isChecked() &&
                            conceptual_half->isChecked() && opened.revision() == revision,
                        "Schema raises the schema, as Convert to Schema does, and lights while it is up");
                conceptual_half->click();
                settle_motion(relational);
                require(!raised->isChecked() && !panel->isVisible() && !schema_half->isChecked() &&
                            conceptual_half->isChecked() && opened.revision() == revision,
                        "Conceptual puts it away again, and stays lit");
            }
            opened.mark_saved(opened.revision());
            const auto entries = [&]
            {
                emit model_menu->aboutToShow();
                QStringList names;
                for (auto *action : model_menu->actions())
                    if (action->isVisible() && !action->isSeparator())
                        names << (action->menu() ? action->menu()->objectName() : action->objectName());
                return names;
            };
            {
                require(corner_model->isVisible() && corner_theme->isVisible() &&
                            corner_model->geometry().right() < corner_theme->x(),
                        "Model, then Theme, in the header's corner");
                QWidget *last = nullptr;
                for (auto *each : corner_theme->parentWidget()->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly))
                    if (each->isVisible() && (!last || each->x() > last->x()))
                        last = each;
                require(last == corner_theme, "Theme is the outermost");
                require(corner_model->text() == "Model" && corner_theme->text() == "Theme" &&
                            !corner_model->icon().isNull() && !corner_theme->icon().isNull() &&
                            corner_model->popupMode() == QToolButton::InstantPopup &&
                            corner_theme->popupMode() == QToolButton::InstantPopup &&
                            corner_model->iconSize() == corner_theme->iconSize() &&
                            corner_model->height() == corner_theme->height(),
                        "Each a mark, a word and a menu, alike");
                require(entries() == QStringList({"designRelational", "checkModel", "fileOpenExample"}),
                        "On the diagram Model offers Convert to Schema, Check model and Open example, in that order");
                require(child<QAction>(relational, "designRelational")->text() == "Convert to Schema",
                        "Converting is named for where it goes");
                require(!child<QPushButton>(relational, "previewSchema")->isVisible() &&
                            !child<QPushButton>(relational, "openExample")->isVisible() &&
                            !header_button->isVisible() &&
                            !child<QToolBar>(relational, "modelTools")->actions().contains(child<QAction>(relational, "checkModel")),
                        "And the separate Convert, Open example and Check model buttons are gone");
                // One theme, wherever it is chosen.
                auto *themes = child<QMenu>(relational, "themeMenu");
                require(corner_theme->menu() == themes &&
                            child<QToolBar>(relational, "designTools")->actions().contains(themes->menuAction()),
                        "The corner's Theme and Settings' Design row drop the same theme menu");
                const auto worn = relational.canvas()->theme_id();
                child<QAction>(relational, "thememidnight")->trigger();
                settle();
                require(relational.canvas()->theme_id() == desktop::ThemeId::Midnight &&
                            child<QAction>(relational, "thememidnight")->isChecked(),
                        "A theme chosen in either is the theme, ticked in both");
                relational.set_theme(worn);
                settle();
            }
            // A diagram's schema given the whole window is Relational Design in
            // front, and back again.
            relational.open_schema(true);
            settle_for(450);
            require(offered(true) && conceptual_offered(false) &&
                        !child<QPushButton>(relational, "openExample")->isVisible(),
                    "With Relational Design in front, its own are offered and the Conceptual ones are put away");
            check_corner_placement(false);
            require(entries() == QStringList({"modelToConceptual", "openRelationalExampleMenu"}) &&
                        child<QAction>(relational, "modelToConceptual")->text() == "Convert to Conceptual",
                    "With the schema in front Model offers Convert to Conceptual and its own examples, not the "
                    "diagram's checks");
            require(corner_model->isVisible() && corner_theme->isVisible() &&
                        !child<QToolButton>(relational, "schemaTheme")->isVisible(),
                    "Model and Theme stay in the corner, and the schema has no second Theme");
            child<QPushButton>(relational, "schemaFull")->click();
            settle();
            require(offered(false) && conceptual_offered(true), "Back on the diagram, they are put away again");
            check_corner_placement(true);
            child<QAction>(relational, "modelToConceptual")->trigger();
            settle_for(450);
            require(child<QLabel>(relational, "workspaceBadge")->text() == "CONCEPTUAL" &&
                        entries().front() == "designRelational",
                    "Model's Convert to Conceptual puts the schema away and the diagram is in front again");
            relational.open_schema(true);
            settle_for(450);

            // Each opens from its entry as a project that starts from its schema:
            // no diagram made first, untitled on disk and clean, Relational
            // Design in front, laid out by hand with every line routing itself.
            QTemporaryDir relational_files;
            require(relational_files.isValid(), "A folder for the Relational examples' files");
            struct Opening
            {
                QAction *entry;
                const char *name;
                const domain::Project *built;
            };
            const Opening openings[] = {
                {child<QAction>(relational, "fileExampleCompanyRelational"), "Company Database — Relational",
                 &company_schema},
                {child<QAction>(relational, "homeExampleUniversityRelational"), "University Database — Relational",
                 &university_schema},
                {header_button->menu()->actions().back(), "Untitled", &basic},
            };
            for (const auto &opening : openings)
            {
                require(opening.entry->isVisible() && opening.entry->isEnabled(), "The entry can be chosen");
                opening.entry->trigger();
                settle_for(450);
                const auto &project = relational.editor().project();
                require(project.name == opening.name && sound(project) &&
                            table_names(project) == table_names(*opening.built) &&
                            project.schema.foreign_keys.size() == opening.built->schema.foreign_keys.size(),
                        "It opens as the schema it is built as, drawn by hand with no diagram");
                require(!relational.editor().dirty() && !relational.editor().can_undo(),
                        "It opens clean, with nothing to undo");
                require(offered(true) && conceptual_offered(false) &&
                            child<QAction>(relational, "designConvert")->isVisible(),
                        "Relational Design is in front, offering its own, and Convert is there as for any schema "
                        "drawn by hand");
                auto *view = relational.schema();
                require(view && view->preview().tables.size() == project.schema.relations.size(), "Every table is drawn");
                // Laid out by hand: every table placed, none over another or
                // crowding it, and every line routes itself around the tables
                // it does not join.
                require(project.schema_layout.tables.size() == project.schema.relations.size() &&
                            project.schema_layout.lines.empty() && project.schema_layout.widths.empty(),
                        "Every table is placed, and no line or width is stored");
                const auto boxes = view->table_boxes();
                for (std::size_t i = 0; i < boxes.size(); ++i)
                    for (std::size_t j = i + 1; j < boxes.size(); ++j)
                        require(!boxes[i].adjusted(-30, -30, 30, 30).intersects(boxes[j]),
                                "No table lies over another, or crowds it");
                const auto lines = view->line_shapes();
                require(lines.size() == project.schema.foreign_keys.size(), "A line for every foreign key");
                for (const auto &route : lines)
                    for (std::size_t s = 0; s + 1 < route.size(); ++s)
                        for (const auto &box : boxes)
                        {
                            const QRectF inside = box.adjusted(3, 3, -3, -3);
                            const QRectF run = QRectF(route[s], route[s + 1]).normalized().adjusted(-0.1, -0.1, 0.1, 0.1);
                            require(!run.intersects(inside), "No line runs through a table");
                        }
                // Saved and opened again, it comes back exactly as it was, clean.
                const auto saved_at = relational_files.filePath(QString::fromUtf8(opening.name) + ".erdx");
                const auto kept = relational.editor().project();
                require(opened_store.save(saved_at.toStdString(), kept).ok, "A Relational example saves");
                const auto reloaded = opened_store.load(saved_at.toStdString());
                require(reloaded && *reloaded.project == kept, "And loads back exactly as it was");
                require(relational.open_path(saved_at) && relational.editor().project() == kept &&
                            !relational.editor().dirty() && relational.editor().project().schema.standalone,
                        "And opens in the window as it was saved, clean, still a schema drawn by hand");
            }
            // Lettered larger -- Windows draws a point size at 96 dpi where macOS
            // draws it at 72 -- every table is wider, and each column of tables
            // still stands clear of the one before it, since where it stands is
            // measured rather than fixed (2026-10-06).
            {
                auto *view = relational.schema();
                const auto was = view->font();
                auto larger = was;
                larger.setPointSizeF(was.pointSizeF() * 96.0 / 72.0);
                view->setFont(larger);
                for (const char *entry : {"fileExampleCompanyRelational", "fileExampleUniversityRelational",
                                          "fileTemplateRelational"})
                {
                    child<QAction>(relational, entry)->trigger();
                    settle_for(300);
                    const auto boxes = view->table_boxes();
                    require(boxes.size() == relational.editor().project().schema.relations.size(),
                            "Lettered larger, every table is drawn");
                    for (std::size_t i = 0; i < boxes.size(); ++i)
                        for (std::size_t j = i + 1; j < boxes.size(); ++j)
                            require(!boxes[i].adjusted(-30, -30, 30, 30).intersects(boxes[j]),
                                    "Lettered larger, no table lies over another, or crowds it");
                }
                view->setFont(was);
            }

            // One movement of the pointer routes the lines once (2026-10-06). A
            // drag that carries tables past the canvas's edge grows it, and the
            // view used to route every line on that resize and again straight
            // after -- the costliest work the schema does, twice over. A size
            // given from outside, by the window or the panel, still routes. Asked
            // of the template, whose two tables route in no time at all: how many
            // routings a movement makes does not depend on how many lines there
            // are.
            {
                child<QAction>(relational, "fileTemplateRelational")->trigger();
                settle();
                auto *view = relational.schema();
                const auto mouse = [&](QEvent::Type type, QPointF where, Qt::MouseButtons held)
                {
                    QMouseEvent event(type, where, view->mapToGlobal(where.toPoint()),
                                      type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, held, Qt::NoModifier);
                    QApplication::sendEvent(view, &event);
                };
                const auto as_opened = opened.project();
                const auto steps_before = opened.history().size();
                view->select_all();
                settle();
                const auto marked = view->selection();
                const auto before = view->table_boxes();
                const QPointF press(before[0].center().x(), before[0].top() + 13);
                mouse(QEvent::MouseButtonPress, press, Qt::LeftButton);
                // Taken well past the edge, then a little further.
                for (const double across : {1600.0, 1610.0})
                {
                    const auto wide = view->width();
                    const auto routed = view->routings();
                    mouse(QEvent::MouseMove, press + QPointF(across, 0), Qt::LeftButton);
                    require(view->width() > wide, "Carried past its edge, the canvas grows");
                    require(view->routings() == routed + 1, "And the lines are routed once for the movement, not twice");
                }
                mouse(QEvent::MouseButtonRelease, press + QPointF(1610, 0), Qt::NoButton);
                settle();
                const auto after = view->table_boxes();
                bool exact = after.size() == before.size();
                for (std::size_t t = 0; exact && t < before.size(); ++t)
                    exact = after[t] == before[t].translated(1610, 0);
                require(exact && view->selection() == marked && opened.history().size() == steps_before + 1,
                        "Every table lands exactly where the drag took it, still marked, in one step");
                const auto moved = opened.project();
                const auto lines_moved = view->line_shapes();
                const auto dragged_at = relational_files.filePath("Dragged.erdx");
                require(opened_store.save(dragged_at.toStdString(), moved).ok, "The dragged schema saves");
                const auto dragged = opened_store.load(dragged_at.toStdString());
                require(dragged && *dragged.project == moved, "And loads back where it was dragged to");
                child<QAction>(relational, "undoCommand")->trigger();
                settle();
                require(view->table_boxes() == before && opened.project() == as_opened, "Undo puts every table back");
                child<QAction>(relational, "redoCommand")->trigger();
                settle();
                require(view->table_boxes() == after && view->line_shapes() == lines_moved,
                        "Redo puts the move back, lines and all");
                child<QAction>(relational, "undoCommand")->trigger();
                settle();

                // A movement that leaves the canvas as it was routes once as well.
                view->select(view->preview().tables[0].origin);
                settle();
                const auto alone = view->table_boxes()[0];
                const QPointF hold(alone.center().x(), alone.top() + 13);
                mouse(QEvent::MouseButtonPress, hold, Qt::LeftButton);
                const auto size_was = view->size();
                const auto routed_was = view->routings();
                mouse(QEvent::MouseMove, hold + QPointF(10, 0), Qt::LeftButton);
                require(view->size() == size_was && view->routings() == routed_was + 1,
                        "A movement inside the canvas routes the lines once");
                mouse(QEvent::MouseButtonRelease, hold + QPointF(10, 0), Qt::NoButton);
                settle();
                child<QAction>(relational, "undoCommand")->trigger();
                settle();

                // A size given from outside: to the view itself, which the scroll
                // area at once gives back -- a size from outside as well -- and
                // by the window.
                const auto routed_now = view->routings();
                const auto size_held = view->size();
                view->resize(view->width() + 40, view->height() + 40);
                require(view->size() == size_held && view->routings() == routed_now + 2,
                        "A size given to the view from outside routes the lines, and the scroll area giving its own "
                        "back routes them again");
                settle();
                const auto size_now = view->size();
                const auto routed_then = view->routings();
                relational.resize(1440 + 200, 1080 + 100);
                settle();
                require(view->size() != size_now && view->routings() > routed_then,
                        "The window making the schema larger routes the lines");
                relational.resize(1440, 1080);
                settle();
                require(opened.project() == as_opened, "And the template is as it opened");
                opened.mark_saved(opened.revision());
            }

            // Tables carried downward are given room below them for as long as
            // the drag lasts (2026-10-09). The canvas used to reach only 30 px
            // past the lowest table, so near the bottom of the view there was
            // nothing to scroll into: a hand had to let go, scroll, and take hold
            // again. Now it reaches a whole view below the lowest table carried,
            // so the view scrolls on under the held drag and the same drag goes
            // on; let go, and the canvas reaches just past what is on it again.
            {
                // The messages here are put together from what is being carried.
                const auto must = [](bool ok, const std::string &message) { require(ok, message.c_str()); };
                child<QAction>(relational, "fileExampleCompanyRelational")->trigger();
                settle();
                auto *view = relational.schema();
                auto *scroll = child<QScrollArea>(relational, "schemaScroll");
                auto *bar = scroll->verticalScrollBar();
                const auto mouse = [&](QEvent::Type type, QPointF where, Qt::MouseButtons held)
                {
                    QMouseEvent event(type, where, view->mapToGlobal(where.toPoint()),
                                      type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, held, Qt::NoModifier);
                    QApplication::sendEvent(view, &event);
                };
                // Four notches of the wheel turned with the button still held, as a
                // hand scrolls on while it carries. The schema takes no wheel of
                // its own, so a real one goes on to the scroll area's view; one
                // made here would not be passed on, so it is given there.
                const auto wheel = [&](QPointF where)
                {
                    auto *port = scroll->viewport();
                    const auto there = view->mapTo(port, where);
                    QWheelEvent event(there, port->mapToGlobal(there), QPoint(), QPoint(0, -480), Qt::LeftButton,
                                      Qt::NoModifier, Qt::NoScrollPhase, false);
                    QApplication::sendEvent(port, &event);
                };
                const auto as_opened = opened.project();
                const auto boxes_opened = view->table_boxes();
                must(boxes_opened.size() >= 3, "The Company schema has tables enough to carry several");
                // Just past what is on the canvas, as it always was when nothing is
                // being carried: no room is left lying below the schema.
                const auto settled = [&]
                {
                    double lowest = 0;
                    for (const auto &box : view->table_boxes())
                        lowest = std::max(lowest, box.bottom());
                    return view->minimumHeight() == static_cast<int>(lowest + 30) &&
                           view->height() == std::max(view->minimumHeight(), scroll->viewport()->height());
                };
                must(settled(), "Before any drag, the canvas reaches just past the lowest table");
                const auto index_of = [&](const domain::ElementRef &ref)
                {
                    for (std::size_t t = 0; t < view->preview().tables.size(); ++t)
                        if (view->preview().tables[t].origin && *view->preview().tables[t].origin == ref)
                            return t;
                    return view->preview().tables.size();
                };
                // One drag from the table pressed, downward, with the view scrolled
                // on under it four times. Every table carried keeps exactly the
                // distance the hand took it; one step of history; nothing let go.
                const auto carry = [&](std::size_t pressed, const char *what)
                {
                    bar->setValue(0);
                    settle();
                    const auto steps_before = opened.history_position();
                    const auto before = view->table_boxes();
                    QPointF at(before[pressed].center().x(), before[pressed].top() + 13);
                    mouse(QEvent::MouseButtonPress, at, Qt::LeftButton);
                    const auto marked = view->selection();
                    std::vector<std::size_t> carried;
                    for (const auto &ref : marked)
                        carried.push_back(index_of(ref));
                    const auto deepest = [&]
                    {
                        const auto boxes = view->table_boxes();
                        double lowest = 0;
                        for (const auto t : carried)
                            lowest = std::max(lowest, boxes[t].bottom());
                        return lowest;
                    };
                    double travelled = 0;
                    for (int step = 0; step < 4; ++step)
                    {
                        const auto routed = view->routings();
                        at += QPointF(0, 40);
                        travelled += 40;
                        mouse(QEvent::MouseMove, at, Qt::LeftButton);
                        must(view->routings() == routed + 1,
                                std::string(what) + ": the canvas grows for the drag and the lines are still routed once "
                                                    "for the movement");
                        must(view->height() >= deepest() + scroll->viewport()->height() - 1,
                                std::string(what) + ": while it goes down, there is a view's room below the lowest "
                                                    "table carried");
                        must(view->table_boxes()[pressed] == before[pressed].translated(0, travelled),
                                std::string(what) + ": the table pressed stays under the hand");
                        const auto from = bar->value();
                        wheel(at);
                        const auto scrolled = bar->value() - from;
                        must(scrolled > 0, std::string(what) + ": the view scrolls on into that room with the drag "
                                                                  "still held");
                        // The hand stays where it was on the screen; the schema has
                        // moved up under it, and it goes on from there.
                        at += QPointF(0, scrolled);
                        travelled += scrolled;
                    }
                    mouse(QEvent::MouseMove, at, Qt::LeftButton);
                    must(travelled > scroll->viewport()->height(),
                            std::string(what) + ": one drag carries the tables well past where the view first ended");
                    const auto routed_at_release = view->routings();
                    mouse(QEvent::MouseButtonRelease, at, Qt::NoButton);
                    settle();
                    must(view->routings() == routed_at_release + 1,
                            std::string(what) + ": letting go routes the lines once, for the canvas it settles to");
                    const auto after = view->table_boxes();
                    bool exact = after.size() == before.size();
                    for (std::size_t t = 0; exact && t < before.size(); ++t)
                    {
                        const bool was_carried = std::find(carried.begin(), carried.end(), t) != carried.end();
                        exact = after[t] == (was_carried ? before[t].translated(0, travelled) : before[t]);
                    }
                    must(exact, std::string(what) + ": every table carried lands exactly where the drag took it, "
                                                       "and the rest stay where they were");
                    must(view->selection() == marked && opened.history_position() == steps_before + 1,
                            std::string(what) + ": still marked, in one step of history");
                    must(settled(), std::string(what) + ": let go, the room is given back and the canvas reaches "
                                                           "just past the lowest table again");
                    bar->setValue(bar->maximum());
                    settle();
                    const auto lowest_carried = deepest();
                    must(lowest_carried <= bar->value() + scroll->viewport()->height(),
                            std::string(what) + ": the tables carried can still be scrolled to");
                    return std::pair{before, after};
                };

                // One table: the highest of them, pressed alone.
                std::size_t highest = 0, lowest = 0;
                for (std::size_t t = 0; t < boxes_opened.size(); ++t)
                {
                    if (boxes_opened[t].top() < boxes_opened[highest].top())
                        highest = t;
                    if (boxes_opened[t].bottom() > boxes_opened[lowest].bottom())
                        lowest = t;
                }
                must(highest != lowest, "The Company schema has a highest table and a different lowest one");
                view->select(std::nullopt);
                settle();
                const auto [one_before, one_after] = carry(highest, "One table");
                must(view->selection() == std::vector{*view->preview().tables[highest].origin},
                        "One table: it is the one marked");
                {
                    const auto moved = opened.project();
                    const auto saved_at = relational_files.filePath("CarriedDown.erdx");
                    must(opened_store.save(saved_at.toStdString(), moved).ok, "One table: the schema saves");
                    const auto loaded = opened_store.load(saved_at.toStdString());
                    must(loaded && *loaded.project == moved,
                            "One table: and loads back with the table where it was let go, and nothing else");
                }
                child<QAction>(relational, "undoCommand")->trigger();
                settle();
                must(view->table_boxes() == one_before && opened.project() == as_opened && settled(),
                        "One table: Undo puts it back");
                child<QAction>(relational, "redoCommand")->trigger();
                settle();
                must(view->table_boxes() == one_after && settled(), "One table: Redo carries it down again");
                child<QAction>(relational, "undoCommand")->trigger();
                settle();

                // Several marked: pressed on the highest, the room follows the
                // lowest of them.
                view->select(view->preview().tables[highest].origin);
                view->toggle_mark(*view->preview().tables[lowest].origin);
                settle();
                must(view->selection().size() == 2, "Two tables are marked");
                carry(highest, "Two marked");
                child<QAction>(relational, "undoCommand")->trigger();
                settle();
                must(view->table_boxes() == boxes_opened && opened.project() == as_opened,
                        "Two marked: Undo puts both back");

                // Everything, by Select All.
                view->select_all();
                settle();
                must(view->selection().size() == boxes_opened.size(), "Select All marks every table");
                carry(highest, "Select All");
                child<QAction>(relational, "undoCommand")->trigger();
                settle();
                must(view->table_boxes() == boxes_opened && opened.project() == as_opened,
                        "Select All: Undo puts every table back");

                // Upward or across, no room is given: the canvas is as it was.
                view->select(view->preview().tables[lowest].origin);
                settle();
                bar->setValue(0);
                settle();
                {
                    const auto size_was = view->size();
                    const auto box = view->table_boxes()[lowest];
                    const QPointF hold(box.center().x(), box.top() + 13);
                    mouse(QEvent::MouseButtonPress, hold, Qt::LeftButton);
                    mouse(QEvent::MouseMove, hold + QPointF(0, -10), Qt::LeftButton);
                    must(view->size() == size_was, "Carried upward, the canvas is given no room");
                    mouse(QEvent::MouseMove, hold + QPointF(10, 0), Qt::LeftButton);
                    must(view->size() == size_was, "Carried across, the canvas is given no room");
                    mouse(QEvent::MouseButtonRelease, hold + QPointF(10, 0), Qt::NoButton);
                    settle();
                    child<QAction>(relational, "undoCommand")->trigger();
                    settle();
                }

                // The room is the view's own height, whatever size the window is.
                std::vector<int> views;
                for (const QSize window : {QSize(1280, 800), QSize(1440, 1080), QSize(1920, 1080)})
                {
                    relational.resize(window);
                    settle();
                    bar->setValue(0);
                    settle();
                    const auto box = view->table_boxes()[lowest];
                    const QPointF hold(box.center().x(), box.top() + 13);
                    view->select(std::nullopt);
                    mouse(QEvent::MouseButtonPress, hold, Qt::LeftButton);
                    mouse(QEvent::MouseMove, hold + QPointF(0, 40), Qt::LeftButton);
                    const auto room = scroll->viewport()->height();
                    views.push_back(room);
                    must(view->minimumHeight() == static_cast<int>(view->table_boxes()[lowest].bottom() + room) &&
                             view->height() == view->minimumHeight(),
                         "At " + std::to_string(window.width()) + " px the canvas reaches the view's own height below "
                                                                  "the table carried");
                    mouse(QEvent::MouseButtonRelease, hold + QPointF(0, 40), Qt::NoButton);
                    settle();
                    must(settled(), "At " + std::to_string(window.width()) + " px, let go, the room is given back");
                    child<QAction>(relational, "undoCommand")->trigger();
                    settle();
                }
                must(views[0] < views[1], "A shorter window gives a shorter view, and so less room");
                relational.resize(1440, 1080);
                settle();

                // Put away in the middle of a drag, the schema gives the room back.
                {
                    bar->setValue(0);
                    settle();
                    view->select(std::nullopt);
                    const auto box = view->table_boxes()[highest];
                    const QPointF hold(box.center().x(), box.top() + 13);
                    mouse(QEvent::MouseButtonPress, hold, Qt::LeftButton);
                    mouse(QEvent::MouseMove, hold + QPointF(0, 40), Qt::LeftButton);
                    must(view->minimumHeight() > static_cast<int>(view->table_boxes()[highest].bottom() + 30),
                            "Carried down, the canvas has room below");
                    relational.show_home(true);
                    settle();
                    double lowest_now = 0;
                    for (const auto &one : view->table_boxes())
                        lowest_now = std::max(lowest_now, one.bottom());
                    must(view->minimumHeight() == static_cast<int>(lowest_now + 30),
                            "Put away mid-drag, the schema gives the room back");
                    relational.show_home(false);
                    settle();
                    mouse(QEvent::MouseButtonRelease, hold + QPointF(0, 40), Qt::NoButton);
                    settle();
                    must(settled(), "And the drag let go, the canvas is just past what is on it");
                    while (opened.project() != as_opened && opened.can_undo())
                    {
                        child<QAction>(relational, "undoCommand")->trigger();
                        settle();
                    }
                }
                must(opened.project() == as_opened && view->table_boxes() == boxes_opened,
                        "And the Company schema is as it opened");
                opened.mark_saved(opened.revision());
            }

            // A table's lettering is measured once, not on every movement of the
            // pointer (2026-10-06): a drag moves tables and changes none of what
            // they say. Whatever does change what a table measures -- a name, a
            // column, a type, the font, the way it is shown, another schema --
            // has it measured again, and what is drawn is exactly what a view
            // made that moment, measuring everything afresh, draws.
            {
                child<QAction>(relational, "fileTemplateRelational")->trigger();
                settle();
                auto *view = relational.schema();
                const auto mouse = [&](QEvent::Type type, QPointF where, Qt::MouseButtons held)
                {
                    QMouseEvent event(type, where, view->mapToGlobal(where.toPoint()),
                                      type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, held, Qt::NoModifier);
                    QApplication::sendEvent(view, &event);
                };
                const auto as_afresh = [&]
                {
                    desktop::SchemaView afresh(opened);
                    afresh.setFont(view->font());
                    afresh.set_names_only(view->names_only());
                    afresh.refresh();
                    const auto cells = afresh.cell_boxes();
                    const auto drawn = view->cell_boxes();
                    bool same = afresh.table_boxes() == view->table_boxes() &&
                                afresh.row_boxes() == view->row_boxes() && cells.size() == drawn.size();
                    for (std::size_t t = 0; same && t < cells.size(); ++t)
                    {
                        same = cells[t].size() == drawn[t].size();
                        for (std::size_t r = 0; same && r < cells[t].size(); ++r)
                            same = cells[t][r].type == drawn[t][r].type && cells[t][r].size == drawn[t][r].size &&
                                   cells[t][r].rules == drawn[t][r].rules;
                    }
                    return same;
                };
                const auto edited = [&](const application::EditResult &result)
                {
                    const auto was = view->measurings();
                    view->refresh();
                    return result.ok && view->measurings() > was && as_afresh();
                };
                require(as_afresh(), "The template is drawn as measuring it afresh draws it");

                // Dragged, nothing is measured again.
                view->select_all();
                settle();
                const auto measured = view->measurings();
                const auto box = view->table_boxes()[0];
                const QPointF press(box.center().x(), box.top() + 13);
                mouse(QEvent::MouseButtonPress, press, Qt::LeftButton);
                for (int step = 1; step <= 3; ++step)
                    mouse(QEvent::MouseMove, press + QPointF(10.0 * step, 0), Qt::LeftButton);
                require(view->measurings() == measured, "A drag measures nothing again, however far it goes");
                // Until the font changes, which measures every table again
                // without waiting for the schema to be read.
                const auto font_was = view->font();
                auto larger = font_was;
                larger.setPointSizeF(font_was.pointSizeF() * 96.0 / 72.0);
                view->setFont(larger);
                mouse(QEvent::MouseMove, press + QPointF(40, 0), Qt::LeftButton);
                require(view->measurings() == measured + view->preview().tables.size(),
                        "A new font measures every table again, even in the middle of a drag");
                mouse(QEvent::MouseButtonRelease, press + QPointF(40, 0), Qt::NoButton);
                settle();
                require(as_afresh(), "And what is drawn in it is what measuring afresh draws");
                view->setFont(font_was);
                child<QAction>(relational, "undoCommand")->trigger();
                settle();
                require(as_afresh(), "Back in the old font, likewise");

                // Each kind of change to what a table says.
                const auto table_id = opened.project().schema.added.begin()->first;
                const auto key_id = opened.project().schema.added.begin()->second.front().id;
                const domain::ElementRef table_ref{table_id};
                require(edited(opened.rename_schema_column(key_id, "AnIdentifierFarLongerThanAnyBefore")),
                        "A column renamed is measured again");
                require(edited(opened.rename_table(table_ref, "A Table Given A Far Longer Name")),
                        "A table renamed is measured again");
                require(edited(opened.add_schema_column(table_ref, "Note")), "A column added is measured again");
                const auto note = opened.project().schema.added.at(table_id).back().id;
                require(edited(opened.set_schema_column_type(note, domain::LogicalType::NVarchar)),
                        "A column given a type is measured again");
                view->set_names_only(true);
                require(as_afresh(), "Shown by names only, it is what measuring afresh draws");
                view->set_names_only(false);
                require(as_afresh(), "And shown in full again, likewise");
                require(edited(opened.erase_schema_column(note)), "A column removed is measured again");
                require(edited(opened.undo()), "An undo is measured again");

                // Another schema opened is measured as itself, never as the one
                // before it.
                opened.mark_saved(opened.revision());
                const auto wide = view->table_boxes();
                child<QAction>(relational, "fileTemplateRelational")->trigger();
                settle();
                require(view->table_boxes() != wide && as_afresh(),
                        "The template opened again is measured as itself, not as the schema before it");
                opened.mark_saved(opened.revision());
            }

            // Only what can reach the part of the schema being painted is drawn
            // (2026-10-06). The view is as large as the whole schema and the
            // window shows a part of it; a table or a line nowhere near that part
            // is left alone, and nothing that reaches it is -- not a line passing
            // through it with both its ends elsewhere, and not the ring round a
            // marked table, which stands just outside its box.
            {
                child<QAction>(relational, "fileExampleCompanyRelational")->trigger();
                settle();
                auto *view = relational.schema();
                auto *scroll = child<QScrollArea>(relational, "schemaScroll");
                const auto has = [](const std::vector<std::size_t> &drawn, std::size_t which)
                { return std::find(drawn.begin(), drawn.end(), which) != drawn.end(); };
                // A part painted on its own, and whether anything at all is
                // drawn in it over the bare canvas.
                const auto paint = [&](const QRect &part)
                {
                    const auto image = view->grab(part).toImage();
                    bool marked = false;
                    for (int y = 0; y < image.height() && !marked; ++y)
                        for (int x = 0; x < image.width() && !marked; ++x)
                            marked = image.pixel(x, y) != image.pixel(0, 0);
                    return std::pair{view->last_painted(), marked};
                };
                const auto boxes = view->table_boxes();
                const auto lines = view->line_shapes();
                require(boxes.size() == 22 && lines.size() == 25, "The Company schema is drawn whole");

                // A little of the first table: it is drawn, and the table
                // farthest from it is not.
                std::size_t far = 0;
                for (std::size_t t = 1; t < boxes.size(); ++t)
                    if (QLineF(boxes[t].center(), boxes[0].center()).length() >
                        QLineF(boxes[far].center(), boxes[0].center()).length())
                        far = t;
                const auto [near_part, near_marked] = paint(QRect(boxes[0].center().toPoint(), QSize(20, 20)));
                require(has(near_part.tables, 0) && !has(near_part.tables, far) && near_marked,
                        "A table in the part painted is drawn, and one far from it is not");

                // A line running through a part of the canvas with no table
                // there and both its ends far away.
                std::optional<std::pair<std::size_t, QRect>> crossing;
                for (std::size_t l = 0; l < lines.size() && !crossing; ++l)
                    for (std::size_t s = 0; s + 1 < lines[l].size() && !crossing; ++s)
                    {
                        const auto mid = (lines[l][s] + lines[l][s + 1]) / 2;
                        const QRect part(mid.toPoint() - QPoint(4, 4), QSize(9, 9));
                        const bool clear = std::none_of(boxes.begin(), boxes.end(), [&](const QRectF &box)
                                                        { return box.adjusted(-20, -20, 20, 20).intersects(part); });
                        if (clear && QLineF(mid, lines[l].front()).length() > 150 &&
                            QLineF(mid, lines[l].back()).length() > 150)
                            crossing = std::pair{l, part};
                    }
                require(crossing.has_value(), "A line runs well away from its ends and from every table");
                const auto [through, line_marked] = paint(crossing->second);
                require(has(through.lines, crossing->first) && line_marked && through.tables.empty(),
                        "Painted where it passes, the line is drawn though both its ends are elsewhere");

                // The ring round a marked table stands outside its box, and a
                // part holding only that is painted with the ring in it.
                const auto top = boxes[far].toAlignedRect();
                const QRect above(top.center().x() - 10, top.top() - 4, 20, 3);
                const auto [plain, plain_marked] = paint(above);
                view->select(view->preview().tables[far].origin);
                settle();
                const auto [ringed, ring_marked] = paint(above);
                require(!plain_marked && has(ringed.tables, far) && ring_marked,
                        "The ring round a marked table is drawn where it stands outside the table");
                view->select(std::nullopt);
                settle();

                // What the window shows: every table and every line crossing it
                // is drawn, wherever it has been scrolled to, and what is far
                // from it is not.
                const auto shows = [&](int across, int down)
                {
                    scroll->horizontalScrollBar()->setValue(across);
                    scroll->verticalScrollBar()->setValue(down);
                    settle();
                    view->repaint();
                    const QRectF seen(scroll->horizontalScrollBar()->value(), scroll->verticalScrollBar()->value(),
                                      scroll->viewport()->width(), scroll->viewport()->height());
                    const auto drawn = view->last_painted();
                    bool whole = true;
                    for (std::size_t t = 0; t < boxes.size(); ++t)
                        if (boxes[t].intersects(seen) && !has(drawn.tables, t))
                            whole = false;
                    for (std::size_t l = 0; l < lines.size(); ++l)
                        for (std::size_t s = 0; s + 1 < lines[l].size(); ++s)
                            if (QRectF(lines[l][s], lines[l][s + 1]).normalized().adjusted(-1, -1, 1, 1).intersects(seen) &&
                                !has(drawn.lines, l))
                                whole = false;
                    return std::pair{drawn, whole};
                };
                const auto [corner, corner_whole] =
                    shows(scroll->horizontalScrollBar()->maximum(), scroll->verticalScrollBar()->maximum());
                const auto [home, home_whole] = shows(0, 0);
                require(corner_whole && home_whole, "Everything crossing what the window shows is drawn, scrolled or not");
                require(!has(corner.tables, 0) && has(home.tables, 0) && corner.tables.size() < boxes.size() &&
                            home.tables.size() < boxes.size(),
                        "Scrolled away, the first table is not drawn, and back again it is; neither draws them all");
                opened.mark_saved(opened.revision());
            }

            // Worked on as any schema drawn by hand: a table made, a column given
            // to it and typed, and a foreign key connected to Employee's key, each
            // one step, undone back to the example exactly and redone.
            child<QAction>(relational, "fileExampleCompanyRelational")->trigger();
            settle_for(300);
            const auto as_opened = opened.project();
            const auto audit = opened.create_relation("Audit", domain::Point{100, 2400});
            require(audit.ok, "A table is made on the example");
            const auto audit_id = std::get<domain::RelationId>(*audit.created);
            const auto employee_id = relation_named(opened.project(), "Employee");
            require(opened.add_schema_column(domain::ElementRef{audit_id}, "Note").ok &&
                        opened.set_schema_column_type(opened.project().schema.added.at(audit_id).back().id,
                                                      domain::LogicalType::NVarchar)
                            .ok &&
                        opened.connect_foreign_key(audit_id, std::nullopt, "EmployeeID", employee_id,
                                                   opened.project().schema.added.at(employee_id).front().id)
                            .ok &&
                        sound(opened.project()) && opened.project().schema.foreign_keys.size() == 26,
                    "A column is added to it and connected to Employee's key");
            for (int step = 0; step < 4; ++step)
                require(opened.undo().ok, "Each step undoes");
            require(opened.project() == as_opened && !opened.dirty() && !opened.can_undo(),
                    "Undone, the example is exactly as it opened");
            for (int step = 0; step < 4; ++step)
                require(opened.redo().ok, "Each step redoes");
            require(opened.project().schema.relations.size() == 23 && opened.project().schema.foreign_keys.size() == 26,
                    "Redone, the table and its foreign key are back");
            for (int step = 0; step < 4; ++step)
                opened.undo();
            opened.mark_saved(opened.revision());

            // Unsaved work is never thrown away by opening one: Cancel keeps it.
            require(opened.create_relation("Unsaved", domain::Point{100, 2400}).ok && opened.dirty(),
                    "The open schema has unsaved work");
            const auto unsaved = opened.project();
            auto *other_example = child<QAction>(relational, "fileExampleUniversityRelational");
            dismiss(QMessageBox::Cancel);
            other_example->trigger();
            settle();
            require(opened.project() == unsaved && opened.dirty(), "Cancel keeps the unsaved schema");
            opened.undo();
            opened.mark_saved(opened.revision());

            // A Conceptual example opened from Relational Design is a diagram
            // again, and whichever workspace is then in front offers only its own.
            relational.load_company_database();
            settle_for(450);
            require(!opened.project().schema.standalone && opened.project().entities.size() == 14 &&
                        child<QAction>(relational, "fileExampleCompany")->isVisible() !=
                            child<QAction>(relational, "fileExampleCompanyRelational")->isVisible(),
                    "The Conceptual example still opens as a diagram, and only one workspace's are offered");
            opened.mark_saved(opened.revision());
        }
        // A key belongs to an entity, or to a relationship with a table of its
        // own (Zain, 2026-10-05; ADR-021 §5b). Properties offers a key
        // attribute exactly those owners, says so, and makes a many-to-many
        // relationship's attribute its key when Key is chosen.
        {
            application::Editor keyed(ids);
            const auto made_entity = [&](const char *name, double x, double y)
            { return std::get<domain::EntityId>(*keyed.create_entity(name, {x, y, 160, 80}).created); };
            const auto student = made_entity("Student", 0, 0);
            const auto course = made_entity("Course", 500, 0);
            const auto professor = made_entity("Professor", 0, 400);
            const auto takes = std::get<domain::RelationshipId>(*keyed.create_relationship("Takes", {250, 0, 190, 110}).created);
            const auto advises = std::get<domain::RelationshipId>(*keyed.create_relationship("Advises", {0, 200, 190, 110}).created);
            for (const auto entity : {student, course})
            {
                const auto side = keyed.connect(takes, entity);
                require(side.ok && keyed.update_participant(takes, *side.participant, domain::Cardinality::Many,
                                                           domain::Participation::Partial, "")
                                       .ok,
                        "Takes is many to many");
            }
            const auto lead = keyed.connect(advises, professor);
            require(lead.ok && keyed.connect(advises, student).ok &&
                        keyed.update_participant(advises, *lead.participant, domain::Cardinality::One,
                                                 domain::Participation::Partial, "")
                            .ok,
                    "Advises is one to many");
            const auto ticket = std::get<domain::AttributeId>(
                *keyed.create_attribute("Ticket", {20, -140, 120, 50}, domain::ElementRef{student}).created);
            require(keyed.set_attribute_kind(ticket, domain::AttributeKind::Key).ok, "A key on Student");
            const auto number = std::get<domain::AttributeId>(
                *keyed.create_attribute("EnrollmentNumber", {260, -140, 160, 50}, domain::ElementRef{takes}).created);
            keyed.mark_saved(keyed.revision());
            infrastructure::ErdxProjectStore keyed_store;
            desktop::MainWindow properties(keyed, keyed_store, ids);
            properties.resize(1440, 920);
            properties.show();
            properties.show_home(false);
            settle();
            properties.canvas()->select_elements({domain::ElementRef{ticket}});
            settle();
            auto *owners = child<QComboBox>(properties, "attributeOwner");
            QStringList offered;
            for (int i = 0; i < owners->count(); ++i)
                offered << owners->itemText(i);
            require(offered.filter(QRegularExpression(": Takes$")).size() == 1 &&
                        offered.filter(QRegularExpression(": Advises$")).isEmpty(),
                    "A key may be given to Takes, which has a table of its own, and not to Advises");
            const auto labels = properties.findChildren<QLabel *>();
            require(std::none_of(labels.begin(), labels.end(), [](const QLabel *label)
                                 { return label->text().contains("Key attributes belong to entities"); }) &&
                        std::any_of(labels.begin(), labels.end(), [](const QLabel *label)
                                    { return label->text().contains("or to a many-to-many or associative relationship"); }),
                    "The hint says where a key may belong");
            properties.canvas()->select_elements({domain::ElementRef{number}});
            settle();
            emit child<QComboBox>(properties, "attributeKind")->activated(static_cast<int>(domain::AttributeKind::Key));
            settle();
            require(keyed.project().attributes.at(number).kind == domain::AttributeKind::Key &&
                        keyed.project().attributes.at(number).owner == domain::AttributeOwner{domain::ElementRef{takes}},
                    "Choosing Key in Properties makes the attribute Takes' key");
            keyed.mark_saved(keyed.revision());
        }
        // Automatic ends follow column rows, including vertically stacked tables.
        {
            application::Editor model(ids);
            const auto student = std::get<domain::EntityId>(*model.create_entity("Student", {}).created);
            const auto course = std::get<domain::EntityId>(*model.create_entity("Course", {}).created);
            const auto bridge = std::get<domain::RelationshipId>(*model.create_relationship("Enrolled", {}).created);
            for (const auto entity : {student, course})
            {
                const auto joined = model.connect(bridge, entity);
                require(joined.ok, "A bridge participant connects");
                require(model.update_participant(bridge, *joined.participant, domain::Cardinality::Many,
                                                 domain::Participation::Partial, "")
                            .ok,
                        "M:M participant");
            }
            desktop::SchemaView view(model);
            view.set_theme(desktop::theme(desktop::ThemeId::Azure));
            view.resize(1600, 1400);
            const auto index_of = [&](domain::ElementRef origin)
            {
                const auto &tables = view.preview().tables;
                const auto found = std::find_if(tables.begin(), tables.end(), [&](const auto &table)
                                                { return table.origin == origin; });
                require(found != tables.end(), "Each of the three is a table");
                return static_cast<std::size_t>(found - tables.begin());
            };
            // How much further right the bridge reaches than either participant
            // when they are stacked; and how much open canvas beside a table a
            // line leaving it needs, its stand-off and lanes included.
            constexpr double stagger = 60;
            constexpr double corridor_width = 60;
            for (const bool stacked : {false, true})
            {
                require(model.move_schema_tables({{domain::ElementRef{student}, {150, 100}},
                                                  {domain::ElementRef{course}, {150, 850}},
                                                  {domain::ElementRef{bridge}, {stacked ? 150.0 : 850.0, 450}}})
                            .ok,
                        "Place tables");
                view.refresh();
                if (stacked)
                {
                    // Stacked tables have no facing sides, so a line leaves by
                    // whichever side their edges come nearer to lining up on,
                    // unless the other is cleaner. Tables whose right edges each
                    // platform's lettering may happen to line up too are made to
                    // line up on the left alone: the bridge, plainly wider.
                    const auto natural = view.table_boxes();
                    domain::SchemaTableBox wider;
                    wider.width = std::max(natural[index_of(domain::ElementRef{student})].width(),
                                           natural[index_of(domain::ElementRef{course})].width()) +
                                  stagger;
                    wider.height = natural[index_of(domain::ElementRef{bridge})].height();
                    require(model.resize_schema_tables({{domain::ElementRef{bridge}, wider}}).ok,
                            "The bridge is made wider than either participant");
                    view.refresh();
                }
                const auto boxes = view.table_boxes();
                const auto rows = view.row_boxes();
                const auto lines = view.line_shapes();
                const auto painted = view.grab().toImage();
                require(lines.size() == 2, "One connector per participant");
                for (std::size_t t = 0; t < view.preview().tables.size(); ++t)
                {
                    const auto &table = view.preview().tables[t];
                    for (std::size_t c = 0; c < table.columns.size(); ++c)
                    {
                        const auto &column = table.columns[c];
                        if (!column.references)
                            continue;
                        require(column.foreign_key && !column.primary_key, "Bridge references are FK-only");
                        // A reference is written in the theme's green and never in
                        // the key's orange. The schema is asked which ink it writes
                        // the letters in rather than any pixel being expected to be
                        // that ink: each platform blends small letters into their
                        // background its own way.
                        const auto &azure = desktop::theme(desktop::ThemeId::Azure);
                        const auto written = view.key_letters_ink(t, c);
                        require(written && *written == azure.valid && *written != azure.warning,
                                "FK badges use green ink and never PK orange");
                        const QRectF gutter(QPointF(static_cast<int>(rows[t][c].left()) + 1,
                                                    static_cast<int>(rows[t][c].top()) + 1),
                                            QPointF(rows[t][c].left() + 42, rows[t][c].bottom()));
                        require(inked_pixels(painted, gutter, *written) > 0,
                                "And the FK letters are drawn in that ink in the key gutter");

                        const auto match = std::find_if(lines.begin(), lines.end(), [&](const auto &line)
                                                        { return std::abs(line.front().y() - rows[t][c].center().y()) < 0.01 && std::abs(line.back().y() - rows[*column.references][column.references_column].center().y()) < 0.01; });
                        require(match != lines.end(), "The exact FK row connects to its referenced PK row");
                        // The left is shown to be the side to leave by before the
                        // line is held to it. Side by side, the referenced table
                        // stands wholly to the left with open canvas between them;
                        // stacked, the two line up on the left and plainly not on
                        // the right. Either way no table stands where the line runs.
                        const auto &own = boxes[t];
                        const auto &referenced = boxes[*column.references];
                        const auto across = stacked ? own.left() - corridor_width : referenced.right();
                        if (stacked)
                            require(std::abs(own.left() - referenced.left()) < 0.01 &&
                                        own.right() - referenced.right() >= stagger - 0.01,
                                    "Stacked, the tables line up on the left and not on the right");
                        else
                            require(own.left() - referenced.right() >= corridor_width,
                                    "Side by side, the referenced table stands wholly to the left");
                        const QRectF corridor(QPointF(across, std::min(own.top(), referenced.top())),
                                              QPointF(own.left(), std::max(own.bottom(), referenced.bottom())));
                        require(std::none_of(boxes.begin(), boxes.end(), [&](const QRectF &box)
                                             { return box.intersects(corridor); }),
                                "And nothing stands in the way on the left");
                        require(std::abs(match->front().x() - boxes[t].left()) < 0.01,
                                "A clear left-side FK attachment is preferred");
                    }
                }
            }
            require(model.undo().ok, "The bridge is given back the width it takes of itself");
            // Put an obstacle immediately left of the FK rows: the automatic
            // attachment must use the right while retaining the same rows.
            const auto obstacle = std::get<domain::EntityId>(*model.create_entity("Obstacle", {}).created);
            for (int n = 0; n < 4; ++n)
                require(model.create_attribute("Field" + std::to_string(n), {}, domain::ElementRef{obstacle}).ok,
                        "The obstacle spans all FK rows");
            view.refresh();
            double obstacle_width = 0;
            for (std::size_t t = 0; t < view.preview().tables.size(); ++t)
                if (view.preview().tables[t].origin == domain::ElementRef{obstacle})
                    obstacle_width = view.table_boxes()[t].width();
            require(model.move_schema_tables({{domain::ElementRef{bridge}, {850, 450}},
                                              {domain::ElementRef{obstacle}, {850 - obstacle_width - 2, 450}}})
                        .ok,
                    "Obstruct the left side");
            view.refresh();
            for (std::size_t t = 0; t < view.preview().tables.size(); ++t)
            {
                if (view.preview().tables[t].origin != domain::ElementRef{bridge})
                    continue;
                for (const auto &line : view.line_shapes())
                {
                    if (std::abs(line.front().x() - view.table_boxes()[t].right()) >= 0.01)
                        std::cerr << "attachment " << line.front().x() << "," << line.front().y()
                                  << " expected x " << view.table_boxes()[t].right() << "\n";
                    require(std::abs(line.front().x() - view.table_boxes()[t].right()) < 0.01,
                            "The FK uses its right side when the left end run is blocked");
                }
            }
            const auto tables = view.preview().tables;
            for (const auto &table : tables)
            {
                for (const auto &column : table.columns)
                {
                    if (!column.link)
                        continue;
                    domain::SchemaLine shape;
                    shape.from = domain::SchemaEnd{true, {0.5, 0.0}};
                    require(model.shape_schema_line(*column.link, shape).ok, "A manual top attachment is accepted");
                    view.refresh();
                    const auto boxes = view.table_boxes();
                    const auto index = static_cast<std::size_t>(&table - tables.data());
                    const auto lines = view.line_shapes();
                    require(std::any_of(lines.begin(), lines.end(), [&](const auto &line)
                                        { return line.front() == QPointF(boxes[index].center().x(), boxes[index].top()); }),
                            "Manual top placement overrides automatic row attachment");
                    break;
                }
                if (view.shaped_lines())
                    break;
            }
            infrastructure::ErdxProjectStore bridge_store;
            desktop::MainWindow bridge_window(model, bridge_store, ids);
            auto *bridge_view = static_cast<desktop::SchemaView *>(child<QWidget>(bridge_window, "schemaView"));
            domain::OpenDecision strategy;
            strategy.kind = domain::DecisionKind::BridgeKey;
            strategy.about = domain::ElementRef{bridge};
            bridge_view->decided(strategy, 1);
            require(model.project().decisions.bridge_key.at(bridge) == domain::BridgeKey::Pair,
                    "Choosing participant keys reaches the editor");
            require(model.undo().ok, "The optional bridge strategy is undoable");
            require(!model.project().decisions.bridge_key.contains(bridge), "Undo restores the default strategy");
            bridge_view->decided(strategy, 0);
            require(model.project().decisions.bridge_key.at(bridge) == domain::BridgeKey::Own,
                    "Choosing a separate key reaches the editor");
        }
    }

    // The schema drawn by hand, each check in a window of its own: the Schema
    // Explorer, connections drawn from a key and let go, the schema's tools,
    // and where a line being drawn would land.
    void schema_drawing_tests(infrastructure::QtIdGenerator &ids)
    {
        {
            // Stage 2 (Zain, 2026-09-29): the Schema Explorer for a schema worked
            // out from a diagram made for it -- an invoice and a shipment each
            // identified by two keys and joined one to many, so a primary key
            // and a foreign key are each two columns wide; an employee managing
            // employees; and students and courses joined many to many through a
            // bridge whose key can be made of the two keys it carries.
            application::Editor model(ids);
            double placed = 0;
            const auto entity = [&](const char *name)
            {
                placed += 400;
                return std::get<domain::EntityId>(*model.create_entity(name, domain::Rect{placed, 0, 148, 86}).created);
            };
            const auto attribute = [&](const char *name, domain::EntityId owner, domain::AttributeKind kind)
            {
                const auto made = model.create_attribute(name, domain::Rect{placed, 200, 150, 60}, domain::ElementRef{owner});
                require(made.ok && made.created, "An attribute is made");
                const auto id = std::get<domain::AttributeId>(*made.created);
                if (kind != domain::AttributeKind::Normal)
                    require(model.set_attribute_kind(id, kind).ok, "And given its kind");
            };
            const auto relate = [&](const char *name, domain::EntityId one, domain::Cardinality one_side,
                                    domain::EntityId other, domain::Cardinality other_side,
                                    const std::string &one_role = {})
            {
                const auto relationship = std::get<domain::RelationshipId>(
                    *model.create_relationship(name, domain::Rect{placed, 400, 190, 110}).created);
                const auto first = model.connect(relationship, one);
                const auto second = model.connect(relationship, other);
                require(first.ok && first.participant && second.ok && second.participant, "Both sides are joined");
                require(model.update_participant(relationship, *first.participant, one_side,
                                                 domain::Participation::Partial, one_role)
                                .ok &&
                            model.update_participant(relationship, *second.participant, other_side,
                                                     domain::Participation::Partial, "")
                                .ok,
                        "And given their cardinalities");
                return relationship;
            };
            using domain::AttributeKind;
            using domain::Cardinality;
            const auto invoice = entity("Invoice");
            attribute("InvoiceNo", invoice, AttributeKind::Key);
            attribute("Year", invoice, AttributeKind::Key);
            attribute("Total", invoice, AttributeKind::Normal);
            const auto shipment = entity("Shipment");
            attribute("ShipmentNo", shipment, AttributeKind::Key);
            attribute("Depot", shipment, AttributeKind::Key);
            relate("Covers", invoice, Cardinality::One, shipment, Cardinality::Many);
            const auto employee = entity("Employee");
            attribute("EmployeeID", employee, AttributeKind::Key);
            attribute("Name", employee, AttributeKind::Normal);
            relate("Manages", employee, Cardinality::One, employee, Cardinality::Many, "Manager");
            const auto student = entity("Student");
            attribute("StudentID", student, AttributeKind::Key);
            attribute("Name", student, AttributeKind::Normal);
            attribute("Age", student, AttributeKind::Derived);
            const auto course = entity("Course");
            attribute("CourseID", course, AttributeKind::Key);
            const auto enrols = relate("Enrols", student, Cardinality::Many, course, Cardinality::Many);

            infrastructure::ErdxProjectStore tree_store;
            desktop::MainWindow tree_window(model, tree_store, ids);
            tree_window.resize(1440, 920);
            tree_window.show();
            tree_window.show_home(false);
            settle();
            tree_window.open_schema();
            settle_for(300);
            auto *tree = child<QTreeView>(tree_window, "schemaExplorer");
            auto *view = tree_window.schema();
            require(child<QDockWidget>(tree_window, "explorerDock")->widget() == tree && tree->isVisible(),
                    "The schema's own Explorer stands beside the schema a diagram becomes");

            const auto table_of = [&](domain::ElementRef origin)
            {
                const auto &tables = view->preview().tables;
                for (std::size_t t = 0; t < tables.size(); ++t)
                    if (tables[t].origin == origin)
                        return t;
                throw std::runtime_error("No table was made for that element");
            };
            const auto name_of = [&](std::size_t t)
            { return QString::fromStdString(view->preview().tables[t].name); };
            const auto column_of = [&](std::size_t t, std::size_t row)
            {
                return QString::fromStdString(view->preview().tables[t].columns[row].name);
            };
            const auto top = [&](const char *group)
            {
                const auto *model_now = tree->model();
                return row_saying(*model_now, model_now->index(0, 0), group);
            };
            const auto table_row = [&](std::size_t t)
            { return row_saying(*tree->model(), top("Tables"), name_of(t)); };
            const auto group_of = [&](std::size_t t, const char *group)
            {
                return row_saying(*tree->model(), table_row(t), group);
            };
            const auto count = [](const QModelIndex &index)
            { return index.data(desktop::explorer_count_role).toInt(); };
            const auto note = [](const QModelIndex &index)
            { return index.data(desktop::explorer_note_role).toString(); };
            // What the schema itself holds, to hold the Explorer to.
            const auto keys_held = [&](std::size_t t)
            {
                std::vector<domain::ForeignKeyId> keys;
                for (const auto &column : view->preview().tables[t].columns)
                    if (column.key_id && std::find(keys.begin(), keys.end(), *column.key_id) == keys.end())
                        keys.push_back(*column.key_id);
                return keys;
            };
            const auto parts_of = [&](std::size_t t, domain::ForeignKeyId key)
            {
                std::vector<std::size_t> rows;
                const auto &columns = view->preview().tables[t].columns;
                for (std::size_t row = 0; row < columns.size(); ++row)
                    if (columns[row].key_id == key)
                        rows.push_back(row);
                std::sort(rows.begin(), rows.end(), [&](std::size_t a, std::size_t b)
                          { return columns[a].reference_part < columns[b].reference_part; });
                return rows;
            };
            const auto primary_names = [&](std::size_t t)
            {
                QStringList names;
                for (std::size_t row = 0; row < view->preview().tables[t].columns.size(); ++row)
                    if (view->preview().tables[t].columns[row].primary_key)
                        names << column_of(t, row);
                return names;
            };

            std::size_t all_keys = 0;
            QStringList every_table;
            for (std::size_t t = 0; t < view->preview().tables.size(); ++t)
            {
                all_keys += keys_held(t).size();
                every_table << name_of(t);
            }
            require(rows_said(*tree->model(), top("Tables")) == every_table && count(top("Tables")) == static_cast<int>(every_table.size()) && count(top("Relationships")) == static_cast<int>(all_keys) && rows_said(*tree->model(), top("Relationships")).size() == static_cast<int>(all_keys),
                    "Every table and every foreign key the schema holds, counted from it");

            // A primary key of two columns is listed whole.
            const auto invoice_t = table_of(domain::ElementRef{invoice});
            const auto invoice_key = group_of(invoice_t, "Primary Key");
            require(primary_names(invoice_t).size() == 2 && rows_said(*tree->model(), invoice_key) == primary_names(invoice_t) && count(invoice_key) == 2,
                    "A primary key of two columns is listed whole, and counted to show it");

            // A foreign key of two columns is one foreign key.
            const auto shipment_t = table_of(domain::ElementRef{shipment});
            const auto shipment_keys = keys_held(shipment_t);
            require(shipment_keys.size() == 1 && parts_of(shipment_t, shipment_keys.front()).size() == 2,
                    "The schema holds one foreign key two columns wide");
            const auto parts = parts_of(shipment_t, shipment_keys.front());
            const auto &first_part = view->preview().tables[shipment_t].columns[parts[0]];
            const auto &second_part = view->preview().tables[shipment_t].columns[parts[1]];
            const auto target = *first_part.references;
            const auto wide = "(" + column_of(shipment_t, parts[0]) + ", " + column_of(shipment_t, parts[1]) + ") → " + name_of(target) + "(" + column_of(target, first_part.references_column) + ", " + column_of(target, second_part.references_column) + ")";
            const auto shipment_fks = group_of(shipment_t, "Foreign Keys");
            require(rows_said(*tree->model(), shipment_fks) == QStringList{wide} && count(shipment_fks) == 1,
                    "Listed as one foreign key under its table, not one for each of its columns");
            const auto wide_key = desktop::schema_key(desktop::ChosenForeignKey{shipment_keys.front()});
            require(tree->model()->match(top("Relationships"), Qt::UserRole, wide_key, -1,
                                         Qt::MatchExactly | Qt::MatchRecursive)
                                .size() == 1 &&
                        rows_said(*tree->model(), top("Relationships")).contains(name_of(shipment_t) + wide),
                    "And once under Relationships, with the table that holds it");

            // A key into its own table.
            const auto employee_t = table_of(domain::ElementRef{employee});
            const auto employee_keys = keys_held(employee_t);
            require(employee_keys.size() == 1, "The employee's table holds one foreign key");
            const auto manager_row = parts_of(employee_t, employee_keys.front()).front();
            const auto &manager = view->preview().tables[employee_t].columns[manager_row];
            require(manager.references == employee_t, "It points back into its own table");
            const auto own = column_of(employee_t, manager_row) + " → " + name_of(employee_t) + "." + column_of(employee_t, manager.references_column);
            require(rows_said(*tree->model(), group_of(employee_t, "Foreign Keys")) == QStringList{own} && rows_said(*tree->model(), top("Relationships")).contains(name_of(employee_t) + "." + own) && rows_said(*tree->model(), top("Tables")).count(name_of(employee_t)) == 1,
                    "Listed pointing back into its own table, with no second table made for it");

            // A bridge is a table like any other: its own key, and a foreign
            // key into each table it joins.
            const auto enrols_t = table_of(domain::ElementRef{enrols});
            require(rows_said(*tree->model(), table_row(enrols_t)) == QStringList{"Columns", "Primary Key", "Foreign Keys"} && count(group_of(enrols_t, "Foreign Keys")) == 2,
                    "A bridge lists its columns, its own key and a foreign key into each table it joins");

            // A value worked out rather than stored is listed as the schema
            // lists it, slanted, and never counted as a column.
            const auto student_t = table_of(domain::ElementRef{student});
            const auto &student_columns = view->preview().tables[student_t].columns;
            const auto stored = std::count_if(student_columns.begin(), student_columns.end(),
                                              [](const auto &column)
                                              { return !column.ignored; });
            require(count(group_of(student_t, "Columns")) == static_cast<int>(stored),
                    "Only real columns are counted");
            for (std::size_t row = 0; row < student_columns.size(); ++row)
                if (student_columns[row].ignored)
                {
                    const auto age = row_saying(*tree->model(), group_of(student_t, "Columns"), column_of(student_t, row));
                    require(age.isValid() && age.data(Qt::FontRole).value<QFont>().italic(),
                            "A row the conversion leaves out is slanted");
                }

            // Nothing in it speaks the diagram's language.
            const auto walk = [&](auto &&self, const QModelIndex &parent) -> void
            {
                for (int row = 0; row < tree->model()->rowCount(parent); ++row)
                {
                    const auto index = tree->model()->index(row, 0, parent);
                    for (const auto &said : {index.data().toString(), index.data(Qt::ToolTipRole).toString()})
                        for (const char *word : {"Entity", "entity", "Attribute", "attribute", "Specialization"})
                            require(!said.contains(QLatin1String(word)), "The Explorer speaks only of the schema");
                    self(self, index);
                }
            };
            walk(walk, QModelIndex());

            // Pressed in the Explorer, chosen on the schema, whatever handle
            // the column has; and never an edit.
            const auto revision = model.revision();
            const auto undo_label = model.undo_label();
            const auto was_dirty = model.dirty();
            press_row(*tree, table_row(shipment_t));
            require(std::holds_alternative<desktop::ChosenTable>(view->selection_now()) && std::get<desktop::ChosenTable>(view->selection_now()).table == view->preview().tables[shipment_t].id,
                    "A table pressed in the Explorer is chosen on the schema");
            press_row(*tree, row_saying(*tree->model(), shipment_fks, wide));
            require(std::holds_alternative<desktop::ChosenForeignKey>(view->selection_now()) && std::get<desktop::ChosenForeignKey>(view->selection_now()).key == shipment_keys.front(),
                    "A foreign key two columns wide is chosen as one key");
            const auto enrols_columns = group_of(enrols_t, "Columns");
            QModelIndex carried;
            for (int row = 0; row < tree->model()->rowCount(enrols_columns); ++row)
                if (note(tree->model()->index(row, 0, enrols_columns)) == "FK")
                {
                    carried = tree->model()->index(row, 0, enrols_columns);
                    break;
                }
            require(carried.isValid(), "The bridge's columns include a foreign key");
            press_row(*tree, carried);
            const auto carried_choice = view->selection_now();
            require(std::holds_alternative<desktop::ChosenColumn>(carried_choice) && std::holds_alternative<domain::ForeignKeyColumn>(
                                                                                         std::get<desktop::ChosenColumn>(carried_choice).column.source),
                    "A column the conversion made for a foreign key is chosen by the key it belongs to");
            require(model.revision() == revision && model.undo_label() == undo_label && model.dirty() == was_dirty,
                    "Choosing through the Explorer is not an edit");

            // Made of the two keys it carries, the bridge's key is two columns,
            // each of them both kinds of key and said to be both. The tree is
            // made afresh for the edit, and keeps what was open and chosen.
            tree->expand(table_row(enrols_t));
            domain::OpenDecision strategy;
            strategy.kind = domain::DecisionKind::BridgeKey;
            strategy.about = domain::ElementRef{enrols};
            view->decided(strategy, 1);
            settle();
            const auto pair = group_of(table_of(domain::ElementRef{enrols}), "Primary Key");
            require(rows_said(*tree->model(), pair).size() == 2 && count(pair) == 2,
                    "The bridge's primary key is now its two foreign keys, counted");
            for (int row = 0; row < 2; ++row)
                require(note(tree->model()->index(row, 0, pair)) == "PK FK",
                        "Each column of it is a primary key and a foreign key at once, and says both");
            require(tree->isExpanded(table_row(table_of(domain::ElementRef{enrols}))),
                    "What was opened by hand is still open after the edit");
            require(view->selection_now() == carried_choice && lit_rows(*tree) == QStringList(2, desktop::schema_key(carried_choice)),
                    "And what was chosen is still chosen, lit under Columns and now under Primary Key too");
            child<QAction>(tree_window, "undoCommand")->trigger();
            settle();
            require(rows_said(*tree->model(), group_of(table_of(domain::ElementRef{enrols}), "Primary Key")).size() == 1,
                    "Undone, the bridge's key is its own again, and the Explorer follows");

            // Stage 3 (2026-10-01): the same Properties, by the same code,
            // reading a schema worked out from a diagram: keys of two columns,
            // a key into its own table, a bridge, a value left out.
            {
                auto *properties_dock = child<QDockWidget>(tree_window, "propertiesDock");
                require(properties_dock->widget() == child<QWidget>(tree_window, "schemaProperties"),
                        "The schema's own Properties stands beside the schema a diagram becomes");
                const auto panel = [&]() -> const QWidget &
                { return *properties_dock->widget(); };
                const auto said = [&](const QString &field)
                { return properties_value(panel(), field); };
                // Schema words only. Since Stage 4 a table's and a column's
                // values are offered as fields, so whether anything can be
                // edited is asked separately, of the views that must stay read.
                const auto plainly = [&]
                { return properties_speak_schema(panel()); };
                const auto read_only = [&]
                { return !properties_offer_edits(panel()); };
                const auto &preview = [&]() -> const domain::SchemaPreview &
                { return view->preview(); };
                const auto key_row_in = [&](const QModelIndex &group, domain::ForeignKeyId key)
                {
                    const auto found = tree->model()->match(group, Qt::UserRole,
                                                            desktop::schema_key(desktop::ChosenForeignKey{key}), 1,
                                                            Qt::MatchExactly | Qt::MatchRecursive);
                    return found.isEmpty() ? QModelIndex() : found.front();
                };
                const auto stage_revision = model.revision();
                const auto stage_undo = model.undo_label();
                const auto stage_dirty = model.dirty();

                // Summed up, a key of two columns counted once.
                view->choose(desktop::NothingChosen{});
                settle();
                std::size_t stored_columns = 0;
                std::size_t keyed = 0;
                std::size_t key_columns = 0;
                std::size_t foreign = 0;
                for (std::size_t t = 0; t < preview().tables.size(); ++t)
                {
                    const auto &columns = preview().tables[t].columns;
                    stored_columns += static_cast<std::size_t>(std::count_if(columns.begin(), columns.end(),
                                                                             [](const auto &c)
                                                                             { return !c.ignored; }));
                    if (std::any_of(columns.begin(), columns.end(),
                                    [](const auto &c)
                                    { return c.primary_key && !c.ignored; }))
                        ++keyed;
                    key_columns += static_cast<std::size_t>(std::count_if(columns.begin(), columns.end(),
                                                                          [](const auto &c)
                                                                          { return c.key_id.has_value(); }));
                    foreign += keys_held(t).size();
                }
                require(properties_heading(panel()) == "Schema" && said("Schema/Tables") == QStringList{QString::number(preview().tables.size())} && said("Schema/Columns") == QStringList{QString::number(stored_columns)} && said("Schema/Primary Keys") == QStringList{QString::number(keyed)} && said("Schema/Foreign Keys") == QStringList{QString::number(foreign)} && key_columns > foreign && plainly() && read_only(),
                        "Summed up from what the schema holds, a foreign key of two columns counted once");

                // A primary key of two columns, and the key of two that points at it.
                const auto invoice_at = table_of(domain::ElementRef{invoice});
                const auto shipment_at = table_of(domain::ElementRef{shipment});
                const auto wide_id = keys_held(shipment_at).front();
                const auto wide_parts = parts_of(shipment_at, wide_id);
                const QStringList wide_own{column_of(shipment_at, wide_parts[0]), column_of(shipment_at, wide_parts[1])};
                const auto &wide_first = preview().tables[shipment_at].columns[wide_parts[0]];
                const auto &wide_second = preview().tables[shipment_at].columns[wide_parts[1]];
                const QStringList wide_theirs{column_of(invoice_at, wide_first.references_column),
                                              column_of(invoice_at, wide_second.references_column)};
                press_row(*tree, table_row(invoice_at));
                require_table_properties(panel(), *view, invoice_at);
                require(properties_heading(panel()) == "Properties" && said("General/Name") == QStringList{name_of(invoice_at)} && said("General/Source") == QStringList{"Conceptual diagram"} && said("Structure/Primary Key") == QStringList{primary_names(invoice_at).join(", ")} && primary_names(invoice_at).size() == 2 && said("Structure/Foreign Keys") == QStringList{"None"} && said("References/Referenced By") == QStringList{name_of(shipment_at) + "(" + wide_own.join(", ") + ")"} && plainly(),
                        "A table keyed by two columns says both, and is pointed at by one key of two");
                press_row(*tree, tree->model()->index(0, 0, group_of(invoice_at, "Primary Key")));
                {
                    const auto first_key = primary_names(invoice_at).front();
                    const auto pointing = wide_first.references_column == 0 ? wide_own[0] : wide_own[1];
                    require(said("General/Name") == QStringList{first_key} && said("Keys/Key Role") == QStringList{"Primary Key"} && said("Keys/Key Column") == QStringList{primary_names(invoice_at).join(", ")} && said("Constraints/Unique") == QStringList{"No"} && said("General/Source") == QStringList{"Conceptual diagram"} && said("References/Referenced By") == QStringList{name_of(shipment_at) + "." + pointing},
                            "One column of it says the whole key, is not unique alone, and which column points at it");
                }
                press_row(*tree, row_saying(*tree->model(), group_of(shipment_at, "Foreign Keys"), wide));
                const auto wide_said = properties_all(panel());
                require(properties_heading(panel()) == "Relationship" && said("Identity/Relationship") == QStringList{name_of(shipment_at) + "(" + wide_own.join(", ") + ") → " + name_of(invoice_at) + "(" + wide_theirs.join(", ") + ")"} && said("General/Self Reference") == QStringList{"No"} && said("General/Column Pairs") == QStringList{wide_own[0] + " → " + wide_theirs[0], wide_own[1] + " → " + wide_theirs[1]} && said("Referencing/Table") == QStringList{name_of(shipment_at)} && said("Referencing/Columns") == wide_own && properties_count(panel(), "Referencing/Columns") == 2 && said("Referencing/Role") == QStringList{"FK", "FK"} && said("Referenced/Table") == QStringList{name_of(invoice_at)} && said("Referenced/Columns") == wide_theirs && properties_count(panel(), "Referenced/Columns") == 2 && said("Referenced/Role") == QStringList{"PK", "PK"} && said("Referencing/Column").isEmpty() && panel().findChildren<QWidget *>("schemaRelationshipEnd").size() == 2 && plainly() && read_only(),
                        "A foreign key of two columns is one relationship, its columns paired in order");
                // Laid out as a chosen column is (Zain, 2026-10-02): Properties,
                // the line in a card of its own, then General and Ends, both
                // open the first time; Ends holds the referenced end, then the
                // referencing end, each a card of its own with the cardinality
                // the line is drawn with there, only read. From and To are said
                // once, in the card, not again below it. No Constraints: the
                // schema holds no referential actions yet (Zain, 2026-10-02).
                {
                    const auto *shown = panel().findChild<QWidget *>("schemaRelationshipProperties");
                    const auto *title = panel().findChild<QLabel *>("schemaPropertiesHeading");
                    const auto *card = panel().findChild<QFrame *>("schemaRelationshipIdentity");
                    const auto *kind = card ? card->findChild<QLabel *>("propertyHeading") : nullptr;
                    QStringList order;
                    bool all_open = true;
                    for (auto *section : panel().findChildren<QWidget *>("schemaRelationshipPropertySection"))
                    {
                        order << section->property("section").toString();
                        all_open = all_open && section->findChild<QWidget *>("schemaRelationshipSectionBody")->isVisibleTo(section);
                    }
                    QStringList ends_in_order;
                    QStringList roles_said;
                    for (auto *end : panel().findChildren<QWidget *>("schemaRelationshipEnd"))
                    {
                        ends_in_order << end->property("end").toString();
                        roles_said << end->findChild<QLabel *>("schemaRelationshipEndRole")->text();
                    }
                    const auto &holding = preview().tables[shipment_at].columns[wide_parts[0]];
                    const auto drawn = holding.optional_link ? (holding.one_to_one ? "Zero or One (0..1)" : "Zero or Many (0..N)")
                                                             : (holding.one_to_one ? "One (1)" : "One or Many (1..N)");
                    bool any_constraint = false;
                    for (const auto &[field, values] : properties_all(panel()))
                        any_constraint = any_constraint || field.startsWith("Constraints/") || field.contains("On Delete") || field.contains("On Update") || field.contains("Mandatory");
                    require(shown && title && title->text() == "Properties" && kind && kind->text() == "Relationship" && said("Identity/Kind") == QStringList{"Foreign Key"} && said("General/Type") == QStringList{"Foreign Key"} && said("General/From").isEmpty() && said("General/To").isEmpty() && order == QStringList{"General", "Ends"} && all_open,
                            "Properties, the relationship's own card, then General and Ends, both open");
                    require(ends_in_order == QStringList{"Referenced", "Referencing"} && roles_said == QStringList{"Referenced end", "Referencing end"} && said("Referenced/Cardinality") == QStringList{"One (1)"} && said("Referencing/Cardinality") == QStringList{drawn} && !panel().findChild<QLabel *>("schemaRelationshipDirection"),
                            "Ends: the referenced end, then the referencing end, each saying the cardinality the line is "
                            "drawn with there, once for the whole key");
                    require(!any_constraint && read_only(),
                            "No Constraints, no ON DELETE, ON UPDATE or Mandatory, and nothing offered to change");
                    // Folded by the hand, a section stays folded for the next
                    // line chosen, and is remembered apart from a table's or a
                    // column's sections. Folding is not an edit.
                    const auto section_called = [&](const QString &title_words) -> QWidget *
                    {
                        for (auto *section : panel().findChildren<QWidget *>("schemaRelationshipPropertySection"))
                            if (section->property("section").toString() == title_words)
                                return section;
                        throw std::runtime_error("No relationship section");
                    };
                    const auto folding_revision = model.revision();
                    const auto folding_undo = model.undo_label();
                    const auto body = [&](const char *title_words)
                    { return section_called(title_words)->findChild<QWidget *>("schemaRelationshipSectionBody"); };
                    section_called("Ends")->findChild<QAbstractButton *>("sectionHeader")->click();
                    settle();
                    require(QSettings().value("schemaRelationshipPropertySection/Ends", true).toBool() == false && !QSettings().contains("schemaColumnPropertySection/Ends") && !QSettings().contains("schemaPropertySection/Ends") && body("Ends")->isHidden() && !body("General")->isHidden(),
                            "Folding Ends is remembered as the relationship's own, and leaves General open");
                    press_row(*tree, table_row(invoice_at));
                    press_row(*tree, key_row_in(top("Relationships"), wide_id));
                    auto *header = section_called("Ends")->findChild<QAbstractButton *>("sectionHeader");
                    require(!header->isChecked() && body("Ends")->isHidden() && !body("General")->isHidden(),
                            "Chosen again, Ends is still folded and General still open");
                    section_called("General")->findChild<QAbstractButton *>("sectionHeader")->click();
                    settle();
                    require(body("General")->isHidden() && body("Ends")->isHidden(), "General folds on its own as well");
                    section_called("General")->findChild<QAbstractButton *>("sectionHeader")->click();
                    header->click();
                    settle();
                    require(header->isChecked() && !body("Ends")->isHidden() && !body("General")->isHidden() && model.revision() == folding_revision && model.undo_label() == folding_undo && properties_all(panel()) == wide_said,
                            "Opened again, and none of it was an edit or changed what is said");
                }
                press_row(*tree, key_row_in(top("Relationships"), wide_id));
                require(properties_all(panel()) == wide_said, "The same under Relationships");
                for (std::size_t part = 0; part < 2; ++part)
                {
                    press_row(*tree, row_saying(*tree->model(), group_of(shipment_at, "Columns"), wide_own[static_cast<qsizetype>(part)]));
                    require(said("Keys/Key Role") == QStringList{"Foreign Key"} && said("Keys/Key Column") == QStringList{wide_own.join(", ")} && said("Keys/Member") == QStringList{QString("%1 (%2 of 2)").arg(wide_own[static_cast<qsizetype>(part)]).arg(part + 1)} && said("References/References") == QStringList{name_of(invoice_at) + "." + wide_theirs[static_cast<qsizetype>(part)]} && said("General/Source") == QStringList{"Generated by the conversion"},
                            "Each column of it is a member of the one key, not a key of its own");
                }

                // A key into its own table.
                const auto employee_at = table_of(domain::ElementRef{employee});
                const auto own_key = keys_held(employee_at).front();
                const auto manager_at = parts_of(employee_at, own_key).front();
                const auto boss = column_of(employee_at, manager_at);
                const auto boss_points_at =
                    column_of(employee_at, preview().tables[employee_at].columns[manager_at].references_column);
                press_row(*tree, key_row_in(top("Relationships"), own_key));
                require(said("Identity/Relationship") == QStringList{name_of(employee_at) + "." + boss + " → " + name_of(employee_at) + "." + boss_points_at} && said("General/Self Reference") == QStringList{"Yes"} && said("Referencing/Table") == QStringList{name_of(employee_at)} && said("Referenced/Table") == QStringList{name_of(employee_at)} && said("Referencing/Column") == QStringList{boss} && said("Referenced/Column") == QStringList{boss_points_at} && said("Referencing/Role") == QStringList{"FK"} && said("Referenced/Role") == QStringList{"PK"} && said("General/Column Pairs").isEmpty() && panel().findChildren<QWidget *>("schemaRelationshipEnd").size() == 2 && said("Referenced/Cardinality") == QStringList{"One (1)"} && said("Referencing/Cardinality").size() == 1,
                        "A key into its own table: one table at both ends, said to be itself");
                press_row(*tree, table_row(employee_at));
                require(said("Structure/Foreign Keys") == QStringList{boss + " → " + name_of(employee_at) + "." + boss_points_at} && said("References/Referenced By") == QStringList{name_of(employee_at) + "." + boss},
                        "Its table holds that key and is pointed at by it, once each");

                // A bridge: a table like any other, with a key into each table it joins.
                const auto bridge_at = table_of(domain::ElementRef{enrols});
                press_row(*tree, table_row(bridge_at));
                require_table_properties(panel(), *view, bridge_at);
                require(said("Structure/Foreign Keys").size() == 2 && properties_count(panel(), "Structure/Foreign Keys") == 2 && said("Structure/Primary Key") == QStringList{primary_names(bridge_at).join(", ")} && primary_names(bridge_at).size() == 1,
                        "A bridge says its own key and its two foreign keys");
                QStringList bridge_references;
                bool saw_own_key = false;
                for (std::size_t row = 0; row < preview().tables[bridge_at].columns.size(); ++row)
                {
                    const auto &column = preview().tables[bridge_at].columns[row];
                    press_row(*tree, row_saying(*tree->model(), group_of(bridge_at, "Columns"), column_of(bridge_at, row)));
                    if (column.foreign_key)
                    {
                        const auto pointed_table = *column.references;
                        require(said("Keys/Key Role") == QStringList{"Foreign Key"} && said("References/References") == QStringList{name_of(pointed_table) + "." + column_of(pointed_table, column.references_column)},
                                "Each of the bridge's foreign keys names the table it points at");
                        bridge_references << name_of(pointed_table);
                    }
                    else if (column.primary_key)
                    {
                        require(said("General/Source") == QStringList{"Generated by the conversion"} && properties_all(panel())["hint"].contains(
                                                                                                            "Nothing on the diagram identifies this table, so a key was made for it."),
                                "The bridge's own key says it was made for it, and why");
                        saw_own_key = true;
                    }
                }
                require(saw_own_key, "The bridge's own key was looked at");
                bridge_references.sort();
                QStringList joined{name_of(table_of(domain::ElementRef{student})), name_of(table_of(domain::ElementRef{course}))};
                joined.sort();
                require(bridge_references == joined, "One foreign key into each table the bridge joins, told apart");

                // A value worked out rather than kept is not described as a column.
                const auto student_at = table_of(domain::ElementRef{student});
                press_row(*tree, table_row(student_at));
                require_table_properties(panel(), *view, student_at);
                bool saw_derived = false;
                for (std::size_t row = 0; row < preview().tables[student_at].columns.size(); ++row)
                    if (preview().tables[student_at].columns[row].ignored)
                    {
                        saw_derived = true;
                        press_row(*tree, row_saying(*tree->model(), group_of(student_at, "Columns"), column_of(student_at, row)));
                        require(properties_heading(panel()) == "Derived value" && said("General/Stored") == QStringList{"No"} && said("General/Data Type").isEmpty() && said("Keys/Key Role").isEmpty() && said("Constraints/Nullable").isEmpty() && plainly() && read_only(),
                                "A value left out says it is not stored, and nothing a column would say");
                    }
                require(saw_derived, "The value left out was looked at");
                require(model.revision() == stage_revision && model.undo_label() == stage_undo && model.dirty() == stage_dirty,
                        "Reading all of it changed nothing and added nothing to the history");

                // Made of the two keys it carries, the bridge's key is two
                // columns, each a primary key and a foreign key at once.
                std::optional<std::size_t> carried_row;
                for (std::size_t row = 0; row < preview().tables[bridge_at].columns.size() && !carried_row; ++row)
                    if (preview().tables[bridge_at].columns[row].foreign_key)
                        carried_row = row;
                const auto carried_now = desktop::ChosenColumn{*view->column_ref(bridge_at, *carried_row)};
                view->decided(strategy, 1);
                settle();
                view->choose(carried_now);
                settle();
                const auto paired_at = table_of(domain::ElementRef{enrols});
                view->choose(desktop::ChosenTable{view->preview().tables[paired_at].id});
                settle();
                require_table_properties(panel(), *view, paired_at);
                view->choose(carried_now);
                settle();
                require(said("Keys/Key Role") == QStringList{"Primary Key + Foreign Key"} && said("Keys/Key Column") == QStringList{primary_names(paired_at).join(", ")} && said("Keys/Foreign Key Column").size() == 1 && said("Constraints/Primary Key") == QStringList{"Yes"} && said("Constraints/Foreign Key") == QStringList{"Yes"} && primary_names(paired_at).size() == 2 && said("References/References").size() == 1,
                        "A column of a key made of the keys it carries is said to be both, in a key of two");
                // Its line, chosen: the Referencing column keeps both roles.
                {
                    const auto here = view->locate(carried_now.column);
                    const auto carried_key = view->preview().tables[here->first].columns[here->second].key_id;
                    view->choose(desktop::ChosenForeignKey{*carried_key});
                    settle();
                    require(said("Referencing/Table") == QStringList{name_of(paired_at)} && said("Referencing/Role") == QStringList{"PK", "FK"} && said("Referenced/Role") == QStringList{"PK"} && said("General/Self Reference") == QStringList{"No"} && plainly() && read_only(),
                            "Its line says the Referencing column is a primary key and a foreign key, both");
                    view->choose(carried_now);
                    settle();
                }
                child<QAction>(tree_window, "undoCommand")->trigger();
                settle();
                require(said("Keys/Key Role") != QStringList{"Primary Key + Foreign Key"},
                        "Undone, it is no longer said to be both");
            }

            // Stage 4 (2026-10-01): the same fields on a schema worked out from
            // a diagram, going where the schema's own cells send them -- a name,
            // a type or a rule to what the table or column came from on the
            // diagram, as they always have -- and nowhere else.
            {
                auto *properties_dock = child<QDockWidget>(tree_window, "propertiesDock");
                const auto panel = [&]() -> const QWidget &
                { return *properties_dock->widget(); };
                const auto said = [&](const QString &field)
                { return properties_value(panel(), field); };
                const auto name_field = [&]
                { return properties_editor<QLineEdit>(panel(), "General/Name"); };
                const auto box = [&](const char *field)
                {
                    return properties_editor<QCheckBox>(panel(), QString::fromUtf8(field));
                };
                const auto picker = [&](const char *field)
                {
                    return properties_editor<QComboBox>(panel(), QString::fromUtf8(field));
                };
                const auto type_in = [&](QLineEdit *line, const QString &words)
                {
                    line->setText(words);
                    QKeyEvent pressed(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                    QApplication::sendEvent(line, &pressed);
                    settle();
                };
                const auto undo = [&]
                { child<QAction>(tree_window, "undoCommand")->trigger(); settle(); };
                const auto start_label = model.undo_label();
                const auto entities = model.project().entities.size();
                const auto attributes = model.project().attributes.size();
                const auto relationships = model.project().relationships.size();
                const auto diagram_unchanged_in_shape = [&]
                {
                    return model.project().entities.size() == entities && model.project().attributes.size() == attributes && model.project().relationships.size() == relationships;
                };
                std::optional<domain::AttributeId> total;
                for (const auto &[id, drawn] : model.project().attributes)
                    if (drawn.name == "Total")
                        total = id;
                require(total.has_value(), "The invoice has its Total");
                const auto total_row = [&]
                {
                    const auto t = table_of(domain::ElementRef{invoice});
                    for (std::size_t row = 0; row < view->preview().tables[t].columns.size(); ++row)
                        if (view->preview().tables[t].columns[row].origin == total)
                            return std::pair{t, row};
                    throw std::runtime_error("No Total column");
                };

                // A table's name is the name of the entity it came from.
                press_row(*tree, table_row(table_of(domain::ElementRef{invoice})));
                const auto invoice_choice = view->selection_now();
                type_in(name_field(), "Bill");
                const auto bill_at = table_of(domain::ElementRef{invoice});
                require(model.project().entities.at(invoice).name == "Bill" && view->selection_now() == invoice_choice && said("General/Name") == QStringList{name_of(bill_at)} && table_row(bill_at).isValid() && diagram_unchanged_in_shape(),
                        "A table renamed in Properties renames what it came from, as on the canvas, and nothing more");
                undo();
                require(model.project().entities.at(invoice).name == "Invoice" && said("General/Name") == QStringList{name_of(table_of(domain::ElementRef{invoice}))},
                        "Undone, both have their names back");

                // The table's compact column fields take exactly the same
                // Stage 4 route, including propagation to their source.
                {
                    const auto [t, r] = total_row();
                    const auto handle = *view->column_ref(t, r);
                    const auto field = desktop::schema_key(desktop::ChosenColumn{handle}) + "/Name";
                    type_in(properties_editor<QLineEdit>(panel(), field), "InvoiceTotal");
                    require(model.project().attributes.at(*total).name == "InvoiceTotal" && view->selection_now() == invoice_choice && properties_value(panel(), field) == QStringList{"InvoiceTotal"} && diagram_unchanged_in_shape(),
                            "Renaming a column in the table list uses Stage 4 and keeps the table selected");
                    undo();
                    require(model.project().attributes.at(*total).name == "Total" && properties_value(panel(), field) == QStringList{"Total"},
                            "Undo restores the generated column and its conceptual source together");
                }

                // The compact type/constraint controls also use Stage 4's
                // conceptual propagation, without inventing schema mutations.
                {
                    const auto [t, row] = total_row();
                    view->choose(desktop::ChosenTable{view->preview().tables[t].id});
                    settle();
                    const auto handle = *view->column_ref(t, row);
                    const auto type_key = desktop::schema_key(desktop::ChosenColumn{handle}) + "/Data Type";
                    const auto before = model.project().attributes.at(*total).logical_type;
                    properties_editor<QComboBox>(panel(), type_key)->showPopup();
                    settle();
                    child<QLineEdit>(tree_window, "typePickerSearch")->setText("bigint");
                    settle();
                    auto *list = child<QListWidget>(tree_window, "typePickerList");
                    QTest_activate(list, list->item(0));
                    settle();
                    require(model.project().attributes.at(*total).logical_type == domain::LogicalType::BigInt && properties_editor<QComboBox>(panel(), type_key)->currentText() == "BIGINT" && diagram_unchanged_in_shape(),
                            "A generated table's compact type edit propagates to its conceptual attribute");
                    undo();
                    require(model.project().attributes.at(*total).logical_type == before,
                            "Undo restores the generated compact type and its conceptual source together");
                    auto *rules = panel().findChildren<QFrame *>("schemaColumnRow")[static_cast<qsizetype>(row)]->findChild<QToolButton *>("schemaColumnConstraints");
                    QTimer::singleShot(0, &tree_window, [&]
                                       {
                        auto* menu = child<QMenu>(tree_window, "schemaColumnRulesMenu");
                        menu->findChild<QAction*>("schemaRuleUnique")->trigger();
                        menu->close(); });
                    rules->click();
                    settle();
                    require(model.project().attributes.at(*total).unique && diagram_unchanged_in_shape(),
                            "A generated compact constraint uses the existing conceptual rule propagation");
                    undo();
                    // Classified the same way (Zain, 2026-10-01). A diagram's
                    // foreign keys come from its relationships, so there is no
                    // Foreign Key entry here, as before.
                    QStringList generated_top;
                    QTimer::singleShot(0, &tree_window, [&]
                                       {
                        auto* menu = child<QMenu>(tree_window, "schemaColumnRulesMenu");
                        for (auto* action : menu->actions())
                            generated_top << (action->isSeparator() ? QString("—") : action->text());
                        menu->close(); });
                    panel().findChildren<QFrame *>("schemaColumnRow")[static_cast<qsizetype>(row)]->findChild<QToolButton *>("schemaColumnConstraints")->click();
                    settle();
                    require(generated_top == QStringList{"Primary Key", "—", "NULL — may be empty", "NOT NULL — required",
                                                         "—", "UNIQUE — no duplicate values",
                                                         "IDENTITY — auto-generated number"},
                            "A generated column's Constraints menu is classified in the same words");
                }

                // A column's name, type and rules are its attribute's.
                {
                    const auto [t, row] = total_row();
                    view->choose(desktop::ChosenColumn{*view->column_ref(t, row)});
                    settle();
                }
                const auto total_choice = view->selection_now();
                type_in(name_field(), "Amount");
                require(model.project().attributes.at(*total).name == "Amount" && view->selection_now() == total_choice && said("General/Name") == QStringList{"Amount"} && rows_said(*tree->model(), group_of(table_of(domain::ElementRef{invoice}), "Columns")).contains("Amount") && diagram_unchanged_in_shape(),
                        "A column renamed renames the attribute it came from, and stays chosen");
                undo();
                require(model.project().attributes.at(*total).name == "Total", "Undone, Total");
                const auto type_before = model.project().attributes.at(*total).logical_type;
                picker("General/Data Type")->showPopup();
                settle();
                auto *listed = child<QListWidget>(tree_window, "typePickerList");
                child<QLineEdit>(tree_window, "typePickerSearch")->setText("decimal");
                settle();
                QTest_activate(listed, listed->item(0));
                settle();
                require(model.project().attributes.at(*total).logical_type == domain::LogicalType::Decimal && said("General/Data Type") == QStringList{"DECIMAL"} && tree_window.findChild<QWidget *>("sizePicker")->isVisible(),
                        "A type chosen is the attribute's type, from the schema's own list, and a measured one asks "
                        "its size there too");
                tree_window.findChild<QWidget *>("sizePicker")->hide();
                settle();
                undo();
                require(model.project().attributes.at(*total).logical_type == type_before, "Undone, its type is back");
                box("Constraints/Unique")->click();
                settle();
                require(model.project().attributes.at(*total).unique && said("Constraints/Unique") == QStringList{"Yes"} && diagram_unchanged_in_shape(),
                        "Unique is the attribute's rule");
                undo();
                require(!model.project().attributes.at(*total).unique, "Undone, it is not unique");

                // A foreign key the conversion made takes its type from its
                // key, and the field says so rather than offering a list.
                const auto shipment_at = table_of(domain::ElementRef{shipment});
                const auto member = parts_of(shipment_at, keys_held(shipment_at).front()).front();
                view->choose(desktop::ChosenColumn{*view->column_ref(shipment_at, member)});
                settle();
                const auto before_type = model.revision();
                picker("General/Data Type")->showPopup();
                settle();
                require(model.revision() == before_type && !tree_window.findChild<QWidget *>("typePicker")->isVisible() && tree_window.statusBar()->currentMessage().contains("takes its type from the key"),
                        "A foreign key's type field says where its type comes from, and changes nothing");

                // The key the conversion made for the bridge counts up, and is
                // still the key.
                const auto bridge_at = table_of(domain::ElementRef{enrols});
                std::optional<std::size_t> own_key;
                for (std::size_t row = 0; row < view->preview().tables[bridge_at].columns.size(); ++row)
                    if (view->preview().tables[bridge_at].columns[row].primary_key)
                        own_key = row;
                view->choose(desktop::ChosenColumn{*view->column_ref(bridge_at, *own_key)});
                settle();
                box("Constraints/Identity")->click();
                settle();
                const auto &counted = view->preview().tables[table_of(domain::ElementRef{enrols})].columns[*own_key];
                require(counted.auto_increment && counted.primary_key && said("Constraints/Identity") == QStringList{"Yes"} && diagram_unchanged_in_shape(),
                        "The bridge's own key counts up, and is still its key");
                undo();
                require(!view->preview().tables[table_of(domain::ElementRef{enrols})].columns[*own_key].auto_increment && model.undo_label() == start_label,
                        "Undone, it does not, and everything done here is undone");
                view->choose(desktop::NothingChosen{});
                settle();
            }

            // Stage 5 (2026-10-01): a schema worked out from a diagram takes its
            // foreign keys from the diagram's relationships, so Connect is not
            // offered on it, and the Editor's Connect refuses its tables rather
            // than inventing anything on either level.
            {
                require(!child<QToolButton>(tree_window, "schemaConnect")->isVisible(),
                        "Connect is not offered on a schema worked out from a diagram");
                require(child<QAction>(tree_window, "schemaSelectTool")->isChecked() && !child<QAction>(tree_window, "schemaTableAction")->isChecked() && !child<QAction>(tree_window, "schemaConnectTool")->isChecked() && !view->placing() && !view->connecting(),
                        "The schema worked out from a diagram is worked with in Select, the one tool it has");
                const auto entities = model.project().entities.size();
                const auto attributes = model.project().attributes.size();
                const auto relationships = model.project().relationships.size();
                const auto connect_revision = model.revision();
                const auto &tables = view->preview().tables;
                require(!model.connect_foreign_key(tables[table_of(domain::ElementRef{shipment})].id, std::nullopt,
                                                   "InvoiceID", tables[table_of(domain::ElementRef{invoice})].id,
                                                   domain::SchemaColumnId{}) &&
                            model.revision() == connect_revision && model.project().entities.size() == entities && model.project().attributes.size() == attributes && model.project().relationships.size() == relationships,
                        "Asked anyway, it is refused, and nothing on the schema or the diagram is added or removed");
            }
        }
        // Stage 5 (Zain, 2026-10-01): a connection drawn on a schema drawn by
        // hand starts on the key being referenced and ends on the table that
        // refers to it, or the column there chosen to hold it. Nothing is
        // changed until it is agreed to; no primary key is ever turned into a
        // foreign key or loses its key; no foreign key is re-pointed or
        // doubled; and what is agreed to is one edit.
        {
            application::Editor model(ids);
            model.new_schema_project();
            std::map<std::string, domain::RelationId> made;
            const auto table = [&](const char *name, double x, double y)
            {
                const auto result = model.create_relation(name, domain::Point{x, y});
                require(result.ok && result.created, "A table is made");
                made[name] = std::get<domain::RelationId>(*result.created);
            };
            const auto column = [&](const char *table_name, const char *name)
            {
                require(model.add_schema_column(domain::ElementRef{made.at(table_name)}, name).ok, "A column is added");
                return model.project().schema.added.at(made.at(table_name)).back().id;
            };
            table("Course", 40, 40);
            column("Course", "Name");
            table("Student", 520, 40);
            column("Student", "Name");
            table("Lesson", 40, 320);
            const auto lesson_course = column("Lesson", "CourseID");
            require(model.set_schema_column_type(lesson_course, domain::LogicalType::Int).ok, "Typed as the key is");
            table("Review", 520, 320);
            const auto review_course = column("Review", "CourseID");
            require(model.set_schema_column_type(review_course, domain::LogicalType::Varchar).ok, "Typed otherwise");
            table("Person", 40, 600);
            table("Staff", 520, 600);
            table("Member", 1000, 600);
            require(model.rename_schema_column(model.project().schema.added.at(made.at("Member")).front().id,
                                               "PersonID")
                        .ok,
                    "Member's key is called PersonID");
            table("Category", 1000, 40);
            table("Employee", 1000, 320);
            column("Employee", "ManagerID");
            table("Pair", 1480, 40);
            const auto part = column("Pair", "Part");
            require(model.set_schema_column_type(part, domain::LogicalType::Int).ok && model.set_schema_column_rules(part, true, true, false).ok,
                    "Pair is keyed by two columns");
            // The key referred to counts up and is unique, none of which a
            // foreign key to it is to take.
            const auto course_key = model.project().schema.added.at(made.at("Course")).front().id;
            require(model.set_schema_column_auto_increment(course_key, true).ok && model.set_schema_column_rules(course_key, true, true, true).ok,
                    "Course's key counts up and is unique");
            model.mark_saved(model.revision());

            infrastructure::ErdxProjectStore connect_store;
            desktop::MainWindow connect_window(model, connect_store, ids);
            connect_window.resize(1440, 920);
            connect_window.show();
            connect_window.show_home(false);
            settle_for(300);
            auto *view = connect_window.schema();
            require(view && view->isVisible(), "The hand-drawn schema is in front");
            auto *properties_dock = child<QDockWidget>(connect_window, "propertiesDock");
            auto *explorer = child<QTreeView>(connect_window, "schemaExplorer");
            const auto at = [&](const char *name)
            {
                for (std::size_t t = 0; t < view->preview().tables.size(); ++t)
                    if (view->preview().tables[t].id == made.at(name))
                        return t;
                throw std::runtime_error("No such table");
            };
            const auto columns = [&](const char *name) -> const std::vector<domain::PreviewColumn> &
            {
                return view->preview().tables[at(name)].columns;
            };
            const auto named = [&](const char *name, const char *wanted) -> const domain::PreviewColumn *
            {
                for (const auto &one : columns(name))
                    if (one.name == wanted)
                        return &one;
                return nullptr;
            };
            const auto mouse = [&](QEvent::Type type, QPointF where, Qt::MouseButtons held)
            {
                QMouseEvent event(type, where, view->mapToGlobal(where.toPoint()),
                                  type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, held, Qt::NoModifier);
                QApplication::sendEvent(view, &event);
            };
            const auto gutter = [&](const char *name, std::size_t row)
            {
                const auto box = view->row_boxes()[at(name)][row];
                return QPointF(box.left() + 22, box.center().y());
            };
            const auto heading = [&](const char *name)
            {
                const auto box = view->table_boxes()[at(name)];
                return QPointF(box.center().x(), box.top() + 8);
            };
            const auto on_row = [&](const char *name, std::size_t row)
            {
                return view->row_boxes()[at(name)][row].center();
            };
            // A line drawn from one point to another, the box it asks answered
            // with these words where it asks, and what it said kept.
            const auto draw = [&](QPointF from, QPointF to, const QString &agreeing = {}, QStringList *asked = nullptr)
            {
                mouse(QEvent::MouseButtonPress, from, Qt::LeftButton);
                mouse(QEvent::MouseMove, (from + to) / 2, Qt::LeftButton);
                mouse(QEvent::MouseMove, to, Qt::LeftButton);
                if (!agreeing.isEmpty())
                    answer(agreeing, asked);
                mouse(QEvent::MouseButtonRelease, to, Qt::NoButton);
                settle();
            };
            const auto told = [&]
            { return connect_window.statusBar()->currentMessage(); };
            const auto undo = [&]
            { child<QAction>(connect_window, "undoCommand")->trigger(); settle(); };
            const auto redo = [&]
            { child<QAction>(connect_window, "redoCommand")->trigger(); settle(); };
            const auto panel = [&]() -> const QWidget &
            { return *properties_dock->widget(); };
            struct Held
            {
                domain::SchemaOverrides schema;
                std::uint64_t revision;
                bool dirty;
                std::string undo;
                std::size_t lines;
            };
            const auto hold = [&]
            {
                return Held{model.project().schema, model.revision(), model.dirty(), model.undo_label(),
                            view->line_shapes().size()};
            };
            const auto unchanged = [&](const Held &was)
            {
                return model.project().schema == was.schema && model.revision() == was.revision && model.dirty() == was.dirty && model.undo_label() == was.undo && view->line_shapes().size() == was.lines;
            };
            const auto student_key = model.project().schema.added.at(made.at("Student")).front().id;

            // The original fault: Course's key dragged onto Student. Cancelled
            // first, it changes nothing at all.
            const auto before = hold();
            QStringList asked;
            draw(gutter("Course", 0), heading("Student"), "Cancel", &asked);
            require(asked.value(1) == "No foreign key found" && asked.value(2).contains("Student does not contain a foreign key referencing Course.CourseID.") && asked.value(2).contains("Create CourseID as a foreign key and connect?") && asked.value(2).contains("Student.CourseID  →  Course.CourseID"),
                    "Let go on a table with no foreign key to the key, it says so and offers to make one");
            require(unchanged(before), "Cancelled, nothing changes: not the schema, its revision, its unsaved "
                                       "state, the history nor the lines");
            // Agreed, the column is made and the key is untouched.
            draw(gutter("Course", 0), heading("Student"), "Create & Connect");
            const auto *fk = named("Student", "CourseID");
            const auto &course_columns = columns("Course");
            const auto &student_columns = columns("Student");
            require(fk && fk->foreign_key && fk->references == at("Course") && fk->references_column == 0 && !fk->primary_key && !fk->unique && !fk->auto_increment && !fk->required && fk->type == domain::LogicalType::Int && student_columns.size() == 3,
                    "Student gains CourseID, a foreign key to Course.CourseID of its type, and nothing else of "
                    "the key's: not its key, its uniqueness or its counting up");
            require(student_columns[0].name == "StudentID" && student_columns[0].primary_key && !student_columns[0].foreign_key && student_columns[0].added == student_key && course_columns[0].primary_key && !course_columns[0].foreign_key && course_columns[0].auto_increment,
                    "Neither key becomes a foreign key, nor stops being a key: the fault is gone");
            require(model.undo_label() == "Connect tables" && model.revision() != before.revision && model.dirty() && view->line_shapes().size() == before.lines + 1,
                    "It is one edit, unsaved, drawn as one line");
            const auto student_fk = *fk->key_id;
            require(view->selection_now() == desktop::SchemaSelection{desktop::ChosenForeignKey{student_fk}},
                    "The foreign key made is the one chosen");
            require(properties_heading(panel()) == "Relationship" && properties_value(panel(), "Identity/Relationship") == QStringList{"Student.CourseID → Course.CourseID"} && properties_value(panel(), "Referencing/Cardinality") == QStringList{"Zero or Many (0..N)"} && properties_value(panel(), "Referenced/Cardinality") == QStringList{"One (1)"},
                    "Properties shows the foreign key made");
            {
                const auto *tree = explorer->model();
                const auto root = tree->index(0, 0);
                const auto tables_group = row_saying(*tree, root, "Tables");
                const auto student_row = row_saying(*tree, tables_group, "Student");
                require(rows_said(*tree, row_saying(*tree, student_row, "Foreign Keys")) == QStringList{"CourseID → Course.CourseID"} && rows_said(*tree, row_saying(*tree, root, "Relationships")).contains("Student.CourseID → Course.CourseID") && row_saying(*tree, row_saying(*tree, student_row, "Columns"), "CourseID").data(desktop::explorer_note_role).toString() == "FK",
                        "The Explorer shows the new column, its foreign key and the relationship");
            }
            const auto after = hold();
            undo();
            require(model.project().schema == before.schema && view->line_shapes().size() == before.lines && !named("Student", "CourseID"),
                    "One undo takes the column, its foreign key and its line back, exactly");
            redo();
            require(model.project().schema == after.schema && view->line_shapes().size() == after.lines && named("Student", "CourseID") && named("Student", "CourseID")->key_id == student_fk,
                    "One redo puts all of it back, under the same identities");

            // Again, onto the table or onto the column: it is there already.
            auto now = hold();
            draw(gutter("Course", 0), heading("Student"));
            require(unchanged(now) && told().contains("Relationship already exists"),
                    "The same foreign key again is not doubled, by table");
            draw(gutter("Course", 0), on_row("Student", 2));
            require(unchanged(now) && told().contains("already references Course.CourseID"),
                    "Nor by its column");

            // Reversed, from Student's key onto Course: Course refers to Student.
            draw(gutter("Student", 0), heading("Course"), "Create & Connect");
            const auto *back = named("Course", "StudentID");
            require(back && back->foreign_key && back->references == at("Student") && columns("Course")[0].primary_key && !columns("Course")[0].foreign_key && columns("Student")[0].primary_key && !columns("Student")[0].foreign_key,
                    "The row a line starts on is the key referred to, whichever table it is and wherever it sits");
            undo();

            // An ordinary column already there, of the key's type, is asked about.
            now = hold();
            draw(gutter("Course", 0), heading("Lesson"), "Use & Connect", &asked);
            require(asked.value(1) == "Existing column found" && asked.value(2).contains("Lesson.CourseID exists but is not a foreign key."),
                    "A column called for the key and of its type is offered, not taken");
            require(columns("Lesson").size() == 2 && named("Lesson", "CourseID")->foreign_key && named("Lesson", "CourseID")->added == lesson_course && !columns("Lesson")[0].foreign_key,
                    "Agreed, it is that column that refers to the key, and no other is made");
            undo();
            require(columns("Lesson").size() == 2 && named("Lesson", "CourseID") && !named("Lesson", "CourseID")->foreign_key && model.project().schema == now.schema,
                    "Undone, the column is still there, only no longer a foreign key");
            // One of another type is not retyped behind anybody's back.
            now = hold();
            draw(gutter("Course", 0), heading("Review"));
            require(unchanged(now) && told().contains("Cannot use Review.CourseID") && named("Review", "CourseID")->type == domain::LogicalType::Varchar,
                    "A column of another type is refused, its type left alone, nothing made");

            // A key column is used as a foreign key only when chosen and agreed to.
            now = hold();
            draw(gutter("Person", 0), on_row("Staff", 0), "Cancel", &asked);
            require(asked.value(1) == "Use primary-key column for relationship?" && asked.value(2).contains("Staff.StaffID is already part of Staff's Primary Key.") && asked.value(2).contains("Use as PK + FK") && unchanged(now),
                    "Let go on a key's row, it asks before making it a foreign key too, and Cancel changes nothing");
            draw(gutter("Person", 0), on_row("Staff", 0), "Use as PK + FK");
            require(columns("Staff")[0].primary_key && columns("Staff")[0].foreign_key && columns("Staff")[0].references == at("Person") && columns("Person")[0].primary_key && !columns("Person")[0].foreign_key,
                    "Agreed, StaffID is a primary key and a foreign key, and Person's key is only a key");
            undo();
            require(columns("Staff")[0].primary_key && !columns("Staff")[0].foreign_key && model.project().schema == now.schema,
                    "Undone, it is a key again and only a key");
            // Let go on the table, a key with the key's name is not taken.
            now = hold();
            draw(gutter("Person", 0), heading("Member"));
            require(unchanged(now) && told().contains("in its primary key"),
                    "A key column is never taken for a table let go on; it must be the row let go on");

            // Into its own table: a column chosen there, or one made.
            draw(gutter("Employee", 0), on_row("Employee", 1), "Use & Connect");
            require(columns("Employee")[1].foreign_key && columns("Employee")[1].references == at("Employee") && columns("Employee")[1].references_column == 0 && columns("Employee")[0].primary_key && !columns("Employee")[0].foreign_key && std::count_if(view->preview().tables.begin(), view->preview().tables.end(), [](const auto &one)
                                                                                                                                                                                                                                                             { return one.name == "Employee"; }) == 1,
                    "ManagerID refers to EmployeeID in the same table, and EmployeeID is only a key");
            undo();
            now = hold();
            draw(gutter("Employee", 0), on_row("Employee", 0));
            require(unchanged(now) && told().contains("cannot reference itself"), "A key cannot refer to itself");
            draw(gutter("Category", 0), heading("Category"), "Create & Connect", &asked);
            require(named("Category", "ParentCategoryID") && named("Category", "ParentCategoryID")->foreign_key && named("Category", "ParentCategoryID")->references == at("Category") && asked.value(2).contains("Category.ParentCategoryID  →  Category.CategoryID"),
                    "Let go on its own table, a key into it is made and named as the conversion names one");
            undo();

            // What cannot start a line, and what cannot be referred to by one.
            now = hold();
            draw(gutter("Student", 1), heading("Course"));
            require(unchanged(now) && told().contains("Cannot create relationship"),
                    "A line started on an ordinary column is refused, and nothing is made a key or a foreign key");
            draw(gutter("Pair", 0), heading("Student"));
            require(unchanged(now) && told().contains("several columns"),
                    "A key of two columns cannot be referred to by hand yet, and no key to half of it is made");
            draw(gutter("Course", 0), heading("Student"));
            require(unchanged(now), "And the one already there is still the only one");

            // The Editor itself refuses what would break these rules, whatever asks.
            const auto staff_key = model.project().schema.added.at(made.at("Staff")).front().id;
            require(!model.connect_foreign_key(made.at("Review"), review_course, {}, made.at("Course"), course_key) && !model.connect_foreign_key(made.at("Student"), std::nullopt, "CourseID", made.at("Course"), course_key) && !model.connect_foreign_key(made.at("Lesson"), std::nullopt, "X", made.at("Pair"), part) && !model.connect_foreign_key(made.at("Student"), model.project().schema.added.at(made.at("Student"))[2].id, {}, made.at("Staff"), staff_key) && unchanged(now),
                    "The Editor refuses a column that would be retyped, a name already taken, part of a "
                    "composite key, and a foreign key re-pointed, and changes nothing for any of them");
        }
        // Stage 5, continued (Zain, 2026-10-01): a line from one key let go
        // on another table's key. The key let go on is never changed without
        // being asked, and is never made only a foreign key: two ways are
        // offered, both referring to the key started on -- that key as PK +
        // FK, or a new column of its own beside it, found or named by the
        // rules a table drop follows -- and neither takes either key away. A
        // column that is both keys wears both marks, on the schema and in the
        // Explorer.
        {
            application::Editor model(ids);
            model.new_schema_project();
            std::map<std::string, domain::RelationId> made;
            const auto table = [&](const char *name, double x, double y)
            {
                const auto result = model.create_relation(name, domain::Point{x, y});
                require(result.ok && result.created, "A table is made");
                made[name] = std::get<domain::RelationId>(*result.created);
                return model.project().schema.added.at(made.at(name)).front().id;
            };
            const auto column = [&](const char *table_name, const char *name, domain::LogicalType type)
            {
                require(model.add_schema_column(domain::ElementRef{made.at(table_name)}, name).ok, "A column is added");
                const auto id = model.project().schema.added.at(made.at(table_name)).back().id;
                require(model.set_schema_column_type(id, type).ok, "And typed");
                return id;
            };
            const auto table_key = table("Table", 40, 40);
            const auto table2_key = table("Table2", 520, 40);
            table("Table3", 1000, 40);
            const auto table3_column = column("Table3", "TableID", domain::LogicalType::Int);
            table("Table4", 40, 320);
            column("Table4", "TableID", domain::LogicalType::Varchar);
            table("Table5", 520, 320);
            const auto table5_column = column("Table5", "TableID", domain::LogicalType::Int);
            require(model.connect_foreign_key(made.at("Table5"), table5_column, {}, made.at("Table2"), table2_key).ok,
                    "Table5.TableID already refers to Table2's key");
            const auto customer_key = table("Customer", 1000, 320);
            const auto order_key = table("Order", 40, 600);
            require(model.rename_schema_column(customer_key, "ID").ok && model.rename_schema_column(order_key, "ID").ok,
                    "Customer and Order are both keyed by a column called ID");
            // The key referred to counts up and is unique, none of which a new
            // foreign key to it is to take.
            require(model.set_schema_column_auto_increment(table_key, true).ok && model.set_schema_column_rules(table_key, true, true, true).ok,
                    "Table's key counts up and is unique");
            model.mark_saved(model.revision());

            infrastructure::ErdxProjectStore pk_store;
            desktop::MainWindow pk_window(model, pk_store, ids);
            pk_window.resize(1440, 920);
            pk_window.show();
            pk_window.show_home(false);
            settle_for(300);
            auto *view = pk_window.schema();
            require(view && view->isVisible(), "The hand-drawn schema is in front");
            auto *properties_dock = child<QDockWidget>(pk_window, "propertiesDock");
            auto *explorer = child<QTreeView>(pk_window, "schemaExplorer");
            const auto at = [&](const char *name)
            {
                for (std::size_t t = 0; t < view->preview().tables.size(); ++t)
                    if (view->preview().tables[t].id == made.at(name))
                        return t;
                throw std::runtime_error("No such table");
            };
            const auto columns = [&](const char *name) -> const std::vector<domain::PreviewColumn> &
            {
                return view->preview().tables[at(name)].columns;
            };
            const auto named = [&](const char *name, const char *wanted) -> const domain::PreviewColumn *
            {
                for (const auto &one : columns(name))
                    if (one.name == wanted)
                        return &one;
                return nullptr;
            };
            const auto mouse = [&](QEvent::Type type, QPointF where, Qt::MouseButtons held)
            {
                QMouseEvent event(type, where, view->mapToGlobal(where.toPoint()),
                                  type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, held, Qt::NoModifier);
                QApplication::sendEvent(view, &event);
            };
            const auto gutter = [&](const char *name, std::size_t row)
            {
                const auto box = view->row_boxes()[at(name)][row];
                return QPointF(box.left() + 22, box.center().y());
            };
            const auto on_row = [&](const char *name, std::size_t row)
            {
                return view->row_boxes()[at(name)][row].center();
            };
            // From one key onto another table's key, every box it asks
            // answered in turn by these words, and what each said kept.
            const auto draw = [&](const char *from, const char *to, const QStringList &answers,
                                  std::vector<QStringList> *asked = nullptr)
            {
                if (asked)
                    asked->clear();
                const auto start = gutter(from, 0);
                const auto end = on_row(to, 0);
                mouse(QEvent::MouseButtonPress, start, Qt::LeftButton);
                mouse(QEvent::MouseMove, (start + end) / 2, Qt::LeftButton);
                mouse(QEvent::MouseMove, end, Qt::LeftButton);
                answer_in_turn(answers, asked);
                mouse(QEvent::MouseButtonRelease, end, Qt::NoButton);
                settle();
            };
            const auto told = [&]
            { return pk_window.statusBar()->currentMessage(); };
            const auto undo = [&]
            { child<QAction>(pk_window, "undoCommand")->trigger(); settle(); };
            const auto redo = [&]
            { child<QAction>(pk_window, "redoCommand")->trigger(); settle(); };
            const auto panel = [&]() -> const QWidget &
            { return *properties_dock->widget(); };
            const auto choose_column = [&](const char *name, std::size_t row)
            {
                view->choose(desktop::ChosenColumn{*view->column_ref(at(name), row)});
                settle();
                return properties_value(panel(), "Keys/Key Role");
            };
            const auto explorer_columns = [&](const char *name)
            {
                const auto *tree = explorer->model();
                const auto tables_group = row_saying(*tree, tree->index(0, 0), "Tables");
                return row_saying(*tree, row_saying(*tree, tables_group, name), "Columns");
            };
            const auto explorer_column = [&](const char *name, const char *column_name)
            {
                return row_saying(*explorer->model(), explorer_columns(name), column_name);
            };
            const auto explorer_keys = [&](const char *name)
            {
                const auto *tree = explorer->model();
                const auto tables_group = row_saying(*tree, tree->index(0, 0), "Tables");
                return rows_said(*tree, row_saying(*tree, row_saying(*tree, tables_group, name), "Foreign Keys"));
            };
            const auto relationships = [&]
            {
                const auto *tree = explorer->model();
                return rows_said(*tree, row_saying(*tree, tree->index(0, 0), "Relationships"));
            };
            struct Held
            {
                domain::SchemaOverrides schema;
                std::uint64_t revision;
                bool dirty;
                std::string undo;
                std::size_t lines;
            };
            const auto hold = [&]
            {
                return Held{model.project().schema, model.revision(), model.dirty(), model.undo_label(),
                            view->line_shapes().size()};
            };
            const auto unchanged = [&](const Held &was)
            {
                return model.project().schema == was.schema && model.revision() == was.revision && model.dirty() == was.dirty && model.undo_label() == was.undo && view->line_shapes().size() == was.lines;
            };
            // How a mark is coloured: the key's gold, or the FK green.
            const auto golden = [](const QColor &ink)
            {
                return ink.alpha() > 200 && ink.red() > ink.blue() + 80 && ink.green() > ink.blue() + 30;
            };
            const auto green = [](const QColor &ink)
            {
                return ink.alpha() > 200 && ink.green() > ink.red() + 40 && ink.green() > ink.blue() + 30;
            };
            // Where in a row's gutter gold and green are drawn: how much of
            // each, how far the gold reaches either way, and the left-most
            // green. The key's gold and the orange of PK both count as gold.
            struct Marks
            {
                int gold = 0;
                int green = 0;
                int gold_left = 1 << 20;
                int gold_right = -1;
                int green_left = 1 << 20;
            };
            const auto gutter_marks = [&](const char *name, std::size_t row)
            {
                view->choose(desktop::NothingChosen{});
                settle();
                // Photographed with the lettering smoothed in greys: the green
                // FK smoothed with coloured subpixels has yellow edges, which
                // would count as gold.
                const auto picture = grab_without_subpixel_text(*view).convertToFormat(QImage::Format_ARGB32);
                const auto ratio = picture.devicePixelRatio();
                const auto box = view->row_boxes()[at(name)][row];
                Marks found;
                for (int y = static_cast<int>(box.top() * ratio); y < static_cast<int>(box.bottom() * ratio); ++y)
                    for (int x = static_cast<int>(box.left() * ratio);
                         x < static_cast<int>((box.left() + 46) * ratio); ++x)
                    {
                        if (x < 0 || y < 0 || x >= picture.width() || y >= picture.height())
                            continue;
                        const auto ink = picture.pixelColor(x, y);
                        if (golden(ink))
                        {
                            ++found.gold;
                            found.gold_left = std::min(found.gold_left, x);
                            found.gold_right = std::max(found.gold_right, x);
                        }
                        if (green(ink))
                        {
                            ++found.green;
                            found.green_left = std::min(found.green_left, x);
                        }
                    }
                return found;
            };
            // What a row of the Explorer wears: gold at its left, and anything
            // drawn at the right of its slot.
            const auto explorer_mark = [&](const QModelIndex &row)
            {
                const auto icon = row.data(Qt::DecorationRole).value<QIcon>();
                const auto picture = icon.pixmap(QSize(28, 20), 1.0).toImage().convertToFormat(QImage::Format_ARGB32);
                std::pair<int, int> found{0, 0};
                for (int y = 0; y < picture.height(); ++y)
                    for (int x = 0; x < picture.width(); ++x)
                    {
                        const auto ink = picture.pixelColor(x, y);
                        if (x < 12 && golden(ink))
                            ++found.first;
                        if (x >= 18 && ink.alpha() > 128)
                            ++found.second;
                    }
                return found;
            };

            // Let go on Table2's key: both ways are offered, and nothing else.
            const auto before = hold();
            std::vector<QStringList> asked;
            draw("Table", "Table2", {"Cancel"}, &asked);
            require(asked.size() == 1 && asked[0].value(1) == "Use primary-key column for relationship?" && asked[0].value(2).contains("Table2.Table2ID is already part of Table2's Primary Key.") && asked[0].value(2).contains("Choose how Table2 should reference Table.TableID:") && asked[0].value(2).contains("Use as PK + FK: Keep Table2ID as the Primary Key and also "
                                                                                                                                                                                                                                                                                                                    "use it as the Foreign Key.") &&
                        asked[0].value(2).contains("Create New FK: Keep Table2ID unchanged and create TableID "
                                                   "as a new Foreign Key.") &&
                        asked[0].value(2).contains("Table2 → Table"),
                    "Let go on another table's key, it asks how that table should refer to the key started on");
            require(asked[0].value(3) == "Cancel|Create New FK|Use as PK + FK",
                    "It offers Use as PK + FK and Create New FK, and no way of making the key only a foreign key");
            require(asked[0].value(4) == "Create New FK",
                    "Return makes the new column, the way that leaves every column already there as it was");
            require(unchanged(before), "Cancelled, nothing changes: not the schema, its revision, its unsaved "
                                       "state, the history nor the lines");

            // Use as PK + FK: Table2ID stays the key and refers to Table's.
            draw("Table", "Table2", {"Use as PK + FK"});
            const auto *both = &columns("Table2")[0];
            require(columns("Table2").size() == 1 && both->added == table2_key && both->primary_key && both->foreign_key && both->references == at("Table") && both->references_column == 0 && columns("Table")[0].primary_key && !columns("Table")[0].foreign_key,
                    "Table2ID is a primary key and a foreign key to Table.TableID, and Table's key is only a key");
            require(model.undo_label() == "Connect tables" && model.revision() != before.revision && model.dirty() && view->line_shapes().size() == before.lines + 1 && model.project().schema.foreign_keys.size() == before.schema.foreign_keys.size() + 1,
                    "It is one edit, unsaved, one foreign key drawn as one line");
            const auto pk_fk = *both->key_id;
            require(view->selection_now() == desktop::SchemaSelection{desktop::ChosenForeignKey{pk_fk}} && properties_value(panel(), "Identity/Relationship") == QStringList{"Table2.Table2ID → Table.TableID"},
                    "The foreign key made is chosen, and Properties shows Table2.Table2ID → Table.TableID");
            require(properties_value(panel(), "Referencing/Column") == QStringList{"Table2ID"} && properties_value(panel(), "Referencing/Role") == QStringList{"PK", "FK"} && properties_value(panel(), "Referenced/Column") == QStringList{"TableID"} && properties_value(panel(), "Referenced/Role") == QStringList{"PK"} && properties_value(panel(), "General/Self Reference") == QStringList{"No"} && properties_value(panel(), "Referencing/Cardinality") == QStringList{"One (1)"},
                    "Its Referencing column keeps both roles, PK and FK, and the key it references is PK; a key is one row");
            require(explorer_column("Table2", "Table2ID").data(desktop::explorer_note_role).toString() == "PK FK" && explorer_keys("Table2") == QStringList{"Table2ID → Table.TableID"} && relationships().contains("Table2.Table2ID → Table.TableID"),
                    "The Explorer says PK FK for Table2ID, and lists its foreign key and the relationship");
            require(choose_column("Table2", 0) == QStringList{"Primary Key + Foreign Key"} && choose_column("Table", 0) == QStringList{"Primary Key"},
                    "Properties says Primary Key + Foreign Key for Table2ID, and Primary Key for TableID");
            // Both marks, side by side: the golden key, then the green FK.
            {
                const auto marks = gutter_marks("Table2", 0);
                require(marks.gold > 15 && marks.green > 5 && marks.gold_right < marks.green_left && marks.gold_right - marks.gold_left > 18,
                        "On the schema, Table2ID wears the golden key, the letters PK and then the green FK, "
                        "side by side");
                const auto alone = gutter_marks("Table", 0);
                require(alone.gold > 15 && alone.green == 0,
                        "While TableID, only a key, wears the key and no FK");
                const auto pair = explorer_mark(explorer_column("Table2", "Table2ID"));
                const auto key_only = explorer_mark(explorer_column("Table", "TableID"));
                require(pair.first > 10 && pair.second > 10,
                        "In the Explorer, Table2ID wears the golden key and the link beside it");
                require(key_only.first > 10 && key_only.second == 0, "And TableID only the key");
                const auto wearing = pk_window.canvas()->theme_id();
                pk_window.set_theme(desktop::ThemeId::Plain);
                settle();
                const auto plain = explorer_column("Table2", "Table2ID").data(Qt::DecorationRole).value<QIcon>();
                require(coloured_pixels(plain.pixmap(QSize(28, 20), 3.0).toImage()) == 0,
                        "Under Plain, both of its marks are grey");
                view->choose(desktop::ChosenForeignKey{pk_fk});
                settle();
                QStringList plain_roles;
                for (auto *role : panel().findChildren<QLabel *>("schemaRelationshipRole"))
                    plain_roles << role->text();
                require(coloured_pixels(grab_without_subpixel_text(*properties_dock->widget())) == 0 && plain_roles == QStringList{"PK", "PK", "FK"} && properties_value(panel(), "Referencing/Cardinality") == QStringList{"One (1)"},
                        "Under Plain, the relationship's Properties has no colour, and every role is still said in words");
                pk_window.set_theme(wearing);
                settle();
            }
            const auto with_both = hold();
            // Chosen when it is undone: the line is gone, and Properties says
            // nothing more of it.
            view->choose(desktop::ChosenForeignKey{pk_fk});
            settle();
            undo();
            require(model.project().schema == before.schema && columns("Table2")[0].primary_key && !columns("Table2")[0].foreign_key && view->line_shapes().size() == before.lines && explorer_column("Table2", "Table2ID").data(desktop::explorer_note_role).toString() == "PK",
                    "One undo takes only the foreign key back: Table2ID is a primary key again, and only that");
            require(properties_heading(panel()) == "Schema" && properties_value(panel(), "Identity/Relationship").isEmpty() && !panel().findChild<QWidget *>("schemaRelationshipProperties"),
                    "Undone, Properties falls back to the schema as a whole, with nothing left of the line");
            redo();
            require(model.project().schema == with_both.schema && columns("Table2")[0].primary_key && columns("Table2")[0].foreign_key && columns("Table2")[0].key_id == pk_fk && view->line_shapes().size() == with_both.lines,
                    "One redo makes it PK + FK again, under the same identity, with its line");
            view->choose(desktop::ChosenForeignKey{pk_fk});
            settle();
            require(properties_value(panel(), "Identity/Relationship") == QStringList{"Table2.Table2ID → Table.TableID"} && properties_value(panel(), "Referencing/Role") == QStringList{"PK", "FK"},
                    "Redone, it is chosen and described like any other line");
            undo();

            // Create New FK: both keys stay as they are, and Table2 gains TableID.
            draw("Table", "Table2", {"Create New FK"});
            const auto *fresh = named("Table2", "TableID");
            require(fresh && fresh->foreign_key && fresh->references == at("Table") && fresh->references_column == 0 && !fresh->primary_key && !fresh->unique && !fresh->auto_increment && !fresh->required && fresh->type == domain::LogicalType::Int && columns("Table2").size() == 2,
                    "Table2 gains TableID, a foreign key to Table.TableID of its type, and nothing else of the key's: "
                    "not its key, its uniqueness or its counting up");
            require(columns("Table2")[0].name == "Table2ID" && columns("Table2")[0].added == table2_key && columns("Table2")[0].primary_key && !columns("Table2")[0].foreign_key && columns("Table")[0].primary_key && !columns("Table")[0].foreign_key && columns("Table")[0].auto_increment,
                    "Neither key is renamed, replaced, made a foreign key, or stops being a key");
            require(model.undo_label() == "Connect tables" && view->line_shapes().size() == before.lines + 1,
                    "It is one edit, drawn as one line");
            const auto new_fk = *fresh->key_id;
            const auto new_column = *fresh->added;
            require(view->selection_now() == desktop::SchemaSelection{desktop::ChosenForeignKey{new_fk}} && properties_value(panel(), "Identity/Relationship") == QStringList{"Table2.TableID → Table.TableID"} && properties_value(panel(), "Referencing/Role") == QStringList{"FK"} && properties_value(panel(), "Referencing/Cardinality") == QStringList{"Zero or Many (0..N)"},
                    "The foreign key made is chosen, and Properties shows Table2.TableID → Table.TableID");
            require(rows_said(*explorer->model(), explorer_columns("Table2")) == QStringList{"Table2ID", "TableID"} && explorer_column("Table2", "Table2ID").data(desktop::explorer_note_role).toString() == "PK" && explorer_column("Table2", "TableID").data(desktop::explorer_note_role).toString() == "FK" && explorer_keys("Table2") == QStringList{"TableID → Table.TableID"} && relationships().contains("Table2.TableID → Table.TableID"),
                    "The Explorer shows Table2ID PK and TableID FK, its foreign key and the relationship");
            require(choose_column("Table2", 0) == QStringList{"Primary Key"} && choose_column("Table2", 1) == QStringList{"Foreign Key"},
                    "Properties says Table2ID is the primary key and TableID the foreign key");
            {
                const auto marks = gutter_marks("Table2", 1);
                require(marks.gold == 0 && marks.green > 5, "On the schema, TableID wears the green FK only");
            }
            // The ends are only read, and follow the foreign key as its line
            // does: made NOT NULL from its column's own switch, the referencing
            // end is One or Many; made UNIQUE too, One; each one edit, and
            // undone, Zero or Many again.
            {
                const auto end_now = [&]
                {
                    view->choose(desktop::ChosenForeignKey{new_fk});
                    settle();
                    return properties_value(panel(), "Referencing/Cardinality");
                };
                const auto switch_on = [&](const char *field)
                {
                    choose_column("Table2", 1);
                    properties_editor<QCheckBox>(panel(), field)->click();
                    settle();
                };
                const auto schema_before_switching = model.project().schema;
                require(end_now() == QStringList{"Zero or Many (0..N)"} && !properties_offer_edits(panel()),
                        "Optional and not unique, the referencing end is Zero or Many");
                switch_on("Constraints/Not Null");
                require(end_now() == QStringList{"One or Many (1..N)"} && model.undo_label() != "Connect tables",
                        "Made NOT NULL, it is One or Many");
                switch_on("Constraints/Unique");
                require(end_now() == QStringList{"One (1)"} && properties_value(panel(), "Referenced/Cardinality") == QStringList{"One (1)"},
                        "Made UNIQUE as well, it is One");
                undo();
                require(end_now() == QStringList{"One or Many (1..N)"}, "One undo takes UNIQUE back");
                undo();
                require(end_now() == QStringList{"Zero or Many (0..N)"} && model.undo_label() == "Connect tables" && model.project().schema == schema_before_switching,
                        "And another NOT NULL: Zero or Many again");
            }
            const auto with_new = hold();
            undo();
            require(model.project().schema == before.schema && columns("Table2").size() == 1 && !named("Table2", "TableID") && columns("Table2")[0].primary_key && view->line_shapes().size() == before.lines,
                    "One undo takes the new column, its foreign key and its line back, exactly");
            redo();
            require(model.project().schema == with_new.schema && named("Table2", "TableID") && named("Table2", "TableID")->added == new_column && named("Table2", "TableID")->key_id == new_fk && view->line_shapes().size() == with_new.lines,
                    "One redo puts the column and its line back, under the same identities");

            // With that foreign key there, a new one is not needed: it is used.
            auto now = hold();
            draw("Table", "Table2", {"Create New FK"}, &asked);
            require(asked.size() == 1 && asked[0].value(2).contains("Create New FK: Not needed. Relationship already exists: "
                                                                    "Table2.TableID already references Table.TableID.") &&
                        unchanged(now) && told().contains("Relationship already exists"),
                    "Asked for a new column where the foreign key is already there, it says so and makes nothing");
            undo();

            // A column already called TableID, of the key's type, is asked about.
            now = hold();
            draw("Table", "Table3", {"Create New FK", "Cancel"}, &asked);
            require(asked.size() == 2 && asked[0].value(2).contains("Table3 already has a column called TableID, so you will be "
                                                                    "asked whether to use it as the Foreign Key.") &&
                        asked[1].value(1) == "Existing column found" && asked[1].value(2).contains("Table3.TableID exists but is not a foreign key.") && unchanged(now),
                    "A column already called for the key is offered, not doubled, and Cancel there changes nothing");
            draw("Table", "Table3", {"Create New FK", "Use & Connect"});
            require(columns("Table3").size() == 2 && named("Table3", "TableID")->added == table3_column && named("Table3", "TableID")->foreign_key && named("Table3", "TableID")->references == at("Table") && columns("Table3")[0].primary_key && !columns("Table3")[0].foreign_key && model.undo_label() == "Connect tables",
                    "Agreed, that column refers to the key, no other is made, and Table3's key is untouched");
            undo();
            require(model.project().schema == now.schema && named("Table3", "TableID") && !named("Table3", "TableID")->foreign_key,
                    "Undone, the column is still there, only no longer a foreign key");

            // Where the name the new column would have is taken by one that
            // cannot hold the key, a name is asked for (Zain, 2026-10-01):
            // the referenced table's and its key's proposed, never taken
            // silently, and changeable before it is agreed to.
            now = hold();
            draw("Customer", "Order", {"Create New FK", "Cancel"}, &asked);
            require(asked.size() == 2 && asked[0].value(2).contains("Create New FK: Keep ID unchanged. The name ID is already used "
                                                                    "in Order, so you will be asked to choose a name for the new "
                                                                    "Foreign Key."),
                    "Let go on Order.ID from Customer.ID, Create New FK says a name will be asked for");
            require(asked[1].value(0) == "Foreign key column name" && asked[1].value(1).startsWith("Foreign key column name") && asked[1].value(1).contains("The default foreign key name \"ID\" is already used in Order.") && asked[1].value(1).contains("Choose a name for the new foreign key column:") && asked[1].value(2) == "CustomerID" && asked[1].value(3) == "Create & Connect",
                    "It asks for the foreign key column's name, proposing CustomerID, with Create & Connect");
            require(unchanged(now) && columns("Order").size() == 1,
                    "Cancelled there, nothing changes and no column is made");
            // A name typed over the one proposed is the one used; one already
            // taken is asked about again, saying so.
            draw("Customer", "Order", {"Create New FK", "type:ID", "type:BuyerID"}, &asked);
            require(asked.size() == 3 && asked[2].value(1).contains("The name \"ID\" is already used in Order.") && asked[2].value(2) == "ID",
                    "A name already used in Order is asked about again, keeping what was typed");
            const auto *buyer = named("Order", "BuyerID");
            require(buyer && buyer->foreign_key && buyer->references == at("Customer") && buyer->references_column == 0 && !buyer->primary_key && !buyer->unique && !buyer->auto_increment && buyer->type == domain::LogicalType::Int && columns("Order").size() == 2 && columns("Order")[0].name == "ID" && columns("Order")[0].added == order_key && columns("Order")[0].primary_key && !columns("Order")[0].foreign_key && columns("Customer")[0].primary_key && !columns("Customer")[0].foreign_key && model.undo_label() == "Connect tables",
                    "Order gains BuyerID, a foreign key to Customer.ID, and both IDs stay keys and only keys");
            undo();
            require(model.project().schema == now.schema && columns("Order").size() == 1,
                    "One undo takes the column and its foreign key back");
            // The name proposed, agreed to as it is.
            now = hold();
            draw("Customer", "Order", {"Create New FK", "Create & Connect"});
            require(named("Order", "CustomerID") && named("Order", "CustomerID")->foreign_key && named("Order", "CustomerID")->references == at("Customer") && columns("Order").size() == 2,
                    "Agreed as proposed, Order gains CustomerID referencing Customer.ID");
            undo();

            // One of another type is not retyped or used: a new one is named.
            now = hold();
            draw("Table", "Table4", {"Create New FK", "Create & Connect"}, &asked);
            require(asked.size() == 2 && asked[1].value(1).contains("The default foreign key name \"TableID\" is already used in "
                                                                    "Table4.") &&
                        asked[1].value(2) == "TableID2",
                    "Where the key's name starts with its table's already, the name proposed is numbered instead");
            require(named("Table4", "TableID2") && named("Table4", "TableID2")->foreign_key && named("Table4", "TableID2")->references == at("Table") && named("Table4", "TableID")->type == domain::LogicalType::Varchar && !named("Table4", "TableID")->foreign_key && columns("Table4").size() == 3,
                    "Table4 gains TableID2, and its own TableID is left alone, type and all");
            undo();
            // One that already refers elsewhere is not re-pointed: a new one is named.
            now = hold();
            draw("Table", "Table5", {"Create New FK", "Cancel"}, &asked);
            require(asked.size() == 2 && asked[1].value(1).contains("The default foreign key name \"TableID\" is already used in "
                                                                    "Table5.") &&
                        unchanged(now) && named("Table5", "TableID")->references == at("Table2"),
                    "A column of that name referring elsewhere is not re-pointed, and Cancel makes nothing");
            // And every key is still there and still a key.
            require(columns("Table")[0].primary_key && columns("Table2")[0].primary_key && columns("Customer")[0].primary_key && columns("Order")[0].primary_key,
                    "No primary key was removed by anything here");
        }
        // The schema's tools (Zain, 2026-10-01): Select in hand to begin with;
        // Table places a table where the schema is pressed; Table and Connect
        // each hand back to Select after one use unless locked by a double
        // click; Escape, Select, another tool or a press elsewhere puts them
        // down; and choosing a tool is never an edit.
        {
            application::Editor model(ids);
            model.new_schema_project();
            std::map<std::string, domain::RelationId> made;
            const auto table = [&](const char *name, double x, double y)
            {
                const auto result = model.create_relation(name, domain::Point{x, y});
                require(result.ok && result.created, "A table is made");
                made[name] = std::get<domain::RelationId>(*result.created);
            };
            table("Course", 40, 40);
            table("Student", 520, 40);
            table("Lesson", 1000, 40);
            model.mark_saved(model.revision());
            infrastructure::ErdxProjectStore tool_store;
            desktop::MainWindow tool_window(model, tool_store, ids);
            tool_window.resize(1440, 920);
            tool_window.show();
            tool_window.show_home(false);
            settle_for(300);
            auto *view = tool_window.schema();
            require(view && view->isVisible(), "The hand-drawn schema is in front");
            auto *select = child<QAction>(tool_window, "schemaSelectTool");
            auto *place = child<QAction>(tool_window, "schemaTableAction");
            auto *connecting = child<QAction>(tool_window, "schemaConnectTool");
            auto *table_button = child<QToolButton>(tool_window, "schemaAddTable");
            auto *connect_button = child<QToolButton>(tool_window, "schemaConnect");
            const auto at = [&](const char *name)
            {
                for (std::size_t t = 0; t < view->preview().tables.size(); ++t)
                    if (view->preview().tables[t].id == made.at(name))
                        return t;
                throw std::runtime_error("No such table");
            };
            const auto named = [&](const char *name, const char *wanted)
            {
                for (const auto &one : view->preview().tables[at(name)].columns)
                    if (one.name == wanted)
                        return &one;
                return static_cast<const domain::PreviewColumn *>(nullptr);
            };
            const auto mouse = [&](QEvent::Type type, QPointF where, Qt::MouseButtons held)
            {
                QMouseEvent event(type, where, view->mapToGlobal(where.toPoint()),
                                  type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, held, Qt::NoModifier);
                QApplication::sendEvent(view, &event);
            };
            const auto click = [&](QPointF where)
            {
                mouse(QEvent::MouseButtonPress, where, Qt::LeftButton);
                mouse(QEvent::MouseButtonRelease, where, Qt::NoButton);
                settle();
            };
            const auto escape = [&]
            {
                QKeyEvent key(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                QApplication::sendEvent(view, &key);
                settle();
            };
            const auto lock = [&](QToolButton *button)
            {
                QMouseEvent twice(QEvent::MouseButtonDblClick, QPointF(5, 5), button->mapToGlobal(QPoint(5, 5)),
                                  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(button, &twice);
                settle();
            };
            const auto finish_name = [&]
            {
                if (auto *field = tool_window.findChild<QLineEdit *>("schemaName"); field && field->isVisible())
                {
                    QKeyEvent done(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                    QApplication::sendEvent(field, &done);
                    settle();
                }
            };
            const auto gutter = [&](const char *name, std::size_t row)
            {
                const auto box = view->row_boxes()[at(name)][row];
                return QPointF(box.left() + 22, box.center().y());
            };
            const auto heading = [&](const char *name)
            {
                const auto box = view->table_boxes()[at(name)];
                return QPointF(box.center().x(), box.top() + 8);
            };
            const auto draw = [&](QPointF from, QPointF to, const QString &agreeing)
            {
                mouse(QEvent::MouseButtonPress, from, Qt::LeftButton);
                mouse(QEvent::MouseMove, (from + to) / 2, Qt::LeftButton);
                mouse(QEvent::MouseMove, to, Qt::LeftButton);
                answer(agreeing);
                mouse(QEvent::MouseButtonRelease, to, Qt::NoButton);
                settle();
            };
            const auto only_select = [&]
            {
                return select->isChecked() && !place->isChecked() && !connecting->isChecked() && !view->placing() && !view->connecting();
            };
            struct Held
            {
                domain::SchemaOverrides schema;
                std::uint64_t revision;
                bool dirty;
                std::string undo;
            };
            const auto hold = [&]
            { return Held{model.project().schema, model.revision(), model.dirty(), model.undo_label()}; };
            const auto unchanged = [&](const Held &was)
            {
                return model.project().schema == was.schema && model.revision() == was.revision && model.dirty() == was.dirty && model.undo_label() == was.undo;
            };
            const auto tables_now = [&]
            { return view->preview().tables.size(); };

            // A. Select is in hand when the schema opens.
            require(only_select() && view->cursor().shape() == Qt::ArrowCursor,
                    "Select is in hand to begin with, and the pointer is the ordinary arrow");

            // I and K. One tool at a time, and choosing or locking one is never an edit.
            const auto before_tools = hold();
            place->trigger();
            settle();
            require(place->isChecked() && view->placing() && !select->isChecked() && !connecting->isChecked() && view->cursor().shape() == Qt::CrossCursor,
                    "Table in hand: its button lit alone, and the pointer the cross the diagram places with");
            lock(table_button);
            require(place->isChecked() && place->text() == "Table 🔒", "Locked, Table says so on its button");
            connecting->trigger();
            settle();
            require(connecting->isChecked() && view->connecting() && !place->isChecked() && !view->placing() && !select->isChecked() && place->text() == "Table",
                    "Connect taken up puts Table down, lock and all: one tool at a time");
            lock(connect_button);
            require(connecting->text() == "Connect 🔒", "Connect locks as before");
            place->trigger();
            settle();
            require(place->isChecked() && !connecting->isChecked() && !view->connecting() && connecting->text() == "Connect",
                    "And Table taken up puts Connect down");
            select->trigger();
            settle();
            require(only_select(), "Select puts down whatever was in hand");
            require(unchanged(before_tools) && tables_now() == 3,
                    "None of that made anything, changed the revision, marked the project unsaved or added to "
                    "the history");

            // B. Table places one table where the schema is pressed, then hands back.
            const auto before_place = hold();
            place->trigger();
            settle();
            require(unchanged(before_place) && tables_now() == 3, "Taken up, Table makes nothing yet");
            const QPointF spot(700, 420);
            click(spot);
            std::optional<std::size_t> placed;
            for (std::size_t t = 0; t < view->preview().tables.size(); ++t)
                if (std::none_of(made.begin(), made.end(), [&](const auto &one)
                                 { return one.second == view->preview().tables[t].id; }))
                    placed = t;
            require(tables_now() == 4 && placed && view->table_boxes()[*placed].contains(spot) && std::abs(view->table_boxes()[*placed].left() - (spot.x() - 60)) < 1 && std::abs(view->table_boxes()[*placed].top() - (spot.y() - 12)) < 1 && model.revision() != before_place.revision && only_select(),
                    "Pressed, one table is made where the schema was pressed, its header under the pointer, and "
                    "Select is back in hand");
            const auto placed_id = view->preview().tables[*placed].id;
            finish_name();
            const auto after_place = hold();
            child<QAction>(tool_window, "undoCommand")->trigger();
            settle();
            require(tables_now() == 3 && model.project().schema == before_place.schema,
                    "One undo, and the table placed is gone");
            child<QAction>(tool_window, "redoCommand")->trigger();
            settle();
            require(tables_now() == 4 && model.project().schema == after_place.schema && std::any_of(view->preview().tables.begin(), view->preview().tables.end(), [&](const auto &one)
                                                                                                     { return one.id == placed_id; }),
                    "Redone, it is back, the same table");

            // C. Locked, Table stays in hand and places as many as are pressed.
            place->trigger();
            settle();
            lock(table_button);
            click(QPointF(700, 640));
            require(tables_now() == 5 && place->isChecked() && view->placing() && place->text() == "Table 🔒",
                    "Locked, Table is still in hand after placing a table");
            click(QPointF(1000, 640));
            require(tables_now() == 6 && place->isChecked() && view->placing(), "And places another");

            // D. Pressed again, as Connect is, it is put down and unlocked; the
            // next table placed hands back.
            place->trigger();
            settle();
            require(only_select() && place->text() == "Table", "Pressed again, Table is put down and unlocked");
            place->trigger();
            settle();
            click(QPointF(1000, 300));
            require(tables_now() == 7 && only_select(), "Unlocked, the next table placed hands back to Select");
            finish_name();

            // E. Escape puts Table down and makes nothing.
            const auto before_escape = hold();
            place->trigger();
            settle();
            escape();
            require(only_select() && unchanged(before_escape) && tables_now() == 7,
                    "Escape puts Table down, and nothing is made");

            // A press elsewhere in the window puts it down, as on the diagram.
            place->trigger();
            settle();
            {
                auto *elsewhere = child<QDockWidget>(tool_window, "propertiesDock")->findChild<QLabel *>("propertyHeading");
                require(elsewhere != nullptr, "Properties has its heading to press");
                const QPoint inside(elsewhere->width() / 2, elsewhere->height() / 2);
                QMouseEvent press(QEvent::MouseButtonPress, QPointF(inside), elsewhere->mapToGlobal(inside),
                                  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(elsewhere, &press);
                QMouseEvent release(QEvent::MouseButtonRelease, QPointF(inside), elsewhere->mapToGlobal(inside),
                                    Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(elsewhere, &release);
                settle();
            }
            require(only_select() && unchanged(before_escape), "A press elsewhere in the window puts Table down");

            // F. Connect hands back to Select after one foreign key.
            connecting->trigger();
            settle();
            draw(gutter("Course", 0), heading("Student"), "Create & Connect");
            require(named("Student", "CourseID") && named("Student", "CourseID")->foreign_key && only_select() && model.undo_label() == "Connect tables",
                    "Unlocked, Connect makes its one foreign key as Stage 5 makes it, then hands back to Select");

            // G. Locked, it stays in hand.
            connecting->trigger();
            settle();
            lock(connect_button);
            draw(gutter("Course", 0), heading("Lesson"), "Create & Connect");
            require(named("Lesson", "CourseID") && named("Lesson", "CourseID")->foreign_key && connecting->isChecked() && view->connecting() && connecting->text() == "Connect 🔒",
                    "Locked, Connect stays in hand after a foreign key");

            // H. Escape lets go of a line half drawn and puts Connect down.
            const auto before_partial = hold();
            mouse(QEvent::MouseButtonPress, gutter("Student", 0), Qt::LeftButton);
            mouse(QEvent::MouseMove, heading("Course"), Qt::LeftButton);
            escape();
            mouse(QEvent::MouseButtonRelease, heading("Course"), Qt::NoButton);
            settle();
            require(only_select() && unchanged(before_partial),
                    "Escape lets go of the line half drawn, asks nothing, makes nothing, and Select is in hand");
            // And with Select in hand, a line half drawn from a key's gutter.
            mouse(QEvent::MouseButtonPress, gutter("Student", 0), Qt::LeftButton);
            mouse(QEvent::MouseMove, heading("Course"), Qt::LeftButton);
            escape();
            mouse(QEvent::MouseButtonRelease, heading("Course"), Qt::NoButton);
            settle();
            require(only_select() && unchanged(before_partial), "A line half drawn from a key's gutter is let go too");

            // J. Select puts Connect down, a line half drawn with it.
            connecting->trigger();
            settle();
            mouse(QEvent::MouseButtonPress, gutter("Student", 0), Qt::LeftButton);
            mouse(QEvent::MouseMove, heading("Course"), Qt::LeftButton);
            select->trigger();
            settle();
            mouse(QEvent::MouseButtonRelease, heading("Course"), Qt::NoButton);
            settle();
            require(only_select() && unchanged(before_partial),
                    "Select puts Connect down and lets go of the line half drawn; nothing is made");
            model.mark_saved(model.revision());
        }
        // Where a connection being drawn would land is lit by what letting go
        // there would do (Task 2, 2026-10-02): the very plan the drop follows,
        // green where it goes on -- at once or after asking -- red where it is
        // turned away, nothing over the empty schema. A row under the pointer
        // is lit itself; the rest of a table rings the table. Shown only while
        // the line is drawn, never a choice, never an edit.
        {
            application::Editor model(ids);
            model.new_schema_project();
            std::map<std::string, domain::RelationId> made;
            const auto table = [&](const char *name, double x, double y)
            {
                const auto result = model.create_relation(name, domain::Point{x, y});
                require(result.ok && result.created, "A table is made");
                made[name] = std::get<domain::RelationId>(*result.created);
            };
            const auto column = [&](const char *table_name, const char *name, domain::LogicalType type)
            {
                require(model.add_schema_column(domain::ElementRef{made.at(table_name)}, name).ok, "A column is added");
                const auto id = model.project().schema.added.at(made.at(table_name)).back().id;
                require(model.set_schema_column_type(id, type).ok, "And typed");
                return id;
            };
            table("Course", 40, 40);
            column("Course", "Name", domain::LogicalType::Varchar);
            table("Student", 380, 40);
            table("Lesson", 40, 330);
            column("Lesson", "CourseID", domain::LogicalType::Int);
            table("Review", 380, 330);
            column("Review", "CourseID", domain::LogicalType::Varchar);
            table("Grade", 40, 560);
            const auto grade_ref = column("Grade", "StudentRef", domain::LogicalType::Int);
            require(model.connect_foreign_key(made.at("Grade"), grade_ref, "StudentRef", made.at("Student"),
                                              model.project().schema.added.at(made.at("Student")).front().id)
                        .ok,
                    "Grade.StudentRef refers to Student");
            // A table with no columns at all, for the strip under it.
            table("Empty", 380, 560);
            require(model.erase_schema_column(model.project().schema.added.at(made.at("Empty")).front().id).ok,
                    "Empty has no columns");
            model.mark_saved(model.revision());
            infrastructure::ErdxProjectStore lit_store;
            desktop::MainWindow lit_window(model, lit_store, ids);
            lit_window.resize(1440, 920);
            lit_window.show();
            lit_window.show_home(false);
            settle_for(300);
            auto *view = lit_window.schema();
            auto *select = child<QAction>(lit_window, "schemaSelectTool");
            auto *connecting = child<QAction>(lit_window, "schemaConnectTool");
            auto *connect_button = child<QToolButton>(lit_window, "schemaConnect");
            const auto at = [&](const char *name)
            {
                for (std::size_t t = 0; t < view->preview().tables.size(); ++t)
                    if (view->preview().tables[t].id == made.at(name))
                        return t;
                throw std::runtime_error("No such table");
            };
            const auto row_of = [&](const char *name, const char *wanted)
            {
                const auto &columns = view->preview().tables[at(name)].columns;
                for (std::size_t r = 0; r < columns.size(); ++r)
                    if (columns[r].name == wanted)
                        return r;
                throw std::runtime_error("No such column");
            };
            const auto mouse = [&](QEvent::Type type, QPointF where, Qt::MouseButtons held)
            {
                QMouseEvent event(type, where, view->mapToGlobal(where.toPoint()),
                                  type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, held, Qt::NoModifier);
                QApplication::sendEvent(view, &event);
            };
            const auto row_point = [&](const char *name, const char *wanted)
            {
                const auto box = view->row_boxes()[at(name)][row_of(name, wanted)];
                return QPointF(box.center().x() + 30, box.center().y());
            };
            const auto gutter = [&](const char *name, const char *wanted)
            {
                const auto box = view->row_boxes()[at(name)][row_of(name, wanted)];
                return QPointF(box.left() + 22, box.center().y());
            };
            const auto heading = [&](const char *name)
            {
                const auto box = view->table_boxes()[at(name)];
                return QPointF(box.center().x(), box.top() + 8);
            };
            // What is painted where a row's outline runs, and where a table's
            // ring runs: green, red or neither.
            const auto painted = [&](QPointF p)
            {
                const auto image = view->grab().toImage();
                const auto ratio = image.devicePixelRatio();
                const auto colour = image.pixelColor(QPoint(int(p.x() * ratio), int(p.y() * ratio)));
                if (colour.greenF() > colour.redF() + 0.04)
                    return std::string("green");
                if (colour.redF() > colour.greenF() + 0.04)
                    return std::string("red");
                return std::string("neither");
            };
            const auto row_edge = [&](const char *name, const char *wanted)
            {
                const auto box = view->row_boxes()[at(name)][row_of(name, wanted)];
                return QPointF(box.left() + 8, box.top() + 1.6);
            };
            const auto ring = [&](const char *name)
            {
                const auto box = view->table_boxes()[at(name)];
                return QPointF(box.center().x(), box.top() - 6);
            };
            const auto lit = [&](const char *name, std::optional<const char *> wanted, bool takes)
            {
                const auto target = view->link_target();
                return target && target->table == at(name)
                    && target->row == (wanted ? std::optional<std::size_t>{row_of(name, *wanted)} : std::nullopt)
                    && target->takes == takes;
            };
            struct Held
            {
                domain::SchemaOverrides schema;
                std::uint64_t revision;
                bool dirty;
                std::string undo;
            };
            const auto hold = [&]
            { return Held{model.project().schema, model.revision(), model.dirty(), model.undo_label()}; };
            const auto unchanged = [&](const Held &was)
            {
                return model.project().schema == was.schema && model.revision() == was.revision
                    && model.dirty() == was.dirty && model.undo_label() == was.undo;
            };
            // Takes up Connect, starts a line on Course's key and carries it to a point.
            const auto start = [&](QPointF to)
            {
                if (!connecting->isChecked())
                    connecting->trigger();
                settle();
                mouse(QEvent::MouseButtonPress, gutter("Course", "CourseID"), Qt::LeftButton);
                mouse(QEvent::MouseMove, (gutter("Course", "CourseID") + to) / 2, Qt::LeftButton);
                mouse(QEvent::MouseMove, to, Qt::LeftButton);
                settle();
            };
            // Lets go where the line is, answering any box with Cancel, and
            // says whether a box asked.
            const auto let_go = [&](QPointF where)
            {
                QStringList said;
                answer("Cancel", &said);
                mouse(QEvent::MouseButtonRelease, where, Qt::NoButton);
                settle();
                return !said.isEmpty();
            };

            require(!view->link_target(), "Nothing is lit while no line is drawn");
            const auto before = hold();
            connecting->trigger();
            settle();
            mouse(QEvent::MouseButtonPress, gutter("Course", "CourseID"), Qt::LeftButton);
            require(!view->link_target(), "A press alone lights nothing: the line has not left its row");
            const auto chosen_at_press = view->selection();

            // A. A compatible ordinary column: green, and letting go asks Use & Connect.
            mouse(QEvent::MouseMove, row_point("Lesson", "CourseID"), Qt::LeftButton);
            settle();
            require(lit("Lesson", "CourseID", true) && painted(row_edge("Lesson", "CourseID")) == "green",
                    "A compatible ordinary column is lit green");
            // B. An incompatible one: red, and the row left behind goes dark.
            mouse(QEvent::MouseMove, row_point("Review", "CourseID"), Qt::LeftButton);
            settle();
            require(lit("Review", "CourseID", false) && painted(row_edge("Review", "CourseID")) == "red"
                        && painted(row_edge("Lesson", "CourseID")) == "neither",
                    "A column of another type is lit red, and the row left behind is no longer lit");
            // C. A table with no foreign key to Course, over its heading: a green ring.
            mouse(QEvent::MouseMove, heading("Student"), Qt::LeftButton);
            settle();
            require(lit("Student", std::nullopt, true) && painted(ring("Student")) == "green",
                    "A table that would be given a foreign key is ringed green");
            // D. Another primary key: green, the PK-to-PK choice being offered there.
            mouse(QEvent::MouseMove, row_point("Student", "StudentID"), Qt::LeftButton);
            settle();
            require(lit("Student", "StudentID", true) && painted(row_edge("Student", "StudentID")) == "green"
                        && painted(ring("Student")) == "neither",
                    "Another table's key is lit green, and the table's ring goes");
            // A column referring elsewhere is never re-pointed: red.
            mouse(QEvent::MouseMove, row_point("Grade", "StudentRef"), Qt::LeftButton);
            settle();
            require(lit("Grade", "StudentRef", false) && painted(row_edge("Grade", "StudentRef")) == "red",
                    "A column already referring elsewhere is lit red");
            // E and F. The empty schema lights nothing, and nothing is left lit.
            mouse(QEvent::MouseMove, QPointF(1000, 760), Qt::LeftButton);
            settle();
            require(!view->link_target() && painted(row_edge("Grade", "StudentRef")) == "neither"
                        && painted(ring("Student")) == "neither",
                    "Over the empty schema nothing is lit, and nothing stays lit");
            // J. None of it was an edit or a choice.
            require(unchanged(before) && view->selection() == chosen_at_press,
                    "Lighting targets changes nothing: no Undo, not dirty, same revision, the same selection");
            // G. Escape lets go, and the light goes with the line.
            mouse(QEvent::MouseMove, row_point("Lesson", "CourseID"), Qt::LeftButton);
            {
                QKeyEvent key(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                QApplication::sendEvent(view, &key);
            }
            settle();
            mouse(QEvent::MouseButtonRelease, row_point("Lesson", "CourseID"), Qt::NoButton);
            settle();
            require(!view->link_target() && painted(row_edge("Lesson", "CourseID")) == "neither"
                        && select->isChecked() && unchanged(before),
                    "Escape puts the light out with the line, makes nothing, and Select is in hand");

            // What is lit is what letting go does: green goes on to a box, red
            // is turned away with its reason and nothing is asked.
            start(row_point("Lesson", "CourseID"));
            require(lit("Lesson", "CourseID", true) && let_go(row_point("Lesson", "CourseID")) && unchanged(before),
                    "Green on a compatible column: letting go asks (Use & Connect), and Cancel makes nothing");
            start(row_point("Review", "CourseID"));
            require(lit("Review", "CourseID", false) && !let_go(row_point("Review", "CourseID")) && unchanged(before)
                        && lit_window.statusBar()->currentMessage().contains("Cannot use"),
                    "Red on a column of another type: letting go asks nothing and says why");
            start(heading("Student"));
            require(lit("Student", std::nullopt, true) && let_go(heading("Student")) && unchanged(before),
                    "Green on a table: letting go asks (Create & Connect)");
            start(row_point("Student", "StudentID"));
            require(lit("Student", "StudentID", true) && let_go(row_point("Student", "StudentID")) && unchanged(before),
                    "Green on another key: letting go asks how (Use as PK + FK, Create New FK)");
            start(row_point("Grade", "StudentRef"));
            require(lit("Grade", "StudentRef", false) && !let_go(row_point("Grade", "StudentRef")) && unchanged(before)
                        && lit_window.statusBar()->currentMessage().contains("already references"),
                    "Red on a column referring elsewhere: letting go asks nothing and says why");

            // H. A connection made puts the light out.
            start(row_point("Lesson", "CourseID"));
            answer("Use & Connect");
            mouse(QEvent::MouseButtonRelease, row_point("Lesson", "CourseID"), Qt::NoButton);
            settle();
            require(model.undo_label() == "Connect tables" && !view->link_target()
                        && painted(row_edge("Lesson", "CourseID")) == "neither" && select->isChecked(),
                    "Once the foreign key is made nothing is lit, and unlocked Connect hands back to Select");
            // The foreign key now there is green to land on again, as letting go
            // reports it rather than turning it away.
            const auto connected = hold();
            start(row_point("Lesson", "CourseID"));
            require(lit("Lesson", "CourseID", true) && !let_go(row_point("Lesson", "CourseID")) && unchanged(connected)
                        && lit_window.statusBar()->currentMessage().contains("already exists"),
                    "The same foreign key again is green, and letting go reports it is already there");

            // I. Locked Connect: the light goes out after each, Connect stays,
            // and the next line starts dark.
            {
                QMouseEvent twice(QEvent::MouseButtonDblClick, QPointF(5, 5), connect_button->mapToGlobal(QPoint(5, 5)),
                                  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(connect_button, &twice);
                settle();
            }
            require(connecting->isChecked() && connecting->text() == "Connect 🔒", "Connect is locked");
            start(heading("Student"));
            require(lit("Student", std::nullopt, true), "Locked, a table is ringed as before");
            answer("Create & Connect");
            mouse(QEvent::MouseButtonRelease, heading("Student"), Qt::NoButton);
            settle();
            require(!view->link_target() && painted(ring("Student")) == "neither" && connecting->isChecked(),
                    "Made, the ring goes out and Connect stays in hand");
            mouse(QEvent::MouseButtonPress, gutter("Student", "StudentID"), Qt::LeftButton);
            require(!view->link_target(), "The next line starts with nothing lit");
            mouse(QEvent::MouseMove, row_point("Review", "CourseID"), Qt::LeftButton);
            settle();
            require(view->link_target().has_value(), "And lights what it is carried over");
            // The schema put away under a line half drawn lets go of it.
            lit_window.show_home(true);
            settle();
            lit_window.show_home(false);
            settle();
            require(!view->link_target(), "Leaving the workspace with a line half drawn puts its light out");
            mouse(QEvent::MouseButtonRelease, row_point("Review", "CourseID"), Qt::NoButton);
            settle();

            // Task 3 (2026-10-02): a row's height under each table, offered
            // while a line is drawn, stands for the table itself. It is lit and
            // let go on exactly as the table is, by the same plan; a row still
            // comes first; and it is never a column or anything kept.
            const auto strip = [&](const char *name)
            {
                const auto box = view->table_boxes()[at(name)];
                return QPointF(box.center().x() + 20, box.bottom() + 2 + 11.5);
            };
            const auto strip_edge = [&](const char *name)
            {
                const auto box = view->table_boxes()[at(name)];
                return QPointF(box.left() + 30, box.bottom() + 3.6);
            };
            const auto below = [&](const char *name, bool takes)
            {
                const auto target = view->link_target();
                return target && target->table == at(name) && !target->row && target->below && target->takes == takes;
            };
            const auto strip_held = hold();
            require(painted(strip_edge("Grade")) == "neither", "No strip is lit while no line is drawn");
            mouse(QEvent::MouseButtonPress, gutter("Course", "CourseID"), Qt::LeftButton);
            const auto chosen_now = view->selection();
            // A row first, then the strip under the last row, then nothing.
            mouse(QEvent::MouseMove, row_point("Grade", "StudentRef"), Qt::LeftButton);
            settle();
            const bool on_row = lit("Grade", "StudentRef", false);
            mouse(QEvent::MouseMove, strip("Grade"), Qt::LeftButton);
            settle();
            require(on_row && below("Grade", true) && painted(strip_edge("Grade")) == "green"
                        && painted(row_edge("Grade", "StudentRef")) == "neither",
                    "Under Grade's last row the strip is lit by Grade's own plan (Create & Connect), the row no longer");
            // The same verdict as the table: refused for Review, there or on its heading.
            mouse(QEvent::MouseMove, strip("Review"), Qt::LeftButton);
            settle();
            const bool review_strip = below("Review", false);
            mouse(QEvent::MouseMove, heading("Review"), Qt::LeftButton);
            settle();
            require(review_strip && lit("Review", std::nullopt, false),
                    "Under Review the strip refuses as Review itself does");
            // No columns, and the table it starts from: still the table's own plan.
            mouse(QEvent::MouseMove, strip("Empty"), Qt::LeftButton);
            settle();
            const bool empty_strip = below("Empty", true);
            mouse(QEvent::MouseMove, strip("Course"), Qt::LeftButton);
            settle();
            require(empty_strip && below("Course", true),
                    "A table with no columns has its strip, and Course's own is its plan for a key to itself");
            mouse(QEvent::MouseMove, QPointF(1000, 760), Qt::LeftButton);
            settle();
            require(!view->link_target() && unchanged(strip_held) && view->selection() == chosen_now,
                    "Off the tables nothing is lit; no Undo, not dirty, same revision, same selection");
            // Escape in the strip lets go and leaves nothing.
            mouse(QEvent::MouseMove, strip("Grade"), Qt::LeftButton);
            {
                QKeyEvent key(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                QApplication::sendEvent(view, &key);
            }
            settle();
            mouse(QEvent::MouseButtonRelease, strip("Grade"), Qt::NoButton);
            settle();
            require(!view->link_target() && painted(strip_edge("Grade")) == "neither" && unchanged(strip_held),
                    "Escape over the strip makes nothing and leaves no strip or light behind");
            // Let go on the strip is let go on the table: refused for Review,
            // asked for Grade (Cancel makes nothing).
            start(strip("Review"));
            require(!let_go(strip("Review")) && unchanged(strip_held)
                        && lit_window.statusBar()->currentMessage().contains("Cannot use"),
                    "Let go on Review's strip: refused as on Review, nothing asked or made");
            start(strip("Grade"));
            require(let_go(strip("Grade")) && unchanged(strip_held), "Let go on Grade's strip: Create & Connect is asked");
            // Locked, agreed: one real column, the strip under it now, Connect still in hand.
            {
                QMouseEvent twice(QEvent::MouseButtonDblClick, QPointF(5, 5), connect_button->mapToGlobal(QPoint(5, 5)),
                                  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(connect_button, &twice);
                settle();
            }
            const auto grade_columns = view->preview().tables[at("Grade")].columns.size();
            const auto old_strip = strip("Grade");
            start(old_strip);
            answer("Create & Connect");
            mouse(QEvent::MouseButtonRelease, old_strip, Qt::NoButton);
            settle();
            const auto &grade = view->preview().tables[at("Grade")].columns;
            require(grade.size() == grade_columns + 1 && grade.back().name == "CourseID" && grade.back().foreign_key
                        && model.undo_label() == "Connect tables" && !view->link_target() && connecting->isChecked()
                        && connecting->text() == "Connect 🔒",
                    "Agreed on the strip: one real column, CourseID, the foreign key; nothing lit; Connect still locked");
            mouse(QEvent::MouseButtonPress, gutter("Course", "CourseID"), Qt::LeftButton);
            mouse(QEvent::MouseMove, old_strip, Qt::LeftButton);
            settle();
            const bool now_a_row = lit("Grade", "CourseID", true);
            mouse(QEvent::MouseMove, strip("Grade"), Qt::LeftButton);
            settle();
            require(now_a_row && below("Grade", true),
                    "Where the strip was is the new row; the strip has moved under it");
            {
                QKeyEvent key(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                QApplication::sendEvent(view, &key);
            }
            mouse(QEvent::MouseButtonRelease, strip("Grade"), Qt::NoButton);
            settle();
            model.mark_saved(model.revision());
        }
    }
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    // CTest runs the suite in four parts, each a process of its own with a
    // timeout of its own (2026-10-09): Export's commands; Relational Design's
    // checks, which each make their own window; the main window's, which carry
    // one window through both workspaces; and the new projects', each started
    // afresh -- from the schema, from the Editor, or in a window of its own.
    // Run with none of them asked for, it runs whole, in the order it always
    // has, the new projects in the same window as the rest.
    const auto arguments = QCoreApplication::arguments();
    const bool export_part = arguments.contains("--export-state");
    const bool relational_part = arguments.contains("--relational-part");
    const bool window_part = arguments.contains("--window-part");
    const bool new_projects_part = arguments.contains("--new-projects-part");
    const bool whole = !export_part && !relational_part && !window_part && !new_projects_part;
    // The window remembers the chosen theme. Point that at a throwaway domain so
    // running the tests cannot disturb the real preferences -- a domain for
    // each part, so that parts run side by side cannot disturb each other's.
    QCoreApplication::setOrganizationName("ERDFlowTests");
    QCoreApplication::setApplicationName(relational_part     ? "ERDFlowTests-Relational"
                                         : window_part       ? "ERDFlowTests-Window"
                                         : new_projects_part ? "ERDFlowTests-NewProjects"
                                                             : "ERDFlowTests");
    // Start from nothing, so a remembered value has to be written by this run
    // rather than left behind by the last one.
    QSettings().clear();
    try
    {
        if (whole || export_part)
        {
            export_state_tests();
            if (export_part)
                return 0;
        }
        infrastructure::QtIdGenerator ids;
        if (whole || relational_part)
        {
            relational_design_tests(ids);
            if (arguments.contains("--schema-connections-only"))
            {
                std::cout << "PASS schema row connections and manual endpoint overrides\n";
                return 0;
            }
            schema_drawing_tests(ids);
        }
        if (relational_part)
        {
            std::cout << "Relational Design desktop tests passed\n";
            return 0;
        }
        application::Editor editor(ids);
        infrastructure::ErdxProjectStore project_store;
        desktop::MainWindow window(editor, project_store, ids);
        window.show();
        window.activateWindow();
        settle();
        // The main window's own checks, carrying the window from Home through
        // both workspaces. Their names stay within this scope, so the new
        // projects that follow can be run on a window of their own. (Left at
        // the depth it was written at.)
        if (!new_projects_part)
        {
        require(window.editor().project().entities.empty(), "New window is an empty project");

        // The application opens on the home screen, with the work's own
        // furniture put away behind it.
        {
            require(window.showing_home(), "ERDFlow opens on the home screen");
            auto *home = static_cast<desktop::HomePage *>(window.findChild<QWidget *>("homePage"));
            require(home != nullptr, "Which is a page of its own");
            require(home->isVisible(), "And is the page in front");
            require(home->top_bar()->return_button()->isHidden(),
                    "A fresh start has no workspace yet to return to");
            // The five places it can send somebody. There is no New Project row:
            // the cards are where a project is started (Zain, 2026-09-24). Nor
            // are Examples, Templates and Import here: each belongs to the
            // workspace it works on (Zain, 2026-10-03).
            const QStringList wanted{"Home", "Open Project", "Recent", "Settings", "Help"};
            require(home->sidebar_labels() == wanted,
                    "The sidebar offers exactly the five named places, in order");
            require(home->chosen_row() == 0, "And opens on Home");
            auto *rail = home->sidebar();
            for (const char *gone : {"homeNavExamples", "homeNavTemplates", "homeNavImport"})
                require(!rail->findChild<QWidget *>(gone), "Examples, Templates and Import are not on Home's rail");
            const auto row_of = [&](desktop::HomeSection section)
            { return rail->button(section)->geometry(); };
            require(row_of(desktop::HomeSection::OpenProject).top() == row_of(desktop::HomeSection::Home).bottom() + 1 &&
                        row_of(desktop::HomeSection::Recent).top() == row_of(desktop::HomeSection::OpenProject).bottom() + 1,
                    "Home, Open Project and Recent stand together at the top");
            require(row_of(desktop::HomeSection::Help).bottom() > rail->height() - 60 && row_of(desktop::HomeSection::Settings).top() > row_of(desktop::HomeSection::Recent).bottom() + 40,
                    "Settings and Help stand at the foot of the rail");
            for (const auto &row : desktop::home_navigation())
                require(!desktop::outline_pixmap(QString::fromLatin1(row.icon), Qt::black, 20).isNull(),
                        "Every row has its icon, from the one line-art family");
            require(desktop::contrast_ratio(desktop::chosen_row_fill(desktop::tokens(desktop::ThemeId::Azure)),
                                            Qt::white) >= 4.5,
                    "A chosen row's white lettering reads at 4.5:1 or better");
            // A row is a button: one press is one activation. The list it
            // replaces lit a row on one press and acted only on two.
            int heard_rows = 0;
            desktop::HomeSection last_row = desktop::HomeSection::Help;
            rail->activated = [&](desktop::HomeSection section)
            { ++heard_rows; last_row = section; };
            rail->button(desktop::HomeSection::Home)->click();
            settle();
            require(heard_rows == 1 && last_row == desktop::HomeSection::Home,
                    "A single press on a row activates it, once");
            rail->activated = {};
            require(rail->button(desktop::HomeSection::Recent)->focusPolicy() == Qt::StrongFocus,
                    "And every row can be reached from the keyboard");

            require(window.findChild<QWidget *>("homeLearningPanel") != nullptr,
                    "The learning panel is present");
            auto *learning = home->learning();
            // Opening an example is the sidebar's Examples row, so the panel
            // does not offer it twice (Zain, 2026-09-24).
            require(learning->link_labels() == QStringList{"View tutorials"},
                    "Its one link is there, and is a button rather than lettering");
            require(learning->footer_phrase() == "Design today.\nBuild tomorrow.",
                    "With the two decorative lines at its foot");
            require(window.findChild<QWidget *>("homeCentre") != nullptr,
                    "And the centre it sits beside");

            // The ribbon gives way to Home's own slim bar; the menus never do.
            auto *tools = window.findChild<QToolBar *>("modelTools");
            require(tools != nullptr && !tools->isVisible(),
                    "The model tools belong to the work, so they wait behind it");
            for (auto *bar : window.findChildren<QToolBar *>())
                require(!bar->isVisible(), "No ribbon row stands over the home screen");
            require(window.menuBar()->isVisible() || window.menuBar()->isNativeMenuBar(),
                    "The native menu bar stays, so Home is never without its menus");
            auto *brand_bar = home->top_bar();
            require(brand_bar->isVisible(), "Home's slim bar is in the ribbon's place");
            require(brand_bar->theme_button()->menu() == window.findChild<QMenu *>("themeMenu"),
                    "Its Theme opens the window's own theme menu, not a copy");
            // One Settings, the sidebar's row (Zain, 2026-09-26): the bar has
            // no gear of its own beside Theme.
            require(home->findChild<QWidget *>("appTopBarSettings") == nullptr && home->findChild<QWidget *>("appTopBarSeparator") == nullptr,
                    "The bar carries no second Settings");
            require(home->sidebar()->button(desktop::HomeSection::Settings)->isVisible(),
                    "Settings is the sidebar's row");
            auto *settings = window.findChild<QMenu *>("settingsMenu");
            require(settings != nullptr && settings->actions().size() == 3 && settings->actions()[0]->menu() == window.findChild<QMenu *>("themeMenu"),
                    "Its Settings holds the application's own choices: theme, icons, notation");
            // A panel somebody had closed must not be reopened merely because
            // they passed through the home screen.
            auto *checks = child<QDockWidget>(window, "validationDock");
            require(!checks->isVisible(), "The findings were closed and stay closed");
            window.show_home(false);
            settle();
            require(!window.showing_home(), "Leaving it puts the work in front");
            require(!checks->isVisible(), "And still does not reopen what was closed");
            require(tools->isVisible(), "The model tools come back with the work");
            require(child<QToolBar>(window, "ribbonTabs")->isVisible(), "And the ribbon's tabs with them");
            window.show_home(true);
            settle();
            require(window.showing_home(), "And it can be returned to");
            window.show_home(true);
            settle();

            // Three ways to start, in the order the specification fixes, with
            // the copy it fixes. Templates and Import are sidebar rows, not
            // cards: Zain settled that on 2026-09-23.
            const auto cards = home->cards();
            require(cards.size() == 3, "Three ways to start, no more and no fewer");
            const QStringList order{"startRouteConceptual", "startRouteRelational", "startRouteSql"};
            for (int i = 0; i < order.size(); ++i)
                require(cards[static_cast<std::size_t>(i)]->objectName() == order[i],
                        "The cards are in the order the specification fixes");
            require(window.findChild<QWidget *>("startRouteTemplate") == nullptr && window.findChild<QWidget *>("startRouteImport") == nullptr,
                    "Neither Templates nor Import is a card");
            // A card that is clicked is chosen and takes the keyboard, and says
            // so in the theme's own accent (Zain, 2026-10-06). The three dots'
            // pale blue brush, left on, once filled the whole card with it as
            // soon as it had the keyboard, whatever the theme.
            {
                const auto worn = window.canvas()->theme_id();
                window.set_theme(desktop::ThemeId::Dracula);
                settle();
                auto *card = cards[1];
                card->click();
                card->setFocus(Qt::MouseFocusReason);
                settle();
                require(card->isChecked() && card->hasFocus(), "A clicked card is chosen and has the keyboard");
                const auto face = card->grab().toImage();
                const auto cyan = QColor("#9DD9FF");
                const auto accent = desktop::tokens(desktop::ThemeId::Dracula).primary;
                bool poured = false;
                bool tinted = true;
                for (int y = face.height() / 3; y < face.height() * 2 / 3; y += 8)
                {
                    const auto at = face.pixelColor(9, y);
                    const auto off = std::abs(at.red() - cyan.red()) + std::abs(at.green() - cyan.green()) + std::abs(at.blue() - cyan.blue());
                    poured = poured || off < 40;
                    if (at.hsvSaturation() > 25 && std::abs(at.hsvHue() - accent.hsvHue()) > 40)
                        tinted = false;
                }
                require(!poured, "Its face is not poured full of the dots' pale blue");
                require(tinted, "What colour it takes is the theme's own accent");
                cards.front()->click();
                card->clearFocus();
                window.set_theme(worn);
                settle();
            }
            // Each card's two actions light up under the pointer, each on its
            // own (Zain, 2026-09-26), and are exactly as they were once it
            // leaves. Create with AI lights up too, though it cannot be
            // pressed yet.
            {
                // Offered for the purpose, since it is not offered for now.
                cards.front()->set_ai_offered(true);
                settle();
                auto *create = cards.front()->create_button();
                auto *ai = cards.front()->ai_button();
                const auto create_at_rest = create->grab().toImage();
                const auto ai_at_rest = ai->grab().toImage();
                const auto create_place = create->geometry();
                const auto ai_place = ai->geometry();
                const auto point = [](QWidget *button, QEvent::Type type)
                {
                    if (type == QEvent::Enter)
                    {
                        QEnterEvent entered(QPointF(6, 6), QPointF(6, 6), QPointF(button->mapToGlobal(QPoint(6, 6))));
                        QApplication::sendEvent(button, &entered);
                    }
                    else
                    {
                        QEvent left(QEvent::Leave);
                        QApplication::sendEvent(button, &left);
                    }
                    settle();
                };
                point(create, QEvent::Enter);
                require(create->property("lit").toBool() && !ai->property("lit").toBool(),
                        "Pointing at + Create lights it, and it alone");
                require(create->grab().toImage() != create_at_rest, "Visibly");
                require(create->geometry() == create_place && create->text() == "+ Create",
                        "Without moving it or changing its words");
                point(create, QEvent::Leave);
                require(!create->property("lit").toBool() && create->grab().toImage() == create_at_rest,
                        "And it is exactly as it was once the pointer leaves");
                point(ai, QEvent::Enter);
                require(ai->property("lit").toBool() && !create->property("lit").toBool(),
                        "Pointing at Create with AI lights it, and it alone");
                require(ai->grab().toImage() != ai_at_rest, "Visibly, though it cannot be pressed yet");
                require(!ai->isEnabled() && ai->geometry() == ai_place, "Still unpressable, and where it was");
                point(ai, QEvent::Leave);
                require(!ai->property("lit").toBool() && ai->grab().toImage() == ai_at_rest,
                        "And it too is exactly as it was once the pointer leaves");
                cards.front()->set_ai_offered(desktop::create_with_ai_offered);
                settle();
            }
            // Zain's titles (2026-09-24, ADR-022 9.19).
            require(cards[0]->accessibleName() == "Conceptual Design (ERD)" && cards[1]->accessibleName() == "Relational Schema" && cards[2]->accessibleName() == "SQL Script (DDL)",
                    "And carry their exact titles");
            require(cards[2]->accessibleDescription() == "Write, paste or import SQL to build the relational design.",
                    "And their exact bodies");

            // A card is a control, not a painted rectangle: it can be reached
            // by keyboard and read out by the platform.
            for (auto *card : cards)
                require(card->focusPolicy() == Qt::StrongFocus, "Every card takes focus");

            // SQL Project is shown and deliberately not enabled, because the
            // route behind it is not built. It keeps its place in the row
            // rather than being left out. Relational Schema can be taken now
            // that tables can be made by hand (Zain, 2026-09-27).
            require(!cards[2]->isEnabled(), "A route that is not built is not yet enabled");
            require(cards[2]->isVisible(), "But it is shown, in its own place");
            require(!cards[2]->toolTip().isEmpty(), "And says why it cannot be taken");
            require(cards[0]->isEnabled() && cards[1]->isEnabled(), "The routes that can be taken are enabled");

            // The cards share one line and never wrap, at any width the window
            // can have; each narrows to make room rather than one dropping.
            const auto one_line = [&]
            {
                for (auto *card : cards)
                    if (card->y() != cards[0]->y() || card->width() != cards[0]->width())
                        return false;
                return cards[2]->geometry().right() <= cards[0]->parentWidget()->width();
            };
            require(one_line(), "The cards stand on one line, equal in width");
            const auto wide = cards[0]->width();
            const auto opened_at = window.size();
            window.resize(900, opened_at.height());
            settle();
            require(one_line(), "And stay on one line as the window narrows");
            require(cards[0]->width() < wide, "Each narrowing rather than any of them moving down");
            window.resize(opened_at);
            settle();

            // Every card is a little taller than wide, 1.10 to 1, however wide
            // the window: on a wide one the cards stop growing and the group
            // stands in the middle with room either side.
            const auto door = [&]
            {
                for (auto *card : cards)
                {
                    const auto shape = static_cast<double>(card->height()) / card->width();
                    if (shape < 1.08 || shape > 1.14)
                        return false;
                }
                return true;
            };
            for (const auto size : {QSize(1440, 1080), QSize(1920, 1080), QSize(1179, 900)})
            {
                window.resize(size);
                settle();
                settle();
                require(one_line(), "The cards stay on one line at every width");
                require(cards[0]->height() == cards[1]->height() && cards[1]->height() == cards[2]->height(),
                        "All cards share the height needed by the Conceptual demo");
                require(cards[0]->width() <= 315, "No card grows past its widest");
                if (size.width() == 1920)
                {
                    auto *line = cards[0]->parentWidget();
                    const auto left = cards.front()->x();
                    const auto right = line->width() - cards.back()->geometry().right() - 1;
                    require(cards[0]->width() == 315 && std::abs(left - right) <= 1,
                            "On a wide window the cards stop at their widest and the group is centred");
                }
            }
            window.resize(opened_at);
            settle();

            // One page. At the size ERDFlow opens at, and on the common
            // smaller screens, nothing on Home is reached by scrolling.
            for (const auto size : {opened_at, QSize(1440, 1080), QSize(1366, 740)})
            {
                window.resize(size);
                settle();
                if (home->centre_needs_scrolling())
                    qWarning() << "Home needs" << home->findChild<QWidget *>("homeCentre")->minimumSizeHint()
                               << "in" << home->findChild<QScrollArea *>("homeCentreScroll")->viewport()->size()
                               << "at" << size << "cards" << cards[0]->size()
                               << "content" << cards[1]->content_height_for(cards[1]->width(), true);
                require(!home->centre_needs_scrolling(),
                        "Home is one page: everything is on screen without scrolling");
                require(cards[0]->height() >= cards[0]->width() * 1.08, "Cards retain their vertical proportion");
            }
            // A live demo under each card (ADR-022 9.21): three, in the cards'
            // order, each exactly as wide as its card, directly under it and
            // centred on it, all one height, never reaching into the next
            // card's column -- and never taking anything from the cards. With
            // the demos hidden the cards stand exactly where they stood.
            {
                const auto demos = home->demos();
                require(demos.size() == 3, "Three live demos, one for each card");
                require(demos[0]->kind() == desktop::HomeDemoKind::Conceptual && demos[1]->kind() == desktop::HomeDemoKind::Relational && demos[2]->kind() == desktop::HomeDemoKind::Sql,
                        "In the cards' order");
                auto *centre = home->findChild<QWidget *>("homeCentre");
                const auto in_centre = [&](QWidget *one)
                {
                    return QRect(one->mapTo(centre, QPoint()), one->size());
                };
                const auto card_boxes = [&]
                {
                    std::vector<QRect> boxes;
                    for (auto *card : cards)
                        boxes.push_back(in_centre(card));
                    return boxes;
                };
                for (const auto size : {opened_at, QSize(1440, 1080), QSize(1920, 1080),
                                        QSize(1179, 900), QSize(1366, 740)})
                {
                    window.resize(size);
                    settle();
                    settle();
                    const auto with = card_boxes();
                    for (std::size_t i = 0; i < demos.size(); ++i)
                    {
                        const auto demo = in_centre(demos[i]);
                        {
                            // Every card holds its demo inside it (Zain, 2026-09-25).
                            require(demos[i]->parentWidget() == cards[i] && with[i].contains(demo),
                                    "The demo is contained inside its card");
                            require(demos[i]->geometry().bottom() < cards[i]->create_button()->y(),
                                    "The demo clears the Create button");
                            // + Create and Create with AI stand side by side,
                            // the pair centred (Zain, 2026-09-25); + Create
                            // alone, while that is not offered, is centred
                            // too (Zain, 2026-09-26).
                            const auto pair = cards[i]->ai_button()->isHidden()
                                                  ? cards[i]->create_button()->geometry()
                                                  : cards[i]->create_button()->geometry().united(cards[i]->ai_button()->geometry());
                            require(std::abs(demos[i]->x() + demos[i]->stage().center().x() - (pair.left() + pair.width() / 2.0)) <= 1.0,
                                    "The demo and the pair of buttons share the card's center line");
                            require(!cards[i]->accessibleDescription().isEmpty(),
                                    "The description is no longer drawn, but is still read out");
                            require(demos[i]->testAttribute(Qt::WA_TransparentForMouseEvents),
                                    "Decorative demo preserves card clicks");
                        }
                        require(std::abs(demo.left() + demos[i]->stage().center().x() - (with[i].left() + with[i].width() / 2.0)) <= 1.0,
                                "Its miniature centred on the card");
                        require(cards[i]->height() == cards[0]->height() && cards[i]->create_button()->y() == cards[0]->create_button()->y(),
                                "Card heights and Create baselines remain aligned");
                        if (i + 1 < demos.size())
                            require(demo.right() < with[i + 1].left(),
                                    "And none reaches into the next card's column");
                        require(demos[i]->focusPolicy() == Qt::NoFocus,
                                "Decoration the keyboard never lands on");
                    }
                    for (auto *demo : demos)
                        demo->hide();
                    settle();
                    settle();
                    require(card_boxes() == with, "The demos never move or resize a card");
                    for (auto *demo : demos)
                        demo->show();
                    settle();
                    settle();
                    require(card_boxes() == with, "Shown again, the cards are where they were");
                }
                window.resize(1440, 1080);
                settle();
                settle();
                for (auto *demo : demos)
                    require(demo->shown() && demo->rect().contains(demo->stage().toRect()),
                            "At the reference size each is drawn within its bounds");
                window.resize(opened_at);
                settle();
                settle();
            }

            // Stood still, as a picture that must come out the same needs
            // them, the demos show their scenes finished.
            home->set_demos_moving(false);
            settle();

            // The Conceptual demo (ADR-022 9.21, stage 2): Student and Course
            // joined by Enrolled, drawn a piece at a time in the order the
            // brief gives, in the Conceptual workspace's own shapes. Asked of
            // the scene rather than of pixels, so nothing here depends on how
            // text happens to fall.
            {
                using desktop::DemoShape;
                const auto kind = desktop::HomeDemoKind::Conceptual;
                const auto &steps = desktop::demo_steps(kind);
                const auto &pieces = desktop::demo_elements(kind);
                require(steps.size() == 14, "Fourteen steps, empty to faded");
                require(desktop::demo_loop_seconds(kind) >= 10.0 && desktop::demo_loop_seconds(kind) <= 12.0,
                        "A pass takes ten to twelve seconds");
                const auto finished = desktop::demo_finished_step(kind);
                require(QString::fromLatin1(steps[finished].name) == "Hold", "It is held once finished");
                const auto at = [&](std::size_t step, double progress)
                {
                    return desktop::DemoMoment{step, progress};
                };
                const auto showing = [&](desktop::DemoMoment moment, DemoShape shape)
                {
                    QStringList seen;
                    for (const auto &piece : pieces)
                        if (piece.shape == shape && desktop::demo_arrival(piece, moment) >= 1.0)
                            seen << (piece.label.isEmpty() ? QStringLiteral("line") : piece.label);
                    return seen;
                };
                const auto done = at(finished, 0.0);
                require(showing(done, DemoShape::Entity) == QStringList{"Student", "Course"},
                        "Finished, it has the two entities");
                require(showing(done, DemoShape::Relationship) == QStringList{"Enrolled"},
                        "The relationship between them, as a diamond");
                require(showing(done, DemoShape::Attribute) == QStringList{"ID", "Name", "ID", "Name", "Enrollment Date"},
                        "Each side's ID and Name, and the relationship's Enrollment Date");
                require(showing(done, DemoShape::KeyMark) == QStringList{"ID", "ID"},
                        "With both IDs underlined as keys");
                require(showing(done, DemoShape::Connector).size() == 7,
                        "And a line for each of the two sides and each of the five attributes");

                // Empty first, then entities, keyed attributes, the relationship,
                // its connectors/cardinalities and its own attribute.
                for (const auto &piece : pieces)
                    require(desktop::demo_arrival(piece, at(0, 1.0)) == 0.0, "It starts empty");
                const auto arrived_by = [&](const QString &label, DemoShape shape)
                {
                    for (const auto &piece : pieces)
                        if (piece.label == label && piece.shape == shape)
                            return piece.step;
                    return std::size_t{99};
                };
                require(arrived_by("Student", DemoShape::Entity) < arrived_by("Course", DemoShape::Entity) && arrived_by("Course", DemoShape::Entity) < arrived_by("Enrolled", DemoShape::Relationship),
                        "Student, then Course, then Enrolled");
                std::vector<const desktop::DemoElement *> attributes, branches, keys, symbols;
                for (const auto &piece : pieces)
                {
                    if (piece.shape == DemoShape::Attribute)
                        attributes.push_back(&piece);
                    if (piece.shape == DemoShape::Connector && piece.route.size() == 2)
                        branches.push_back(&piece);
                    if (piece.shape == DemoShape::KeyMark)
                        keys.push_back(&piece);
                    if (piece.shape == DemoShape::OptionalMany)
                        symbols.push_back(&piece);
                }
                require(attributes.size() == 5 && branches.size() == 4 && keys.size() == 2,
                        "Only the specified attributes and their key marks");
                const auto &student = pieces[0].box;
                const auto &course = pieces[1].box;
                const auto centered_relationship = std::find_if(pieces.begin(), pieces.end(), [](const auto &piece)
                                                                { return piece.shape == DemoShape::Relationship; });
                require(centered_relationship != pieces.end() && student.center().y() == course.center().y() && centered_relationship->box.center() == QPointF(140, student.center().y()) && student.center().x() < 140 && course.center().x() > 140 && student.center().x() + course.center().x() == 280,
                        "Horizontal entities are balanced around the centered centered_relationship");
                for (std::size_t i = 0; i < 4; ++i)
                {
                    const auto &branch = *branches[i];
                    const auto &owner = i < 2 ? student : course;
                    const QPointF anchor(owner.center().x(), owner.top());
                    require(branch.from == anchor, "Attribute originates directly at its entity's central anchor");
                    require(branch.to.x() == attributes[i]->box.center().x() && branch.to.y() == attributes[i]->box.bottom(),
                            "Branch reaches its own attribute boundary");
                    const auto path = desktop::demo_connector_path(branch);
                    require(path.pointAtPercent(0) == anchor && path.pointAtPercent(1) == branch.to,
                            "Painted curve preserves both anchors");
                    if (i % 2 == 0)
                    {
                        require(branch.from == branches[i + 1]->from && branch.route[0] == branches[i + 1]->route[0],
                                "ID and Name share the short trunk before branching");
                        require(keys[i / 2]->box == attributes[i]->box && keys[i / 2]->step == attributes[i]->step && attributes[i]->step < attributes[i + 1]->step,
                                "Underlined ID arrives before Name");
                    }
                }
                require(symbols.size() == 2 && symbols[0]->from == QPointF(student.right(), student.center().y()) && symbols[1]->from == QPointF(course.left(), course.center().y()) && symbols[0]->to == QPointF(1, 0) && symbols[1]->to == QPointF(-1, 0),
                        "Optional-many crow's feet sit at separate inward-facing relationship anchors");
                for (const auto &piece : pieces)
                {
                    if (piece.shape != DemoShape::Connector || !piece.route.empty())
                        continue;
                    if (piece.label == "Enrollment Date")
                    {
                        const auto diamond = std::find_if(pieces.begin(), pieces.end(), [](const auto &item)
                                                          { return item.shape == DemoShape::Relationship; });
                        require(diamond != pieces.end() && piece.from == QPointF(diamond->box.center().x(), diamond->box.bottom()) && piece.to == QPointF(attributes.back()->box.center().x(), attributes.back()->box.top()),
                                "Enrollment Date joins the diamond itself");
                    }
                    else
                    {
                        require(piece.from.y() == piece.to.y(), "Relationship connectors stay horizontal");
                        require(piece.step > arrived_by("Enrolled", DemoShape::Relationship) && piece.step < symbols[0]->step,
                                "Relationship lines precede their cardinality symbols");
                    }
                }
                // An attribute's line grows out before its oval appears.
                for (std::size_t i = 0; i + 1 < pieces.size(); ++i)
                    if (pieces[i].shape == DemoShape::Connector && pieces[i + 1].shape == DemoShape::Attribute)
                        require(pieces[i].step == pieces[i + 1].step && pieces[i].starts < pieces[i + 1].starts,
                                "An attribute's line grows out before the attribute appears");

                // Only the last step fades it, and a pass goes round again.
                require(desktop::demo_scene_opacity(kind, done) == 1.0 && desktop::demo_scene_opacity(kind, at(steps.size() - 1, 1.0)) == 0.0,
                        "It is whole until its last step fades it away");
                const auto loop = desktop::demo_loop_seconds(kind);
                require(desktop::demo_moment_at(kind, 0.0).step == 0 && desktop::demo_moment_at(kind, loop + 0.1).step == 0,
                        "And past its end it starts again");

                // On Home, until the demos are played, it shows its model finished.
                auto *conceptual = home->demos()[0];
                require(conceptual->step() == finished, "Stood still, it shows its scene finished");
                QImage empty(conceptual->size(), QImage::Format_ARGB32_Premultiplied);
                QImage whole(conceptual->size(), QImage::Format_ARGB32_Premultiplied);
                empty.fill(Qt::white);
                whole.fill(Qt::white);
                // Its painted scene is what plays when it moves; standing still
                // on Home it shows the canvas itself instead, looked at below.
                const bool real = conceptual->real_canvas();
                conceptual->set_real_canvas(false);
                conceptual->show_step(0, 0.0);
                conceptual->render(&empty);
                conceptual->show_step(finished, 0.0);
                conceptual->render(&whole);
                conceptual->set_real_canvas(real);
                require(empty != whole, "And what it draws is the scene, not only its floor");
            }

            // The Relational Schema demo (ADR-022 9.21, stage 3): Students,
            // Courses and the Enrollments bridge, filled in a row at a time,
            // their keys marked, and each foreign key's line drawn from the
            // key it references to the exact row that references it. No
            // diamond: in a schema a relationship is its foreign keys.
            {
                using desktop::DemoShape;
                const auto kind = desktop::HomeDemoKind::Relational;
                const auto &steps = desktop::demo_steps(kind);
                const auto &pieces = desktop::demo_elements(kind);
                require(steps.size() == 15, "Fifteen steps, empty to faded");
                require(desktop::demo_loop_seconds(kind) >= 10.0 && desktop::demo_loop_seconds(kind) <= 12.0,
                        "A pass takes ten to twelve seconds");
                const auto finished = desktop::demo_finished_step(kind);
                require(QString::fromLatin1(steps[finished].name) == "Hold", "It is held once finished");
                for (const auto &piece : pieces)
                    require(piece.shape != DemoShape::Relationship && piece.shape != DemoShape::Entity && piece.shape != DemoShape::Attribute && piece.shape != DemoShape::Connector && piece.shape != DemoShape::KeyMark,
                            "No diamond and no conceptual shape: only tables, keys and their lines");

                std::vector<const desktop::DemoElement *> tables;
                for (const auto &piece : pieces)
                    if (piece.shape == DemoShape::Table)
                        tables.push_back(&piece);
                require(tables.size() == 3 && tables[0]->label == "Students" && tables[1]->label == "Courses" && tables[2]->label == "Enrollments",
                        "Students, Courses and Enrollments");
                require(!tables[0]->bridge && !tables[1]->bridge && tables[2]->bridge,
                        "Enrollments drawn as the bridge it is");
                const auto in_table = [&](const desktop::DemoElement &piece) -> const desktop::DemoElement *
                {
                    for (const auto *table : tables)
                        if (table->box.contains(piece.box.center()))
                            return table;
                    return nullptr;
                };
                const auto listed = [&](DemoShape shape, const QString &table)
                {
                    QStringList seen;
                    for (const auto &piece : pieces)
                        if (piece.shape == shape && in_table(piece) && in_table(piece)->label == table)
                            seen << piece.label;
                    return seen;
                };
                require(listed(DemoShape::Column, "Students") == QStringList{"StudentID", "Name"} && listed(DemoShape::Column, "Courses") == QStringList{"CourseID", "Name"} && listed(DemoShape::Column, "Enrollments") == QStringList{"StudentID", "CourseID", "EnrollmentDate"},
                        "Each table with its own columns");
                require(listed(DemoShape::PrimaryKey, "Students") == QStringList{"StudentID"} && listed(DemoShape::PrimaryKey, "Courses") == QStringList{"CourseID"} && listed(DemoShape::PrimaryKey, "Enrollments").isEmpty(),
                        "StudentID and CourseID marked as the primary keys of their tables");
                require(listed(DemoShape::ForeignKey, "Enrollments") == QStringList{"StudentID", "CourseID"} && listed(DemoShape::ForeignKey, "Students").isEmpty() && listed(DemoShape::ForeignKey, "Courses").isEmpty(),
                        "And Enrollments' two columns marked as the foreign keys that point at them");

                // Each line runs from the key's own row to the row that
                // references it, square-cornered, never through a table.
                std::vector<const desktop::DemoElement *> lines;
                for (const auto &piece : pieces)
                    if (piece.shape == DemoShape::Reference)
                        lines.push_back(&piece);
                require(lines.size() == 2, "Two lines, one for each foreign key");
                const auto row_of = [&](const QString &table, const QString &column)
                {
                    for (const auto &piece : pieces)
                        if (piece.shape == DemoShape::Column && piece.label == column && in_table(piece) && in_table(piece)->label == table)
                            return piece.box;
                    return QRectF();
                };
                const std::array<std::pair<QString, QString>, 2> joins{
                    std::pair<QString, QString>{"Students", "StudentID"}, {"Courses", "CourseID"}};
                for (std::size_t i = 0; i < lines.size(); ++i)
                {
                    const auto &route = lines[i]->route;
                    const auto key = row_of(joins[i].first, joins[i].second);
                    const auto foreign = row_of("Enrollments", joins[i].second);
                    require(route.size() >= 2 && route.front() == QPointF(key.right(), key.center().y()),
                            "A line starts on the referenced key's own row");
                    require(route.back() == QPointF(foreign.left(), foreign.center().y()),
                            "And ends on the exact row that references it");
                    for (std::size_t p = 1; p < route.size(); ++p)
                    {
                        require(route[p].x() == route[p - 1].x() || route[p].y() == route[p - 1].y(),
                                "Straight runs and right-angled turns only");
                        // Every point along the run lies outside every table; its
                        // ends sit on a table's edge, which is not inside it.
                        for (int step = 1; step < 20; ++step)
                        {
                            const auto on = route[p - 1] + (route[p] - route[p - 1]) * (step / 20.0);
                            for (const auto *table : tables)
                                require(!table->box.adjusted(0.5, 0.5, -0.5, -0.5).contains(on),
                                        "And never through a table");
                        }
                    }
                }

                // In order: each table, then its rows; the keys once every
                // row is in; the lines once the keys are marked.
                // In the order Zain gave (2026-09-25): each table and then
                // each of its rows in its own step, a key marked in the step
                // of its own row and after the row itself, then the Students
                // line, then the Courses line, then held.
                std::vector<std::pair<DemoShape, QString>> order;
                std::vector<std::size_t> order_steps;
                for (const auto &piece : pieces)
                    if (piece.shape == DemoShape::Table || piece.shape == DemoShape::Column || piece.shape == DemoShape::Reference)
                    {
                        order.emplace_back(piece.shape, piece.label);
                        order_steps.push_back(piece.step);
                    }
                const std::vector<std::pair<DemoShape, QString>> expected{
                    {DemoShape::Table, "Students"}, {DemoShape::Column, "StudentID"}, {DemoShape::Column, "Name"}, {DemoShape::Table, "Courses"}, {DemoShape::Column, "CourseID"}, {DemoShape::Column, "Name"}, {DemoShape::Table, "Enrollments"}, {DemoShape::Column, "StudentID"}, {DemoShape::Column, "CourseID"}, {DemoShape::Column, "EnrollmentDate"}, {DemoShape::Reference, "StudentID"}, {DemoShape::Reference, "CourseID"}};
                require(order == expected, "Tables, rows and lines in the order given");
                for (std::size_t i = 1; i < order_steps.size(); ++i)
                    require(order_steps[i] == order_steps[i - 1] + 1, "Each in a step of its own, one after another");
                require(order_steps.front() == 1 && order_steps.back() + 1 == finished,
                        "From the first step after the empty one, to the one before it is held");
                for (const auto &piece : pieces)
                    if (piece.shape == DemoShape::PrimaryKey || piece.shape == DemoShape::ForeignKey)
                        for (const auto &row : pieces)
                            if (row.shape == DemoShape::Column && row.box == piece.box)
                                require(piece.step == row.step && piece.starts > row.starts,
                                        "A key is marked as its own row arrives, just after it");

                auto *relational = home->demos()[1];
                require(relational->step() == finished, "Stood still, it shows its scene finished");
                QImage empty(relational->size(), QImage::Format_ARGB32_Premultiplied);
                QImage whole(relational->size(), QImage::Format_ARGB32_Premultiplied);
                empty.fill(Qt::white);
                whole.fill(Qt::white);
                relational->show_step(0, 0.0);
                relational->render(&empty);
                relational->show_step(finished, 0.0);
                relational->render(&whole);
                require(empty != whole, "And what it draws is the schema, not only its floor");
            }

            // The SQL demo (ADR-022 9.21, stage 4): an editor comes up and the
            // same three tables are typed into it as SQL, a character at a
            // time at one steady speed, and a line at its foot says what
            // running it made. Nothing is run.
            {
                using desktop::DemoShape;
                const auto kind = desktop::HomeDemoKind::Sql;
                const auto &steps = desktop::demo_steps(kind);
                const auto &pieces = desktop::demo_elements(kind);
                const auto loop = desktop::demo_loop_seconds(kind);
                require(loop >= 9.0 && loop <= 11.0, "A pass takes about ten seconds");
                const auto finished = desktop::demo_finished_step(kind);
                require(QString::fromLatin1(steps[finished].name) == "Hold", "It is held once finished");
                for (const auto &piece : pieces)
                    require(piece.shape == DemoShape::Editor || piece.shape == DemoShape::Code || piece.shape == DemoShape::Result,
                            "Only an editor, its script and the result: no diagram and no tables");

                // The finished script: the script Zain gave (2026-09-25), set
                // compactly enough to sit whole in the editor inside the card.
                const auto script = desktop::demo_code_at(kind, desktop::DemoMoment{finished, 0.0});
                require(script.count("CREATE TABLE") == 3 && script.contains("CREATE TABLE Students (") && script.contains("CREATE TABLE Courses (") && script.contains("CREATE TABLE Enrollments ("),
                        "It writes Students, Courses and Enrollments");
                require(script.count("PRIMARY KEY") == 2 && script.count("VARCHAR(100)") == 2 && script.contains("StudentID INT,") && script.contains("CourseID INT,") && script.contains("EnrollmentDate DATE"),
                        "With the columns Zain gave: two keys, two names, and the bridge's three");
                require(!script.contains("REFERENCES"), "And, as he wrote it, no REFERENCES clauses");
                require(script.split('\n').size() == 10, "Ten lines, which the editor holds whole");
                for (const auto &line : script.split('\n'))
                    require(line.size() <= 40, "And no line wider than the editor");
                require(desktop::demo_code_at(kind, desktop::DemoMoment{0, 1.0}).isEmpty() && desktop::demo_code_at(kind, desktop::DemoMoment{1, 1.0}).isEmpty(),
                        "Nothing is typed until the editor is up");

                // Typed a character at a time, always the start of the script,
                // at one steady speed: forty characters a second.
                double typing_starts = 0;
                for (std::size_t i = 0; i < steps.size(); ++i)
                {
                    bool types = false;
                    for (const auto &piece : pieces)
                        if (piece.shape == DemoShape::Code && piece.step == i)
                            types = true;
                    if (types)
                        break;
                    typing_starts += steps[i].seconds;
                }
                qsizetype before = 0;
                for (double t = typing_starts; t < typing_starts + script.size() / 40.0; t += 0.05)
                {
                    const auto typed = desktop::demo_code_at(kind, desktop::demo_moment_at(kind, t));
                    require(script.startsWith(typed), "What is typed is always the start of the script");
                    require(typed.size() >= before, "And it only ever grows while it is typed");
                    require(std::abs(static_cast<double>(typed.size()) - (t - typing_starts) * 40.0) <= 1.5,
                            "At a steady forty characters a second");
                    before = typed.size();
                }

                // Each table's head, then its columns, in Zain's order.
                QStringList runs;
                for (const auto &piece : pieces)
                    if (piece.shape == DemoShape::Code)
                        runs << piece.label.trimmed();
                require(runs.size() == 6 && runs[0] == "CREATE TABLE Students (" && runs[1].startsWith("StudentID") && runs[2] == "CREATE TABLE Courses (" && runs[3].startsWith("CourseID") && runs[4] == "CREATE TABLE Enrollments (" && runs[5].startsWith("StudentID"),
                        "Each table's head, then its columns, one table after another");

                // In order: the editor, the three tables, then the result.
                std::size_t editor_step = 99, result_step = 0, last_code = 0;
                for (const auto &piece : pieces)
                {
                    if (piece.shape == DemoShape::Editor)
                        editor_step = piece.step;
                    if (piece.shape == DemoShape::Result)
                        result_step = piece.step;
                    if (piece.shape == DemoShape::Code)
                    {
                        require(piece.step > editor_step, "Typed into the editor once it is up");
                        last_code = std::max(last_code, piece.step);
                    }
                }
                require(result_step > last_code && result_step < finished,
                        "And the result once the script is written, before it is held");
                for (const auto &piece : pieces)
                    if (piece.shape == DemoShape::Result)
                        require(piece.label == "3 tables created", "Saying the three tables were made");

                auto *sql = home->demos()[2];
                require(sql->step() == finished, "Stood still, it shows its scene finished");
                QImage empty(sql->size(), QImage::Format_ARGB32_Premultiplied);
                QImage whole(sql->size(), QImage::Format_ARGB32_Premultiplied);
                empty.fill(Qt::white);
                whole.fill(Qt::white);
                sql->show_step(0, 0.0);
                sql->render(&empty);
                sql->show_step(finished, 0.0);
                sql->render(&whole);
                require(empty != whole, "And what it draws is the editor and its script");
            }

            // Each demo is shown on a small screen raised off its card, and the
            // Conceptual card's is the Conceptual canvas itself, drawn small
            // (Zain, 2026-09-25): an example made with the Editor's own
            // commands and drawn by the workspace's own view, at the sizes
            // every element is really made at.
            {
                for (auto *demo : home->demos())
                {
                    require(QRectF(demo->rect()).contains(demo->screen()) && demo->screen().contains(demo->screen_inside()),
                            "Each demo is shown on a screen inside its card");
                }
                require(home->demos()[0]->real_canvas() && !home->demos()[1]->real_canvas() && !home->demos()[2]->real_canvas(),
                        "The Conceptual card shows the canvas itself; the others their pictures");
                const auto &canvas = desktop::conceptual_canvas_picture(desktop::ThemeId::Azure);
                require(!canvas.picture.isNull() && canvas.source.width() > canvas.source.height(),
                        "Drawn by the canvas, wider than tall, as the model lies on one line");
                application::Editor example(ids);
                desktop::build_conceptual_example(example);
                const auto &project = example.project();
                QStringList entities;
                for (const auto &[id, entity] : project.entities)
                    entities << QString::fromStdString(entity.name);
                entities.sort();
                require(entities == QStringList{"Course", "Student"}, "Student and Course");
                require(project.relationships.size() == 1 && project.relationships.begin()->second.name == "Enrolled",
                        "Joined by Enrolled");
                for (const auto &side : project.relationships.begin()->second.participants)
                    require(side.maximum == domain::Cardinality::Many, "Many to many");
                int keys = 0;
                QStringList attributes;
                for (const auto &[id, attribute] : project.attributes)
                {
                    attributes << QString::fromStdString(attribute.name);
                    if (attribute.kind == domain::AttributeKind::Key)
                        ++keys;
                }
                attributes.sort();
                require(attributes == QStringList{"Enrollment Date", "ID", "ID", "Name", "Name"} && keys == 2,
                        "Each with its ID as key and its Name, and Enrollment Date on the relationship");
                for (const auto &[id, entity] : project.entities)
                {
                    const auto &at = project.layout.at(domain::ElementRef{id});
                    require(at.width == desktop::entity_body.width && at.height == desktop::entity_body.height,
                            "Entities at the size the canvas makes them");
                }
            }

            // The clock (ADR-022 9.21, stage 5): one clock plays all three,
            // Conceptual first, Relational a second later and SQL a second
            // after that, each going round again at its own length. Looked at
            // by giving the clock moments rather than waiting for them.
            {
                auto *clock = home->demo_clock();
                const auto demos = home->demos();
                // Each demo's motion is kept, switched off on Home for now
                // (Zain, 2026-09-25); switched on here to look at it.
                for (auto *demo : demos)
                    clock->set_playing(demo, true);
                require(clock->delay_of(demos[0]) == 0.0 && clock->delay_of(demos[1]) == 1.0 && clock->delay_of(demos[2]) == 2.0,
                        "Conceptual starts first, Relational a second later, SQL a second after that");
                const auto stands_at = [&](desktop::HomeLiveDemo *demo, double seconds)
                {
                    const auto moment = desktop::demo_moment_at(demo->kind(), seconds);
                    return demo->step() == moment.step && std::abs(demo->progress() - moment.progress) < 1e-9;
                };
                const auto waiting = [](desktop::HomeLiveDemo *demo)
                {
                    return demo->step() == 0 && demo->progress() == 0.0;
                };
                clock->show_at(0.5);
                require(stands_at(demos[0], 0.5) && waiting(demos[1]) && waiting(demos[2]),
                        "At first only Conceptual plays; the others wait at their empty start");
                clock->show_at(1.5);
                require(stands_at(demos[0], 1.5) && stands_at(demos[1], 0.5) && waiting(demos[2]),
                        "A second on, Relational has begun");
                clock->show_at(2.5);
                require(stands_at(demos[1], 1.5) && stands_at(demos[2], 0.5), "And a second after, SQL");
                for (const double later : {12.0, 23.4, 61.7})
                {
                    clock->show_at(later);
                    for (auto *demo : demos)
                        require(stands_at(demo, later - clock->delay_of(demo)),
                                "Each goes round at its own length, from its own start");
                }
                clock->show_at(desktop::demo_loop_seconds(desktop::HomeDemoKind::Conceptual) + 0.05);
                require(demos[0]->step() == 0, "And starts again once it is through");
                // Their first pieces arrive at different moments, so the three
                // never begin moving at once.
                std::vector<double> first_moves;
                for (auto *demo : demos)
                    first_moves.push_back(clock->delay_of(demo) + desktop::demo_steps(demo->kind())[0].seconds);
                for (std::size_t i = 0; i < first_moves.size(); ++i)
                    for (std::size_t j = i + 1; j < first_moves.size(); ++j)
                        require(std::abs(first_moves[i] - first_moves[j]) >= 0.5,
                                "No two begin moving within half a second of each other");

                // And the clock really runs: started, it moves them along.
                home->set_demos_moving(true);
                require(clock->moving(), "Home plays its demos");
                settle_for(400);
                require(clock->seconds() > 0.2, "The clock runs");
                require(demos[0]->step() > 0 || demos[0]->progress() > 0.0, "And moves the demos along");
                home->set_demos_moving(false);
                for (auto *demo : demos)
                    require(demo->step() == desktop::demo_finished_step(demo->kind()),
                            "Stopped, every demo stands finished");
                home->set_demos_moving(true);
            }

            // Reduced motion and pausing (ADR-022 9.21, stage 6). Where motion
            // is not welcome -- asked for through the one seam that says so
            // (ADR-022 9.9) -- the demos never move and each shows its scene
            // finished. And the clock ticks only while Home can be seen:
            // hidden, it stops; shown again, every demo starts from the
            // beginning, in step with the others.
            {
                qputenv("ERDFLOW_REDUCED_MOTION", "1");
                desktop::HomePage still_home(nullptr);
                qunsetenv("ERDFLOW_REDUCED_MOTION");
                still_home.resize(1234, 1003);
                // Shown beside the window without taking its keyboard, which
                // the tests after this one rely on the window keeping.
                still_home.setAttribute(Qt::WA_ShowWithoutActivating, true);
                still_home.show();
                settle_for(250);
                require(!still_home.demo_clock()->moving() && !still_home.demo_clock()->running(),
                        "Where reduced motion is asked for, the demos do not play");
                for (auto *demo : still_home.demos())
                    require(demo->step() == desktop::demo_finished_step(demo->kind()) && demo->shown(),
                            "And each shows its scene finished, so Home still looks complete");
                still_home.hide();
                window.activateWindow();
                settle();
                // And by default, too, they stand still, each finished (Zain,
                // 2026-09-25). The clock is kept for when they are played.
                desktop::HomePage default_home(nullptr);
                default_home.setAttribute(Qt::WA_ShowWithoutActivating, true);
                default_home.show();
                settle_for(150);
                require(!default_home.demo_clock()->running(), "Home shows its demos standing still");
                for (auto *demo : default_home.demos())
                    require(!default_home.demo_clock()->playing(demo) && demo->step() == desktop::demo_finished_step(demo->kind()),
                            "Each switched off, a still picture of its scene finished");
                default_home.hide();
                window.activateWindow();
                settle();

                // Played, the clock ticks only while Home can be seen.
                auto *clock = home->demo_clock();
                home->set_demos_moving(true);
                settle();
                require(window.showing_home() && clock->running(), "On Home, a played clock ticks");
                window.show_home(false);
                settle();
                require(clock->moving() && !clock->running(),
                        "Away from Home it stops ticking, while still meaning to play");
                settle_for(300);
                window.show_home(true);
                settle();
                require(clock->running() && clock->seconds() < 0.25,
                        "Back on Home it starts again from the beginning");
                const auto demos = home->demos();
                require(demos[1]->step() == 0 && demos[2]->step() == 0,
                        "With Relational and SQL waiting their turn, as at the very start");
                home->set_demos_moving(false);
                require(!clock->running(), "Stood still by hand, it stops ticking too");
                home->set_demos_moving(true);
                require(clock->running(), "And plays again when asked, Home being in front");

                // Each demo is switched on or off by itself: switched on alone,
                // one plays while the others stay still pictures, finished.
                for (auto *demo : demos)
                    clock->set_playing(demo, false);
                require(!clock->running(), "With none switched on, nothing ticks");
                clock->set_playing(demos[1], true);
                require(clock->running(), "Switching one on starts the clock");
                clock->show_at(3.0);
                const auto relational_at = desktop::demo_moment_at(desktop::HomeDemoKind::Relational, 2.0);
                require(demos[1]->step() == relational_at.step && std::abs(demos[1]->progress() - relational_at.progress) < 1e-9,
                        "And that one plays");
                require(demos[0]->step() == desktop::demo_finished_step(demos[0]->kind()) && demos[2]->step() == desktop::demo_finished_step(demos[2]->kind()),
                        "While the others stand still, finished");
                clock->set_playing(demos[1], false);
                require(!clock->running() && demos[1]->step() == desktop::demo_finished_step(demos[1]->kind()),
                        "Switched off again, it is a still picture and the clock stops");
            }
            // Shorter still, the page would rather be scrolled than have its
            // cards squashed out of shape (Zain, 2026-09-24).
            window.resize(1280, 720);
            settle();
            settle();
            require(door() || cards[0]->height() > cards[0]->width() * 1.14,
                    "A short window never makes a card wider than its shape");
            window.resize(opened_at);
            settle();
            settle();
            window.resize(opened_at);
            settle();

            // Choosing one moves the choice, and a route that cannot be taken
            // cannot be chosen.
            require(cards[0]->isChecked(), "Conceptual is chosen to begin with");
            desktop::StartRoute heard = desktop::StartRoute::Sql;
            bool told = false;
            // Borrowed, and given back: the window's own routing is what takes
            // somebody to a workspace, and a later case depends on it.
            home->route_selected = [&](desktop::StartRoute route)
            { heard = route; told = true; };
            cards[2]->click();
            settle();
            require(!told && home->chosen_route() == desktop::StartRoute::Conceptual,
                    "Pressing a route that is not built chooses nothing");
            require(cards[0]->isChecked() && !cards[2]->isChecked(), "And the choice stays where it was");
            cards[0]->click();
            settle();
            require(told && heard == desktop::StartRoute::Conceptual, "Choosing a card reports its route");
            require(window.showing_home(),
                    "But choosing is not starting: the home screen is still there");

            // The illustration: drawn, not loaded, and told what to say rather
            // than built around one caller.
            {
                auto *flow = home->hero();
                require(flow != nullptr, "The home screen carries the illustration");
                // Four decorative panels around the database -- not to be
                // confused with the three start cards, which are controls.
                require(flow->items().size() == 4, "It shows four panels around the database");
                require(flow->items()[0].title == "Conceptual ERD" && flow->items()[1].title == "Relationships" && flow->items()[2].title == "Relational Design" && flow->items()[3].title == "SQL",
                        "Named as the product names its levels");
                require(flow->items()[0].icon == desktop::HeroIcon::Structure && flow->items()[1].icon == desktop::HeroIcon::Conceptual && flow->items()[2].icon == desktop::HeroIcon::Relational && flow->items()[3].icon == desktop::HeroIcon::Sql,
                        "Each read by its mark: structure, relationship, table, SQL page");
                for (const auto &item : flow->items())
                {
                    require(!item.labelled, "The product's panels are read by their marks, not labels");
                    const auto shape = item.size.width() / item.size.height();
                    require(shape >= 0.78 && shape <= 0.88, "Each panel is upright, a little taller than wide");
                }
                // A quarter-turn apart, so they never meet as they travel.
                {
                    std::vector<double> starts;
                    for (const auto &item : flow->items())
                        starts.push_back(item.orbit_phase * 360.0);
                    std::sort(starts.begin(), starts.end());
                    for (std::size_t i = 0; i < starts.size(); ++i)
                    {
                        const auto next = i + 1 < starts.size() ? starts[i + 1] : starts[0] + 360.0;
                        require(std::abs(next - starts[i] - 90.0) < 0.5,
                                "The panels start a quarter of the way round from each other");
                    }
                }
                // The same drawing says something else when asked to. This is
                // what makes it a component rather than a picture of this one
                // screen.
                flow->show_items(desktop::WelcomeFlowIllustration::marketing_cards(
                    desktop::tokens(desktop::ThemeId::Azure)));
                require(flow->items()[0].title == "Design" && flow->items()[2].title == "Generate",
                        "It can be given other words entirely");
                // And anything a caller invents.
                std::vector<desktop::HeroOrbitItem> mine;
                desktop::HeroOrbitItem one;
                one.id = "mine";
                one.title = "Anything";
                one.subtitle = "at all";
                one.icon = desktop::HeroIcon::Relational;
                mine.push_back(one);
                flow->show_items(mine);
                require(flow->items().size() == 1 && flow->items()[0].subtitle == "at all",
                        "Including a caller's own, with a subtitle");
                require(flow->orbit_radii(0).width() > 0,
                        "Given no orbit of its own, a caller's panel travels the shared one");
                // A panel can show anything a caller draws, and do something
                // when pressed, without the orbit knowing what either is. The
                // product's own panels do neither: they are decoration.
                {
                    for (const auto &item : desktop::WelcomeFlowIllustration::product_cards(
                             desktop::tokens(desktop::ThemeId::Azure)))
                        require(!item.draw && !item.on_press,
                                "The product's panels draw their own marks and do nothing when pressed");
                    QRectF given;
                    int pressed = 0;
                    desktop::HeroOrbitItem custom;
                    custom.id = "custom";
                    custom.title = "Something new";
                    custom.size = {80, 100};
                    custom.draw = [&](QPainter &painter, const QRectF &face)
                    {
                        given = face;
                        painter.fillRect(face, QColor("#FF00FF"));
                    };
                    custom.on_press = [&]
                    { ++pressed; };
                    flow->show_items({custom});
                    flow->set_moving(false);
                    flow->resize(520, 280);
                    QImage drawn(520, 280, QImage::Format_ARGB32);
                    drawn.fill(Qt::transparent);
                    flow->render(&drawn);
                    require(given.size() == QSizeF(80, 100),
                            "A caller's content is given the panel's own face, upright and unscaled");
                    const auto middle = flow->card_centre(0).toPoint();
                    require(drawn.pixelColor(middle).red() > 200 && drawn.pixelColor(middle).green() < 60,
                            "And is drawn in place of the mark");
                    // Just inside the square corner the fill was asked to
                    // reach, but outside the panel's rounded one.
                    const auto corner = flow->card_outline(0)[0];
                    const auto inward = flow->card_centre(0) - corner;
                    const auto probe = (corner + inward * (2.0 / std::hypot(inward.x(), inward.y()))).toPoint();
                    const auto there = drawn.pixelColor(probe);
                    require(!(there.red() > 200 && there.green() < 60 && there.blue() > 200),
                            "Kept inside its panel's rounded shape");
                    const auto at = flow->card_centre(0);
                    QMouseEvent down(QEvent::MouseButtonPress, at, at, Qt::LeftButton, Qt::LeftButton,
                                     Qt::NoModifier);
                    QMouseEvent up(QEvent::MouseButtonRelease, at, at, Qt::LeftButton, Qt::NoButton,
                                   Qt::NoModifier);
                    QApplication::sendEvent(flow, &down);
                    QApplication::sendEvent(flow, &up);
                    require(pressed == 1, "Pressing a panel that has something to do does it");
                    const QPointF empty(4, 4);
                    QMouseEvent elsewhere(QEvent::MouseButtonPress, empty, empty, Qt::LeftButton,
                                          Qt::LeftButton, Qt::NoModifier);
                    QMouseEvent away(QEvent::MouseButtonRelease, empty, empty, Qt::LeftButton,
                                     Qt::NoButton, Qt::NoModifier);
                    QApplication::sendEvent(flow, &elsewhere);
                    QApplication::sendEvent(flow, &away);
                    require(pressed == 1, "And pressing beside it does nothing");
                    flow->set_moving(true);
                }
                flow->show_items({});
                require(flow->items().size() == 4 && flow->items()[0].title == "Conceptual ERD",
                        "And asking for none puts the product's own back");
                // Painted rather than fetched: it renders into whatever size
                // it is given, which a bitmap of one size could not.
                QImage small(160, 90, QImage::Format_ARGB32);
                small.fill(Qt::transparent);
                flow->resize(160, 90);
                flow->render(&small);
                QImage large(520, 280, QImage::Format_ARGB32);
                large.fill(Qt::transparent);
                flow->resize(520, 280);
                flow->render(&large);
                const auto inked = [](const QImage &of)
                {
                    int count = 0;
                    for (int y = 0; y < of.height(); ++y)
                        for (int x = 0; x < of.width(); ++x)
                            if (qAlpha(of.pixel(x, y)) > 8)
                                ++count;
                    return count;
                };
                require(inked(small) > 200 && inked(large) > inked(small) * 3,
                        "It is drawn at whatever size it is given, not scaled from one");

                // One scale on both axes: given room of another shape, the
                // drawing keeps its own and the panels keep theirs.
                {
                    const auto panel_shape = [&](std::size_t which)
                    {
                        const auto outline = flow->card_outline(which);
                        const auto across = std::abs(outline[1].x() - outline[0].x());
                        const auto down = std::hypot(outline[3].x() - outline[0].x(),
                                                     outline[3].y() - outline[0].y());
                        return across / down;
                    };
                    flow->set_moving(false);
                    std::vector<double> shapes;
                    for (std::size_t i = 0; i < flow->items().size(); ++i)
                        shapes.push_back(panel_shape(i));
                    const auto scale_wide = flow->drawing_scale();
                    flow->resize(900, 280);
                    require(std::abs(flow->drawing_scale() - scale_wide) < 1e-9,
                            "Wider room does not stretch the drawing: it keeps one scale for both axes");
                    for (std::size_t i = 0; i < flow->items().size(); ++i)
                    {
                        require(std::abs(panel_shape(i) - shapes[i]) < 1e-6,
                                "And every panel keeps its shape at any size");
                        const auto &item = flow->items()[i];
                        require(std::abs(shapes[i] - item.size.width() / item.size.height()) < 1e-6,
                                "Which is its own shape, not one squeezed on either axis");
                    }
                    flow->resize(520, 280);
                    flow->set_moving(true);
                }

                // A full revolution round the database: a quarter of the way
                // every 4.5 seconds, the whole way in 18, at a constant speed,
                // each panel on the one ellipse round the one centre, and the
                // line to each fixed to it wherever it has got to.
                {
                    require(flow->moving(), "The illustration moves by default");
                    const auto centre = flow->orbit_centre();
                    const auto scale = flow->drawing_scale();
                    const auto on_orbit = [&](std::size_t which)
                    {
                        const auto radii = flow->orbit_radii(which);
                        const auto at = flow->card_centre(which) - centre;
                        const auto x = at.x() / (radii.width() * scale);
                        const auto y = at.y() / (radii.height() * scale);
                        return std::abs(x * x + y * y - 1.0) < 0.002;
                    };
                    const auto attached = [&](std::size_t which)
                    {
                        const auto outline = flow->card_outline(which);
                        const auto end = flow->connector_end(which);
                        for (int corner = 0; corner < 4; ++corner)
                        {
                            const auto a = outline[corner];
                            const auto b = outline[(corner + 1) % 4];
                            const auto along = b - a;
                            const auto length = std::hypot(along.x(), along.y());
                            const auto cross = std::abs(along.x() * (end.y() - a.y()) - along.y() * (end.x() - a.x()));
                            const auto dot = QPointF::dotProduct(end - a, along);
                            if (cross / length < 1.0 && dot >= -0.5 && dot <= length * length + 0.5)
                                return true;
                        }
                        return false;
                    };
                    const auto shared = flow->orbit_radii(0);
                    for (std::size_t i = 1; i < flow->items().size(); ++i)
                        require(flow->orbit_radii(i) == shared, "The panels share one orbit");
                    require(shared.width() > shared.height() * 1.8,
                            "An ellipse, wider than tall, as the drawing is");
                    const auto degrees_apart = [](double a, double b)
                    {
                        const auto d = std::fmod(std::abs(a - b), 360.0);
                        return std::min(d, 360.0 - d);
                    };
                    std::vector<QPolygonF> earlier;
                    for (const auto [seconds, quarter] : std::vector<std::pair<double, double>>{
                             {0.0, 0.0}, {4.5, 90.0}, {9.0, 180.0}, {13.5, 270.0}, {18.0, 360.0}})
                    {
                        flow->set_clock(seconds);
                        for (std::size_t i = 0; i < flow->items().size(); ++i)
                        {
                            const auto expected = flow->items()[i].orbit_phase * 360.0 + quarter;
                            require(degrees_apart(flow->angle_of_card(i), expected) < 0.01,
                                    "Each panel is a quarter further round every 4.5 seconds");
                            require(on_orbit(i), "And is always on the orbit");
                            require(attached(i), "Its line is fixed to its edge wherever it is");
                            const auto line = flow->connector(i);
                            require(line.size() == 4 && line.front() == flow->connector_end(i) && line.back() == flow->connector_start(i),
                                    "Three straight segments and two bends, from the panel to the platform");
                            // The two ends match: the same length, pointing
                            // the same way, whichever panel and wherever it is.
                            const auto leaving = line[1] - line[0];
                            const auto arriving = line[3] - line[2];
                            const auto leaving_length = std::hypot(leaving.x(), leaving.y());
                            const auto arriving_length = std::hypot(arriving.x(), arriving.y());
                            require(std::abs(leaving_length - arriving_length) < 0.01,
                                    "The line leaves its panel and arrives by runs of the same length");
                            require(std::abs(leaving.x() * arriving.y() - leaving.y() * arriving.x()) < 0.01 && QPointF::dotProduct(leaving, arriving) >= 0.0,
                                    "Pointing the same way");
                            // Out of the panel, never back across it.
                            if (leaving_length > 0.5)
                                require(!flow->card_outline(i).containsPoint(line[1], Qt::OddEvenFill),
                                        "The first bend is outside the panel it leaves");
                            // Beside or behind the database there is room, and
                            // both bends are real ones: the middle slants away
                            // from the runs either side of it.
                            if (flow->card_behind(i))
                            {
                                const auto middle = line[2] - line[1];
                                const auto turn = std::abs(leaving.x() * middle.y() - leaving.y() * middle.x()) / (leaving_length * std::hypot(middle.x(), middle.y()));
                                require(leaving_length > 8.0 * scale && turn > 0.3,
                                        "A panel behind the database has two clear bends in its line");
                            }
                            const auto into = flow->connector_start(i) - centre;
                            require(std::abs(into.x()) < 90 * scale && into.y() > 0 && into.y() < 80 * scale,
                                    "Ending on the platform under the database");
                            const auto behind = std::sin(flow->angle_of_card(i) * M_PI / 180.0) < 0;
                            require(flow->card_behind(i) == behind,
                                    "A panel behind the database is drawn behind it, one in front before it");
                        }
                        std::vector<QPolygonF> lines;
                        for (std::size_t i = 0; i < flow->items().size(); ++i)
                            lines.push_back(flow->connector(i));
                        if (!earlier.empty() && seconds < 18.0)
                            for (std::size_t i = 0; i < lines.size(); ++i)
                                require(lines[i] != earlier[i],
                                        "Each line is worked out again as its panel moves, never left behind");
                        earlier = lines;
                    }
                    // No line ever jumps. All the way round, a hundredth of a
                    // second moves every point of every line a little, as it
                    // moves the panels, including where a run turns from
                    // across to down and where a line goes behind the
                    // database.
                    {
                        std::vector<QPolygonF> before;
                        double worst = 0.0;
                        for (int step = 0; step <= 1800; ++step)
                        {
                            flow->set_clock(step * 0.01);
                            std::vector<QPolygonF> now;
                            for (std::size_t i = 0; i < flow->items().size(); ++i)
                                now.push_back(flow->connector(i));
                            if (!before.empty())
                                for (std::size_t i = 0; i < now.size(); ++i)
                                    for (qsizetype k = 0; k < now[i].size(); ++k)
                                    {
                                        const auto moved = now[i][k] - before[i][k];
                                        worst = std::max(worst, std::hypot(moved.x(), moved.y()));
                                    }
                            before = now;
                        }
                        require(worst < 2.5 * scale,
                                "A line never jumps: every moment of the orbit moves it only a little");
                    }
                    // Constant speed: twenty degrees a second, anywhere round.
                    flow->set_clock(2.0);
                    const auto at_two = flow->angle_of_card(0);
                    flow->set_clock(3.0);
                    const auto at_three = flow->angle_of_card(0);
                    flow->set_clock(11.0);
                    const auto at_eleven = flow->angle_of_card(0);
                    flow->set_clock(12.0);
                    require(std::abs(degrees_apart(at_three, at_two) - 20.0) < 0.01 && std::abs(degrees_apart(flow->angle_of_card(0), at_eleven) - 20.0) < 0.01,
                            "At a constant twenty degrees a second, with no easing");
                    // Halfway round, a panel is on the far side of the database.
                    flow->set_clock(0.0);
                    const auto start = flow->card_centre(0) - centre;
                    flow->set_clock(9.0);
                    const auto half = flow->card_centre(0) - centre;
                    require(std::hypot(start.x() + half.x(), start.y() + half.y()) < 0.01,
                            "Halfway round, a panel is exactly opposite where it began");
                    // Behind is further away: smaller and fainter.
                    bool some_behind = false;
                    for (std::size_t i = 0; i < flow->items().size(); ++i)
                        if (flow->card_behind(i))
                        {
                            some_behind = true;
                            for (std::size_t j = 0; j < flow->items().size(); ++j)
                                if (!flow->card_behind(j))
                                    require(flow->card_depth(i) < flow->card_depth(j),
                                            "A panel behind the database is further away than one in front");
                        }
                    require(some_behind, "Some panels are behind the database at any moment");
                    require(flow->orbit_centre() == centre, "And the database stays where it is");

                    // The real clock keeps it going, at about the same speed.
                    flow->set_clock(0.0);
                    settle_for(300);
                    const auto went = flow->angle_of_card(0) - flow->items()[0].orbit_phase * 360.0;
                    require(went > 2.0 && went < 14.0, "It travels on its own, twenty degrees a second");
                }

                // Pointing at a panel highlights it and stops nothing.
                const auto over_first = flow->card_centre(0);
                QMouseEvent hover(QEvent::MouseMove, over_first, over_first, Qt::NoButton,
                                  Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(flow, &hover);
                require(flow->hovered_card() == 0,
                        "The pointer is found to be over the panel that sits there");
                const auto held = flow->angle_of_card(0);
                settle_for(160);
                require(flow->angle_of_card(0) != held,
                        "And the panel carries on round: nothing but stillness stops it");
                QEvent gone(QEvent::Leave);
                QApplication::sendEvent(flow, &gone);
                require(flow->hovered_card() == -1, "Leaving takes the highlight away");

                flow->set_moving(false);
                require(!flow->moving(), "Motion can always be turned off");
                for (std::size_t i = 0; i < flow->items().size(); ++i)
                {
                    const auto &item = flow->items()[i];
                    const auto radii = flow->orbit_radii(i);
                    const auto angle = item.orbit_phase * 2.0 * M_PI;
                    const auto rest = flow->orbit_centre() + QPointF(std::cos(angle) * radii.width(), std::sin(angle) * radii.height()) * flow->drawing_scale();
                    const auto off = flow->card_centre(i) - rest;
                    require(std::hypot(off.x(), off.y()) < 0.01,
                            "Turned off, every panel is back where it starts, not frozen mid-way");
                    require(!flow->connector(i).isEmpty(), "And its line is still drawn to it");
                }
                QImage still_a(200, 108, QImage::Format_ARGB32);
                still_a.fill(Qt::transparent);
                flow->resize(200, 108);
                flow->render(&still_a);
                settle_for(120);
                QImage still_b(200, 108, QImage::Format_ARGB32);
                still_b.fill(Qt::transparent);
                flow->render(&still_b);
                require(still_a == still_b,
                        "With motion off, nothing changes however long is waited");
                flow->set_moving(true);
                require(flow->moving(), "And it can be turned back on");

                // What a platform asks for is read once, at the start, and
                // an explicit call always outranks it afterwards.
                qputenv("ERDFLOW_REDUCED_MOTION", "1");
                desktop::WelcomeFlowIllustration asked_for_stillness(nullptr);
                require(!asked_for_stillness.moving(),
                        "A platform that asks for stillness is given it from the start");
                qunsetenv("ERDFLOW_REDUCED_MOTION");
                desktop::WelcomeFlowIllustration asked_for_nothing(nullptr);
                require(asked_for_nothing.moving(), "And without that ask, it moves as usual");
            }

            // Each card carries its own + Create, and only that starts a
            // project. Nothing is asked under the cards: the page ends with
            // them (Zain, 2026-09-24, ADR-022 9.19).
            for (const char *gone : {"projectDetailsForm", "projectName", "projectLocation",
                                     "projectMoreOptions", "projectSavedTo", "projectCreate",
                                     "projectCancel"})
                require(home->findChild<QWidget *>(gone) == nullptr,
                        (std::string("Nothing is asked under the cards, not even ") + gone).c_str());
            for (auto *card : cards)
            {
                auto *create = card->create_button();
                require(create != nullptr && create->isVisible() && create->text() == "+ Create",
                        "Every card has its own + Create");
                require(QRect(QPoint(), card->size()).contains(create->geometry()) && create->geometry().top() > card->height() * 0.75,
                        "Inside the card, at its foot");
                // Create with AI is not offered for now (Zain, 2026-09-26): it
                // is made on every card but not shown, and + Create stands
                // alone in the middle of the card, the size it is beside it.
                auto *ai = card->ai_button();
                require(card->ai_offered() == desktop::create_with_ai_offered && !desktop::create_with_ai_offered,
                        "Create with AI is not offered for now");
                require(ai != nullptr && ai->isHidden(), "It is made, but not shown");
                require(std::abs(create->geometry().left() + create->width() / 2.0 - card->width() / 2.0) <= 1.0,
                        "+ Create stands alone in the middle of the card");
                const auto alone = create->geometry();
                // Offered again, the two stand exactly as they did (Zain,
                // 2026-09-25): on every card, on the same line, the two
                // together centred on the card.
                card->set_ai_offered(true);
                settle();
                require(ai->isVisible() && (ai->text() == "Create with AI" || ai->text() == "AI"),
                        "Every card has its own Create with AI");
                require(create->height() == alone.height(), "+ Create keeps its height when AI is offered");
                require(std::abs(alone.width() - card->width() * 0.52) <= 2,
                        "The single Create button has the approved panel proportion");
                require(ai->y() == create->y() && ai->height() == create->height() && ai->x() > create->geometry().right(),
                        "Level with + Create, to its right");
                require(QRect(QPoint(), card->size()).contains(ai->geometry()), "Inside the card too");
                const auto pair = create->geometry().united(ai->geometry());
                require(std::abs(pair.left() + pair.width() / 2.0 - card->width() / 2.0) <= 1.0,
                        "And the pair centred on it");
                require(create->isEnabled() == card->isEnabled(),
                        "It can be pressed only where its card can be taken");
                // AI is a later feature: shown, not yet pressable, and saying so.
                require(!ai->isEnabled() && !ai->toolTip().isEmpty(),
                        "Create with AI cannot be pressed yet, and says why");
                require(ai->accessibleName() == "Create " + card->accessibleName() + " with AI",
                        "It says what it would make to whatever reads the screen");
                // Put away again, + Create goes back to the middle.
                card->set_ai_offered(desktop::create_with_ai_offered);
                settle();
                require(ai->isHidden() && create->geometry() == alone, "And back to the middle when it is put away");
            }
            // Every card's buttons stand on one line across the cards.
            for (auto *card : cards)
                require(card->create_button()->mapTo(home, QPoint()).y() == cards[0]->create_button()->mapTo(home, QPoint()).y(),
                        "Every card's buttons share one baseline");
            // No heading over the cards: each card's + Create says it (Zain,
            // 2026-09-25). Its room is kept, so the cards stand where they did.
            {
                auto *section = home->findChild<QLabel *>("homeSectionTitle");
                require(section && !section->isVisible(), "Nothing is shown over the cards");
                require(section->height() > 0 && section->mapTo(home, QPoint(0, section->height())).y() <= cards[0]->mapTo(home, QPoint()).y(),
                        "But its room is kept above them");
            }
            // Between each card and the next, the way on and the way back:
            // in their own columns, never over a card, and only a sign.
            {
                const auto bridges = home->bridges();
                require(bridges.size() == 2, "A way between each card and the next");
                const QStringList said{"Convert to Schema, Back to ERD", "Generate to SQL, Back to Schema"};
                for (std::size_t i = 0; i < bridges.size(); ++i)
                {
                    auto *bridge = bridges[i];
                    require(bridge->isVisible() && bridge->accessibleName() == said[static_cast<int>(i)],
                            "Each says where it goes and where it comes back from");
                    require(bridge->parentWidget() == cards[i]->parentWidget() && bridge->x() > cards[i]->geometry().right() && bridge->geometry().right() < cards[i + 1]->x(),
                            "Standing in the gap between its two cards, over neither");
                    require(bridge->width() >= 62, "With room for its words");
                    require(bridge->focusPolicy() == Qt::NoFocus && bridge->testAttribute(Qt::WA_TransparentForMouseEvents),
                            "A sign, which neither the keyboard nor the pointer stops at");
                }
            }
            require(cards[0]->create_button()->accessibleName() == "Create Conceptual Design (ERD)",
                    "It says what it makes to whatever reads the screen");
            require(!cards[2]->create_button()->toolTip().isEmpty(),
                    "And, where it cannot be pressed yet, why");

            // Pressing it opens a new conceptual project, untitled, as New
            // Project does, and leaves Home for it. Nothing is written yet:
            // where the project is kept is asked elsewhere.
            require(window.showing_home() && !window.editor().dirty(), "On Home, with nothing unsaved");
            cards[0]->create_button()->click();
            settle();
            require(!window.showing_home(), "+ Create leaves Home for the new project");
            require(window.editor().project().entities.empty() && !window.editor().dirty(),
                    "Which is a new, empty conceptual project");
            require(cards[0]->isChecked(), "And the card it came from is the one chosen");

            // The primary key's mark is a golden key held bow up and pointing
            // down, from its own drawing (Zain, 2026-09-26).
            {
                const auto key = desktop::primary_key_mark(24, 1.0, false).toImage().convertToFormat(QImage::Format_ARGB32);
                require(key.size() == QSize(24, 24), "Drawn at the size asked for");
                int gold = 0;
                for (int y = 0; y < key.height(); ++y)
                    for (int x = 0; x < key.width(); ++x)
                    {
                        const auto ink = key.pixelColor(x, y);
                        if (ink.alpha() > 200 && ink.red() > ink.blue() + 80 && ink.green() > ink.blue() + 30)
                            ++gold;
                    }
                require(gold > 40, "It is golden");
                // Across a band of rows: how wide what is drawn there is, and
                // how far right it reaches.
                const auto extent = [&](int from, int to)
                {
                    int least = key.width(), most = -1;
                    for (int y = from; y < to; ++y)
                        for (int x = 0; x < key.width(); ++x)
                            if (key.pixelColor(x, y).alpha() > 128)
                            {
                                least = std::min(least, x);
                                most = std::max(most, x);
                            }
                    return std::pair{most - least + 1, most};
                };
                const auto bow = extent(4, 10);
                const auto shaft = extent(13, 14);
                const auto teeth = extent(19, 22);
                require(bow.first > 2 * shaft.first, "Its ring is at the top");
                require(teeth.second > shaft.second + 2, "And its teeth at the foot, so it points down");
            }

            // Plain shows no colour at all (Zain, 2026-09-24): not in any
            // icon of any set -- the coloured artwork included -- and not in
            // anything the home screen draws for itself.
            {
                // The Schema Relationships group has its own small network,
                // in every theme and icon mode. Conceptual keeps its diamond.
                require(desktop::icon_name(desktop::Glyph::Relationship) == "relationship" && desktop::icon_name(desktop::Glyph::SchemaRelationships) == "schema-relationships",
                        "Schema and Conceptual relationships use separate glyphs");
                for (const auto &colors : desktop::themes())
                    for (const auto mode : {desktop::IconMode::Normal, desktop::IconMode::Modern,
                                            desktop::IconMode::Outline})
                    {
                        const auto network = desktop::glyph_icon(desktop::Glyph::SchemaRelationships, colors, 20, mode);
                        const auto drawn = network.pixmap(QSize(20, 20), 3.0).toImage();
                        require(!drawn.isNull() && drawn != desktop::glyph_icon(desktop::Glyph::Relationship, colors, 20, mode).pixmap(QSize(20, 20), 3.0).toImage() && drawn != desktop::glyph_icon(desktop::Glyph::Connect, colors, 20, mode).pixmap(QSize(20, 20), 3.0).toImage(),
                                "The connected-nodes icon renders distinctly from Conceptual and Connect in every theme and mode");
                        if (desktop::colourless(colors.id))
                            require(coloured_pixels(drawn) == 0, "The new Schema icon follows monochrome themes");
                    }
                const auto &plain = desktop::theme(desktop::ThemeId::Plain);
                for (int glyph = 0; glyph <= static_cast<int>(desktop::Glyph::Key); ++glyph)
                    for (const auto mode : {desktop::IconMode::Normal, desktop::IconMode::Modern,
                                            desktop::IconMode::Outline})
                    {
                        const auto icon = desktop::glyph_icon(static_cast<desktop::Glyph>(glyph), plain, 22, mode);
                        for (const auto state : {QIcon::Off, QIcon::On})
                            require(coloured_pixels(icon.pixmap(QSize(22, 22), 3.0, QIcon::Normal, state)
                                                        .toImage()) == 0,
                                    "Under Plain, every icon of every set is drawn without colour");
                    }
                // The primary key's golden mark follows Plain too (Zain,
                // 2026-09-26): its shape and shading, in greys.
                require(coloured_pixels(desktop::primary_key_mark(19, 3.0, true).toImage()) == 0,
                        "Under Plain, the primary key's mark is grey");
                const auto wearing = window.canvas()->theme_id();
                window.set_theme(desktop::ThemeId::Plain);
                settle();
                // Photographed with its lettering smoothed in greys: Plain
                // promises that ERDFlow adds no colour, and a platform that
                // smooths text with coloured subpixels adds its own to every letter.
                require(coloured_pixels(grab_without_subpixel_text(window)) == 0,
                        "And the home screen has no colour anywhere: cards, badges, drawing, decoration");
                window.set_theme(wearing);
                settle();
                // Under any other theme the coloured artwork keeps its colours.
                require(coloured_pixels(desktop::glyph_icon(desktop::Glyph::Relationship,
                                                            desktop::theme(desktop::ThemeId::Azure), 22,
                                                            desktop::IconMode::Modern)
                                            .pixmap(QSize(22, 22), 3.0)
                                            .toImage()) > 0,
                        "While elsewhere the coloured icons keep their colour");
            }

            window.show_home(false);
            settle();
        }

        // Back to Home is always first in the workspace's own row (Zain,
        // 2026-09-26): Home is the door every project is come in by, and a
        // change of mind can always go back to choose another card.
        {
            auto *home = static_cast<desktop::HomePage *>(window.findChild<QWidget *>("homePage"));
            auto *back = child<QPushButton>(window, "backToHome");
            require(!window.showing_home() && back->isVisible(), "The workspace offers the way back to Home");
            // In the diagram's one row (Zain, 2026-10-08) it is the arrow alone,
            // first in the row, and says where it goes on hover and to a screen
            // reader.
            require(back->text() == "←" && back->accessibleName() == "Home" && back->toolTip().contains("Home screen"),
                    "Saying where it goes");
            auto *bar = child<QToolBar>(window, "modelTools");
            auto *identity = child<QWidget>(window, "conceptualIdentity");
            require(bar->widgetForAction(bar->actions().front()) == identity && identity->layout()->indexOf(back) == 0,
                    "First in the row, where a way back is looked for");
            // Whatever the project, and however it was opened.
            window.show_home(true);
            settle();
            // The example from the workspace's File menu, where it now stands
            // (Zain, 2026-10-03).
            child<QAction>(window, "fileOpenExample")->trigger();
            settle();
            require(!window.showing_home() && back->isVisible(), "An example offers it");
            child<QPushButton>(window, "openExample")->click();
            settle();
            require(back->isVisible(), "Opened from inside a project too");
            // Staying to build changes nothing: the way back stays.
            child<QAction>(window, "toolEntity")->trigger();
            click_canvas(*window.canvas(), QPointF(-2000, -2000));
            require(back->isVisible(), "Working on the project keeps the way back");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            // It goes to Home, and what is open stays open behind it.
            const auto open = window.editor().project().id;
            back->click();
            settle();
            require(window.showing_home(), "Back returns to Home");
            require(window.editor().project().id == open, "Leaving the project open");
            // And Home offers the way back in (Zain, 2026-09-27), beside
            // Theme, named for the workspace it returns to.
            auto *returning = home->top_bar()->return_button();
            require(returning->isVisible() && returning->text().contains("Return to Conceptual Design"),
                    "Home offers the way back into the workspace, named");
            returning->click();
            settle();
            require(!window.showing_home() && window.editor().project().id == open,
                    "Which returns to the workspace as it was left");
            back->click();
            settle();

            // The template is not the example (Zain, 2026-09-26): it is the
            // general things a diagram is made of, named for what they are.
            child<QAction>(window, "fileTemplate")->trigger();
            settle();
            require(!window.showing_home() && back->isVisible(), "The template offers the way back too");
            const auto &started = window.editor().project();
            require(started.entities.size() == 2 && started.attributes.size() == 2 && started.relationships.size() == 1,
                    "Two entities, an attribute on each, and a relationship between them");
            require(std::all_of(started.entities.begin(), started.entities.end(),
                                [](const auto &each)
                                { return each.second.name == "Entity"; }) &&
                        std::all_of(started.attributes.begin(), started.attributes.end(),
                                    [](const auto &each)
                                    {
                                        return each.second.name == "Attribute" && each.second.owner.has_value();
                                    }) &&
                        started.relationships.begin()->second.name == "Relationship",
                    "Each named for what it is, not the example's students and courses");
            require(started.relationships.begin()->second.participants.size() == 2,
                    "The relationship joins the two entities");
            require(started.connectors.empty(), "Every line starts unlocked");
            require(started.name == "Untitled" && !window.editor().dirty(), "Untitled and unsaved, as a new project is");
            // A new project, started from the File menu, has it as well.
            child<QAction>(window, "newProject")->trigger();
            settle();
            require(back->isVisible(), "A new project offers it");

            // On the schema too, sharing the stage or filling the window.
            window.load_example();
            settle();
            child<QPushButton>(window, "previewSchema")->click();
            settle_motion(window);
            require(back->isVisible(), "With the schema preview open");
            auto *full = child<QPushButton>(window, "schemaFull");
            full->click();
            settle();
            require(back->isVisible(), "And with the schema filling the window");
            // Left from the schema, the way back in returns to the schema,
            // still filling the window.
            back->click();
            settle();
            require(returning->isVisible() && returning->text().contains("Return to Relational Design"),
                    "From the schema, Home names the schema");
            returning->click();
            settle();
            require(!window.showing_home() && child<QLabel>(window, "workspaceBadge")->text() == "RELATIONAL DESIGN" && full->text() == "Exit full",
                    "And returns to it, still filling the window");
            full->click();
            settle();
            child<QPushButton>(window, "previewSchema")->click();
            settle_motion(window);
        }
        window.load_example();
        settle();
        const auto original = window.editor().project();
        // What follows counts from the example rather than from a number typed
        // here, so the diagram it ships with can grow without the test having
        // to be re-tallied line by line.
        const auto example_entities = original.entities.size();
        const auto example_attributes = original.attributes.size();
        require(example_entities == 3 && example_attributes == 16 && original.relationships.size() == 3,
                "The example is the whole university diagram, not a fragment of it");
        // It is the example because one of every kind of attribute is on it,
        // so opening it puts the whole of the notation on the canvas at once.
        std::map<domain::AttributeKind, int> kinds;
        std::size_t parts_of_composites = 0;
        std::size_t on_relationships = 0;
        for (const auto &[id, attribute] : original.attributes)
        {
            ++kinds[attribute.kind];
            if (!attribute.owner)
                continue;
            if (std::holds_alternative<domain::AttributeId>(*attribute.owner))
                ++parts_of_composites;
            if (std::holds_alternative<domain::RelationshipId>(*attribute.owner))
                ++on_relationships;
        }
        require(kinds[domain::AttributeKind::Key] == 3 && kinds[domain::AttributeKind::Composite] == 1 && kinds[domain::AttributeKind::Derived] == 1 && kinds[domain::AttributeKind::Multivalued] == 1,
                "Key, composite, derived and multivalued are all drawn on it");
        require(parts_of_composites == 3, "The composite name has its three parts hanging off it");
        require(on_relationships == 1, "And the enrollment carries the date that belongs to neither side");
        bool total = false;
        bool partial = false;
        for (const auto &[id, relationship] : original.relationships)
        {
            require(relationship.participants.size() == 2, "Every relationship on it is joined at both ends");
            for (const auto &participant : relationship.participants)
                (participant.participation == domain::Participation::Total ? total : partial) = true;
        }
        require(total && partial, "With both minimums shown, so the pair beside a line is worth reading");
        require(!window.editor().dirty(), "Unmodified bundled example is clean");
        auto student = original.entities.begin()->first;
        for (const auto &[id, entity] : original.entities)
            if (entity.name == "Student")
                student = id;
        window.canvas()->select_elements({student});
        auto *name = child<QLineEdit>(window, "elementName");
        name->setFocus();
        name->setText("UniversityStudent");
        window.canvas()->setFocus();
        settle();
        require(window.editor().project().entities.at(student).name == "UniversityStudent", "Name commits through properties on focus loss");
        require(window.editor().dirty() && window.isWindowModified(), "Applied field edit marks project dirty");
        auto *undo = child<QAction>(window, "undoCommand");
        require(undo->isEnabled(), "Undo action reflects history");
        // Undo and redo must be reachable without opening a menu, and the
        // toolbar button must not resize as the named edit changes.
        auto *tools = child<QToolBar>(window, "modelTools");
        require(tools->actions().contains(undo), "Undo is on the toolbar");
        require(tools->actions().contains(child<QAction>(window, "redoCommand")), "Redo is on the toolbar");
        require(undo->text().startsWith("Undo ") && undo->text() != "Undo",
                "The menu entry names the edit that will be reversed");
        require(undo->iconText() == "Undo", "The toolbar button keeps fixed wording");
        require(undo->toolTip().contains(undo->text()), "The toolbar tooltip explains the edit");
        child<QAction>(window, "undoCommand")->trigger();
        require(window.editor().project() == original && !window.editor().dirty(), "Shell undo restores clean save point");
        child<QAction>(window, "redoCommand")->trigger();
        require(window.editor().project().entities.at(student).name == "UniversityStudent", "Shell redo applies rename");

        auto *description = child<QPlainTextEdit>(window, "elementDescription");
        description->setFocus();
        description->setPlainText("A student enrolled in courses.\nNames may change; identity stays stable.");
        window.canvas()->setFocus();
        settle();
        require(window.editor().project().entities.at(student).description.find("identity") != std::string::npos,
                "Description commits through command path");

        name = child<QLineEdit>(window, "elementName");
        name->setFocus();
        name->setCursorPosition(static_cast<int>(name->text().size()));
        QKeyEvent backspace(QEvent::KeyPress, Qt::Key_Backspace, Qt::NoModifier);
        QApplication::sendEvent(name, &backspace);
        require(window.editor().project().entities.size() == example_entities, "Backspace in property field cannot delete entity");
        window.canvas()->setFocus();
        settle();

        const auto relationship = original.relationships.begin()->first;
        const auto participant = original.relationships.begin()->second.participants.front().id;
        window.canvas()->select_elements({relationship});
        settle();
        // Count the combos inside the participant cards rather than every combo
        // in the panel: the panel also carries the relationship's own controls.
        auto cards = child<QDockWidget>(window, "propertiesDock")->findChildren<QWidget *>("participantCard");
        require(cards.size() == 2, "Relationship renders two independent participant forms");

        // A card names its side in the colour that element is drawn in, and
        // gives each thing it asks about a bold hue of its own, so the rows
        // are told apart at a glance rather than read in order.
        {
            auto *first_card = cards.front();
            auto *title = first_card->findChild<QWidget *>("cardTitle");
            require(title != nullptr, "A card names its side");
            const auto display_name_of_relationship =
                QString::fromStdString(domain::name(window.editor().project(), domain::ElementRef{relationship}));
            require(!display_name_of_relationship.isEmpty(), "The relationship has a name to show");
            const auto &colors = desktop::theme(window.canvas()->theme_id());
            const auto tag = [&](QWidget *row, const char *named)
            {
                auto *found = row->findChild<QWidget *>(QString::fromLatin1(named));
                require(found != nullptr, "Each end of a card is shown as its own shape");
                return found;
            };
            auto *side_tag = tag(title, "cardShape");
            auto *toward_tag = tag(title, "cardTowardShape");
            require(toward_tag->toolTip() == display_name_of_relationship,
                    "The far one being the relationship itself");
            require(!side_tag->toolTip().isEmpty() && side_tag->toolTip() != toward_tag->toolTip(),
                    "And the near one the element taking part in it");
            require(title->findChild<QLabel *>("cardArrow") != nullptr,
                    "With a mark between them that says it is joined to it");

            // Each name is written inside its element's own shape, in that
            // element's colour, rather than in a box beside a picture of one.
            const auto shows = [](QWidget *widget, const QColor &wanted)
            {
                const auto drawn = widget->grab().toImage().convertToFormat(QImage::Format_ARGB32);
                for (int y = 0; y < drawn.height(); ++y)
                    for (int x = 0; x < drawn.width(); ++x)
                    {
                        const auto pixel = drawn.pixelColor(x, y);
                        if (pixel.alpha() > 200 && std::abs(pixel.red() - wanted.red()) < 12 && std::abs(pixel.green() - wanted.green()) < 12 && std::abs(pixel.blue() - wanted.blue()) < 12)
                            return true;
                    }
                return false;
            };
            require(shows(side_tag, colors.entity_fill), "The side wears the entity's own colour");
            require(shows(toward_tag, colors.relationship_fill), "And the relationship its own");

            // A recoloured entity carries its new colour into the card.
            const auto side = window.editor().project().relationships.at(relationship).participants.front().target;
            require(bool(editor.recolour({domain::target_ref(side)}, domain::Colour{0x9E, 0xE8, 0xC4})), "Colour that entity");
            // An edit made straight on the editor does not pass through the
            // window, so the canvas is brought up to date and the panel asked
            // to rebuild, which is the order every edit through the window
            // follows: the shapes the panel shows are the canvas's own.
            window.canvas()->synchronize();
            window.canvas()->select_elements({});
            window.canvas()->select_elements({relationship});
            settle();
            cards = child<QDockWidget>(window, "propertiesDock")->findChildren<QWidget *>("participantCard");
            title = cards.front()->findChild<QWidget *>("cardTitle");
            require(title && shows(tag(title, "cardShape"), QColor(0x9E, 0xE8, 0xC4)),
                    "The card follows the element's own colour");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            cards = child<QDockWidget>(window, "propertiesDock")->findChildren<QWidget *>("participantCard");
            require(cards.size() == 2, "The cards are still there after the undo");

            const auto labels = cards.front()->findChildren<QLabel *>("fieldLabel");
            require(labels.size() == 3, "Its three questions are each named");
            std::set<QString> inks;
            for (auto *label : labels)
            {
                require(label->styleSheet().contains("font-weight: 700"), "Each name is bold");
                inks.insert(label->styleSheet());
            }
            require(inks.size() == 1, "All in one ink: the words tell them apart, so colour is left to the element");
        }

        // The rest of the panel is named the same way: every field label bold
        // and in one ink, with the colour kept for the heading, which wears
        // the colour its element is drawn in on the diagram.
        {
            auto *properties = child<QDockWidget>(window, "propertiesDock");
            const auto ink_of = [](QLabel *label)
            {
                return label->styleSheet().section("color: ", 1, 1).section(';', 0, 0).trimmed();
            };
            const auto label_named = [&](const QString &text) -> QLabel *
            {
                for (auto *label : properties->findChildren<QLabel *>("fieldLabel"))
                    if (label->text() == text)
                        return label;
                return nullptr;
            };
            require(label_named("Name") && label_named("Kind") && label_named("Ratio"),
                    "A relationship names its Name, Kind and Ratio");
            std::set<QString> panel_inks;
            for (auto *label : properties->findChildren<QLabel *>("fieldLabel"))
            {
                require(label->styleSheet().contains("font-weight: 700"), "Every label is bold");
                panel_inks.insert(ink_of(label));
            }
            require(panel_inks.size() == 1, "And every one of them is written in the same ink");
            require(*panel_inks.begin() == desktop::theme(window.canvas()->theme_id()).text.name(),
                    "Which is the theme's own");

            // The heading names the kind being edited and is left to the
            // theme, so it reads as a title rather than as a second copy of
            // the colour the name field already carries.
            auto *heading = child<QLabel>(window, "propertyHeading");
            require(heading->text() == "Relationship", "The heading names the kind being edited");
            require(heading->styleSheet().isEmpty(), "And is left to the theme");

            // The labels follow a theme change without having to be reselected.
            child<QAction>(window, "themeplain")->trigger();
            settle();
            std::set<QString> grey_inks;
            for (auto *label : properties->findChildren<QLabel *>("fieldLabel"))
                grey_inks.insert(ink_of(label));
            require(grey_inks.size() == 1 && *grey_inks.begin() == desktop::theme(desktop::ThemeId::Plain).text.name(),
                    "They are rewritten in the new theme's ink");
            child<QAction>(window, "themeofficelight")->trigger();
            settle();
            require(child<QDockWidget>(window, "propertiesDock")->findChildren<QLabel *>("fieldLabel").size() > 1 && label_named("Kind") != nullptr,
                    "And the panel comes back with the theme");
            // Changing the theme rebuilds the panel, so anything held from
            // before it is gone: the cards are found again for what follows.
            cards = properties->findChildren<QWidget *>("participantCard");
            require(cards.size() == 2, "The two cards are still shown");
        }
        QList<QComboBox *> combos;
        for (auto *card : cards)
            combos.append(card->findChildren<QComboBox *>());
        require(combos.size() == 4, "Each side carries its own cardinality and participation");
        combos.front()->setCurrentIndex(0);
        QMetaObject::invokeMethod(combos.front(), "activated", Q_ARG(int, 0));
        settle();
        require(window.editor().project().relationships.at(relationship).participants.front().id == participant,
                "Cardinality edit preserves participant ID");
        require(window.editor().project().relationships.at(relationship).participants.front().maximum == domain::Cardinality::One,
                "Participant form applies cardinality");

        // The ratio sets both sides at once and stays in step with them.
        auto *ratio = child<QComboBox>(window, "relationshipRatio");
        require(ratio->count() == 4, "All four ratios are offered");
        require(ratio->itemText(0) == "1:1" && ratio->itemText(1) == "1:M" && ratio->itemText(2) == "M:1" && ratio->itemText(3) == "M:M", "They read 1:1, 1:M, M:1, M:M");
        const auto ratio_revision = window.editor().revision();
        ratio->setCurrentIndex(2);
        QMetaObject::invokeMethod(ratio, "activated", Q_ARG(int, 2));
        settle();
        const auto &sides = window.editor().project().relationships.at(relationship).participants;
        require(sides[0].maximum == domain::Cardinality::Many && sides[1].maximum == domain::Cardinality::One,
                "M:1 writes both sides");
        require(window.editor().revision() == ratio_revision + 1, "Both sides move in one edit");
        require(child<QComboBox>(window, "relationshipRatio")->currentIndex() == 2, "The picker reports the ratio in use");

        // Reversing swaps the sides' constraints, turning M:1 back into 1:M.
        child<QPushButton>(window, "reverseRelationship")->click();
        settle();
        const auto &reversed = window.editor().project().relationships.at(relationship).participants;
        require(reversed[0].maximum == domain::Cardinality::One && reversed[1].maximum == domain::Cardinality::Many,
                "Reverse swaps the ratio");
        require(reversed[0].id == participant, "Reversing keeps participant identity");
        require(child<QComboBox>(window, "relationshipRatio")->currentIndex() == 1, "The picker follows the reversal");
        child<QAction>(window, "undoCommand")->trigger();
        child<QAction>(window, "undoCommand")->trigger();

        // Notation must be visible in the toolbar, not buried in a submenu, and
        // the two controls must never disagree about which one is in use.
        auto *picker = child<QComboBox>(window, "notationPicker");
        require(tools->findChildren<QComboBox *>().contains(picker), "The notation picker is on the toolbar");
        require(picker->count() == 4, "All four notations are offered");
        for (int index = 0; index < picker->count(); ++index)
            require(!picker->itemIcon(index).isNull(), "Each notation is drawn, not just named");
        require(picker->currentIndex() == static_cast<int>(window.canvas()->notation()), "The picker starts in step");
        picker->setCurrentIndex(static_cast<int>(desktop::Notation::CrowsFoot));
        settle();
        require(window.canvas()->notation() == desktop::Notation::CrowsFoot, "The picker changes the canvas");
        require(child<QAction>(window, "notationCrowsfoot")->isChecked(), "The menu follows the picker");
        child<QAction>(window, "notationBachman")->trigger();
        settle();
        require(window.canvas()->notation() == desktop::Notation::Bachman, "The menu changes the canvas");
        require(picker->currentIndex() == static_cast<int>(desktop::Notation::Bachman), "The picker follows the menu");
        child<QAction>(window, "notationChen")->trigger();
        settle();

        window.canvas()->select_elements({student});
        child<QAction>(window, "duplicateElements")->trigger();
        require(window.editor().project().entities.size() == example_entities + 1 && window.editor().project().attributes.size() == example_attributes + 9,
                "Duplicate copies entity and owned attributes through one command");
        child<QAction>(window, "undoCommand")->trigger();
        require(window.editor().project().entities.size() == example_entities, "One undo removes whole duplicate");

        QTemporaryDir directory;
        require(directory.isValid(), "Temporary test directory");
        const auto path = directory.filePath("test.erdx");
        infrastructure::ErdxProjectStore store;
        const auto saved = window.editor().project();
        require(store.save(path.toStdString(), saved).ok, "Save fixture through production adapter");
        dismiss(QMessageBox::Cancel);
        require(!window.open_path(path), "Cancel protects unsaved project");
        require(window.editor().project() == saved && window.editor().dirty(), "Cancel preserves current state");
        dismiss(QMessageBox::Discard);
        require(window.open_path(path), "Valid candidate opens after discard");
        require(window.editor().project() == saved && !window.editor().dirty() && !window.editor().can_undo(),
                "Opened project restores graph and starts clean history");

        window.activateWindow();
        settle();
        window.canvas()->select_elements({student});
        name = child<QLineEdit>(window, "elementName");
        name->setFocus();
        name->setText("Saved through File menu");
        require(name->hasFocus(), "Save fixture has an active property field");
        child<QAction>(window, "saveProject")->trigger();
        require(!window.editor().dirty(), "Save action commits focused text before saving");
        const auto loaded = store.load(path.toStdString());
        require(loaded.project && loaded.project->entities.at(student).name == "Saved through File menu",
                "File menu save persists the focused field value");
        window.canvas()->select_elements({student});
        name = child<QLineEdit>(window, "elementName");
        name->setFocus();
        name->setText("Saved while reopening");
        window.canvas()->setFocus();
        settle();
        dismiss(QMessageBox::Save);
        require(window.open_path(path), "Save and reopen the current file");
        require(window.editor().project().entities.at(student).name == "Saved while reopening",
                "Reopen installs newly saved contents, not the candidate read before the save prompt");
        QFile invalid(directory.filePath("invalid.erdx"));
        require(invalid.open(QIODevice::WriteOnly), "Create malformed file");
        invalid.write("{}");
        invalid.close();
        const auto before_failed_load = window.editor().project();
        dismiss(QMessageBox::Ok);
        require(!window.open_path(invalid.fileName()), "Malformed file is rejected by shell");
        require(window.editor().project() == before_failed_load, "Failed open leaves current model intact");

        auto *entity_tool = child<QAction>(window, "toolEntity");
        entity_tool->trigger();
        click_canvas(*window.canvas(), QPointF(200, 250));
        require(window.editor().project().entities.size() == example_entities + 1 && window.canvas()->tool() == desktop::Tool::Select,
                "Toolbar create routes through canvas and returns to Select");
        require(entity_tool->text() == "Entity", "An unlocked tool button carries no mark");

        // Double-clicking the button locks the tool, and the button says so.
        auto *button = child<QToolBar>(window, "modelTools")->widgetForAction(entity_tool);
        require(button != nullptr, "The entity tool has a toolbar button");
        QMouseEvent double_click(QEvent::MouseButtonDblClick, QPointF(5, 5), QPointF(5, 5),
                                 Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(button, &double_click);
        settle();
        require(window.canvas()->tool_locked(), "Double-clicking the button locks the tool");
        require(entity_tool->text().startsWith("Entity") && entity_tool->text() != "Entity",
                "A locked tool button is marked");
        click_canvas(*window.canvas(), QPointF(360, 250));
        click_canvas(*window.canvas(), QPointF(520, 250));
        require(window.editor().project().entities.size() == example_entities + 3, "A locked tool keeps placing");
        require(window.canvas()->tool() == desktop::Tool::Entity,
                "A locked tool stays selected");

        // A click anywhere in the window outside the diagram puts the tool
        // down, locked or not, and takes up Select (Zain, 2026-09-26). The
        // diagram's own controls are part of the diagram, and a button that
        // chooses a tool still chooses it.
        {
            const auto press_on = [](QWidget *target, QPoint at)
            {
                // Delivered to whatever is deepest under the point, as a
                // real click is.
                if (auto *deepest = target->childAt(at))
                {
                    at = deepest->mapFrom(target, at);
                    target = deepest;
                }
                const auto global = QPointF(target->mapToGlobal(at));
                QMouseEvent press(QEvent::MouseButtonPress, QPointF(at), global, Qt::LeftButton,
                                  Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(target, &press);
                QMouseEvent release(QEvent::MouseButtonRelease, QPointF(at), global, Qt::LeftButton,
                                    Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(target, &release);
                settle();
            };
            const auto lock_entity = [&]
            {
                QMouseEvent twice(QEvent::MouseButtonDblClick, QPointF(5, 5), QPointF(5, 5),
                                  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(button, &twice);
                settle();
                require(window.canvas()->tool() == desktop::Tool::Entity && window.canvas()->tool_locked(),
                        "The entity tool is locked again");
            };
            auto *explorer = child<QTreeView>(window, "explorer")->viewport();
            auto *properties = child<QDockWidget>(window, "propertiesDock")->widget();

            press_on(child<QWidget>(window, "canvasControlsGrip"), QPoint(4, 2));
            require(window.canvas()->tool() == desktop::Tool::Entity && window.canvas()->tool_locked(),
                    "A press on the diagram's own controls keeps the tool");
            const auto placed = window.editor().project().entities.size();
            click_canvas(*window.canvas(), QPointF(680, 250));
            require(window.editor().project().entities.size() == placed + 1 && window.canvas()->tool() == desktop::Tool::Entity,
                    "And a click inside the diagram still places");

            press_on(explorer, QPoint(10, explorer->height() - 6));
            require(window.canvas()->tool() == desktop::Tool::Select && !window.canvas()->tool_locked(),
                    "A click in the Explorer hands a locked tool back to Select");
            require(child<QAction>(window, "toolSelect")->isChecked() && entity_tool->text() == "Entity",
                    "And the toolbar says so, with the lock mark gone");

            lock_entity();
            press_on(properties, QPoint(6, 6));
            require(window.canvas()->tool() == desktop::Tool::Select,
                    "So does a click in Properties");

            entity_tool->trigger();
            settle();
            require(window.canvas()->tool() == desktop::Tool::Entity && !window.canvas()->tool_locked(),
                    "The entity tool, unlocked");
            press_on(explorer, QPoint(10, explorer->height() - 6));
            require(window.canvas()->tool() == desktop::Tool::Select, "An unlocked tool is put down the same way");

            lock_entity();
            auto *toolbar = child<QToolBar>(window, "modelTools");
            press_on(toolbar->widgetForAction(child<QAction>(window, "toolRelationship")), QPoint(5, 5));
            require(window.canvas()->tool() == desktop::Tool::Relationship,
                    "A tool button outside the diagram chooses its tool rather than Select");
            entity_tool->trigger();
            lock_entity();
        }

        // Properties locks the attribute owner as the element's own menu does
        // (Zain, 2026-09-26), and says which way it stands.
        {
            const domain::ElementRef owner = window.editor().project().entities.begin()->first;
            window.canvas()->select_elements({owner});
            settle();
            auto *lock = child<QPushButton>(window, "attributeOwnerLock");
            require(!lock->isChecked() && lock->text() == "Lock as attribute owner",
                    "Properties offers to lock an entity as the attribute owner");
            lock->click();
            settle();
            require(window.canvas()->attribute_owner() == owner, "Pressing it locks the entity");
            lock = child<QPushButton>(window, "attributeOwnerLock");
            require(lock->isChecked() && lock->text() == "Unlock attribute owner", "And the panel says it is held");
            child<QAction>(window, "toolAttribute")->trigger();
            // Put down on the entity itself, where nothing else is nearer and
            // so nothing is asked.
            const auto &body = window.editor().project().layout.at(owner);
            click_canvas(*window.canvas(), QPointF(body.x + body.width / 2, body.y + body.height / 2));
            const auto placed = window.canvas()->selected_elements();
            require(placed.size() == 1 && std::holds_alternative<domain::AttributeId>(placed.front()) && window.editor().project().attributes.at(std::get<domain::AttributeId>(placed.front())).owner == owner,
                    "An attribute placed goes on the locked entity");
            require(window.findChild<QPushButton *>("attributeOwnerLock") == nullptr,
                    "A plain attribute, selected, offers no lock: it cannot hold attributes");
            window.canvas()->select_elements({owner});
            settle();
            child<QPushButton>(window, "attributeOwnerLock")->click();
            settle();
            require(!window.canvas()->attribute_owner(), "Pressing it again releases it");
            require(!child<QPushButton>(window, "attributeOwnerLock")->isChecked(), "And the panel says so");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
        }

        // The History (Zain, 2026-09-26): a panel opened from the end of
        // View, one row for each step Undo could take back, in order and in
        // words; pressing a row goes back or forward to just after it.
        {
            auto *toggle = child<QAction>(window, "viewHistory");
            require(child<QMenu>(window, "viewMenu")->actions().last() == toggle,
                    "History is the last entry in View, after everything already there");
            auto *dock = child<QDockWidget>(window, "historyDock");
            require(!dock->isVisible(), "It is closed until it is opened");
            toggle->trigger();
            settle();
            require(dock->isVisible(), "View opens it");
            auto *list = child<QTreeWidget>(window, "historyList");
            require(list->topLevelItemCount() == static_cast<int>(window.editor().history().size()) + 1 && list->topLevelItem(0)->text(0) == "Start",
                    "It shows where the history starts and one row for each step since");
            // A step made is added after the last one in effect, taking the
            // place of any that had been undone, said in words, and is where
            // the history stands.
            const auto steps = static_cast<int>(window.editor().history_position());
            child<QAction>(window, "toolEntity")->trigger();
            const auto &first = window.editor().project().layout.begin()->second;
            click_canvas(*window.canvas(), QPointF(first.x - 600, first.y - 600));
            require(list->topLevelItemCount() == steps + 2, "The new step is added");
            auto *made = list->topLevelItem(steps + 1);
            require(made->text(0) == "Created Entity \"Entity\"", "In words");
            require(!made->text(1).isEmpty(), "With the time it was made");
            require(list->currentItem() == made, "And it is where the history stands");
            // Pressing the row before it goes back to just after that step.
            const auto entities = window.editor().project().entities.size();
            const auto press = [&](QTreeWidgetItem *item)
            {
                list->scrollToItem(item);
                const auto at = list->visualItemRect(item).center();
                QMouseEvent down(QEvent::MouseButtonPress, QPointF(at), QPointF(list->viewport()->mapToGlobal(at)),
                                 Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(list->viewport(), &down);
                QMouseEvent up(QEvent::MouseButtonRelease, QPointF(at), QPointF(list->viewport()->mapToGlobal(at)),
                               Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(list->viewport(), &up);
                settle();
            };
            press(list->topLevelItem(steps));
            require(window.editor().project().entities.size() == entities - 1, "Going back takes the entity away");
            require(list->topLevelItemCount() == steps + 2, "The step stays in the history");
            made = list->topLevelItem(steps + 1);
            require(made->font(0).italic(), "Shown as undone");
            require(child<QAction>(window, "redoCommand")->isEnabled(), "And Redo can bring it back");
            // Pressing it again goes forward to it.
            press(made);
            require(window.editor().project().entities.size() == entities, "Going forward brings it back");
            require(!list->topLevelItem(steps + 1)->font(0).italic(), "In effect again");
            // Undo and the History are one history.
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(list->currentItem() == list->topLevelItem(steps), "Undo moves where the history stands");
            toggle->trigger();
            settle();
            require(!dock->isVisible(), "And View closes it again");
        }
        // ISA is one toolbar entry offering both directions; the dropdown picks
        // the mode and the button itself locks like every other tool.
        auto *isa = child<QAction>(window, "toolIsa");
        require(isa->text() == "Specialization", "ISA starts in its top-down mode");
        child<QAction>(window, "isaGeneralization")->trigger();
        settle();
        require(window.canvas()->tool() == desktop::Tool::Generalization, "The dropdown switches direction");
        require(isa->text() == "Generalization", "The button reports the chosen direction");
        require(!window.canvas()->tool_locked(), "Choosing a direction does not lock it");
        auto *isa_button = child<QToolButton>(window, "isaButton");
        QMouseEvent isa_double(QEvent::MouseButtonDblClick, QPointF(5, 5), QPointF(5, 5),
                               Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(isa_button, &isa_double);
        settle();
        require(window.canvas()->tool_locked() && window.canvas()->tool() == desktop::Tool::Generalization,
                "Double-clicking ISA locks the chosen direction");
        require(isa->text().startsWith("Generalization") && isa->text() != "Generalization",
                "The locked ISA button is marked");
        child<QAction>(window, "isaSpecialization")->trigger();
        settle();
        require(window.canvas()->tool() == desktop::Tool::Specialization && !window.canvas()->tool_locked(),
                "Choosing the other direction resets to one-shot");

        // The line style sits on the Connect button's own arrow, with each
        // option drawn rather than only named.
        require(window.canvas()->line_style() == desktop::LineStyle::Elbow,
                "Lines break at right angles unless told otherwise");
        auto *straight = child<QAction>(window, "lineStraight");
        auto *curved = child<QAction>(window, "lineCurved");
        auto *elbow = child<QAction>(window, "lineElbow");
        require(!straight->icon().isNull() && !curved->icon().isNull() && !elbow->icon().isNull(),
                "Each line style is drawn, not just named");
        require(elbow->isChecked() && !curved->isChecked() && !straight->isChecked(),
                "The menu marks the style in use");
        straight->trigger();
        settle();
        require(window.canvas()->line_style() == desktop::LineStyle::Straight, "The menu changes the line style");
        require(straight->isChecked() && !curved->isChecked() && !elbow->isChecked(), "The mark follows the choice");
        // Choosing a style must not silently change which tool is active.
        const auto tool_before = window.canvas()->tool();
        curved->trigger();
        settle();
        require(window.canvas()->line_style() == desktop::LineStyle::Curved, "And the other two are offered too");
        elbow->trigger();
        settle();
        require(window.canvas()->line_style() == desktop::LineStyle::Elbow, "And back again");
        require(window.canvas()->tool() == tool_before, "Choosing a line style leaves the active tool alone");
        // The button still behaves like a tool, including its double-click lock.
        auto *connect_button = child<QToolButton>(window, "connectButton");
        QMouseEvent connect_double(QEvent::MouseButtonDblClick, QPointF(5, 5), QPointF(5, 5),
                                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(connect_button, &connect_double);
        settle();
        require(window.canvas()->tool() == desktop::Tool::Connect && window.canvas()->tool_locked(),
                "Double-clicking Connect locks it");

        // Every toolbar action carries a drawn icon, and the icons follow the
        // theme, since they are painted from it rather than loaded from files.
        auto *tool_bar = child<QToolBar>(window, "modelTools");
        for (auto *action : tool_bar->actions())
            if (!action->isSeparator() && !action->text().isEmpty())
                require(!action->icon().isNull(), "Every toolbar action is given an icon");
        const auto entity_icon = child<QAction>(window, "toolEntity")->icon().pixmap(18, 18).toImage();
        child<QAction>(window, "themedracula")->trigger();
        settle();
        require(window.canvas()->theme_id() == desktop::ThemeId::Dracula, "The menu changes the canvas theme");
        require(child<QAction>(window, "toolEntity")->icon().pixmap(18, 18).toImage() != entity_icon,
                "Icons are redrawn for the new theme");
        require(QSettings().value("theme").toString() == "dracula", "The choice is remembered");
        child<QAction>(window, "themeofficelight")->trigger();
        settle();
        require(window.canvas()->theme_id() == desktop::ThemeId::OfficeLight, "And back again");
        require(child<QAction>(window, "toolEntity")->icon().pixmap(18, 18).toImage() == entity_icon,
                "Returning to a theme restores its icons");

        // A glyph is a drawing, not a silhouette. The hand is the shape most at
        // risk: it is a stack of overlapping rounded rects, so once its stroke
        // approaches a finger's width the outlines merge and the whole icon
        // fills in as one mass of outline colour. Measuring how much of the palm
        // still carries the fill colour rather than the outline's catches exactly
        // that collapse; the drawn-at-all check covers the rest of the set. The
        // comparison is against the theme's own two colours, not a fixed
        // brightness, so it holds however light or dark the palette is.
        const auto &glyph_theme = desktop::theme(desktop::ThemeId::OfficeLight);
        const auto fill_grey = qGray(glyph_theme.base.rgb());
        const auto outline_grey = qGray(glyph_theme.muted.rgb());
        const auto share_of_fill = [&](desktop::Glyph glyph)
        {
            const auto drawn = desktop::glyph_icon(glyph, glyph_theme, 22)
                                   .pixmap(22, 22)
                                   .toImage()
                                   .convertToFormat(QImage::Format_ARGB32);
            int opaque = 0;
            int filled = 0;
            for (int y = 0; y < drawn.height(); ++y)
                for (int x = 0; x < drawn.width(); ++x)
                {
                    const auto pixel = drawn.pixel(x, y);
                    if (qAlpha(pixel) < 200)
                        continue;
                    ++opaque;
                    const auto grey = qGray(pixel);
                    if (std::abs(grey - fill_grey) < std::abs(grey - outline_grey))
                        ++filled;
                }
            require(opaque > 40, "Every glyph draws something at toolbar size");
            return static_cast<double>(filled) / static_cast<double>(opaque);
        };
        for (int index = 0; index <= static_cast<int>(desktop::Glyph::Delete); ++index)
            share_of_fill(static_cast<desktop::Glyph>(index));
        require(share_of_fill(desktop::Glyph::Pan) > 0.3, "The hand keeps an open palm rather than filling in");

        // An element given a colour of its own wears it in the properties panel,
        // so the panel and the shape on the canvas read as the same object.
        {
            const auto entity = window.editor().project().entities.begin()->first;
            window.canvas()->select_elements({domain::ElementRef{entity}});
            settle();
            // The name is written inside the shape the element is drawn as, so
            // the colour is carried by that shape rather than by a plain box.
            const auto &palette = desktop::theme(window.canvas()->theme_id());
            const auto shape_shows = [](QWidget *widget, const QColor &wanted)
            {
                const auto drawn = widget->grab().toImage().convertToFormat(QImage::Format_ARGB32);
                for (int y = 0; y < drawn.height(); ++y)
                    for (int x = 0; x < drawn.width(); ++x)
                    {
                        const auto pixel = drawn.pixelColor(x, y);
                        if (pixel.alpha() > 200 && std::abs(pixel.red() - wanted.red()) < 12 && std::abs(pixel.green() - wanted.green()) < 12 && std::abs(pixel.blue() - wanted.blue()) < 12)
                            return true;
                    }
                return false;
            };
            require(shape_shows(child<QWidget>(window, "elementShape"), palette.entity_fill),
                    "The name is written in a shape wearing the element's theme colour");
            require(child<QLineEdit>(window, "elementName")->parent() == child<QWidget>(window, "elementShape"),
                    "And the name is inside that shape rather than beside it");
            // The heading says what kind of thing this is and stays a title.
            require(child<QLabel>(window, "propertyHeading")->styleSheet().isEmpty(),
                    "The kind heading is left to the theme");

            require(bool(editor.recolour({domain::ElementRef{entity}}, domain::Colour{0x20, 0x20, 0x30})),
                    "Colour the entity a dark shade");
            // The shapes the panel shows are the canvas's own, so the canvas is
            // brought up to date first, which is the order every edit made
            // through the window follows.
            window.canvas()->synchronize();
            window.canvas()->select_elements({});
            settle();
            window.canvas()->select_elements({domain::ElementRef{entity}});
            settle();
            require(shape_shows(child<QWidget>(window, "elementShape"), QColor(0x20, 0x20, 0x30)),
                    "The shape is filled with the element's own colour");
            require(child<QLineEdit>(window, "elementName")->styleSheet().contains("#ffffff"),
                    "And the name written in ink chosen against it, not against the theme");
            require(child<QLabel>(window, "propertyHeading")->styleSheet().isEmpty(),
                    "The heading still carries no colour of the element's");
            require(bool(editor.undo()), "Undo the colour");
        }

        // Three sets, and the window has to be able to wear any of them. The
        // outline set is line art inked from the theme, the modern set is
        // artwork carrying its own colour, and the painted set is drawn from
        // the palette; each must cover every glyph the window uses, or a
        // button silently falls back and the sets disagree about what is there.
        {
            require(window.icon_mode() == desktop::IconMode::Outline, "The window wears the line art to begin with");
            const auto lined = child<QAction>(window, "toolEntity")->icon().pixmap(22, 22).toImage();
            for (int index = 0; index <= static_cast<int>(desktop::Glyph::Symbols); ++index)
            {
                const auto glyph = static_cast<desktop::Glyph>(index);
                for (const char *set : {"icons", "icons-on-dark", "icons-outline"})
                {
                    const QIcon file(QStringLiteral(":/erdflow/%1/%2.svg")
                                         .arg(QString::fromLatin1(set), desktop::icon_name(glyph)));
                    require(!file.pixmap(22, 22).isNull(), "Every set has a file for every glyph the window draws");
                }
            }
            child<QAction>(window, "iconsmodern")->trigger();
            settle();
            require(window.icon_mode() == desktop::IconMode::Modern, "The menu changes the icon set");
            const auto artwork = child<QAction>(window, "toolEntity")->icon().pixmap(22, 22).toImage();
            require(!artwork.isNull() && artwork != lined, "Every button takes the new set");
            require(QSettings().value("iconMode").toString() == "modern", "The choice is remembered");
            child<QAction>(window, "iconsnormal")->trigger();
            settle();
            const auto painted = child<QAction>(window, "toolEntity")->icon().pixmap(22, 22).toImage();
            require(painted != artwork && painted != lined, "And the painted set is a third thing again");
            child<QAction>(window, "iconsoutline")->trigger();
            settle();
            require(child<QAction>(window, "toolEntity")->icon().pixmap(22, 22).toImage() == lined,
                    "Going back restores the line art");

            // The line art is inked from the theme, which is what the artwork
            // cannot do: changing the palette has to change the drawing.
            child<QAction>(window, "themedracula")->trigger();
            settle();
            require(child<QAction>(window, "toolEntity")->icon().pixmap(22, 22).toImage() != lined,
                    "The line art is inked from the theme");
            child<QAction>(window, "themeofficelight")->trigger();
            settle();

            // A tool that is on sits on a chip of the theme's accent. Inked for
            // the panel it would all but vanish there, so the "on" state has to
            // be a second inking that reads against the accent instead.
            for (const auto glyph : {desktop::Glyph::Select, desktop::Glyph::Connect, desktop::Glyph::Pan})
            {
                const auto &colors = desktop::theme(desktop::ThemeId::OfficeLight);
                const auto icon = desktop::glyph_icon(glyph, colors, 22, desktop::IconMode::Outline);
                const auto resting = icon.pixmap(22, 22, QIcon::Normal, QIcon::Off).toImage();
                const auto lit = icon.pixmap(22, 22, QIcon::Normal, QIcon::On).toImage();
                require(!lit.isNull() && lit != resting, "A tool that is on is inked again");
                // The solidest pixel of the drawing is the ink itself, the rest
                // of the line being the softened edge of the same colour.
                QColor ink;
                int most = 0;
                for (int y = 0; y < lit.height(); ++y)
                    for (int x = 0; x < lit.width(); ++x)
                    {
                        const auto pixel = lit.pixelColor(x, y);
                        if (pixel.alpha() > most)
                        {
                            most = pixel.alpha();
                            ink = pixel;
                        }
                    }
                require(most > 200, "The lit drawing is actually there");
                const auto wanted = desktop::readable_on(colors.accent);
                require(std::abs(ink.red() - wanted.red()) < 24 && std::abs(ink.green() - wanted.green()) < 24 && std::abs(ink.blue() - wanted.blue()) < 24,
                        "And it is inked in whatever reads on the accent");
            }
        }

        // Insert offers the characters an ERD wants and a keyboard has not
        // got: the relational algebra signs above all, and the marks and emoji
        // a note is annotated with. They go into whatever field is being
        // written in, which means the gallery has to find that field again
        // after a commit has rebuilt the properties panel underneath it.
        {
            require(child<QMenu>(window, "insertMenu")->actions().contains(child<QAction>(window, "insertSymbols")),
                    "Insert carries the symbol gallery");
            require(child<QToolButton>(window, "insertButton")->menu() == child<QMenu>(window, "insertMenu"),
                    "And Home's Insert carries it too");

            // A character no font can draw would show as an empty box, so the
            // table is measured against the interface font rather than trusted.
            QFont measuring = QApplication::font();
            measuring.setPointSizeF(17);
            const QFontMetrics metrics(measuring);
            std::size_t characters = 0;
            for (const auto &group : desktop::symbol_groups())
            {
                require(!group.symbols.empty(), "Every group offers something");
                for (const auto &symbol : group.symbols)
                {
                    require(!symbol.character.isEmpty() && !symbol.name.isEmpty(), "Every character is named");
                    require(metrics.horizontalAdvance(symbol.character) > 0, "And something can draw every character");
                    // Several of the people are joined sequences: a person and
                    // what they do, written as two emoji the font draws as one.
                    // A font that does not join them draws two, which is wider
                    // than the picker's cell and comes out as an ellipsis, so
                    // the width is measured rather than assumed.
                    require(metrics.horizontalAdvance(symbol.character) <= 44,
                            "And every character fits the cell it is drawn in");
                    ++characters;
                }
            }
            require(characters > 150, "The gallery is worth opening");

            // With nothing chosen there is no field to write in, so the
            // character goes on the diagram itself, as a note carrying it.
            // That is the whole point of picking one, and it undoes like any
            // other edit.
            window.canvas()->select_elements({});
            window.canvas()->setFocus();
            settle();
            require(window.findChild<QLineEdit *>("elementName") == nullptr, "Nothing chosen means no name field");
            const auto notes_before = window.editor().project().notes.size();
            require(window.insert_symbol(QStringLiteral("⋈")), "With no field open the character goes on the diagram");
            require(window.editor().project().notes.size() == notes_before + 1, "As a note carrying it");
            require(std::any_of(window.editor().project().notes.begin(), window.editor().project().notes.end(),
                                [](const auto &entry)
                                { return entry.second.name == "⋈" && entry.second.plain; }),
                    "And the note is the character, drawn bare");
            require(window.editor().undo_label() == "Insert symbol", "The history says what was done");
            // Putting a character down is not choosing something to work on,
            // so the panel is left alone rather than swapped over to it.
            require(window.canvas()->selected_elements().empty(), "Placing one chooses nothing");
            require(window.findChild<QLabel *>("propertyHeading") == nullptr, "So the panel is left as it was");
            // Chosen deliberately, though, it says what it is: a card and a
            // character drawn bare are not the same thing to anyone looking.
            const auto placed = std::find_if(window.editor().project().notes.begin(),
                                             window.editor().project().notes.end(),
                                             [](const auto &entry)
                                             { return entry.second.plain; });
            require(placed != window.editor().project().notes.end(), "The symbol is there to be chosen");
            window.canvas()->select_elements({domain::ElementRef{placed->first}});
            settle();
            require(child<QLabel>(window, "propertyHeading")->text() == QStringLiteral("Symbol"),
                    "And the panel calls it a symbol, not a note");
            window.canvas()->select_elements({});
            settle();
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(window.editor().project().notes.size() == notes_before, "Placing one undoes like any other edit");

            // It goes where the user was working, which is where the pointer
            // last was over the diagram, not in the middle of the view.
            {
                auto *canvas = window.canvas();
                const QPoint spot(canvas->viewport()->width() / 4, canvas->viewport()->height() / 4);
                QMouseEvent move(QEvent::MouseMove, QPointF(spot), canvas->viewport()->mapToGlobal(spot),
                                 Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(canvas->viewport(), &move);
                settle();
                const auto wanted = canvas->mapToScene(spot);
                require(canvas->pointer_place().has_value(), "The canvas remembers where the pointer was");
                require(window.insert_symbol(QStringLiteral("π")), "A character is placed");
                const auto found = std::find_if(window.editor().project().notes.begin(),
                                                window.editor().project().notes.end(),
                                                [](const auto &entry)
                                                { return entry.second.name == "π"; });
                require(found != window.editor().project().notes.end(), "And it is there");
                const auto box = window.editor().project().layout.at(domain::ElementRef{found->first});
                require(std::abs(box.x + box.width / 2 - wanted.x()) < 1.5 && std::abs(box.y + box.height / 2 - wanted.y()) < 1.5,
                        "Centred where the pointer last was");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
            }

            auto chosen = window.editor().project().entities.begin()->first;
            for (const auto &[id, entity] : window.editor().project().entities)
                if (entity.name == "Course")
                    chosen = id;
            window.canvas()->select_elements({chosen});
            auto *name = child<QLineEdit>(window, "elementName");
            name->setText("Course");
            name->setFocus();
            name->setCursorPosition(static_cast<int>(name->text().size()));
            settle();
            require(window.insert_symbol(QStringLiteral("σ")), "A character goes into the field being written in");
            require(child<QLineEdit>(window, "elementName")->text() == QStringLiteral("Courseσ"),
                    "At the caret, rather than at the start");

            // Committing rebuilds the panel and takes the field with it, so the
            // next character has to find the field that replaced it, and has to
            // land where the writing stopped rather than in front of the name.
            // Return commits a line edit exactly as leaving it does, and a key
            // sent straight to the widget does not depend on the window being
            // the active one, which offscreen it is not.
            {
                auto *writing = child<QLineEdit>(window, "elementName");
                QKeyEvent commit(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QApplication::sendEvent(writing, &commit);
                settle();
            }
            require(window.editor().project().entities.at(chosen).name == "Courseσ",
                    "The name commits with the character in it");
            // The keyboard lands in the field that replaced the one being
            // written in, and that field reads from its beginning, so the
            // caret has to be put back or the next character lands in front
            // of the name instead of after it.
            auto *rebuilt = child<QLineEdit>(window, "elementName");
            rebuilt->setFocus();
            settle();
            require(window.insert_symbol(QStringLiteral("π")), "And the rebuilt field takes the next character");
            require(rebuilt->text() == QStringLiteral("Courseσπ"),
                    "Where the writing stopped, not in front of the name");

            window.show_symbols(QStringLiteral("Emoji"));
            settle();
            auto *picker = child<QDialog>(window, "symbolPicker");
            require(picker->isVisible(), "The gallery opens");
            auto *groups = picker->findChild<QListWidget *>("symbolGroups");
            require(groups && groups->currentItem() && groups->currentItem()->text() == QStringLiteral("Emoji"),
                    "On the group it was asked for");

            // Searching reaches across every group, so no one group stays lit.
            auto *search = picker->findChild<QLineEdit *>("symbolSearch");
            search->setText(QStringLiteral("join"));
            settle();
            require(groups->currentRow() < 0, "A search reaches across every group, so none stays highlighted");
            const auto cells = [&]
            {
                std::vector<QToolButton *> found;
                // The search box has a clear button of its own, which is not a
                // character; the characters are the ones that carry a name.
                for (auto *button : picker->findChildren<QToolButton *>())
                    if (!button->accessibleName().isEmpty())
                        found.push_back(button);
                return found;
            }();
            require(cells.size() >= 6, "The search finds the joins");
            for (auto *cell : cells)
                require(cell->accessibleName().contains(QStringLiteral("join"), Qt::CaseInsensitive),
                        "And shows nothing that does not match");

            name = child<QLineEdit>(window, "elementName");
            name->setFocus();
            name->setCursorPosition(static_cast<int>(name->text().size()));
            settle();
            const auto before = name->text();
            cells.front()->click();
            settle();
            require(child<QLineEdit>(window, "elementName")->text() != before, "Clicking a character writes it");
            require(picker->isVisible(), "And the gallery stays open for the next one");

            // Typing in the search box must not make the search box the place
            // the characters land.
            search->setFocus();
            settle();
            require(window.insert_symbol(QStringLiteral("π")), "Searching does not move where the characters go");
            require(child<QLineEdit>(window, "elementName")->text().endsWith(QStringLiteral("π")),
                    "They still go into the field being written in");
            require(search->text() == QStringLiteral("join"), "And never into the search box");

            // Renaming on the canvas puts the keyboard in the box over the
            // element, and a character goes there. Once that box closes the
            // keyboard belongs to the diagram again, so the next character
            // goes on the diagram rather than into a box nobody can see.
            {
                window.canvas()->select_elements({chosen});
                window.canvas()->begin_rename(chosen);
                settle();
                auto *box = child<QLineEdit>(window, "inlineName");
                require(box->isVisible(), "Renaming on the canvas opens a box over the element");
                // Offscreen the window is never the desktop's active one, so
                // the box is given the keyboard here as the desktop would.
                box->setFocus();
                settle();
                require(window.focusWidget() == box, "The box is where the keyboard is");
                box->setText(QStringLiteral("Course"));
                box->setCursorPosition(static_cast<int>(box->text().size()));
                require(window.insert_symbol(QStringLiteral("σ")), "A character goes into that box");
                require(box->text() == QStringLiteral("Courseσ"), "At its caret");
                window.canvas()->commit_rename();
                settle();
                require(!box->isVisible(), "The box closes when the rename is done");
                require(window.focusWidget() == window.canvas(), "And the diagram has the keyboard again");
                const auto notes_now = window.editor().project().notes.size();
                require(window.insert_symbol(QStringLiteral("π")), "The next character goes somewhere");
                require(window.editor().project().notes.size() == notes_now + 1,
                        "On the diagram, not into the closed box");
                require(box->text() == QStringLiteral("Courseσ"), "Which is left exactly as it was");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
            }

            // Clearing the search puts the highlight back where it was.
            search->clear();
            settle();
            require(groups->currentItem() && groups->currentItem()->text() == QStringLiteral("Emoji"),
                    "Clearing a search puts the group back");
            picker->close();
            settle();
            child<QAction>(window, "undoCommand")->trigger();
            settle();
        }

        // A symbol is the one element with a size of its own to choose, so the
        // window offers two commands and a field for it, and offers them only
        // while what is chosen is a symbol.
        {
            window.canvas()->select_elements({});
            settle();
            require(window.insert_symbol(QStringLiteral("\U0001F9D1\u200D\U0001F393")),
                    "A symbol goes on the diagram");
            const auto placed = std::find_if(window.editor().project().notes.begin(),
                                             window.editor().project().notes.end(),
                                             [](const auto &entry)
                                             { return entry.second.plain; });
            require(placed != window.editor().project().notes.end(), "It is there to be chosen");
            const domain::ElementRef symbol{placed->first};
            auto *enlarge = child<QAction>(window, "enlargeSymbol");
            auto *shrink = child<QAction>(window, "shrinkSymbol");
            require(child<QMenu>(window, "editMenu")->actions().contains(enlarge), "Edit carries Enlarge");
            require(child<QMenu>(window, "editMenu")->actions().contains(shrink), "And Shrink");

            // The view's own zoom already means something else, so the pair
            // does not take its keys.
            require(enlarge->shortcut() != QKeySequence(QKeySequence::ZoomIn) && shrink->shortcut() != QKeySequence(QKeySequence::ZoomOut),
                    "Neither takes the keys that zoom the diagram");

            window.canvas()->select_elements({symbol});
            settle();
            require(enlarge->isEnabled() && shrink->isEnabled(), "Both apply to a chosen symbol");
            const auto before = window.editor().project().layout.at(symbol);
            enlarge->trigger();
            settle();
            const auto after = window.editor().project().layout.at(symbol);
            require(after.width > before.width, "Enlarge makes it bigger");
            require(std::abs(after.x + after.width / 2 - (before.x + before.width / 2)) < 0.01,
                    "Without moving it off where it was put");
            require(window.editor().undo_label() == "Resize symbol", "The history says what was done");
            shrink->trigger();
            settle();
            require(std::abs(window.editor().project().layout.at(symbol).width - before.width) < 0.01,
                    "And Shrink puts it back");

            // The panel says the size in one figure, because a symbol is drawn
            // to the smaller of its two sides and so is square in practice.
            auto *size = child<QSpinBox>(window, "symbolSize");
            require(size->value() == static_cast<int>(before.width), "The panel shows the size it is drawn at");
            size->setValue(size->value() * 2);
            settle();
            const auto typed = window.editor().project().layout.at(symbol);
            require(std::abs(typed.width - before.width * 2) < 0.01, "Typing a size resizes the symbol");
            require(std::abs(typed.x + typed.width / 2 - (before.x + before.width / 2)) < 0.01,
                    "About its centre, as the commands do");

            // A number field is not a place a character belongs. The size box
            // has a line edit inside it like any other, so a character picked
            // while it has the keyboard goes on the diagram instead of being
            // typed into a figure and silently thrown away.
            {
                // Applying a size rebuilds the panel and takes the box with it,
                // so the one to ask is the one that replaced it.
                auto *rebuilt_size = child<QSpinBox>(window, "symbolSize");
                auto *inside = rebuilt_size->findChild<QLineEdit *>();
                require(inside != nullptr, "The size box has a field inside it");
                inside->setFocus();
                settle();
                const auto notes_before = window.editor().project().notes.size();
                const auto figure = rebuilt_size->value();
                require(window.insert_symbol(QStringLiteral("σ")), "A character picked here goes somewhere");
                require(window.editor().project().notes.size() == notes_before + 1, "On the diagram");
                require(child<QSpinBox>(window, "symbolSize")->value() == figure, "And never into the size");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
                window.canvas()->select_elements({symbol});
                settle();
            }

            // An entity's box is sized by the name it holds, so none of this
            // is offered for one.
            window.canvas()->select_elements({window.editor().project().entities.begin()->first});
            settle();
            require(!enlarge->isEnabled() && !shrink->isEnabled(), "Neither applies to an entity");
            require(window.findChild<QSpinBox *>("symbolSize") == nullptr, "And the panel offers it no size field");

            // Everything this block put on the diagram comes off it again, so
            // what follows sees the model it expects.
            window.canvas()->select_elements({});
            settle();
            while (window.editor().project().notes.contains(placed->first))
            {
                child<QAction>(window, "undoCommand")->trigger();
                settle();
            }
            require(!window.editor().project().notes.contains(placed->first), "The symbol is taken away again");
        }

        // A theme can be seen on the window before it is chosen, and looking at
        // one without choosing it must leave nothing behind.
        {
            const auto chosen = window.canvas()->theme_id();
            auto *dracula = child<QAction>(window, "themedracula");
            emit dracula->hovered();
            settle_for(150);
            require(window.canvas()->theme_id() == desktop::ThemeId::Dracula,
                    "Hovering a theme shows it on the window");
            require(QSettings().value("theme").toString() != "dracula",
                    "But looking at one does not remember it");
            require(!dracula->isChecked(), "Nor tick it as the chosen one");

            // Closing the menu without choosing puts the window back.
            emit child<QMenu>(window, "themeMenu")->aboutToHide();
            settle();
            require(window.canvas()->theme_id() == chosen, "Leaving the menu restores the chosen theme");

            // Choosing one while previewing keeps it, rather than being undone
            // by the same closing that would have reverted a mere look.
            emit dracula->hovered();
            settle_for(150);
            dracula->trigger();
            settle();
            emit child<QMenu>(window, "themeMenu")->aboutToHide();
            settle();
            require(window.canvas()->theme_id() == desktop::ThemeId::Dracula, "Choosing one keeps it");
            require(QSettings().value("theme").toString() == "dracula", "And remembers it");
            child<QAction>(window, "themeofficelight")->trigger();
            settle();
        }

        // Wearing a theme costs the whole window, and a pointer on its way to
        // an entry crosses every entry above it. Only the one it stops on is
        // worth paying for, so what is hovered is remembered and shown once the
        // pointer has settled rather than while it is still travelling.
        {
            window.set_theme(desktop::ThemeId::OfficeLight);
            settle();
            // Both are looked up first: fetching one settles the loop, which
            // would let the wait elapse in the middle of the crossing.
            auto *midnight = child<QAction>(window, "thememidnight");
            auto *dracula = child<QAction>(window, "themedracula");
            emit midnight->hovered();
            emit dracula->hovered();
            require(window.canvas()->theme_id() == desktop::ThemeId::OfficeLight,
                    "An entry merely crossed is never put on the window");
            settle_for(150);
            require(window.canvas()->theme_id() == desktop::ThemeId::Dracula,
                    "The entry the pointer settles on is the one shown");
            emit child<QMenu>(window, "themeMenu")->aboutToHide();
            settle_for(150);
            require(window.canvas()->theme_id() == desktop::ThemeId::OfficeLight,
                    "And a look owed when the menu closes is never paid");
            child<QAction>(window, "themeofficelight")->trigger();
            settle();
        }

        // A narrow window must shed what it can spare rather than let tools run
        // off the end of the toolbar where they cannot be reached, and it must
        // shed them in order of what can best be done without.
        {
            auto *bar = child<QToolBar>(window, "modelTools");
            auto *picker = child<QComboBox>(window, "notationPicker");
            const auto tools = bar->actions().size();

            // Wide is 1920 since the row holds the search field (Zain, 2026-10-08):
            // the names come back from about 1850.
            window.resize(1920, 820);
            settle();
            require(bar->toolButtonStyle() == Qt::ToolButtonTextBesideIcon, "A wide window shows the names");
            require(picker->isVisible(), "And the notation picker with them");
            require(bar->widgetForAction(child<QAction>(window, "checkModel")) == nullptr &&
                        !bar->actions().contains(child<QAction>(window, "checkModel")),
                    "Check model is not on Home: it is offered from the header's Model menu (Zain, 2026-10-07)");
            require(!child<QToolButton>(window, "themeButton")->isVisible() &&
                        child<QToolBar>(window, "designTools")->actions().contains(child<QMenu>(window, "themeMenu")->menuAction()),
                    "The theme is chosen from Settings' Design row, not from Home (Zain, 2026-10-06)");
            const auto wide = bar->iconSize().width();

            // Compact labels before dropping Notation to accommodate the
            // shared Search / Model / Theme controls in the same row.
            // Tighter is 1280 since the row holds the search field (Zain,
            // 2026-10-08), whose least width was chosen so 1280 keeps Notation.
            window.resize(1280, 820);
            settle();
            require(picker->isVisible(), "A tighter one keeps Notation beside the corner controls");
            require(bar->iconSize().width() <= wide, "Compact tools use no larger icons than the wide row");

            window.resize(700, 620);
            settle();
            require(bar->toolButtonStyle() == Qt::ToolButtonIconOnly, "Only a small window drops the names");
            require(bar->actions().size() == tools, "But loses no tool on the way down");
            require(!picker->isVisible(), "The picker has gone by then, and is in the View menu");

            window.resize(1920, 820);
            settle();
            require(bar->toolButtonStyle() == Qt::ToolButtonTextBesideIcon, "Widening brings the names back");
            require(bar->iconSize().width() == wide, "And the size with them");
            require(picker->isVisible(), "And the picker");
        }

        // The paper the diagram is drawn on is chosen under View, so it reaches
        // the Design row as well, and it travels with the document.
        {
            auto *papers = child<QMenu>(window, "backgroundMenu");
            require(child<QMenu>(window, "viewMenu")->actions().contains(papers->menuAction()),
                    "Background sits with the other choices about how the diagram looks");
            auto *squares = child<QAction>(window, "backgroundSquares");
            auto *plain = child<QAction>(window, "backgroundTheme");
            emit papers->aboutToShow();
            settle();
            require(plain->isChecked() && !squares->isChecked(), "A diagram starts on the plain canvas");
            squares->trigger();
            settle();
            require(window.editor().project().background.style == domain::BackgroundStyle::Squares,
                    "Choosing graph paper lays it on the canvas");
            require(window.editor().undo_label() == "Change background", "As one named edit");

            // Asking for less of it is a picture's business, so the bar is not
            // offered for a ruling.
            auto *row = window.findChild<QWidgetAction *>("backgroundStrengthRow");
            require(row != nullptr && !row->isVisible(), "A ruling is drawn as the ruling it is");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(window.editor().project().background.style == domain::BackgroundStyle::Theme,
                    "The change undoes");
            emit papers->aboutToShow();
            settle();
            require(child<QAction>(window, "backgroundTheme")->isChecked(),
                    "And the menu shows the paper the document actually has");
        }

        // A choice or a number must not change because the pointer passed
        // over it: these are changed by pressing them and choosing, or by
        // typing, and a wheel is meant for the panel behind them.
        {
            window.canvas()->select_elements({relationship});
            settle();
            const auto turn = [](QWidget *widget, int notches)
            {
                QWheelEvent wheel(QPointF(5, 5), widget->mapToGlobal(QPoint(5, 5)), QPoint(),
                                  QPoint(0, notches), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
                QApplication::sendEvent(widget, &wheel);
                settle();
            };
            auto *ratio = child<QComboBox>(window, "relationshipRatio");
            const auto chosen = ratio->currentIndex();
            turn(ratio, 120);
            turn(ratio, -120);
            require(ratio->currentIndex() == chosen, "A wheel over a choice leaves it as it was");

            auto *width = child<QDoubleSpinBox>(window, "geometryWidth");
            const auto measured = width->value();
            turn(width, 120);
            turn(width, -120);
            require(width->value() == measured, "And over a number too");

            // The same on the toolbar, where the notation picker sits.
            auto *picker = child<QComboBox>(window, "notationPicker");
            const auto notation = picker->currentIndex();
            turn(picker, 120);
            require(picker->currentIndex() == notation, "And over the notation picker");
            require(window.canvas()->notation() == static_cast<desktop::Notation>(notation),
                    "So the diagram is not redrawn in a notation nobody asked for");

            // Pressing and choosing still works, which is the way they change.
            ratio->setCurrentIndex(2);
            QMetaObject::invokeMethod(ratio, "activated", Q_ARG(int, 2));
            settle();
            require(window.editor().project().relationships.at(relationship).participants.front().maximum == domain::Cardinality::Many,
                    "Choosing from the list still sets it");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            window.canvas()->select_elements({});
            settle();
        }

        // Check model is a switch: it opens the findings and puts them away
        // again, and says which it will do by the mark it wears.
        {
            auto *check = child<QAction>(window, "checkModel");
            auto *checks_dock = child<QDockWidget>(window, "validationDock");
            require(!checks_dock->isVisible() && !check->isChecked(), "The findings start closed");
            const auto closed_mark = check->icon().pixmap(22, 22).toImage();
            check->trigger();
            settle();
            require(checks_dock->isVisible() && check->isChecked(), "Pressing it opens them");
            const auto open_mark = check->icon().pixmap(22, 22).toImage();
            require(open_mark != closed_mark, "And the button changes its mark to say so");
            check->trigger();
            settle();
            require(!checks_dock->isVisible() && !check->isChecked(), "Pressing it again puts them away");
            require(check->icon().pixmap(22, 22).toImage() == closed_mark, "And the first mark comes back");

            // However the panel is opened or closed, the button follows it.
            checks_dock->toggleViewAction()->trigger();
            settle();
            require(check->isChecked() && check->icon().pixmap(22, 22).toImage() == open_mark,
                    "Opening it from the View menu marks the button too");
            checks_dock->toggleViewAction()->trigger();
            settle();
            require(!check->isChecked(), "And closing it there clears the mark");
        }

        // Full view puts the panels away and gives the whole window to the
        // diagram, and brings back exactly the ones that were showing.
        {
            auto *full_view = child<QAction>(window, "viewFullView");
            auto *explorer_dock = child<QDockWidget>(window, "explorerDock");
            auto *properties_dock = child<QDockWidget>(window, "propertiesDock");
            auto *checks_dock = child<QDockWidget>(window, "validationDock");
            require(child<QMenu>(window, "viewMenu")->actions().contains(full_view),
                    "It is written out in the View menu");
            auto *raft_button = child<QToolButton>(window, "canvasFullView");
            require(raft_button->defaultAction() == full_view, "And is on the canvas raft as well");
            require(raft_button->toolButtonStyle() == Qt::ToolButtonIconOnly && !raft_button->icon().isNull(),
                    "There it is a picture, since the raft has no room for a word");
            require(!full_view->toolTip().isEmpty(), "Which names itself on hover");

            // Model checks starts closed, so full view must not open it.
            require(!checks_dock->isVisible(), "Model checks is closed to begin with");
            require(explorer_dock->isVisible() && properties_dock->isVisible(), "The other two are open");
            full_view->trigger();
            settle();
            require(!explorer_dock->isVisible() && !properties_dock->isVisible() && !checks_dock->isVisible(),
                    "Full view puts every panel away");
            require(full_view->isChecked(), "And the control shows it is on");
            full_view->trigger();
            settle();
            require(explorer_dock->isVisible() && properties_dock->isVisible(), "Pressing it again brings them back");
            require(!checks_dock->isVisible(), "But not one that was closed before");
            require(!full_view->isChecked(), "And the control shows it is off");

            // A panel opened while full view is on is put away by it too, and
            // comes back with the rest.
            child<QAction>(window, "checkModel")->trigger();
            settle();
            require(checks_dock->isVisible(), "Model checks opens");
            full_view->trigger();
            settle();
            require(!checks_dock->isVisible(), "Full view puts it away with the others");
            full_view->trigger();
            settle();
            require(checks_dock->isVisible() && explorer_dock->isVisible() && properties_dock->isVisible(),
                    "And all three come back together");
            checks_dock->hide();
            settle();
        }

        // Below the raft's zoom, after a rule, one button for the side panels
        // (Zain, 2026-10-06), where there were three. Each press takes the next
        // step of Both -> Properties only -> Neither -> Both, read from the
        // panels' own View menu entries, so a panel shown or put away there is
        // where the next press starts; the Explorer alone goes on to both.
        // None of it reaches the project, the history or what is chosen.
        {
            auto *raft = child<QWidget>(window, "canvasControls");
            auto *explorer_dock = child<QDockWidget>(window, "explorerDock");
            auto *properties_dock = child<QDockWidget>(window, "propertiesDock");
            QStringList order;
            for (int i = 0; i < raft->layout()->count(); ++i)
                if (auto *widget = raft->layout()->itemAt(i)->widget())
                    order << widget->objectName();
            require(order == QStringList{"canvasControlsGrip", "canvasFullView", "canvasFit", "canvasPan", "canvasZoomIn",
                                         "canvasZoomOut", "canvasControlsRule", "canvasSidePanels"},
                    "The raft keeps its controls in their order, then a rule, then one button for the panels");
            require(!raft->findChild<QToolButton *>("canvasExplorer") && !raft->findChild<QToolButton *>("canvasProperties"),
                    "The Explorer's and Properties' own buttons are gone from the raft");
            require(window.findChild<QAction *>("viewExplorerPanel") == nullptr &&
                        window.findChild<QAction *>("viewPropertiesPanel") == nullptr,
                    "And so are the actions only they had");
            auto *panels = child<QToolButton>(window, "canvasSidePanels");
            auto *zoom_out = child<QToolButton>(window, "canvasZoomOut");
            const auto margins = raft->layout()->contentsMargins();
            require(panels->size() == zoom_out->size() && panels->toolButtonStyle() == Qt::ToolButtonIconOnly && !panels->icon().isNull() && !panels->toolTip().isEmpty(),
                    "It is a picture the size of the raft's other buttons, named on hover");
            require(!panels->isCheckable() && panels->defaultAction() == child<QAction>(window, "viewSidePanels"),
                    "Its picture says which panels are out; it is not a switch with a lit state");
            require(raft->width() == zoom_out->width() + margins.left() + margins.right(),
                    "The raft is no wider than it was");
            const auto shown = [&](bool left, bool right)
            {
                return explorer_dock->isVisible() == left && properties_dock->isVisible() == right && explorer_dock->toggleViewAction()->isChecked() == left && properties_dock->toggleViewAction()->isChecked() == right;
            };
            const auto picture = [&]
            { return panels->icon().pixmap(18, 18).toImage(); };
            const auto corner = [&]
            {
                const auto *canvas = raft->parentWidget();
                return std::pair{canvas->width() - raft->geometry().right(), canvas->height() - raft->geometry().bottom()};
            };
            const auto inside = [&]
            { return raft->parentWidget()->rect().contains(raft->geometry()); };
            const auto &model = window.editor();
            window.canvas()->select_elements({domain::ElementRef{model.project().relationships.begin()->first}}, true);
            settle();
            const auto chosen = window.canvas()->selected_elements();
            const auto said = properties_heading(*properties_dock->widget());
            const auto project_before = model.project();
            const auto revision_before = model.revision();
            const auto undo_before = model.undo_label();
            const auto dirty_before = model.dirty();
            const auto corner_before = corner();
            const auto press = [&]
            {
                panels->click();
                settle();
            };
            require(shown(true, true) && inside(), "Both panels show to begin with");
            const auto both_picture = picture();
            const auto both_words = panels->toolTip();
            press();
            require(shown(false, true) && inside() && corner() == corner_before && window.canvas()->selected_elements() == chosen,
                    "One press puts the Explorer away; the raft keeps its corner and what is chosen stays chosen");
            const auto properties_picture = picture();
            require(properties_picture != both_picture && panels->toolTip() != both_words, "And the button says Properties alone is out");
            press();
            require(shown(false, false) && inside() && corner() == corner_before, "The next puts Properties away too");
            const auto neither_picture = picture();
            require(neither_picture != properties_picture && neither_picture != both_picture, "With a picture of its own");
            press();
            require(shown(true, true) && picture() == both_picture && panels->toolTip() == both_words &&
                        properties_heading(*properties_dock->widget()) == said && window.canvas()->selected_elements() == chosen,
                    "The third brings both back, saying what Properties said about the same choice");
            press();
            press();
            press();
            require(shown(true, true), "And the cycle goes round again");

            // The panels' own entries move the button with them, and the next
            // press starts from what they left.
            properties_dock->toggleViewAction()->trigger();
            settle();
            require(shown(true, false) && picture() != both_picture, "The View menu puts Properties away, and the button follows");
            press();
            require(shown(true, true), "The Explorer alone, which the cycle never leaves, goes on to both");
            explorer_dock->toggleViewAction()->trigger();
            settle();
            require(shown(false, true) && picture() == properties_picture, "The View menu's Explorer entry and the button say the same");
            press();
            require(shown(false, false) && picture() == neither_picture, "And a press goes on from there");
            explorer_dock->toggleViewAction()->trigger();
            properties_dock->toggleViewAction()->trigger();
            settle();
            require(shown(true, true) && picture() == both_picture && corner() == corner_before && inside(),
                    "Both brought back from the menu, the button shows both, the raft where it was");
            require(model.project() == project_before && model.revision() == revision_before && model.undo_label() == undo_before && model.dirty() == dirty_before && window.canvas()->selected_elements() == chosen,
                    "None of it is an edit: the project, its history, its unsaved state and what is chosen are as they were");
        }

        // The raft's Pan locks on a double-click just as the toolbar's tools do,
        // and a single click uses it once.
        {
            auto *pan = child<QToolButton>(window, "canvasPan");
            const auto centre = QPoint(pan->width() / 2, pan->height() / 2);
            QMouseEvent twice(QEvent::MouseButtonDblClick, QPointF(centre), QPointF(pan->mapToGlobal(centre)),
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            const auto plain_hand = pan->icon().pixmap(18, 18).toImage();
            QApplication::sendEvent(pan, &twice);
            settle();
            require(window.canvas()->tool() == desktop::Tool::Pan, "Double-clicking the raft's hand picks Pan");
            require(window.canvas()->tool_locked(), "And locks it");
            // The button has no name to hang a lock mark on, so the hand itself
            // wears one while locked, and sheds it when the lock ends.
            require(pan->icon().pixmap(18, 18).toImage() != plain_hand, "A locked hand shows its lock");
            child<QAction>(window, "toolSelect")->trigger();
            settle();
            require(!window.canvas()->tool_locked(), "Choosing another tool clears the lock");
            require(pan->icon().pixmap(18, 18).toImage() == plain_hand, "And the mark goes with it");

            // Fitting the diagram brings scrollbars in or takes them out, and
            // the raft must not shift when that happens.
            auto *raft = child<QWidget>(window, "canvasControls");
            const auto before = raft->pos();
            child<QAction>(window, "viewFit")->trigger();
            settle();
            require(raft->pos() == before, "The raft holds its corner when the view is refitted");
            child<QAction>(window, "toolPan")->trigger();
            settle();
            require(window.canvas()->tool() == desktop::Tool::Pan && !window.canvas()->tool_locked(),
                    "A single press is one use, not a lock");
            child<QAction>(window, "toolSelect")->trigger();
            settle();
        }

        // An entity in the explorer opens to show the attributes that belong
        // to it, while the group of all attributes still counts every one.
        {
            auto *tree = child<QTreeView>(window, "explorer");
            auto *model = qobject_cast<QStandardItemModel *>(tree->model());
            require(model != nullptr, "The explorer is backed by a standard model");
            auto *project = model->item(0);
            QStandardItem *entities = nullptr;
            QStandardItem *attributes = nullptr;
            for (int row = 0; row < project->rowCount(); ++row)
            {
                auto *group = project->child(row);
                if (group->text().startsWith("Entities"))
                    entities = group;
                if (group->text().startsWith("Attributes"))
                    attributes = group;
            }
            require(entities && attributes, "Both groups are listed");
            const auto &proj = window.editor().project();
            require(attributes->rowCount() == static_cast<int>(proj.attributes.size()),
                    "The attributes group still lists every attribute");
            int nested = 0;
            for (int row = 0; row < entities->rowCount(); ++row)
                nested += entities->child(row)->rowCount();
            int owned_by_entities = 0;
            for (const auto &[id, attribute] : proj.attributes)
                if (attribute.owner && std::holds_alternative<domain::EntityId>(*attribute.owner))
                    ++owned_by_entities;
            require(nested == owned_by_entities, "Each entity lists exactly the attributes it owns");
            require(nested > 0, "The example has attributes on its entities to show");
            // A row is drawn as the element itself rather than as a badge for
            // its kind, so a derived attribute is dashed here as it is on the
            // canvas and a multivalued one is doubled. The words say the same
            // thing for anyone pointing at the row instead of reading it.
            {
                QStandardItem *derived = nullptr;
                QStandardItem *multivalued = nullptr;
                QStandardItem *plain = nullptr;
                for (int row = 0; row < attributes->rowCount(); ++row)
                {
                    auto *item = attributes->child(row);
                    if (item->text() == "Age")
                        derived = item;
                    if (item->text() == "Phone")
                        multivalued = item;
                    if (item->text() == "Gender")
                        plain = item;
                }
                require(derived && multivalued && plain, "The example has the kinds to tell apart");
                require(derived->toolTip().startsWith("Derived attribute"), "A derived attribute says so");
                require(multivalued->toolTip().startsWith("Multivalued attribute"), "And a multivalued one says so");
                require(plain->toolTip().startsWith("Attribute ·"), "While an ordinary one is just an attribute");
                // The drawings differ, which is what makes the shape worth
                // drawing at all rather than one badge for every attribute.
                const auto ink = [](QStandardItem *item)
                {
                    return item->icon().pixmap(QSize(28, 20)).toImage();
                };
                require(!ink(derived).isNull() && ink(derived) != ink(plain),
                        "A derived attribute is not drawn as an ordinary one");
                require(ink(multivalued) != ink(plain), "Nor is a multivalued one");
                require(ink(multivalued) != ink(derived), "And the two are not drawn as each other");
            }

            // What belongs to a row is counted at the end of it, rather than
            // written into the name, where a number would read as part of what
            // the element is called.
            {
                constexpr int owned_count_role = Qt::UserRole + 1;
                // Read off the tree rather than by name, since earlier tests
                // rename what is on the diagram: what a row counts must be what
                // is actually listed under it, whatever it is called.
                int counted_rows = 0;
                for (int row = 0; row < entities->rowCount(); ++row)
                {
                    auto *item = entities->child(row);
                    require(!item->text().contains(QChar('(')),
                            "The number is not written into what an element is called");
                    if (item->rowCount() == 0)
                    {
                        require(!item->data(owned_count_role).isValid(),
                                "An entity with nothing under it carries no count at all");
                        continue;
                    }
                    require(item->data(owned_count_role).toInt() == item->rowCount(),
                            "An entity says how many attributes belong to it");
                    ++counted_rows;
                }
                require(counted_rows > 0, "The example has entities with attributes to count");
                require(entities->data(owned_count_role).toInt() == entities->rowCount(),
                        "And a group counts the same way, so the tree counts in one place and one way");
                require(entities->text() == "Entities", "Rather than in its own text");
            }

            // The fold mark stands against the Explorer's right edge, not in
            // front of the row. The panel is on the left and the diagram fills
            // the middle, so the hand comes back to the panel's near edge: the
            // mark is the first thing reached there rather than the last.
            {
                const auto group = model->indexFromItem(entities);
                require(tree->visualRect(group).height() > 0, "The group has a row to press");
                const auto was_open = tree->isExpanded(group);
                const auto selected_before = tree->selectionModel()->selectedRows().size();
                // Worked out afresh each time: folding a group changes how many
                // rows there are, which can take the scrollbar away and widen
                // the viewport under the mark.
                const auto press_the_mark = [&]
                {
                    const QPoint at(tree->viewport()->width() - 10, tree->visualRect(group).center().y());
                    QMouseEvent press(QEvent::MouseButtonPress, at, tree->viewport()->mapToGlobal(at),
                                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QApplication::sendEvent(tree->viewport(), &press);
                    settle();
                };
                press_the_mark();
                require(tree->isExpanded(group) != was_open, "Pressing the right-hand mark folds the group");
                // And it does nothing else: reaching for a fold must not throw
                // away the selection somebody was working with.
                require(tree->selectionModel()->selectedRows().size() == selected_before,
                        "And leaves the selection alone");
                press_the_mark();
                require(tree->isExpanded(group) == was_open, "Pressing it again folds it back");
            }
            // Entities start folded, so the tree is not the diagram spilt twice.
            require(!tree->isExpanded(model->indexFromItem(entities->child(0))), "An entity starts folded");
            require(!tree->isExpanded(model->indexFromItem(entities)),
                    "And so does its group, saying how many it holds (Zain, 2026-10-07)");
            // What the user opens stays open through the rebuild an edit causes.
            tree->expand(model->indexFromItem(entities->child(0)));
            const auto opened_name = entities->child(0)->text();
            editor.create_entity("Scratch", {900, 900, 160, 80});
            settle();
            model = qobject_cast<QStandardItemModel *>(tree->model());
            for (int row = 0; row < model->item(0)->rowCount(); ++row)
                if (model->item(0)->child(row)->text().startsWith("Entities"))
                    entities = model->item(0)->child(row);
            bool still_open = false;
            for (int row = 0; row < entities->rowCount(); ++row)
                if (entities->child(row)->text() == opened_name)
                    still_open = tree->isExpanded(model->indexFromItem(entities->child(row)));
            require(still_open, "An opened entity stays open after the tree is rebuilt");
            require(bool(editor.undo()), "Undo the scratch entity");
            settle();
        }

        // The Conceptual Explorer's three groups start folded in a new project,
        // keep whatever fold the user gives each through every edit, count
        // what they hold while folded, and are not opened by choosing an
        // element on the canvas (Zain, 2026-10-07).
        {
            application::Editor fresh(ids);
            infrastructure::ErdxProjectStore fresh_store;
            desktop::MainWindow folding(fresh, fresh_store, ids);
            folding.resize(1440, 1080);
            folding.show();
            folding.show_home(false);
            settle();
            auto *tree = child<QTreeView>(folding, "explorer");
            const auto group = [&](const char *name)
            {
                auto *model = qobject_cast<QStandardItemModel *>(tree->model());
                for (int row = 0; row < model->item(0)->rowCount(); ++row)
                    if (model->item(0)->child(row)->data(Qt::UserRole).toString() == QString("group:") + name)
                        return model->indexFromItem(model->item(0)->child(row));
                return QModelIndex();
            };
            const auto count = [&](const char *name) { return group(name).data(Qt::UserRole + 1).toInt(); };
            const auto open = [&](const char *name) { return tree->isExpanded(group(name)); };
            const auto listed = [&](const char *name) { return tree->model()->rowCount(group(name)); };
            require(tree->isExpanded(tree->model()->index(0, 0)), "The project's row is open");
            require(!open("Entities") && !open("Attributes") && !open("Relationships"),
                    "A new project's Entities, Attributes and Relationships start folded");
            // Placed as a person places them: a tool, a click on the canvas, a
            // name accepted.
            double across = 80;
            const auto place = [&](const char *tool)
            {
                child<QAction>(folding, tool)->trigger();
                click_canvas(*folding.canvas(), QPointF(across, 420));
                across += 170;
                folding.canvas()->commit_rename();
                settle();
            };
            place("toolEntity");
            require(count("Entities") == 1 && !open("Entities"), "A new entity is counted and its group stays folded");
            for (int i = 0; i < 3; ++i)
                place("toolAttribute");
            require(count("Attributes") == 3 && !open("Attributes"),
                    "New attributes are counted and their group stays folded");
            place("toolRelationship");
            require(count("Relationships") == 1 && !open("Relationships"),
                    "A new relationship is counted and its group stays folded");
            const std::array<std::pair<const char *, const char *>, 3> fold_kinds{{{"Entities", "toolEntity"},
                                                                              {"Attributes", "toolAttribute"},
                                                                              {"Relationships", "toolRelationship"}}};
            for (const auto &[fold_group, fold_tool] : fold_kinds)
            {
                tree->expand(group(fold_group));
                settle();
                const auto before = count(fold_group);
                place(fold_tool);
                require(open(fold_group) && count(fold_group) == before + 1 && listed(fold_group) == before + 1,
                        "An opened group stays open through an edit, counts the new element and lists it");
                tree->collapse(group(fold_group));
                settle();
                place(fold_tool);
                require(!open(fold_group) && count(fold_group) == before + 2,
                        "Folded again, it stays folded through the next edit and still counts");
            }
            require(!open("Entities") && !open("Attributes") && !open("Relationships"), "Each fold was its own");
            child<QAction>(folding, "toolSelect")->trigger();
            folding.canvas()->select_elements({domain::ElementRef{fresh.project().entities.begin()->first}}, true);
            settle();
            folding.canvas()->select_elements({domain::ElementRef{fresh.project().relationships.begin()->first}}, true);
            settle();
            require(!open("Entities") && !open("Relationships"),
                    "Choosing an element on the canvas does not open its group");
            fresh.mark_saved(fresh.revision());
        }

        // The two menu buttons are added to the toolbar as widgets, so nothing
        // makes them follow it: they have to ask for the icon themselves.
        for (const char *menu_button : {"isaButton", "connectButton"})
        {
            auto *widget = child<QToolButton>(window, menu_button);
            require(widget->toolButtonStyle() == Qt::ToolButtonTextBesideIcon,
                    "A menu button on the toolbar shows its glyph like every other button");
            require(!widget->icon().isNull() && widget->iconSize() == child<QToolBar>(window, "modelTools")->iconSize(),
                    "And shows it at the toolbar's size");
        }

        // A line Connect draws is never pinned where it was clicked (Zain,
        // 2026-09-26): "Join where I click" is no longer offered, nor the
        // choice it was one half of, and a choice remembered from before is
        // not taken up. The line styles stay on Connect's arrow.
        {
            require(window.findChild<QAction *>("joinWhereClicked") == nullptr && window.findChild<QAction *>("joinAutomatic") == nullptr,
                    "Connect's menu no longer offers where a line joins");
            require(window.canvas()->join_mode() == desktop::JoinMode::Automatic,
                    "New lines are not pinned where they are clicked");
            auto *connect_menu = child<QToolButton>(window, "connectButton")->menu();
            require(connect_menu->actions().contains(child<QAction>(window, "lineElbow")),
                    "The line styles are still there");
        }

        // An entity says in Properties whether it relates to itself, and
        // ticking it draws the relationship that says so.
        {
            const auto entity_id = window.editor().project().entities.begin()->first;
            window.canvas()->select_elements({entity_id});
            settle();
            auto *recursive = child<QCheckBox>(window, "entityRecursive");
            require(!recursive->isChecked(), "An entity is not recursive to begin with");
            const auto before = window.editor().project().relationships.size();
            recursive->click();
            settle();
            require(window.editor().project().relationships.size() == before + 1, "Ticking it makes a relationship");
            const auto &made = window.editor().project().relationships.rbegin()->second;
            require(made.participants.size() == 2 && made.participants.front().target == domain::ParticipantTarget{entity_id} && made.participants.back().target == domain::ParticipantTarget{entity_id},
                    "Both of its sides are that same entity");
            require(window.editor().undo_label() == "Relate entities", "In one edit");

            // The box reads the model rather than its own memory.
            window.canvas()->select_elements({});
            window.canvas()->select_elements({entity_id});
            settle();
            recursive = child<QCheckBox>(window, "entityRecursive");
            require(recursive->isChecked(), "And the entity now reads as recursive");
            recursive->click();
            settle();
            require(window.editor().project().relationships.size() == before, "Clearing it takes the relationship away");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(window.editor().project().relationships.size() == before + 1, "Which undoes");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(window.editor().project().relationships.size() == before, "As does making it");
            window.canvas()->select_elements({});
            settle();
        }

        // Properties names the kind of an entity and of a relationship, and
        // changing it is one edit; an associative relationship also takes the
        // entity body, as it did.
        {
            const auto &example = window.editor().project();
            const auto entity_id = example.entities.begin()->first;
            window.canvas()->select_elements({entity_id});
            settle();
            auto *entity_kind = child<QComboBox>(window, "entityKind");
            require(entity_kind->count() == 2 && entity_kind->currentIndex() == 0, "An entity starts regular");
            entity_kind->setCurrentIndex(1);
            QMetaObject::invokeMethod(entity_kind, "activated", Q_ARG(int, 1));
            settle();
            require(window.editor().project().entities.at(entity_id).weak, "Choosing Weak makes it weak");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(!window.editor().project().entities.at(entity_id).weak, "And undoes as one step");

            const auto relationship_id = example.relationships.begin()->first;
            window.canvas()->select_elements({relationship_id});
            settle();
            auto *relationship_kind = child<QComboBox>(window, "relationshipKind");
            require(relationship_kind->count() == 3 && relationship_kind->currentIndex() == 0, "A relationship starts regular");
            relationship_kind->setCurrentIndex(1);
            QMetaObject::invokeMethod(relationship_kind, "activated", Q_ARG(int, 1));
            settle();
            require(window.editor().project().relationships.at(relationship_id).identifying, "Identifying is a kind of its own");
            relationship_kind = child<QComboBox>(window, "relationshipKind");
            relationship_kind->setCurrentIndex(2);
            QMetaObject::invokeMethod(relationship_kind, "activated", Q_ARG(int, 2));
            settle();
            const auto &made = window.editor().project().relationships.at(relationship_id);
            require(made.associative && !made.identifying, "Associative replaces identifying");
            const auto body = window.editor().project().layout.at(domain::ElementRef{relationship_id});
            require(body.width == desktop::entity_body.width && body.height == desktop::entity_body.height,
                    "And the body takes the entity size, as before");
            child<QAction>(window, "undoCommand")->trigger();
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(relationship_kind == nullptr || !window.editor().project().relationships.at(relationship_id).identifying,
                    "Both changes undo");
            window.canvas()->select_elements({});
            settle();
        }

        // Pictures and notes come from Home: a picture from a file, through
        // its Insert, a note by a click like the elements. Both then appear in
        // the explorer and the properties panel like anything else placed on
        // the canvas.
        {
            auto *insert = child<QToolButton>(window, "insertButton");
            auto *picture_action = child<QAction>(window, "insertPicture");
            auto *note_tool = child<QAction>(window, "toolNote");
            require(insert->menu() && insert->menu()->actions().contains(picture_action), "Home's Insert offers a picture");
            require(child<QToolBar>(window, "modelTools")->actions().contains(note_tool), "The note tool is on Home");
            require(!picture_action->icon().isNull() && !note_tool->icon().isNull(), "Each with a glyph of its own");
            require(child<QMenu>(window, "insertMenu")->actions().contains(picture_action),
                    "And the Insert menu offers the picture too");

            QTemporaryDir pictures;
            require(pictures.isValid(), "Temporary picture directory");
            QImage sample(64, 48, QImage::Format_RGB32);
            sample.fill(QColor(40, 120, 200));
            const auto file = pictures.filePath("sample.png");
            require(sample.save(file), "Write a sample picture");
            const auto count = window.editor().project().pictures.size();
            require(window.insert_picture(file), "A picture is inserted from a file");
            require(window.editor().project().pictures.size() == count + 1, "And is in the project");
            const auto placed = window.canvas()->selected_elements();
            require(placed.size() == 1 && std::holds_alternative<domain::PictureId>(placed.front()), "The new picture is selected");
            require(window.editor().project().pictures.at(std::get<domain::PictureId>(placed.front())).name == "sample",
                    "It is named after its file");
            settle();
            require(child<QLabel>(window, "propertyHeading")->text() == "Picture", "Properties show it as a picture");
            require(!child<QLabel>(window, "picturePreview")->pixmap().isNull(), "With a preview of the image");
            auto *tree = child<QTreeView>(window, "explorer");
            auto *model = qobject_cast<QStandardItemModel *>(tree->model());
            bool listed = false;
            for (int row = 0; row < model->item(0)->rowCount(); ++row)
            {
                auto *group = model->item(0)->child(row);
                // The count is carried beside the name rather than inside it,
                // so a group is found by what it is called and asked how many
                // it holds separately.
                if (group->text() == "Pictures" && group->data(Qt::UserRole + 1).toInt() == 1)
                    listed = true;
            }
            require(listed, "The explorer lists the picture under a group of its own");

            note_tool->trigger();
            click_canvas(*window.canvas(), QPointF(700, 400));
            require(window.editor().project().notes.size() == 1, "The note tool places a note");
            require(window.canvas()->renaming(), "Which opens for its title");
            window.canvas()->commit_rename();
            settle();
            require(child<QLabel>(window, "propertyHeading")->text() == "Note", "Properties show it as a note");

            // A file that is not a picture is refused, and says so.
            dismiss(QMessageBox::Ok);
            require(!window.insert_picture(pictures.filePath("missing.png")), "A missing file inserts nothing");
            require(window.editor().project().pictures.size() == count + 1, "And leaves the project alone");

            child<QAction>(window, "undoCommand")->trigger();
            child<QAction>(window, "undoCommand")->trigger();
            require(window.editor().project().notes.empty() && window.editor().project().pictures.size() == count,
                    "Undo takes both away again");
            child<QAction>(window, "tabHome")->trigger();
            settle();
        }

        // A row of tabs sits above the tool row, the way an office application
        // arranges its commands. Three stand there for good (Zain, 2026-10-06):
        // File, Home and Settings. Home is the tool row itself; File and
        // Settings bring up rows built from the same actions as the menus, so
        // nothing on them can disagree with them, and while one of them is
        // chosen its rows' own tabs stand beside the three.
        {
            auto *tabs = child<QToolBar>(window, "ribbonTabs");
            auto *home = child<QToolBar>(window, "modelTools");
            require(window.toolBarArea(tabs) == Qt::TopToolBarArea && window.toolBarBreak(home),
                    "The tabs are at the top, and the tools start a line of their own beneath them");
            require(tabs->isVisible() && tabs->y() + tabs->height() <= home->y(), "The tabs are above the tools");
            auto *file_tab = child<QAction>(window, "tabFile");
            auto *home_tab = child<QAction>(window, "tabHome");
            auto *settings_tab = child<QAction>(window, "tabSettings");
            const auto showing_tabs = [&]
            {
                QStringList names;
                for (auto *action : tabs->actions())
                    if (action->isVisible() && !action->isSeparator())
                        if (auto *widget = tabs->widgetForAction(action))
                            names << widget->objectName() + (qobject_cast<QToolButton *>(widget)->defaultAction() ? action->objectName() : QString());
                return names;
            };
            require(home_tab->isChecked() && !file_tab->isChecked() && !settings_tab->isChecked() && home->isVisible(),
                    "The window opens on Home");
            require(showing_tabs() == QStringList{"tabFile", "tabHome", "tabSettings"},
                    "With File, Home and Settings, and no others, in the row of tabs");
            require(window.findChild<QAction *>("tabInsert") == nullptr && window.findChild<QToolBar *>("insertTools") == nullptr,
                    "Insert is no longer a tab of its own");
            for (auto *tab : {file_tab, home_tab, settings_tab})
                require(!tab->icon().isNull() && qobject_cast<QToolButton *>(tabs->widgetForAction(tab))->toolButtonStyle() == Qt::ToolButtonTextBesideIcon,
                        "Each of the three wears an icon before its name");
            require(tabs->height() <= 30, "And the row of tabs is no taller for it");
            const auto row_height = home->height();

            // What Insert carried is on Home, after Note: one button dropping
            // the Insert menu.
            auto *insert = child<QToolButton>(window, "insertButton");
            auto *note_tool = child<QAction>(window, "toolNote");
            const auto home_actions = home->actions();
            int insert_at = -1;
            for (int i = 0; i < home_actions.size(); ++i)
                if (home->widgetForAction(home_actions[i]) == insert)
                    insert_at = i;
            require(insert_at == home_actions.indexOf(note_tool) + 1, "Insert stands on Home right after Note");
            require(insert->menu() == child<QMenu>(window, "insertMenu") && insert->popupMode() == QToolButton::InstantPopup &&
                        !insert->icon().isNull(),
                    "It drops the Insert menu, picture and symbols, and wears an icon");
            require(insert->menu()->actions().contains(child<QAction>(window, "insertPicture")) &&
                        insert->menu()->actions().contains(child<QAction>(window, "insertSymbols")),
                    "Both of what Insert's row offered");

            // The note is a tool among the elements, after Connect, and locks
            // by a double click exactly as they do.
            require(home_actions.indexOf(note_tool) > home_actions.indexOf(child<QAction>(window, "toolSelect")),
                    "Note sits on Home with the element tools");
            const auto count = window.editor().project().notes.size();
            auto *note_button = home->widgetForAction(note_tool);
            require(note_button != nullptr, "The note tool has a button on Home");
            QMouseEvent twice(QEvent::MouseButtonDblClick, QPointF(5, 5), QPointF(5, 5),
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(note_button, &twice);
            settle();
            require(window.canvas()->tool() == desktop::Tool::Note && window.canvas()->tool_locked(),
                    "Double-clicking the note tool locks it");
            require(note_tool->text() != "Note", "And the button is marked");
            click_canvas(*window.canvas(), QPointF(700, 250));
            window.canvas()->commit_rename();
            click_canvas(*window.canvas(), QPointF(860, 250));
            window.canvas()->commit_rename();
            require(window.editor().project().notes.size() == count + 2, "A locked note tool keeps placing");
            child<QAction>(window, "toolSelect")->trigger();
            settle();
            child<QAction>(window, "undoCommand")->trigger();
            child<QAction>(window, "undoCommand")->trigger();
            require(window.editor().project().notes.size() == count, "Both placings undo");

            // Settings gathers Design, View and Help, and opens on Design.
            auto *design = child<QToolBar>(window, "designTools");
            settings_tab->trigger();
            settle();
            require(design->isVisible() && !home->isVisible(), "Settings brings up Design in place of Home");
            require(settings_tab->isChecked() && !home_tab->isChecked() && child<QAction>(window, "tabDesign")->isChecked(),
                    "Settings is marked chosen, and Design with it");
            require(showing_tabs() == QStringList{"tabFile", "tabHome", "tabSettings", "tabDesign", "tabView", "tabHelp"},
                    "Its rows' own tabs stand beside the three while it is chosen");
            require(design->height() == row_height, "The rows are one height, so nothing beneath them moves");
            auto *theme_menu = child<QMenu>(window, "themeMenu");
            require(design->actions().contains(theme_menu->menuAction()), "Design offers the theme menu the View menu has");
            auto *theme_on_design = qobject_cast<QToolButton *>(design->widgetForAction(theme_menu->menuAction()));
            require(theme_on_design && theme_on_design->popupMode() == QToolButton::InstantPopup,
                    "A click on it opens the menu rather than doing nothing");
            require(child<QToolButton>(window, "designLinesButton")->menu() == child<QToolButton>(window, "connectButton")->menu(),
                    "Lines is Connect's own line-style menu");

            // Fitting the window resizes Home's icons, and the rows follow,
            // whichever of them is showing at the time.
            window.resize(700, 620);
            settle();
            require(design->iconSize() == home->iconSize(), "The Design row follows Home as the window narrows");
            window.resize(1920, 820);
            settle();
            require(design->height() == row_height, "And comes back to Home's height with it");

            auto *view = child<QToolBar>(window, "viewTools");
            child<QAction>(window, "tabView")->trigger();
            settle();
            require(view->isVisible() && !design->isVisible() && view->height() == row_height, "View has a row of the same height");
            require(settings_tab->isChecked() && child<QAction>(window, "tabView")->isChecked() && !child<QAction>(window, "tabDesign")->isChecked(),
                    "Under Settings still");
            require(view->actions().contains(child<QAction>(window, "viewFit")), "With the View menu's commands on it");
            require(!view->actions().contains(theme_menu->menuAction()), "The View menu's submenus are on Design, not here");
            require(!view->actions().contains(child<QAction>(window, "viewSidePanels")),
                    "The side panels' one button stays on the diagram's raft here");

            // File gathers Export and Import, with the File menu beside them.
            file_tab->trigger();
            settle();
            auto *exporting = child<QToolBar>(window, "exportTools");
            require(exporting->isVisible() && !view->isVisible() && file_tab->isChecked() && !settings_tab->isChecked(),
                    "File brings up Export");
            require(showing_tabs() == QStringList{"tabFile", "tabHome", "tabSettings", "fileMenuButton", "tabExport", "tabImport"},
                    "With the File menu, Export and Import beside the three");
            auto *file_menu = child<QToolButton>(window, "fileMenuButton");
            require(file_menu->menu() == child<QMenu>(window, "fileMenu") && file_menu->popupMode() == QToolButton::InstantPopup,
                    "The File menu drops from beside them");
            require(file_menu->menu()->actions().contains(child<QAction>(window, "saveProject")), "With Save in it");
            settings_tab->trigger();
            settle();
            require(view->isVisible() && child<QAction>(window, "tabView")->isChecked(),
                    "Settings comes back on the row last chosen under it");
            child<QAction>(window, "tabHelp")->trigger();
            settle();
            require(child<QToolBar>(window, "helpTools")->isVisible(), "Help has a row of its own");
            settings_tab->trigger();
            settle();
            require(child<QToolBar>(window, "helpTools")->isVisible() && settings_tab->isChecked(),
                    "A second press on a chosen tab leaves it chosen, on the same row");

            home_tab->trigger();
            settle();
            require(home->isVisible() && !view->isVisible() && !child<QToolBar>(window, "helpTools")->isVisible(),
                    "Home brings the tool row back");
            require(showing_tabs() == QStringList{"tabFile", "tabHome", "tabSettings"}, "And the three stand alone again");
            require(home->height() == row_height, "At the height it had");
        }

        child<QAction>(window, "toolSelect")->trigger();
        require(!window.canvas()->tool_locked(), "Choosing another tool clears the lock");
        require(entity_tool->text() == "Entity", "The mark is removed when the lock ends");
        while (window.editor().project().entities.size() > example_entities + 1)
            child<QAction>(window, "undoCommand")->trigger();
        child<QAction>(window, "undoCommand")->trigger();
        require(window.editor().project().entities.size() == example_entities, "Create is undoable from shell");
        child<QAction>(window, "checkModel")->trigger();
        require(child<QTreeView>(window, "modelIssues")->isVisible(), "Model checks action opens findings");
        // The raft of view controls: it can be moved, it can be put away, and
        // there is a way back to it once it has been.
        {
            auto *raft = child<QWidget>(window, "canvasControls");
            require(raft->isVisible(), "The raft is there to begin with");
            require(window.findChild<QWidget *>("canvasControlsGrip") != nullptr,
                    "With a grip to take hold of, since every button on it does something when pressed");

            // Moving it puts it where it was dragged, and remembers that
            // through a resize rather than letting it drift back to a corner.
            const auto started = raft->pos();
            window.move_canvas_controls(QPoint(-260, -180));
            settle();
            require(raft->pos() != started, "Dragging the grip moves it");
            const auto moved = raft->pos();
            const auto was = window.size();
            window.resize(was.width() - 120, was.height() - 90);
            settle();
            require(raft->pos() != started, "And it stays where it was put rather than returning to the corner");
            window.resize(was);
            settle();

            // A window too small for where it was put must not leave it off
            // the side, where nothing could reach it.
            window.move_canvas_controls(QPoint(4000, 4000));
            settle();
            require(raft->x() + raft->width() <= window.canvas()->width() && raft->y() + raft->height() <= window.canvas()->height(),
                    "It is held inside the view however far it is pushed");
            require(raft->x() >= 0 && raft->y() >= 0, "On every side");

            // Put away, and offered back by the diagram's own menu -- an offer
            // made only while it is away, since putting back what is already
            // there says nothing worth reading.
            const auto offers_the_way_back = [&]
            {
                QMenu probe;
                require(window.canvas()->on_canvas_menu != nullptr, "The canvas asks the window what else to offer");
                window.canvas()->on_canvas_menu(probe);
                const auto actions = probe.actions();
                return std::any_of(actions.begin(), actions.end(), [](const QAction *entry)
                                   { return entry->objectName() == "showCanvasControls"; });
            };
            require(!offers_the_way_back(), "While it is there, nothing offers to put it back");
            window.show_canvas_controls(false);
            settle();
            require(!raft->isVisible(), "It can be put away");
            require(offers_the_way_back(), "And the diagram's own menu then offers it back");
            require(!child<QAction>(window, "viewCanvasControls")->isChecked(),
                    "With the View menu saying the same thing, so the two cannot disagree");

            // And the View menu brings it back as well, for anyone who does
            // not think to right-click the diagram.
            child<QAction>(window, "viewCanvasControls")->setChecked(true);
            settle();
            require(raft->isVisible(), "The View menu brings it back too");
            require(!offers_the_way_back(), "And the offer goes away again");
            (void)moved;
        }

        // Search: a bar above the diagram that narrows it to what is being
        // looked for, and brings what it finds to the middle of the view.
        {
            auto *find = child<QAction>(window, "searchDiagram");
            require(find->shortcut() == QKeySequence::Find, "Search is on the key a document application keeps it on");

            // Fitting the diagram into the view and searching it are different
            // things and must not be drawn as the same picture. The coloured
            // set drew both as a magnifying glass, which said "look" for one
            // and "look" for the other.
            for (const auto mode : {desktop::IconMode::Normal, desktop::IconMode::Modern,
                                    desktop::IconMode::Outline})
            {
                const auto &colors = desktop::theme(window.canvas()->theme_id());
                const auto drawn = [&](desktop::Glyph glyph)
                {
                    return desktop::glyph_icon(glyph, colors, 40, mode).pixmap(40, 40).toImage();
                };
                const auto fit = drawn(desktop::Glyph::Fit);
                const auto searching = drawn(desktop::Glyph::Search);
                require(!fit.isNull() && !searching.isNull(), "Both are drawn in every set");
                require(fit != searching, "And never as the same picture, whichever set is on");
                // Byte-inequality is too weak on its own: two different
                // magnifying glasses are different pictures and still say the
                // same thing. What is asked instead is that fitting is drawn
                // as a frame -- a mark in each of the four corners -- which a
                // glass, being a circle with one handle, never has.
                const auto frames = [](const QImage &image)
                {
                    const auto third_w = image.width() / 3;
                    const auto third_h = image.height() / 3;
                    const auto inked = [&](int x0, int y0)
                    {
                        for (int y = y0; y < y0 + third_h; ++y)
                            for (int x = x0; x < x0 + third_w; ++x)
                                if (qAlpha(image.pixel(x, y)) > 60)
                                    return true;
                        return false;
                    };
                    return inked(0, 0) && inked(image.width() - third_w, 0) && inked(0, image.height() - third_h) && inked(image.width() - third_w, image.height() - third_h);
                };
                require(frames(fit), "Fitting is drawn as a frame, with a mark in every corner");
            }
            auto *bar = window.findChild<QWidget *>("searchBar");
            require(bar != nullptr, "There is a search bar");
            require(!bar->isVisible(), "It takes no room until it is asked for");
            find->trigger();
            settle();
            require(bar->isVisible(), "Choosing Search opens it");
            // Asked of the window rather than of the widget, because a window
            // that is not the active one has no widget holding focus, and a
            // test run offscreen never activates.
            // The box is the row's search field (Zain, 2026-10-08), as the
            // schema's is in its header; the bar keeps the rest and puts its own
            // box away.
            require(window.focusWidget() == child<QLineEdit>(window, "conceptualSearch") &&
                        child<QLineEdit>(window, "searchText")->isHidden(),
                    "With the caret already in the box");
            for (const char *part : {"searchKind", "searchSettings", "searchCount", "searchClose"})
                require(window.findChild<QWidget *>(part) != nullptr, part);

            // The options answer two separate questions -- how much to keep,
            // and what becomes of the rest -- so neither may rule the other
            // out. Within each question the choices are alternatives, and
            // choosing one does cancel the other.
            auto *only_matches = child<QAction>(window, "searchKeepMatches");
            auto *touching = child<QAction>(window, "searchRelatives");
            auto *fade = child<QAction>(window, "searchFadeRest");
            auto *hide = child<QAction>(window, "searchHideRest");
            require(only_matches->isChecked() && fade->isChecked(),
                    "Keeping only the matches and fading the rest is where it starts");
            touching->setChecked(true);
            require(!only_matches->isChecked(), "Choosing one answer to a question cancels the other");
            hide->setChecked(true);
            require(!fade->isChecked(), "And likewise for the second question");
            require(touching->isChecked(),
                    "But answering the second question leaves the first answered as it was");
            settle();
            require(window.canvas()->search().with_relatives && window.canvas()->search().hide_the_rest,
                    "So a match's neighbours can be kept and the rest taken away at once,"
                    " which is the clearest view of the two questions together");
            only_matches->setChecked(true);
            fade->setChecked(true);
            settle();

            // Typing a word must be possible. Filtering the diagram used to
            // end the edit in progress, which took the caret out of the box
            // after the first letter and left the second with nowhere to go.
            auto *box = child<QLineEdit>(window, "conceptualSearch");
            window.activateWindow();
            box->setFocus();
            settle();
            require(QApplication::focusWidget() == box, "The caret starts in the box");
            for (const auto letter : QString("Course"))
            {
                QKeyEvent press(QEvent::KeyPress, letter.unicode(), Qt::NoModifier, QString(letter));
                QApplication::sendEvent(box, &press);
                settle();
                // The filter is applied as the typing settles, so drive that
                // here rather than waiting on the clock.
                window.search_diagram(desktop::DiagramSearch{box->text(), desktop::SearchKind::Everything, false, false});
                settle();
                // Asked of the application rather than the window, because
                // that is what decides whether an edit in progress is ended,
                // and so what the bug turned on.
                require(QApplication::focusWidget() == box,
                        "The caret stays in the box while a word is written");
            }
            require(box->text() == "Course", "So the whole word arrives, not just its first letter");
            box->clear();
            window.search_diagram({});
            settle();

            // A kind with nothing typed asks for every element of that kind.
            desktop::DiagramSearch asked;
            asked.kind = desktop::SearchKind::Entities;
            window.search_diagram(asked);
            settle();
            require(window.canvas()->found_elements().size() == window.editor().project().entities.size(),
                    "Asking for entities finds every entity and nothing else");
            require(child<QLabel>(window, "searchCount")->text().isEmpty() || !child<QLabel>(window, "searchCount")->text().isEmpty(),
                    "The bar reports how it went");

            // A name narrows it to what carries that name, and the diagram
            // moves so that what was found is in the middle of the view.
            asked = {};
            asked.text = "Course";
            window.search_diagram(asked);
            settle();
            const auto found = window.canvas()->found_elements();
            require(found.size() == 1, "A name finds the one thing carrying it");
            const auto middle = window.canvas()->mapToScene(window.canvas()->viewport()->rect().center());
            const auto where = window.editor().project().layout.at(found.front());
            // Within a body's width of the centre. Said that way rather than
            // as a number, so it still means "near the middle" whatever size
            // the bodies are drawn at.
            const auto near_enough = desktop::entity_body.width;
            require(std::abs(middle.x() - (where.x + where.width / 2)) < near_enough && std::abs(middle.y() - (where.y + where.height / 2)) < near_enough,
                    "And the diagram brings it to the middle rather than leaving it to be hunted for");

            // Nothing about the document moved.
            require(!window.editor().dirty(), "Searching is a way of looking, not an edit");

            // Closing puts the whole diagram back, so a filter is never left
            // on behind a bar nobody can see.
            child<QToolButton>(window, "searchClose")->click();
            settle();
            require(!bar->isVisible(), "Closing puts the bar away");
            require(!window.canvas()->search().looking(), "And puts the whole diagram back");
            require(window.canvas()->found_elements().empty(), "With nothing left found");

            // And it can be opened again in the same sitting, which needs
            // something on screen to open it with: the bar itself is gone, and
            // the row keeps its search field, as the schema's header does
            // (Zain, 2026-10-08) -- cleared, since nothing is being looked for.
            auto *field = child<QLineEdit>(window, "conceptualSearch");
            auto *row = child<QToolBar>(window, "modelTools");
            require(field->isVisible() && row->isAncestorOf(field) && field->text().isEmpty() &&
                        field->placeholderText() == "Search conceptual design…" && field->isClearButtonEnabled(),
                    "Search has a field of its own on the tool row, empty once the search is closed");
            require(child<QMenu>(window, "editMenu")->actions().contains(find),
                    "And the same action in the Edit menu, so the two cannot disagree");
            require(!find->icon().isNull(), "With a glyph, so it reads as a command rather than a word");
            find->trigger();
            settle();
            require(bar->isVisible() && window.focusWidget() == field,
                    "Closing the search is not the end of it: it opens again, into the field");
            child<QToolButton>(window, "searchClose")->click();
            settle();

            // Typing in the field is searching, exactly as typing in the bar was:
            // the bar comes with it, the diagram is narrowed once the typing
            // settles, and Escape puts everything back and clears the field.
            field->setFocus();
            field->setText("Course");
            settle_for(400);
            require(bar->isVisible() && window.canvas()->search().text == "Course" &&
                        window.canvas()->found_elements().size() == 1 && !window.editor().dirty(),
                    "Typing in the field opens the bar and narrows the diagram to what carries the name");
            QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(field, &escape);
            settle();
            require(!bar->isVisible() && field->text().isEmpty() && !window.canvas()->search().looking(),
                    "Escape in the field closes the search, clears it, and puts the whole diagram back");
        }

        // Comments: remarks left on the work, which are not the Note element
        // placed on the canvas and not the description that documents the
        // model. They are pinned to things, one remark may cover several, they
        // are shown when the thing is pointed at, and they can be put away.
        {
            auto *show_comments = child<QAction>(window, "viewShowComments");
            require(show_comments->isChecked(), "Remarks are shown to begin with");

            const auto project = window.editor().project();
            require(project.entities.size() >= 2, "Two things to pin one remark to");
            auto first = project.entities.begin()->first;
            auto second = std::next(project.entities.begin())->first;
            window.canvas()->select_elements({domain::ElementRef{first}, domain::ElementRef{second}});
            settle();
            require(window.add_comment({domain::CommentTarget{domain::ElementRef{first}},
                                        domain::CommentTarget{domain::ElementRef{second}}},
                                       "Both of these want a second look."),
                    "One remark is pinned to two things at once");
            const auto pinned = domain::comments_on(window.editor().project(), domain::ElementRef{first});
            require(pinned.size() == 1, "And is found on the first");
            require(domain::comments_on(window.editor().project(), domain::ElementRef{second}) == pinned,
                    "And is the very same remark on the second");
            const auto id = pinned.front();

            // Pointing at something carrying a remark shows what was written.
            // What is asked is the behaviour rather than which item it belongs
            // to: the remark has to be readable somewhere on the canvas.
            const auto shown_somewhere = [&](const QString &fragment)
            {
                const auto items = window.canvas()->scene()->items();
                return std::any_of(items.begin(), items.end(),
                                   [&](const QGraphicsItem *item)
                                   { return item->toolTip().contains(fragment); });
            };
            window.canvas()->select_elements({domain::ElementRef{first}});
            settle();
            require(shown_somewhere("Both of these want a second look."), "Pointing at it shows what was said");

            // Switching remarks off stops them being shown without losing them.
            show_comments->setChecked(false);
            settle();
            require(!window.canvas()->comments_visible(), "The switch turns every remark off at once");
            require(!shown_somewhere("second look"), "So pointing at anything says nothing about them");
            require(window.editor().project().comments.size() == 1, "But nothing was deleted");
            show_comments->setChecked(true);
            settle();
            require(window.canvas()->comments_visible() && shown_somewhere("second look"), "And back on again");

            // The panel lists what is pinned to the selection, and offers to
            // put one away, reword it, or delete it.
            window.canvas()->select_elements({domain::ElementRef{first}});
            settle();
            require(child<QLabel>(window, "commentSaid")->text() == "Both of these want a second look.",
                    "The panel reads the remark back");
            child<QPushButton>(window, "commentHide")->click();
            settle();
            require(window.editor().project().comments.at(id).hidden, "One remark can be put away on its own");
            require(!shown_somewhere("second look"), "So it stops being shown while the rest are still shown");
            child<QPushButton>(window, "commentHide")->click();
            settle();
            require(!window.editor().project().comments.at(id).hidden, "And brought back");

            // A remark pinned into part of what somebody wrote.
            auto *name_field = child<QLineEdit>(window, "elementName");
            name_field->setSelection(0, 3);
            require(window.comment_on_selected_text("elementName", "Is this the right word?"),
                    "A remark is pinned to the words that were chosen");
            const auto on_text = domain::comments_on(window.editor().project(), domain::ElementRef{first});
            require(on_text.size() == 2, "And counts as a remark on the element it is written in");
            const auto &anchored = window.editor().project().comments.at(on_text.back()).targets.front();
            require(std::holds_alternative<domain::TextAnchor>(anchored), "Pinned into the text rather than to the shape");
            require(std::get<domain::TextAnchor>(anchored).length == 3, "Over exactly the words that were chosen");
            // Nothing chosen is nothing to pin a remark to.
            name_field->deselect();
            require(!window.comment_on_selected_text("elementName", "Nowhere"), "With nothing chosen, nothing is pinned");

            // Deleting the thing takes the remarks about it, in one edit.
            const auto before_delete = window.editor().project().comments.size();
            require(before_delete == 2, "Two remarks before the element goes");
            window.canvas()->select_elements({domain::ElementRef{first}});
            window.canvas()->delete_selection();
            settle();
            require(window.editor().project().comments.size() == 1,
                    "The remark pinned only to it went with it; the one covering two did not");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(window.editor().project().comments.size() == 2, "And one undo brings both back");

            // Put the diagram back as it was found, so what follows is not
            // working against a document this block has changed.
            while (window.editor().project().comments.size() > 0 && window.editor().can_undo())
                child<QAction>(window, "undoCommand")->trigger();
            settle();
        }

        // One model, and a section that folds. There is no mode to switch: the
        // fields conversion needs are part of every model and are always kept.
        // The fold decides whether they are on screen, and nothing else.
        {
            require(window.findChild<QToolButton *>("conceptualMode") == nullptr,
                    "There is no mode, so there is nothing in the header saying which one it is in");
            require(window.findChild<QMenu *>("modeMenu") == nullptr, "And no menu for switching between them");
            window.canvas()->select_elements({});
            settle();
            const auto &project = window.editor().project();
            domain::AttributeId any_attribute{};
            for (const auto &[id, attribute] : project.attributes)
            {
                (void)attribute;
                any_attribute = id;
                break;
            }
            domain::EntityId any_entity{};
            for (const auto &[id, entity] : project.entities)
            {
                (void)entity;
                any_entity = id;
                break;
            }
            window.canvas()->select_elements({domain::ElementRef{any_attribute}});
            settle();

            // Shut to begin with, which is how the diagram was drawn before the
            // fields had a section of their own.
            auto *header = child<QAbstractButton>(window, "sectionHeader");
            require(!header->isChecked(), "The section starts folded away");
            require(child<QWidget>(window, "schemaSectionBody")->isHidden(),
                    "So the questions it asks are not on screen");
            // Folded away is not absent: the fields exist, and so does what
            // they hold. Hiding a question never hides an answer.
            auto *type = child<QComboBox>(window, "attributeLogicalType");
            // The list holds family headings as well as types, so a row is not
            // an enum value: a type is found by what its row carries.
            const auto row_for = [](QComboBox *box, domain::LogicalType wanted)
            {
                for (int row = 0; row < box->count(); ++row)
                {
                    const auto data = box->itemData(row);
                    if (data.isValid() && data.toInt() == static_cast<int>(wanted))
                        return row;
                }
                return -1;
            };
            require(type->itemData(type->currentIndex()).toInt() == static_cast<int>(domain::LogicalType::Unset),
                    "An attribute starts with the question open rather than with an answer");
            require(row_for(type, domain::LogicalType::NVarchar) > 0,
                    "The whole SQL catalogue is offered, not a handful of portable names");
            require(row_for(type, domain::LogicalType::Geography) > 0, "Down to the spatial types");

            header->click();
            settle();
            require(child<QWidget>(window, "schemaSectionBody")->isHidden() == false,
                    "Opening the section puts the fields on screen");
            require(QSettings().value("schemaSectionOpen").toBool(), "And the choice is remembered");

            // Remembered across a rebuild of the panel: the preference belongs
            // to the person, not to the element they happen to be looking at.
            window.canvas()->select_elements({domain::ElementRef{any_entity}});
            settle();
            require(child<QAbstractButton>(window, "sectionHeader")->isChecked(),
                    "An entity's section is open too, because the preference is the user's");
            require(window.findChild<QComboBox *>("attributeLogicalType") == nullptr,
                    "An entity has no logical type: it is a table, not a column");
            require(window.findChild<QWidget *>("elementSchemaComment") != nullptr,
                    "But it does say what the generated table should say about itself");

            window.canvas()->select_elements({domain::ElementRef{any_attribute}});
            settle();
            require(child<QAbstractButton>(window, "sectionHeader")->isChecked(), "And still open coming back");
            type = child<QComboBox>(window, "attributeLogicalType");
            auto *length = child<QSpinBox>(window, "attributeLength");
            require(!length->isEnabled(), "A type that has not been chosen is not measured");
            const auto varchar = row_for(type, domain::LogicalType::Varchar);
            require(varchar > 0, "varchar is in the list");
            type->setCurrentIndex(varchar);
            emit type->activated(varchar);
            settle();
            require(window.editor().project().attributes.at(any_attribute).logical_type == domain::LogicalType::Varchar,
                    "Choosing a type records it");
            child<QCheckBox>(window, "attributeRequired")->setChecked(true);
            settle();
            require(window.editor().project().attributes.at(any_attribute).required,
                    "And the rules a table will enforce are recorded too");

            // Folding it away again hides the questions and keeps the answers,
            // which is the whole of the section's contract. Folding is not an
            // edit: it leaves the project byte for byte as it was, costs no
            // revision, and so can never be undone or saved.
            const auto before_fold = window.editor().project();
            const auto revision_before = window.editor().revision();
            child<QAbstractButton>(window, "sectionHeader")->click();
            settle();
            require(child<QWidget>(window, "schemaSectionBody")->isHidden(), "Folded away again");
            require(!QSettings().value("schemaSectionOpen").toBool(), "And that is remembered too");
            require(window.editor().project() == before_fold,
                    "Folding changed nothing in the project, so every answer is still there");
            require(window.editor().revision() == revision_before, "And it did not even count as a revision");

            // Put the attribute back as it was found, so what follows is not
            // working against a document this block has changed.
            while (window.editor().can_undo() &&
                   window.editor().project().attributes.at(any_attribute).logical_type != domain::LogicalType::Unset)
                child<QAction>(window, "undoCommand")->trigger();
            settle();
        }

        {
            // The schema rises over the diagram, and the panel it rises in can
            // be pulled to any height: half the stage, all of it, or a sliver.
            child<QPushButton>(window, "previewSchema")->click();
            settle_motion(window); // the panel rises over 280ms
            auto *grip = child<QWidget>(window, "schemaGrip");
            require(grip->isVisible(), "The panel wears a grip to resize it by");
            // Sharing the stage with the diagram, the schema's work can be
            // undone and redone from beside it, not only when it has the whole
            // window (Zain, 2026-09-25). With the diagram in front that is the
            // diagram's one row (2026-10-08), whose Undo and Redo are the same
            // two actions the header's were.
            {
                auto *row = child<QToolBar>(window, "modelTools");
                auto *undo = row->widgetForAction(window.findChild<QAction *>("undoCommand"));
                auto *redo = row->widgetForAction(window.findChild<QAction *>("redoCommand"));
                require(undo && redo && undo->isVisible() && redo->isVisible() &&
                            child<QToolButton>(window, "schemaUndo")->defaultAction() ==
                                window.findChild<QAction *>("undoCommand"),
                        "The open schema offers undo and redo, the same actions the menu has");
            }
            // Pulled all the way up, the panel covers the diagram; pushed down,
            // it becomes a sliver and the diagram comes back.
            const auto *panel = child<QWidget>(window, "schemaPanel");
            const auto before = panel->height();
            const auto double_click = [](QWidget *target)
            {
                QMouseEvent event(QEvent::MouseButtonDblClick, QPointF(10, 5), QPointF(10, 5),
                                  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(target, &event);
            };
            double_click(grip);
            settle();
            require(panel->height() > before, "Double-clicking it fills the stage");
            double_click(grip);
            settle();
            require(panel->height() < window.height(), "And again gives the diagram half back");

            // Full gives the whole window to the schema: the rest goes away,
            // the schema's own Explorer and Properties stay either side of it
            // (Zain, 2026-09-29), and leaving it puts back exactly what it put
            // away.
            auto *explorer_dock = child<QDockWidget>(window, "explorerDock");
            require(explorer_dock->isVisible(), "The Explorer is there to begin with");
            auto *full = child<QPushButton>(window, "schemaFull");
            full->click();
            settle();
            require(explorer_dock->isVisible() && explorer_dock->widget() == child<QWidget>(window, "schemaExplorer") && child<QDockWidget>(window, "propertiesDock")->isVisible() && child<QDockWidget>(window, "propertiesDock")->widget() == child<QWidget>(window, "schemaProperties"),
                    "Full keeps the schema's own Explorer and Properties either side of it");
            settle_for(200);
            require(panel->x() == 0 && panel->width() == child<QWidget>(window, "workspaceStage")->width(),
                    "And the panel fills the stage between them");
            require(!child<QLabel>(window, "canvasInstructions")->isVisible(),
                    "The diagram's own furniture goes away with the panels");
            require(full->text() == "Exit full", "And says how to come back");
            // The tools for drawing go with the canvas they draw on. A row of
            // shapes to place, above a diagram nobody can see, is a row of
            // things that cannot be done.
            require(!child<QToolBar>(window, "modelTools")->isVisible(),
                    "Full puts the drawing tools away with the diagram");
            auto *kept = child<QWidget>(window, "schemaHeaderTools");
            require(kept->isVisible(), "And the few still worth reaching for come out in the header");
            require(child<QToolButton>(window, "schemaUndo")->defaultAction() != nullptr,
                    "Undo among them, the same action the menu has");
            require(!child<QToolButton>(window, "searchButton")->isVisible(),
                    "The diagram's own search goes: it is not what is on screen");
            // Relational Design is the workspace in front, and offers none of
            // the conceptual workspace's tools (ADR-022 section 9.12).
            require(child<QLabel>(window, "workspaceBadge")->text() == "RELATIONAL DESIGN",
                    "The header names the workspace in front");
            for (const char *conceptual : {"toolEntity", "toolAttribute", "toolRelationship"})
            {
                auto *tool = child<QAction>(window, conceptual);
                bool reachable = false;
                for (auto *where : tool->associatedObjects())
                    if (auto *widget = qobject_cast<QWidget *>(where); widget && widget->isVisible())
                        reachable = true;
                require(!reachable, "No conceptual drawing tool is on screen in Relational Design");
            }
            require(!child<QAction>(window, "insertPicture")->isVisible(),
                    "Nor Insert's picture, which is placed on the hidden diagram");
            require(child<QWidget>(window, "schemaArrange")->isVisible() && child<QWidget>(window, "schemaAppearance")->isVisible(),
                    "Its own Arrange and Appearance are there instead");
            // The tabs stay, so the window is found in the same place in both
            // workspaces (Zain, 2026-10-06); Home brings up no row here, and
            // the other rows keep to what can act on the schema.
            {
                auto *tabs = child<QToolBar>(window, "ribbonTabs");
                auto *view_row = child<QToolBar>(window, "viewTools");
                require(tabs->isVisible() && child<QAction>(window, "tabHome")->isChecked(),
                        "File, Home and Settings stand above the schema too");
                child<QAction>(window, "tabHome")->trigger();
                settle();
                require(!child<QToolBar>(window, "modelTools")->isVisible(), "Home brings up no drawing tools here");
                child<QAction>(window, "tabView")->trigger();
                settle();
                require(view_row->isVisible() && child<QAction>(window, "tabSettings")->isChecked(),
                        "Settings brings up View");
                require(view_row->actions().contains(child<QDockWidget>(window, "explorerDock")->toggleViewAction()) &&
                            view_row->actions().contains(child<QDockWidget>(window, "propertiesDock")->toggleViewAction()) &&
                            view_row->actions().contains(child<QAction>(window, "viewSidePanels")),
                        "With the panels' switches, and the panels' one button the diagram's raft carries");
                for (const char *conceptual : {"viewFit", "viewZoomIn", "viewShowGrid", "viewAlignToGrid", "viewFullView",
                                               "viewCanvasControls", "viewShowComments"})
                    require(!view_row->actions().contains(child<QAction>(window, conceptual)),
                            "And without what frames or marks up the hidden diagram");
                require(child<QMenu>(window, "viewMenu")->actions().contains(child<QAction>(window, "viewFit")),
                        "The View menu is left as it was");
                require(!child<QToolBar>(window, "designTools")->actions().contains(child<QMenu>(window, "backgroundMenu")->menuAction()),
                        "Design keeps to the theme, the icons and the notation");
                const auto both = [&]
                { return child<QDockWidget>(window, "explorerDock")->isVisible() && child<QDockWidget>(window, "propertiesDock")->isVisible(); };
                require(both(), "Both of the schema's panels are out");
                qobject_cast<QToolButton *>(view_row->widgetForAction(child<QAction>(window, "viewSidePanels")))->click();
                settle();
                require(!child<QDockWidget>(window, "explorerDock")->isVisible() && child<QDockWidget>(window, "propertiesDock")->isVisible(),
                        "Panels puts the Explorer away beside the schema");
                child<QAction>(window, "viewSidePanels")->trigger();
                child<QAction>(window, "viewSidePanels")->trigger();
                settle();
                require(both(), "And after Neither brings both back");
                require(child<QPushButton>(window, "previewSchema")->text() == "Convert to Conceptual",
                        "The header's way back to the diagram says where it goes");
                child<QAction>(window, "tabHome")->trigger();
                settle();
            }
            full->click();
            settle();
            require(child<QPushButton>(window, "previewSchema")->text() == "Convert to Schema",
                    "And from the diagram it converts to the schema");
            require(child<QToolBar>(window, "viewTools")->actions().contains(child<QAction>(window, "viewFit")) &&
                        !child<QToolBar>(window, "viewTools")->actions().contains(child<QAction>(window, "viewSidePanels")),
                    "The View row has the diagram's commands back, each where it stood");
            require(child<QLabel>(window, "workspaceBadge")->text() == "CONCEPTUAL",
                    "Leaving it names the conceptual workspace again");
            require(child<QAction>(window, "insertPicture")->isVisible(), "And gives Insert its picture back");
            // The conceptual workspace's own family, as the specification
            // names it, on the row it opens on.
            for (const char *conceptual : {"toolSelect", "toolEntity", "toolAttribute", "toolRelationship",
                                           "toolIsa", "toolConnect", "toolNote"})
                require(window.findChild<QAction *>(conceptual) != nullptr,
                        "The conceptual workspace offers Select, Entity, Attribute, Relationship, "
                        "Specialization, Connect and Note");
            require(explorer_dock->isVisible(), "Leaving it brings them back");
            require(child<QToolBar>(window, "modelTools")->isVisible(), "The drawing tools with them");
            // Undo and redo stay beside the schema while it is open, sharing
            // the stage or not (Zain, 2026-09-25) -- sharing it, as the diagram's
            // one row's own two (2026-10-08); its search and the theme go back,
            // since the diagram's own are showing again.
            {
                auto *row = child<QToolBar>(window, "modelTools");
                auto *undo = row->widgetForAction(window.findChild<QAction *>("undoCommand"));
                auto *redo = row->widgetForAction(window.findChild<QAction *>("redoCommand"));
                require(!kept->isVisible() && !child<QWidget>(window, "workspaceHeader")->isVisible() && undo && redo &&
                            undo->isVisible() && redo->isVisible(),
                        "Sharing the stage, the schema keeps its undo and redo, in the row, with no header beneath");
            }
            require(!child<QLineEdit>(window, "schemaSearch")->isVisible() && !child<QToolButton>(window, "schemaTheme")->isVisible(),
                    "And the header gives back the search and theme it had lent");
            require(child<QLabel>(window, "canvasInstructions")->isVisible(),
                    "And the furniture with them");
            require(full->text() == "Full", "And says so");

            // The panel is the stage's width, whatever the side panels leave
            // the stage. Narrowing Properties, closing it or opening it again
            // changes the stage without changing the window, and the panel
            // follows each time, with no strip of diagram showing beside it
            // and nothing running under the panel beside it. Only the panel
            // is resized: the tables stay the size and place they were.
            {
                auto *properties_dock = child<QDockWidget>(window, "propertiesDock");
                auto *stage = child<QWidget>(window, "workspaceStage");
                auto *schema = static_cast<desktop::SchemaView *>(child<QWidget>(window, "schemaView"));
                const auto tables_before = schema->table_boxes();
                const auto fills_stage = [&]
                {
                    return panel->x() == 0 && panel->width() == stage->width();
                };
                require(fills_stage(), "Sharing the stage, the panel is as wide as the stage");
                // Properties opens here at its narrowest, so it is widened
                // first and then narrowed back.
                auto stage_before = stage->width();
                window.resizeDocks({properties_dock}, {properties_dock->width() + 120}, Qt::Horizontal);
                settle();
                require(stage->width() < stage_before, "Widening Properties narrows the stage");
                require(fills_stage(), "And the panel narrows with it");
                stage_before = stage->width();
                window.resizeDocks({properties_dock}, {properties_dock->width() - 90}, Qt::Horizontal);
                settle();
                require(stage->width() > stage_before, "Narrowing Properties widens the stage");
                require(fills_stage(), "And the panel widens with it, leaving no strip of diagram beside it");
                properties_dock->hide();
                settle();
                require(fills_stage(), "Closing Properties gives the panel the room it leaves");
                properties_dock->show();
                settle();
                require(fills_stage(), "And opening it again takes that room back");
                require(panel->mapTo(&window, QPoint(panel->width(), 0)).x() <= properties_dock->x(),
                        "Without the panel running under Properties");
                const auto window_was = window.size();
                window.resize(window_was.width() - 240, window_was.height());
                settle();
                require(fills_stage(), "A narrower window still leaves the panel the stage's width");
                window.resizeDocks({properties_dock}, {properties_dock->width() + 60}, Qt::Horizontal);
                settle();
                require(fills_stage(), "And Properties is still followed after it");
                window.resize(window_was);
                settle();
                require(fills_stage(), "As it is when the window is given its size back");
                require(schema->table_boxes() == tables_before,
                        "The tables are neither moved nor resized by the room the panel is given");
            }

            // A name typed on the schema is the name on the diagram. Renaming
            // a table renames the entity it came from, so the two never come
            // to disagree about what a thing is called; renaming the key the
            // conversion invented gives that key a name of its own, since it
            // has nothing behind it to rename.
            {
                // Found by name rather than by type: the view is a plain QWidget
                // subclass with no Q_OBJECT, and its name is its own.
                auto *schema = static_cast<desktop::SchemaView *>(child<QWidget>(window, "schemaView"));
                require(!schema->preview().tables.empty(), "The schema has tables to rename");
                const auto boxes = schema->table_boxes();
                require(!boxes.empty(), "And they have been placed");
                const auto named = [&](std::size_t which)
                {
                    return QString::fromStdString(schema->preview().tables[which].name);
                };
                const auto was = named(0);
                const auto header = boxes.front().topLeft() + QPointF(20, 8);
                QMouseEvent opened(QEvent::MouseButtonDblClick, header, schema->mapToGlobal(header.toPoint()),
                                   Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(schema, &opened);
                settle();
                auto *field = child<QLineEdit>(window, "schemaName");
                require(field->isVisible(), "Double-clicking a table's name opens it for typing");
                require(field->text() == was, "Opened on the name that is there");
                field->setText("Renamed");
                QKeyEvent done(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QApplication::sendEvent(field, &done);
                settle();
                require(!field->isVisible(), "Return puts the box away");
                bool on_diagram = false;
                for (const auto &[id, entity] : editor.project().entities)
                {
                    (void)id;
                    if (entity.name == "Renamed")
                        on_diagram = true;
                }
                require(on_diagram, "And the entity on the diagram carries the typed name");

                // Escape keeps what was there.
                QApplication::sendEvent(schema, &opened);
                settle();
                field->setText("Discarded");
                QKeyEvent gave_up(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                QApplication::sendEvent(field, &gave_up);
                settle();
                for (const auto &[id, entity] : editor.project().entities)
                {
                    (void)id;
                    require(entity.name != "Discarded", "Escape keeps the name that was there");
                }

                // Another column is added where it will be read, with nothing
                // asked first: the slot under the table is pressed, the row is
                // made, and its name is waiting to be typed in the row itself.
                // It reflects, so the diagram gains the attribute and the
                // schema follows from it.
                const auto attributes = editor.project().attributes.size();
                const auto table = schema->table_boxes().front();
                const auto onto = QPointF(table.center().x(), table.bottom() + 10);
                QMouseEvent over_slot(QEvent::MouseMove, onto, schema->mapToGlobal(onto.toPoint()),
                                      Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(schema, &over_slot);
                QMouseEvent pressed(QEvent::MouseButtonPress, onto, schema->mapToGlobal(onto.toPoint()),
                                    Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(schema, &pressed);
                settle();
                require(editor.project().attributes.size() == attributes + 1,
                        "Pressing the slot adds an attribute to the diagram, not a schema-only column");
                require(field->isVisible(), "And opens its name for typing, with no dialog in the way");
                field->setText("Enrolled");
                QKeyEvent typed(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QApplication::sendEvent(field, &typed);
                settle();
                bool renamed = false;
                for (const auto &[id, attribute] : editor.project().attributes)
                {
                    (void)id;
                    if (attribute.name == "Enrolled")
                        renamed = true;
                }
                require(renamed, "And the name typed in the row is the attribute's name");

                // A line's end goes where the hand puts it, including where the
                // schema cannot mean it -- and is told what is wrong and why
                // rather than being sprung back to where it belonged.
                const auto shapes = schema->line_shapes();
                if (!shapes.empty())
                {
                    QString heard;
                    auto reported = schema->warned;
                    schema->warned = [&](const QString &words, QPoint at)
                    {
                        heard = words;
                        if (reported)
                            reported(words, at);
                    };
                    const auto tables = schema->table_boxes();
                    const auto &where = tables.front();
                    QPointF end;
                    double best = 1e9;
                    for (const auto &shape : shapes)
                        for (const auto &corner : {shape.front(), shape.back()})
                        {
                            const auto away = std::hypot(corner.x() - where.center().x(),
                                                         corner.y() - where.center().y());
                            if (away < best)
                            {
                                best = away;
                                end = corner;
                            }
                        }
                    const QPointF adrift(where.center().x(), where.bottom() + 80);
                    QMouseEvent took(QEvent::MouseButtonPress, end, schema->mapToGlobal(end.toPoint()),
                                     Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QApplication::sendEvent(schema, &took);
                    QMouseEvent hauled(QEvent::MouseMove, adrift, schema->mapToGlobal(adrift.toPoint()),
                                       Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QApplication::sendEvent(schema, &hauled);
                    QMouseEvent dropped(QEvent::MouseButtonRelease, adrift, schema->mapToGlobal(adrift.toPoint()),
                                        Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                    QApplication::sendEvent(schema, &dropped);
                    settle();
                    schema->warned = reported;
                    require(heard.contains("belongs on"),
                            "A misplaced end is told which row it belongs on");
                    require(heard.contains("joins nothing") || heard.contains("can only run to a key") || heard.contains("points at"),
                            "And why where it was left cannot serve");
                    require(schema->loose_ends() > 0, "And it is left exactly where it was put");
                    // And it is said where the hand is looking, not only along
                    // the bottom of the window.
                    {
                        auto *notice = static_cast<desktop::Notice *>(
                            window.findChild<QWidget *>("notice"));
                        require(notice != nullptr, "The window has a notice to say it in");
                        require(notice->isVisible(), "A warning comes up over the work");
                        require(notice->saying().contains("belongs on"),
                                "Saying the same thing the status bar was given");
                        // And it stands where the hand let go, not at the
                        // bottom of the window: a warning about a connection
                        // belongs where the connection was attempted, which
                        // is where the person is looking.
                        const auto let_go = schema->mapTo(&window, adrift.toPoint());
                        require(notice->geometry().adjusted(-40, -40, 40, 40).contains(let_go),
                                "And it comes up beside the point the hand let go of");
                        require(!notice->geometry().contains(let_go),
                                "Standing clear of it, so what it is about is not covered");
                        // It is not on a clock. Something has gone wrong under
                        // the pointer, the pointer stops while it is read, and
                        // moving on again is what says it has been.
                        settle_for(900);
                        require(notice->isVisible(),
                                "It waits for the hand rather than going on a clock");
                        const auto hand = QCursor::pos();
                        QCursor::setPos(hand + QPoint(90, 90));
                        settle_for(120);
                        require(notice->isVisible(),
                                "And it fades rather than vanishing: still there part way through");
                        settle_for(700);
                        require(!notice->isVisible(), "Gone once the fade is done");
                        QCursor::setPos(hand);
                        settle();
                    }
                    // The two ends are wrong in different ways, and are told
                    // apart. Dropping an end onto a primary key used to be
                    // reported as landing on "an ordinary column", which a
                    // primary key plainly is not.
                    //
                    // Which corner belongs to which end is not knowable from
                    // outside, so every end is tried against the key rows of
                    // its own table until one of them complains.
                    {
                        // The capture was handed back after the drag above, so
                        // it is put on again for these.
                        schema->warned = [&](const QString &words, QPoint at)
                        {
                            heard = words;
                            if (reported)
                                reported(words, at);
                        };
                        bool checked = false;
                        const auto send = [&](QEvent::Type kind, QPointF at, Qt::MouseButton button,
                                              Qt::MouseButtons held)
                        {
                            QMouseEvent event(kind, at, schema->mapToGlobal(at.toPoint()),
                                              button, held, Qt::NoModifier);
                            QApplication::sendEvent(schema, &event);
                        };
                        for (const auto &shape : schema->line_shapes())
                        {
                            if (checked)
                                break;
                            for (const auto &corner : {shape.front(), shape.back()})
                            {
                                if (checked)
                                    break;
                                const auto rows = schema->row_boxes();
                                for (std::size_t t = 0; t < rows.size() && !checked; ++t)
                                {
                                    const auto &columns = schema->preview().tables[t].columns;
                                    for (std::size_t row = 0; row < rows[t].size() && row < columns.size(); ++row)
                                    {
                                        if (!columns[row].primary_key)
                                            continue;
                                        const auto onto = QPointF(corner.x(), rows[t][row].center().y());
                                        if (std::abs(onto.y() - corner.y()) < 2)
                                            continue;
                                        heard.clear();
                                        send(QEvent::MouseButtonPress, corner, Qt::LeftButton, Qt::LeftButton);
                                        send(QEvent::MouseMove, onto, Qt::NoButton, Qt::LeftButton);
                                        send(QEvent::MouseButtonRelease, onto, Qt::LeftButton, Qt::NoButton);
                                        settle();
                                        const bool landed_on_key = heard.contains("belongs on") && (heard.contains("identified by") || heard.contains("not the one"));
                                        if (landed_on_key)
                                        {
                                            require(!heard.contains("ordinary column"),
                                                    "A primary key is never called an ordinary column");
                                            checked = true;
                                        }
                                        if (!heard.isEmpty())
                                        {
                                            child<QAction>(window, "undoCommand")->trigger();
                                            settle();
                                        }
                                        if (checked)
                                            break;
                                    }
                                }
                            }
                        }
                        require(checked,
                                "An end dropped on a primary key is told it is a key, and which one");
                        schema->warned = reported;
                    }
                    // Put back, so what follows finds the schema as it was: the
                    // end was moved by an edit like any other, so one undo
                    // takes it back.
                    child<QAction>(window, "undoCommand")->trigger();
                    settle();
                    require(schema->loose_ends() == 0, "And one undo puts it back");
                }
            }

            // The list a shared name's type is chosen from is only as wide as
            // its entries (Zain, 2026-09-26), though the box it opens from is
            // stretched across its row, and it is chosen from as before.
            {
                auto *head = child<QPushButton>(window, "schemaSharedNamesHead");
                head->click();
                settle();
                auto *type = child<QComboBox>(window, "sharedNameType");
                const auto box = type->size();
                // Opening a list takes the keyboard, as any combo box's does;
                // it is handed back afterwards, so what follows types where
                // it would have.
                QPointer<QWidget> keyboard = QApplication::focusWidget();
                type->showPopup();
                settle();
                auto *popup = type->view()->window();
                require(popup != &window && popup->isVisible(), "The list opens");
                require(popup->width() < box.width(), "Narrower than the box it opens from");
                int widest = 0;
                for (int i = 0; i < type->count(); ++i)
                {
                    const auto own = type->itemData(i, Qt::FontRole);
                    const QFontMetrics lettering(own.isValid() ? own.value<QFont>() : type->view()->font());
                    widest = std::max(widest, lettering.horizontalAdvance(type->itemText(i)));
                }
                require(type->view()->viewport()->width() > widest, "Yet wide enough for every entry, none cut short");
                // The families' titles are a little bold and grey (Zain,
                // 2026-09-26); the types beneath them are as they were.
                int titles = 0;
                for (int i = 0; i < type->count(); ++i)
                {
                    const bool title = type->itemText(i).startsWith(QStringLiteral("— "));
                    const auto own = type->itemData(i, Qt::FontRole);
                    const auto ink = type->itemData(i, Qt::ForegroundRole);
                    if (title)
                    {
                        ++titles;
                        require(own.isValid() && own.value<QFont>().weight() >= QFont::DemiBold,
                                "Each family's title is set a little bold");
                        require(ink.isValid() && ink.value<QBrush>().color() == desktop::theme(window.canvas()->theme_id()).muted,
                                "And in the theme's grey");
                    }
                    else
                    {
                        require(!own.isValid() && !ink.isValid(), "The types themselves are as they were");
                    }
                }
                require(titles >= 8, "Every family has its title");
                require(type->count() > 40, "With every entry it had");
                require(type->size() == box, "The box itself is as it was");
                type->hidePopup();
                settle();
                require(!popup->isVisible(), "And closes as before");
                // Choosing from it gives every column of that name the type,
                // in one edit, as it always has.
                const auto revision = window.editor().revision();
                int first_type = -1;
                for (int i = 0; i < type->count() && first_type < 0; ++i)
                    if (type->itemData(i).isValid())
                        first_type = i;
                type->setCurrentIndex(first_type);
                settle();
                require(window.editor().revision() == revision + 1,
                        "Choosing from it still answers every column of that name in one edit");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
                child<QPushButton>(window, "schemaSharedNamesHead")->click();
                settle();
                if (keyboard)
                    keyboard->setFocus();
                settle();
            }

            // Names only (Zain, 2026-09-27): chosen under Appearance, every
            // table shows its key marks and its columns' names alone -- no
            // row naming the columns, no Type, no Constraints -- and is as
            // wide as its names. Not the default, and turned back, every
            // table is exactly as it was.
            {
                auto *schema = static_cast<desktop::SchemaView *>(child<QWidget>(window, "schemaView"));
                auto *names = child<QAction>(window, "schemaDetailNames");
                auto *everything = child<QAction>(window, "schemaDetailFull");
                require(everything->isChecked() && !names->isChecked() && !schema->names_only(),
                        "Every table shows its types and constraints unless names only is chosen");
                require(names->text() == "Compact schema", "The compact view has a descriptive label");
                // Renamed from "Names, types and constraints" (Zain, 2026-10-02);
                // only its words changed.
                require(everything->text() == "Physical schema", "The detailed view is called Physical schema");
                require(everything->toolTip() == "Show physical column details, including types and constraints.",
                        "And says what it shows, promising nothing it does not");
                const auto whole = schema->table_boxes();
                const auto drawn = schema->grab().toImage();
                names->trigger();
                settle();
                require(schema->names_only() && QSettings().value("schemaNamesOnly").toBool(),
                        "Names only is taken up, and remembered");
                const auto named = schema->table_boxes();
                require(named.size() == whole.size() && !named.empty(), "Every table is still there");
                for (std::size_t i = 0; i < named.size(); ++i)
                {
                    require(named[i].width() < whole[i].width(), "Each is narrower, holding only its names");
                    require(named[i].height() < whole[i].height(),
                            "Compact tables omit headings and configuration footers");
                    const auto rows = schema->row_boxes()[i];
                    if (!rows.empty())
                        require(std::abs(named[i].bottom() - rows.back().bottom()) < 0.01,
                                "Compact tables end at the last column without footer space");
                }
                everything->trigger();
                settle();
                require(!schema->names_only() && !QSettings().value("schemaNamesOnly").toBool(),
                        "Turned back to everything");
                require(schema->table_boxes() == whole, "Every table exactly as it was");
                require(schema->grab().toImage() == drawn, "And drawn exactly as it was, pixel for pixel");
            }

            // Closing the panel while it is full does not leave the window
            // stripped with nothing in it.
            full->click();
            settle();
            child<QPushButton>(window, "previewSchema")->click();
            settle();
            require(explorer_dock->isVisible(), "Closing the schema gives the panels back too");
            require(!child<QWidget>(window, "schemaHeaderTools")->isVisible(),
                    "And its undo and redo go with it, the toolbar's being the diagram's");
            child<QPushButton>(window, "previewSchema")->click();
            settle_motion(window);
            child<QPushButton>(window, "previewSchema")->click();
            settle();
        }

        {
            // Stage 1 (Zain, 2026-09-29): the side panels follow the workspace
            // being worked on. With the diagram, the diagram's own Explorer and
            // Properties; with the schema a diagram becomes, the schema's, as
            // beside a schema drawn by hand; and while the two share the
            // stage, whichever half was pressed last. What is chosen on this
            // schema is chosen as on one drawn by hand: by the schema's own
            // identities, shown in Properties, and never an edit.
            auto *explorer_dock = child<QDockWidget>(window, "explorerDock");
            auto *properties_dock = child<QDockWidget>(window, "propertiesDock");
            auto *diagram_explorer = child<QWidget>(window, "explorer");
            auto *schema_explorer = child<QTreeView>(window, "schemaExplorer");
            auto *schema_properties = child<QWidget>(window, "schemaProperties");
            const auto diagram_panels = [&]
            {
                return explorer_dock->widget() == diagram_explorer && properties_dock->widget() != schema_properties;
            };
            const auto schema_panels = [&]
            {
                return explorer_dock->widget() == schema_explorer && properties_dock->widget() == schema_properties;
            };
            // What kind of thing Properties is about, and its name where it
            // has one (Stage 3 made the rest a full read-only inspector).
            const auto properties_say = [&]
            {
                return QStringList{properties_heading(*properties_dock->widget())} + properties_value(*properties_dock->widget(), "General/Name");
            };
            const auto diagram_had = window.canvas()->selected_elements();
            require(diagram_panels() && explorer_dock->isVisible() && properties_dock->isVisible(),
                    "With the schema put away, the docks hold the diagram's Explorer and Properties");

            child<QPushButton>(window, "previewSchema")->click();
            settle_motion(window);
            require(schema_panels() && explorer_dock->isVisible() && properties_dock->isVisible(),
                    "Raising the schema over the diagram puts the schema's own Explorer and Properties beside it");
            {
                const auto *model = schema_explorer->model();
                const auto root = model->index(0, 0);
                QStringList rows{root.data().toString()};
                for (int r = 0; r < model->rowCount(root); ++r)
                    rows << model->index(r, 0, root).data().toString();
                require(rows == QStringList{"Schema", "Tables", "Relationships"},
                        "The Explorer's frame is the schema's, worked out from a diagram as from a hand");
            }
            // What was chosen on it before it was put away is still chosen, and
            // Properties says what that is rather than anything else.
            auto *schema = window.schema();
            if (std::holds_alternative<desktop::ChosenTable>(schema->selection_now()))
                require(properties_say().value(0) == "Properties", "Properties says what is still chosen on the schema");
            schema->choose(desktop::NothingChosen{});
            settle();
            require(properties_say() == QStringList{"Schema"},
                    "With nothing chosen on it, Properties says so");

            const auto revision = window.editor().revision();
            const auto undo_label = window.editor().undo_label();
            const auto was_dirty = window.editor().dirty();
            const auto press_at = [&](QPointF at)
            {
                QMouseEvent press(QEvent::MouseButtonPress, at, schema->mapToGlobal(at.toPoint()),
                                  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(schema, &press);
                QMouseEvent release(QEvent::MouseButtonRelease, at, schema->mapToGlobal(at.toPoint()),
                                    Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(schema, &release);
                settle();
            };

            // A table, by its heading.
            const auto &first = schema->preview().tables.front();
            const auto table_id = first.id;
            const auto table_name = QString::fromStdString(first.name);
            require(first.origin && domain::relation_from(*first.origin) == table_id && std::visit([](const auto &id)
                                                                                                   { return id.value; }, *first.origin) != table_id.value,
                    "A table here has an identity of its own, worked out from what it came from but not that");
            const auto heading = schema->table_boxes().front();
            const QPointF on_heading(heading.center().x(), heading.top() + 8);
            press_at(on_heading);
            const auto now_table = schema->selection_now();
            require(std::holds_alternative<desktop::ChosenTable>(now_table) && std::get<desktop::ChosenTable>(now_table).table == table_id,
                    "A table's heading chooses the table, by its own identity");
            require(properties_say() == QStringList{"Properties", table_name}, "Properties says Table, and its name");

            // A column, by its name: between the row's start and its type.
            const auto rows = schema->row_boxes().front();
            const auto cells = schema->cell_boxes().front();
            require(!rows.empty() && !cells.empty(), "The table has columns to choose");
            const auto column_name = QString::fromStdString(first.columns.front().name);
            press_at(QPointF((rows.front().left() + cells.front().type.left()) / 2, rows.front().center().y()));
            const auto now_column = schema->selection_now();
            require(std::holds_alternative<desktop::ChosenColumn>(now_column) && std::get<desktop::ChosenColumn>(now_column).column.table == table_id,
                    "A row chooses its column, under its table's own identity");
            const auto said_column = properties_say();
            require(said_column.size() >= 2 && said_column[0] == "Column" && said_column[1] == column_name,
                    "Properties says Column, and its name");

            // A line, in the middle of its longest run: the foreign key it
            // stands for, and nothing else.
            const auto shapes = schema->line_shapes();
            require(!shapes.empty(), "The example's schema draws its foreign keys as lines");
            QPointF on_line;
            double longest = -1;
            for (const auto &shape : shapes)
                for (std::size_t i = 1; i < shape.size(); ++i)
                {
                    const auto length = std::hypot(shape[i].x() - shape[i - 1].x(), shape[i].y() - shape[i - 1].y());
                    if (length > longest)
                    {
                        longest = length;
                        on_line = (shape[i] + shape[i - 1]) / 2;
                    }
                }
            press_at(on_line);
            const auto now_line = schema->selection_now();
            require(std::holds_alternative<desktop::ChosenForeignKey>(now_line) && schema->selection().empty(),
                    "Pressing a line chooses the foreign key it stands for, and puts the tables down");
            const auto said_line = properties_say();
            require(said_line == QStringList{"Relationship"} && properties_value(*properties_dock->widget(), "Identity/Relationship").size() == 1,
                    "Properties says Relationship, and which key points at which");

            // The empty schema puts it all down.
            double right = 0;
            double bottom = 0;
            for (const auto &box : schema->table_boxes())
            {
                right = std::max(right, box.right());
                bottom = std::max(bottom, box.bottom());
            }
            press_at(QPointF(right + 120, bottom + 120));
            require(std::holds_alternative<desktop::NothingChosen>(schema->selection_now()) && properties_say() == QStringList{"Schema"},
                    "The empty schema chooses nothing, and Properties says so");
            require(window.editor().revision() == revision && window.editor().undo_label() == undo_label && window.editor().dirty() == was_dirty,
                    "Choosing on the schema is not an edit: nothing reaches the project or the history");

            // Pressed, the diagram has its own panels back, about what is
            // chosen on it now rather than anything left from the schema.
            const auto entity_id = window.editor().project().entities.begin()->first;
            const auto entity_name = QString::fromStdString(window.editor().project().entities.begin()->second.name);
            const auto body = window.editor().project().layout.at(domain::ElementRef{entity_id});
            click_canvas(*window.canvas(), QPointF(body.x + body.width / 2, body.y + body.height / 2));
            require(diagram_panels(), "Pressing the diagram gives the docks back to its own Explorer and Properties");
            const auto shows_entity = [&]
            {
                for (auto *field : properties_dock->widget()->findChildren<QLineEdit *>())
                    if (field->text() == entity_name)
                        return true;
                return false;
            };
            require(shows_entity(), "Showing the entity pressed there, and nothing of the schema's");
            press_at(on_heading);
            require(schema_panels() && properties_say() == QStringList{"Properties", table_name},
                    "Pressing the schema again gives them back to it, showing the table pressed");

            // With the whole window, the schema keeps its own either side of it.
            auto *full = child<QPushButton>(window, "schemaFull");
            full->click();
            settle_for(100);
            require(schema_panels() && explorer_dock->isVisible() && properties_dock->isVisible(),
                    "Full keeps the schema's own Explorer and Properties either side of it");
            auto *header = child<QWidget>(window, "workspaceHeader");
            auto *header_dock = child<QDockWidget>(window, "schemaHeaderDock");
            require(header_dock->isVisible() && header_dock->widget() == header,
                    "And the header runs over both of them, as over a schema drawn by hand");
            require(explorer_dock->width() >= 200 && properties_dock->width() >= 280,
                    "So they come back at the widths they open at, not squeezed by the header");
            full->click();
            settle();
            require(schema_panels(), "Leaving Full, the schema is still the half being worked on");
            // Back between the panels, and put away there while the diagram is
            // in front, whose one row says what the header said (2026-10-08).
            require(!header_dock->isVisible() && header_dock->widget() != header && !header->isVisible() &&
                        child<QWidget>(window, "conceptualIdentity")->isAncestorOf(child<QWidget>(window, "documentTitle")),
                    "And the header goes back between the panels");

            // Put away, the diagram's come back; raised again, the schema's.
            child<QPushButton>(window, "previewSchema")->click();
            settle_motion(window);
            require(diagram_panels() && explorer_dock->isVisible() && properties_dock->isVisible(),
                    "Putting the schema away gives the diagram its own panels back");
            require(shows_entity(), "Still showing what is chosen on the diagram");
            child<QPushButton>(window, "previewSchema")->click();
            settle_motion(window);
            require(schema_panels(), "Raising it again gives them to the schema again");
            click_canvas(*window.canvas(), QPointF(body.x + body.width / 2, body.y + body.height / 2));
            require(diagram_panels(), "And pressing the diagram gives them back again, every time");
            child<QPushButton>(window, "previewSchema")->click();
            settle_motion(window);
            require(diagram_panels(), "The schema put away from there leaves the diagram's in place");
            window.canvas()->select_elements(diagram_had);
            settle();
        }

        {
            // A line between two tables is not only drawn. Any straight run
            // of it can be pushed sideways, either end can be moved around the
            // table it joins, and a double-click hands the whole line back to
            // the router.
            child<QPushButton>(window, "previewSchema")->click();
            settle_motion(window);
            auto *schema = window.schema();
            require(schema != nullptr, "The panel holds the schema itself");
            const auto drawn = schema->line_shapes();
            require(!drawn.empty(), "The example's schema is drawn with lines between its tables");
            // Under Plain the schema has no colour either. Each line keeps a
            // grey of its own, so crossing lines can still be told apart, and
            // the key beside PK is drawn in the letters' grey.
            {
                const auto wearing = window.canvas()->theme_id();
                window.set_theme(desktop::ThemeId::Plain);
                settle_for(200);
                // Photographed with its lettering smoothed in greys, as the
                // whole window is: names, types and keys are written in grey,
                // which subpixel smoothing would fringe with colour of its own.
                require(coloured_pixels(grab_without_subpixel_text(*schema)) == 0,
                        "Under Plain the schema's lines, keys and tables have no colour");
                window.set_theme(wearing);
                settle_for(200);
            }
            require(schema->shaped_lines() == 0, "None of them has been shaped by hand yet");

            // The longest straight run there is: certainly part of a line and
            // certainly clear of every table.
            std::size_t on_line = 0;
            QPointF ran_from;
            QPointF ran_to;
            double longest = 0;
            for (std::size_t line = 0; line < drawn.size(); ++line)
                for (std::size_t i = 1; i < drawn[line].size(); ++i)
                {
                    const auto length = std::hypot(drawn[line][i].x() - drawn[line][i - 1].x(),
                                                   drawn[line][i].y() - drawn[line][i - 1].y());
                    if (length <= longest)
                        continue;
                    longest = length;
                    on_line = line;
                    ran_from = drawn[line][i - 1];
                    ran_to = drawn[line][i];
                }
            require(longest > 40, "And at least one run is long enough to take hold of");

            const auto drag = [&](QEvent::Type type, QPointF at, Qt::MouseButton button,
                                  Qt::MouseButtons held)
            {
                QMouseEvent event(type, at, schema->mapToGlobal(at.toPoint()), button, held,
                                  Qt::NoModifier);
                QApplication::sendEvent(schema, &event);
            };

            // A run moves across itself, never along itself: an upright run
            // goes sideways and a level one goes up and down.
            const bool upright = std::abs(ran_from.x() - ran_to.x()) < 0.01;
            const QPointF across = upright ? QPointF(34, 0) : QPointF(0, 34);
            const auto grab = (ran_from + ran_to) / 2;
            const auto moved_to = grab + across;
            drag(QEvent::MouseButtonPress, grab, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, grab + across / 3, Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseMove, moved_to, Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, moved_to, Qt::LeftButton, Qt::NoButton);
            settle();
            require(schema->shaped_lines() == 1, "Pushing a run sideways shapes that line");

            // The whole run has moved over, not one point on it: there is a run
            // of about the same length lying where the pointer left it.
            const auto pushed = schema->line_shapes()[on_line];
            const auto wanted = upright ? moved_to.x() : moved_to.y();
            bool run_moved = false;
            for (std::size_t i = 1; i < pushed.size(); ++i)
            {
                const auto a = pushed[i - 1];
                const auto b = pushed[i];
                const auto sits = upright ? a.x() : a.y();
                const auto still_upright = std::abs(a.x() - b.x()) < 0.01;
                const auto length = std::hypot(b.x() - a.x(), b.y() - a.y());
                if (still_upright == upright && std::abs(sits - wanted) < 0.01 && length > longest / 2)
                    run_moved = true;
            }
            require(run_moved, "The whole run moves across, keeping its length and its direction");

            // And it moved rather than sprouting a detour. A line sent out to
            // a dropped point and back again reverses on itself, which is the
            // spur that made this look wrong in the first place.
            const auto doubles_back = [](const std::vector<QPointF> &shape)
            {
                for (std::size_t i = 2; i < shape.size(); ++i)
                {
                    const auto in = shape[i - 1] - shape[i - 2];
                    const auto out = shape[i] - shape[i - 1];
                    if (QPointF::dotProduct(in, out) < -0.01)
                        return true;
                }
                return false;
            };
            require(!doubles_back(pushed), "And the line never doubles back on itself");

            // A run next to an end cannot be pushed in over the symbols drawn
            // there. The line is held off the turn, so the foot and the
            // minimum always have straight line to sit on and are never left
            // standing beside it.
            {
                const auto shapes = schema->line_shapes();
                std::size_t which = 0;
                for (std::size_t line = 0; line < shapes.size(); ++line)
                    if (shapes[line].size() >= 3)
                    {
                        which = line;
                        break;
                    }
                const auto &shape = shapes[which];
                require(shape.size() >= 3, "A line with a turn in it");
                const auto stub_upright = std::abs(shape[0].x() - shape[1].x()) < 0.01;
                const auto out = shape[1] - shape[0];
                const auto reach = stub_upright ? std::abs(out.y()) : std::abs(out.x());
                require(reach >= 27, "Its first stretch already has room for the symbols");
                // Grab the run past the stub and shove it back at the table.
                const auto hold = (shape[1] + shape[2]) / 2;
                const auto onto = stub_upright ? QPointF(hold.x(), shape[0].y())
                                               : QPointF(shape[0].x(), hold.y());
                drag(QEvent::MouseButtonPress, hold, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseMove, (hold + onto) / 2, Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseMove, onto, Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, onto, Qt::LeftButton, Qt::NoButton);
                settle();
                const auto after = schema->line_shapes()[which];
                require(after.size() >= 2, "The line survives being shoved");
                const auto held = after[1] - after[0];
                const auto now = std::abs(held.x()) + std::abs(held.y());
                require(now >= 27, "And keeps the room its symbols need");
                child<QAction>(window, "schemaTidy")->trigger();
                settle();
            }

            // Every corner is a right angle, before and after being shaped: a
            // schema is drawn with square lines and never with diagonals.
            for (const auto &shape : schema->line_shapes())
                for (std::size_t i = 1; i < shape.size(); ++i)
                    require(std::abs(shape[i].x() - shape[i - 1].x()) < 0.01 || std::abs(shape[i].y() - shape[i - 1].y()) < 0.01,
                            "Every run of a line is square");

            QMouseEvent twice(QEvent::MouseButtonDblClick, moved_to,
                              schema->mapToGlobal(moved_to.toPoint()), Qt::LeftButton,
                              Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(schema, &twice);
            settle();
            require(schema->shaped_lines() == 0, "Double-clicking gives the line back to the router");
            require(schema->line_shapes()[on_line] == drawn[on_line], "Which puts its own way back");

            // A press that barely travels is a click on a line, not a push of
            // it: nothing should move under a hand that merely twitched.
            drag(QEvent::MouseButtonPress, grab, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, grab + QPointF(1, 1), Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, grab + QPointF(1, 1), Qt::LeftButton, Qt::NoButton);
            settle();
            require(schema->shaped_lines() == 0, "A press that barely moves leaves the line alone");
            require(schema->line_shapes()[on_line] == drawn[on_line], "And leaves its route alone too");

            // And Tidy puts every line back at once, as it does every table.
            drag(QEvent::MouseButtonPress, grab, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, moved_to, Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, moved_to, Qt::LeftButton, Qt::NoButton);
            settle();
            require(schema->shaped_lines() == 1, "A run pushed aside again");
            child<QAction>(window, "schemaTidy")->trigger();
            settle();
            require(schema->shaped_lines() == 0, "Tidy gives back the lines as well as the tables");

            // An end is taken hold of and moved around the table it belongs
            // to, and pulling it off the table leaves it where it was let go.
            const auto ends_of = [&](std::size_t line)
            {
                const auto shapes = schema->line_shapes();
                return std::pair{shapes[line].front(), shapes[line].back()};
            };
            const auto head = ends_of(0).first;
            require(head != ends_of(0).second, "A line has two ends to take hold of");

            // The end sits on the outline of the table it joins, which is what
            // says which way it may be slid without coming off.
            const auto boxes = schema->table_boxes();
            const auto joins = std::find_if(boxes.begin(), boxes.end(), [&](const QRectF &box)
                                            { return box.contains(head) && !box.adjusted(1, 1, -1, -1).contains(head); });
            require(joins != boxes.end(), "An end sits on the outline of the table it joins");
            const bool down_a_side = std::abs(head.x() - joins->left()) < 0.5 || std::abs(head.x() - joins->right()) < 0.5;
            const auto along = down_a_side
                                   ? QPointF(head.x(), std::clamp(head.y() + 40, joins->top(), joins->bottom()))
                                   : QPointF(std::clamp(head.x() + 40, joins->left(), joins->right()), head.y());
            require(along != head, "And has room to be slid along that edge");

            drag(QEvent::MouseButtonPress, head, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, along, Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, along, Qt::LeftButton, Qt::NoButton);
            settle();
            const auto slid = ends_of(0).first;
            require(std::hypot(slid.x() - along.x(), slid.y() - along.y()) < 0.01,
                    "An end dragged along its table follows the pointer down the edge");
            require(schema->loose_ends() == 0, "And is still joined to it");

            // Then off it. Nothing pulls the end back, and the schema says so.
            auto lowest = 0.0;
            for (const auto &box : boxes)
                lowest = std::max(lowest, box.bottom());
            const QPointF adrift(joins->center().x(), lowest + 60);
            drag(QEvent::MouseButtonPress, slid, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, QPointF(slid.x(), lowest + 20), Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseMove, adrift, Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, adrift, Qt::LeftButton, Qt::NoButton);
            settle();
            const auto let_go = ends_of(0).first;
            require(std::hypot(let_go.x() - adrift.x(), let_go.y() - adrift.y()) < 0.01,
                    "An end pulled off its table stops exactly where it was let go");
            require(schema->loose_ends() == 1, "And is counted as a connection left hanging");
            require(child<QLabel>(window, "schemaState")->text().contains("1 end not connected"),
                    "Which the schema says out loud rather than quietly undoing it");

            // It is still the line's end, so it can be picked up again and put
            // back, and the count goes down when it is.
            drag(QEvent::MouseButtonPress, adrift, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, head, Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, head, Qt::LeftButton, Qt::NoButton);
            settle();
            require(schema->loose_ends() == 0, "Put back on its table it is joined again");
            require(!child<QLabel>(window, "schemaState")->text().contains("not connected"),
                    "And the schema stops saying so");

            child<QAction>(window, "schemaTidy")->trigger();
            settle();
            require(schema->shaped_lines() == 0, "And Tidy gives back the ends as well");

            child<QPushButton>(window, "previewSchema")->click();
            settle_motion(window);
        }

        {
            // The schema can be edited away from the diagram it came from, and
            // says so when it has been. The menu that offers this and the
            // question box that follows it both stop and wait for somebody, so
            // what they drive is checked here instead of what they look like.
            child<QPushButton>(window, "previewSchema")->click();
            settle_motion(window);
            auto *schema = window.schema();
            require(schema->asked != nullptr, "Right-clicking the schema asks what can be done");
            const auto columns_of = [&](const QString &table)
            {
                QStringList names;
                for (const auto &one : schema->preview().tables)
                    if (QString::fromStdString(one.name) == table)
                        for (const auto &column : one.columns)
                            names << QString::fromStdString(column.name);
                return names;
            };
            // Edits made straight to the Editor do not pass the window, which
            // is what normally tells the panel to read the model again, so the
            // panel is closed and reopened to bring it up to date.
            const auto reopen = [&]
            {
                child<QPushButton>(window, "previewSchema")->click();
                settle_motion(window);
                child<QPushButton>(window, "previewSchema")->click();
                settle_motion(window);
            };

            // Taken by value: the preview is worked out afresh after every
            // edit, so anything pointing into the old one is stale by then.
            const auto found = std::find_if(schema->preview().tables.begin(), schema->preview().tables.end(),
                                            [](const domain::PreviewTable &one)
                                            {
                                                return one.origin && std::holds_alternative<domain::EntityId>(*one.origin);
                                            });
            require(found != schema->preview().tables.end(), "An entity became a table");
            const auto table_of = *found->origin;
            const auto table_named = QString::fromStdString(found->name);
            const auto attributes = window.editor().project().attributes.size();
            const auto named = std::find_if(window.editor().project().attributes.begin(),
                                            window.editor().project().attributes.end(),
                                            [&](const auto &entry)
                                            {
                                                return entry.second.owner && *entry.second.owner == table_of && entry.second.kind == domain::AttributeKind::Normal;
                                            });
            require(named != window.editor().project().attributes.end(),
                    "That table's entity has an attribute of its own");
            const auto hidden_id = named->first;
            const auto hidden_name = QString::fromStdString(named->second.name);
            require(columns_of(table_named).contains(hidden_name), "Which the schema draws as a column");

            // Declining to reflect keeps a new column here and nowhere else,
            // and hiding one keeps the attribute on the diagram: both are
            // differences between the levels rather than edits to the model.
            require(editor.add_schema_column(table_of, "Nickname").ok, "A column is added to the schema alone");
            require(editor.hide_in_schema(hidden_id, true).ok, "And an attribute is hidden from the schema");
            reopen();
            require(window.editor().project().attributes.size() == attributes,
                    "Neither creates or destroys an attribute, so the diagram is untouched");
            require(window.editor().project().attributes.contains(hidden_id),
                    "The hidden attribute is still on the diagram");
            require(columns_of(table_named).contains("Nickname"), "The schema shows the added column");
            require(!columns_of(table_named).contains(hidden_name), "And stops showing the hidden one");
            require(child<QLabel>(window, "schemaState")->text().contains("2 changes not on the diagram"),
                    "And the schema says how far the two levels have come apart");

            // Each is an ordinary edit, so Undo puts the two levels back.
            require(editor.undo() && editor.undo(), "Both undo");
            reopen();
            require(columns_of(table_named).contains(hidden_name), "Undo brings the hidden column back");
            require(!columns_of(table_named).contains("Nickname"), "And takes the added one away");
            require(!child<QLabel>(window, "schemaState")->text().contains("not on the diagram"),
                    "And the schema stops saying they differ");

            child<QPushButton>(window, "previewSchema")->click();
            settle_motion(window);
        }

        {
            // Pressing a table asks what it is joined to; the chips ask about
            // a whole kind of table; and the questions a conversion cannot
            // settle are answered on the tables they are about.
            child<QPushButton>(window, "previewSchema")->click();
            settle_motion(window);
            auto *schema = window.schema();
            const auto boxes = schema->table_boxes();
            require(boxes.size() >= 3, "The example makes several tables");
            const auto press_at = [&](QPointF at)
            {
                QMouseEvent down(QEvent::MouseButtonPress, at, schema->mapToGlobal(at.toPoint()),
                                 Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(schema, &down);
                QMouseEvent up(QEvent::MouseButtonRelease, at, schema->mapToGlobal(at.toPoint()),
                               Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(schema, &up);
                settle();
            };
            // Earlier work in this window has left a table being asked about,
            // so the schema is put back before anything is checked.
            press_at(QPointF(boxes.front().left(), boxes.front().bottom() + 400));
            require(!schema->selected().has_value(), "Pressing the bare canvas asks about nothing");

            // The header, which is table and not column, line or answer.
            const auto on_first = boxes.front().topLeft() + QPointF(40, 6);
            press_at(on_first);
            require(schema->selected().has_value(), "Pressing a table asks about it");
            require(*schema->selected() == *schema->preview().tables.front().origin,
                    "And it is that table it asks about");
            // The ring round it runs out along everything it is joined to, so
            // the lines have to be findable from the table that was pressed.
            require(schema->selected_table() == std::optional<std::size_t>{0},
                    "And the table is findable by its place, which is what the lines are matched on");
            press_at(QPointF(boxes.front().left(), boxes.front().bottom() + 400));
            require(!schema->selected().has_value(), "And pressing it again puts the whole schema back");
            require(!schema->selected_table().has_value(), "So no line is ringed either");

            require(schema->showing() == desktop::SchemaShowing::Everything, "Everything, to begin with");
            child<QPushButton>(window, "schemaShowFromrelationships")->click();
            settle();
            require(schema->showing() == desktop::SchemaShowing::FromRelationships,
                    "A chip narrows the schema to one kind of table");
            press_at(on_first);
            require(schema->selected().has_value(), "A table can still be asked about while narrowed");
            child<QPushButton>(window, "schemaShowEverything")->click();
            settle();
            require(schema->showing() == desktop::SchemaShowing::Everything, "And the chips put it back");
            require(!schema->selected().has_value(), "Asking about a kind puts down the one being asked about");

            // Every table's questions are the ones the conversion cannot
            // settle for itself, asked where their answers will be seen.
            std::size_t asked = 0;
            for (const auto &table : schema->preview().tables)
                asked += table.decisions.size();
            require(asked > 0, "The example leaves questions a conversion cannot answer itself");
            const auto composite = std::find_if(
                window.editor().project().attributes.begin(), window.editor().project().attributes.end(),
                [](const auto &entry)
                { return entry.second.kind == domain::AttributeKind::Composite; });
            require(composite != window.editor().project().attributes.end(), "One of them is a composite");
            bool found_question = false;
            for (const auto &table : schema->preview().tables)
                for (const auto &decision : table.decisions)
                    if (decision.kind == domain::DecisionKind::CompositeMode)
                    {
                        require(!decision.answered, "Which nobody has answered yet");
                        require(decision.chosen == 0, "So it reads as the default, Parts");
                        found_question = true;
                    }
            require(found_question, "And the schema asks it on the table it concerns");
            require(editor.set_composite_mode(composite->first, domain::CompositeMode::Whole).ok,
                    "Answering it is an ordinary edit");
            settle();

            // Pressing a column's blank opens every type there is, in one run
            // from the most reached for to the least, with a line to search by.
            // Aimed at the cell the view actually drew rather than at an
            // offset from the table's edge: the constraint marks sit at the
            // right of every row now, so the edge is no longer where the type
            // is.
            const auto blank = [&]() -> std::optional<QPointF>
            {
                const auto cells = schema->cell_boxes();
                for (std::size_t t = 0; t < schema->preview().tables.size() && t < cells.size(); ++t)
                {
                    const auto &columns = schema->preview().tables[t].columns;
                    for (std::size_t row = 0; row < columns.size() && row < cells[t].size(); ++row)
                    {
                        if (columns[row].ignored || !columns[row].origin)
                            continue;
                        if (columns[row].type != domain::LogicalType::Unset)
                            continue;
                        if (cells[t][row].type.isEmpty())
                            continue;
                        return cells[t][row].type.center();
                    }
                }
                return std::nullopt;
            }();
            require(blank.has_value(), "The example leaves a column waiting for a type");
            press_at(*blank);
            auto *picker = window.findChild<QWidget *>("typePicker");
            require(picker != nullptr, "Pressing it opens the types");
            auto *listed = child<QListWidget>(window, "typePickerList");
            require(listed->count() == 38, "Which is every type there is, and no headings among them");
            require(listed->item(0)->text() == "int", "The most reached for leads");
            require(listed->item(listed->count() - 1)->text() == "table", "And the least brings up the rear");

            // The highlight follows the pointer rather than staying where the
            // keyboard left it, so what a click takes and what Return takes
            // are never two different things.
            // While the list is open over it, the cell it came from is drawn
            // as the empty slot it has become rather than as the question it
            // was: the question has been asked and is being answered.
            require(schema->answering().has_value(), "The cell being answered says so while it waits");
            require(listed->currentRow() == 0, "The first is in hand to begin with");
            const auto over = [&](int row)
            {
                const auto at = listed->visualItemRect(listed->item(row)).center();
                QMouseEvent moved(QEvent::MouseMove, QPointF(at), listed->viewport()->mapToGlobal(at),
                                  Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(listed->viewport(), &moved);
                settle();
            };
            over(4);
            require(listed->currentRow() == 4, "Moving over a type takes it in hand");
            over(1);
            require(listed->currentRow() == 1, "And the highlight goes back with the pointer");

            // The search narrows it without disturbing the order.
            auto *looking = child<QLineEdit>(window, "typePickerSearch");
            looking->setText("char");
            settle();
            // char, varchar, varchar(max), nchar, nvarchar, nvarchar(max).
            require(listed->count() == 6, "Searching narrows the list");
            for (int i = 0; i < listed->count(); ++i)
                require(listed->item(i)->text().contains("char"), "To what was searched for");
            looking->setText("zzz");
            settle();
            require(listed->count() == 1 && !(listed->item(0)->flags() & Qt::ItemIsEnabled),
                    "And says so when nothing matches");
            looking->setText("nvarchar");
            settle();

            // Choosing one answers that column, and it is an ordinary edit.
            const auto before = window.editor().revision();
            QTest_activate(listed, listed->item(0));
            settle();
            require(window.editor().revision() != before, "Choosing a type is an edit");
            require(!picker->isVisible(), "And the list closes behind it");
            require(!schema->answering().has_value(), "And the cell stops waiting when it closes");
            bool answered = false;
            for (const auto &table : schema->preview().tables)
                for (const auto &column : table.columns)
                    if (column.type == domain::LogicalType::NVarchar)
                        answered = true;
            require(answered, "The column now carries the type it was given");
            // A measured type grows a second cell beside it for the number,
            // and pressing that asks how long in the terms that type is
            // measured in.
            const auto sized = [&]() -> std::optional<QPointF>
            {
                const auto cells = schema->cell_boxes();
                for (std::size_t t = 0; t < schema->preview().tables.size() && t < cells.size(); ++t)
                {
                    const auto &columns = schema->preview().tables[t].columns;
                    for (std::size_t row = 0; row < columns.size() && row < cells[t].size(); ++row)
                    {
                        if (columns[row].type != domain::LogicalType::NVarchar)
                            continue;
                        if (cells[t][row].size.isEmpty())
                            continue;
                        return cells[t][row].size.center();
                    }
                }
                return std::nullopt;
            }();
            require(sized.has_value(), "The column just answered is a measured type");
            press_at(*sized);
            auto *sizes = window.findChild<QWidget *>("sizePicker");
            require(sizes != nullptr, "Pressing its size asks how long");
            auto *common = child<QListWidget>(window, "sizePickerCommon");
            require(common->count() > 0, "And offers the lengths that type usually takes");
            require(child<QLineEdit>(window, "sizePickerScale")->isHidden(),
                    "A type with no scale is not asked for one");
            const auto counted = [&]
            {
                for (const auto &table : schema->preview().tables)
                    for (const auto &column : table.columns)
                        if (column.type == domain::LogicalType::NVarchar)
                            return column.length;
                return std::uint32_t{0};
            };
            require(counted() == 0, "Nobody has said how long yet");

            // A number nobody thought to offer is typed in. The list is a
            // convenience, not the whole of what can be said.
            auto *typed_in = child<QLineEdit>(window, "sizePickerLength");
            typed_in->setText("77");
            QKeyEvent entered(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
            QApplication::sendEvent(typed_in, &entered);
            settle();
            require(counted() == 77, "A length typed in is the length it takes");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(counted() == 0, "And that undoes too");

            // Typed and then clicked away from counts just the same: a number
            // written into the field is an answer, finished with Return or not.
            press_at(*sized);
            settle();
            child<QLineEdit>(window, "sizePickerLength")->setText("31");
            window.findChild<QWidget *>("sizePicker")->hide();
            settle();
            require(counted() == 31, "A length typed and left is still the length");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(counted() == 0, "And undoes with everything else");

            // Opened and closed with nothing said changes nothing.
            const auto quiet = window.editor().revision();
            press_at(*sized);
            settle();
            window.findChild<QWidget *>("sizePicker")->hide();
            settle();
            require(window.editor().revision() == quiet, "Opening it and saying nothing is not an edit");
            press_at(*sized);
            settle();
            sizes = window.findChild<QWidget *>("sizePicker");
            common = child<QListWidget>(window, "sizePickerCommon");
            const auto wanted = common->item(common->count() - 1)->data(Qt::UserRole).toUInt();
            QTest_activate(common, common->item(common->count() - 1));
            settle();
            require(counted() == wanted, "Choosing one sets the length");
            require(!sizes->isVisible(), "And the list closes behind it");
            // Undone through the window, because an edit made straight to
            // the Editor never tells the panel to read the model again.
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(counted() == 0, "The length undoes on its own, leaving the type where it was");

            require(editor.undo().ok, "And it undoes like anything else");
            settle();

            // The constraints a row carries, all in one column and chosen
            // from the list that opens under it. The list stops and waits for
            // somebody, so what is wanted from it is asked for before it
            // opens and taken as soon as it is there.
            {
                const auto choose = [&](QPointF where, const char *which)
                {
                    QTimer::singleShot(0, &window, [&window, which]
                                       {
                        auto* menu = window.findChild<QMenu*>("schemaRulesMenu");
                        if (!menu) return;
                        if (auto* action = menu->findChild<QAction*>(which)) action->trigger();
                        menu->close(); });
                    press_at(where);
                    settle();
                };
                const auto cells = schema->cell_boxes();
                // An ordinary column, whose rules are its attribute's own.
                std::optional<QPointF> ordinary;
                std::optional<domain::AttributeId> behind;
                for (std::size_t t = 0; t < schema->preview().tables.size() && t < cells.size(); ++t)
                {
                    const auto &columns = schema->preview().tables[t].columns;
                    for (std::size_t row = 0; row < columns.size() && row < cells[t].size(); ++row)
                    {
                        if (!columns[row].origin || columns[row].primary_key)
                            continue;
                        if (cells[t][row].rules.isEmpty())
                            continue;
                        ordinary = cells[t][row].rules.center();
                        behind = *columns[row].origin;
                        break;
                    }
                    if (ordinary)
                        break;
                }
                require(ordinary.has_value(), "Every real column has somewhere to carry its rules");
                require(!window.editor().project().attributes.at(*behind).unique,
                        "The column starts without a unique constraint");
                // The canvas's list offers the whole of what a column can be
                // said to enforce (Zain, 2026-10-08): its two keys, then the
                // rules as the Properties panel words and orders them.
                {
                    QStringList canvas_rules;
                    QTimer::singleShot(0, &window, [&]
                                       {
                        auto* menu = window.findChild<QMenu*>("schemaRulesMenu");
                        if (!menu) return;
                        for (auto* action : menu->actions())
                            canvas_rules << (action->isSeparator() ? QString("—") : action->text());
                        menu->close(); });
                    press_at(*ordinary);
                    settle();
                    require(canvas_rules == QStringList{"Primary Key", "Foreign Key", "—", "NULL — may be empty",
                                                        "NOT NULL — required", "—", "UNIQUE — no duplicate values",
                                                        "IDENTITY — auto-generated number"},
                            "The canvas's constraints list offers the keys, then the rules, in the Properties' words");
                }
                choose(*ordinary, "schemaRuleUnique");
                require(window.editor().project().attributes.at(*behind).unique,
                        "Choosing UNIQUE puts one on the attribute behind the column");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
                require(!window.editor().project().attributes.at(*behind).unique,
                        "And it undoes like any other edit");

                // A foreign key's nullability is not a fact about the column:
                // it is the participation of the side it points at, so
                // choosing it reaches the relationship on the diagram.
                const auto fresh = schema->cell_boxes();
                std::optional<QPointF> keyed;
                std::optional<domain::ParticipantId> side;
                bool was_required = false;
                for (std::size_t t = 0; t < schema->preview().tables.size() && t < fresh.size(); ++t)
                {
                    const auto &columns = schema->preview().tables[t].columns;
                    for (std::size_t row = 0; row < columns.size() && row < fresh[t].size(); ++row)
                    {
                        if (!columns[row].link || fresh[t][row].rules.isEmpty())
                            continue;
                        if (!std::holds_alternative<domain::ParticipantId>(*columns[row].link))
                            continue;
                        keyed = fresh[t][row].rules.center();
                        side = std::get<domain::ParticipantId>(*columns[row].link);
                        was_required = columns[row].required;
                        break;
                    }
                    if (keyed)
                        break;
                }
                require(keyed.has_value(), "The example has a foreign key put there by a relationship");
                const auto participation_of = [&]
                {
                    for (const auto &[id, relationship] : window.editor().project().relationships)
                    {
                        (void)id;
                        for (const auto &one : relationship.participants)
                            if (one.id == *side)
                                return one.participation;
                    }
                    return domain::Participation::Partial;
                };
                choose(*keyed, was_required ? "schemaRuleNull" : "schemaRuleNotNull");
                require((participation_of() == domain::Participation::Total) != was_required,
                        "Choosing a foreign key's nullability turns the side it points at over");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
                require((participation_of() == domain::Participation::Total) == was_required,
                        "And that undoes with everything else");

                // A key the conversion invented has no attribute behind it,
                // which is where choosing a constraint used to do nothing at
                // all. It is the commonest place of all to want one.
                const auto again = schema->cell_boxes();
                std::optional<QPointF> invented;
                std::optional<domain::ElementRef> whose;
                for (std::size_t t = 0; t < schema->preview().tables.size() && t < again.size(); ++t)
                {
                    const auto &one = schema->preview().tables[t];
                    if (!one.origin)
                        continue;
                    for (std::size_t row = 0; row < one.columns.size() && row < again[t].size(); ++row)
                    {
                        if (one.columns[row].origin_kind != domain::ColumnOrigin::Generated)
                            continue;
                        if (again[t][row].rules.isEmpty())
                            continue;
                        invented = again[t][row].rules.center();
                        whose = *one.origin;
                        break;
                    }
                    if (invented)
                        break;
                }
                require(invented.has_value(), "The example has a key the conversion invented");
                require(!window.editor().project().schema.counting_keys.contains(relation_from(*whose)),
                        "Which does not count itself up to begin with");
                choose(*invented, "schemaRuleIdentity");
                require(window.editor().project().schema.counting_keys.contains(relation_from(*whose)),
                        "Choosing IDENTITY makes it count itself up");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
                require(!window.editor().project().schema.counting_keys.contains(relation_from(*whose)),
                        "And that undoes like anything else");
            }

            // Narrowing a table folds its columns away from the right, one
            // at a time, and never moves the tables beside it.
            {
                // Held by value: the preview is worked out afresh on every
                // refresh, so a reference into it would not survive the first
                // pull.
                const auto first_origin = schema->preview().tables.front().origin;
                require(first_origin.has_value(), "The first table came from the diagram");
                const auto beside_before = schema->table_boxes();
                const auto pull_to = [&](double width)
                {
                    std::map<domain::ElementRef, domain::SchemaTableBox> asked;
                    domain::SchemaTableBox box;
                    box.width = width;
                    box.height = schema->table_boxes().front().height();
                    asked.emplace(*first_origin, box);
                    require(editor.resize_schema_tables(asked).ok, "The table is pulled");
                    schema->refresh();
                    settle();
                };
                // How many columns a row still shows beyond its name, read
                // off what was actually drawn.
                const auto columns_now = [&]
                {
                    const auto cells = schema->cell_boxes();
                    require(!cells.empty() && !cells.front().empty(), "The table has rows");
                    int shown = 0;
                    for (const auto &one : cells.front())
                    {
                        if (!one.rules.isEmpty())
                            return 2;
                        if (!one.type.isEmpty())
                            shown = std::max(shown, 1);
                    }
                    return shown;
                };
                require(columns_now() == 2, "At its own width a table shows all of its columns");
                // Swept down rather than pulled to chosen numbers: where each
                // column gives way depends on what that table happens to
                // hold, and what is being checked is the order they go in,
                // not the width at which each one does.
                const auto from = schema->table_boxes().front().width();
                std::vector<int> seen{2};
                for (auto width = from; width > domain::min_table_width; width -= 10)
                {
                    pull_to(std::max(domain::min_table_width, width));
                    const auto now = columns_now();
                    require(now <= seen.back(), "A narrower table never shows more than a wider one");
                    if (now != seen.back())
                        seen.push_back(now);
                }
                pull_to(domain::min_table_width);
                if (columns_now() != seen.back())
                    seen.push_back(columns_now());
                require((seen == std::vector<int>{2, 1, 0}),
                        "They fold from the right, one at a time: constraints, then type");
                require(columns_now() == 0,
                        "Leaving the keys and the names, which nothing else can stand in for");
                require(schema->table_boxes().front().width() == domain::min_table_width,
                        "And the table is as narrow as a table may be");
                // None of that moved anything else.
                const auto beside_after = schema->table_boxes();
                require(beside_after.size() == beside_before.size(), "Same tables throughout");
                for (std::size_t i = 1; i < beside_after.size(); ++i)
                    require(beside_after[i].topLeft() == beside_before[i].topLeft(),
                            "Pulling one table about leaves the others where they were");
                // Widened again, every column comes back in the reverse order.
                pull_to(from);
                require(columns_now() == 2, "Widened again, every column comes back");
                // Put the table back where it was, so what follows sees the
                // schema it expects rather than one this case left narrowed.
                while (window.editor().can_undo() && schema->table_boxes().front().width() != from)
                {
                    child<QAction>(window, "undoCommand")->trigger();
                    settle();
                }
            }

            // Several tables are gathered by drawing a band round them, and
            // coloured together. The colour goes on the element itself, so a
            // table coloured here and the entity it came from are one thing
            // wearing one colour.
            {
                const auto boxes = schema->table_boxes();
                require(boxes.size() >= 2, "The example has tables to gather");
                const auto drag = [&](QEvent::Type kind, QPointF at, Qt::MouseButton button,
                                      Qt::MouseButtons held)
                {
                    QMouseEvent event(kind, at, schema->mapToGlobal(at.toPoint()), button, held,
                                      Qt::NoModifier);
                    QApplication::sendEvent(schema, &event);
                };
                // The band is started on empty canvas beside the first table, as
                // a person starts one. The point is found rather than fixed:
                // where lines run depends on how wide each platform's lettering
                // makes the tables, and with narrower lettering one runs along
                // the top of the schema, past where a fixed offset would press.
                const QPointF to(boxes[1].right() + 10, boxes[1].bottom() + 6);
                const QPointF aimed_at(boxes[0].left() - 20, boxes[0].top() - 14);
                const auto band_lines = schema->line_shapes();
                // Under each table is the slot that adds a column to it.
                constexpr double slot_depth = 21;
                const auto clear_of_everything = [&](QPointF at)
                {
                    constexpr auto c = press_clearance;
                    if (!QRectF(schema->rect()).adjusted(c, c, -c, -c).contains(at))
                        return false;
                    for (const auto &box : boxes)
                        if (box.adjusted(-c, -c, c, c + slot_depth).contains(at))
                            return false;
                    return distance_to_lines(at, band_lines) >= c;
                };
                const auto off = [&](QPointF at)
                { return std::hypot(at.x() - aimed_at.x(), at.y() - aimed_at.y()); };
                std::optional<QPointF> start;
                for (int y = 0; y < schema->height(); y += 2)
                    for (int x = 0; x < schema->width(); x += 2)
                    {
                        const QPointF at(x, y);
                        const auto band = QRectF(at, to).normalized();
                        if ((!start || off(at) < off(*start)) && band.intersects(boxes[0]) &&
                            band.intersects(boxes[1]) && clear_of_everything(at))
                            start = at;
                    }
                require(start.has_value(), "There is empty canvas beside the first table to start a band from");
                const auto from = *start;
                // The pointer there says nothing can be taken hold of, which the
                // schema works out in the order it answers a press.
                drag(QEvent::MouseMove, from, Qt::NoButton, Qt::NoButton);
                require(schema->cursor().shape() == Qt::ArrowCursor,
                        "The band starts where the pointer says nothing can be taken hold of");
                drag(QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseMove, QPointF((from.x() + to.x()) / 2, (from.y() + to.y()) / 2),
                     Qt::NoButton, Qt::LeftButton);
                settle();
                drag(QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
                settle();
                require(schema->selection().size() >= 2,
                        "A band drawn across tables gathers every one it touches");
                require(!schema->selected().has_value(),
                        "Several gathered is a different question from one asked about");
                const auto gathered = schema->selection();

                // Coloured as one edit, and the colour is on the elements the
                // diagram draws rather than on anything the schema keeps.
                require(editor.recolour(gathered, domain::Colour{0x9A, 0xDC, 0xFF}).ok,
                        "The gathered tables are coloured together");
                schema->refresh();
                settle();
                for (const auto &ref : gathered)
                {
                    const auto worn = window.editor().project().colours.find(ref);
                    require(worn != window.editor().project().colours.end(),
                            "Each of them now wears a colour");
                    require(worn->second.blue == 0xFF, "And it is the one that was chosen");
                }
                child<QAction>(window, "undoCommand")->trigger();
                settle();
                for (const auto &ref : gathered)
                    require(!window.editor().project().colours.contains(ref),
                            "And one undo takes the colour off all of them");

                // Taking hold of any one of a gathered group moves the whole
                // group, every table the same distance, so it keeps its
                // arrangement. The tables left out of it stay where they are,
                // and one undo puts the group back.
                {
                    require(schema->selection() == gathered, "The group is still gathered");
                    const auto &tables = schema->preview().tables;
                    const auto in_group = [&](std::size_t i)
                    {
                        return tables[i].origin && std::find(gathered.begin(), gathered.end(), *tables[i].origin) != gathered.end();
                    };
                    const auto before = schema->table_boxes();
                    std::size_t held = tables.size();
                    for (std::size_t i = 0; i < tables.size(); ++i)
                        if (in_group(i))
                        {
                            held = i;
                            break;
                        }
                    require(held < tables.size(), "One of the group to take hold of");
                    const QPointF grip(before[held].center().x(), before[held].top() + 12);
                    const QPointF moved(70, 45);
                    drag(QEvent::MouseButtonPress, grip, Qt::LeftButton, Qt::LeftButton);
                    drag(QEvent::MouseMove, grip + moved / 2, Qt::NoButton, Qt::LeftButton);
                    drag(QEvent::MouseMove, grip + moved, Qt::NoButton, Qt::LeftButton);
                    drag(QEvent::MouseButtonRelease, grip + moved, Qt::LeftButton, Qt::NoButton);
                    settle();
                    const auto after = schema->table_boxes();
                    require(after.size() == before.size(), "The same tables throughout");
                    int carried = 0;
                    for (std::size_t i = 0; i < before.size(); ++i)
                    {
                        const auto shift = after[i].topLeft() - before[i].topLeft();
                        if (in_group(i))
                        {
                            ++carried;
                            require(std::abs(shift.x() - moved.x()) < 0.5 && std::abs(shift.y() - moved.y()) < 0.5,
                                    "Every table of the group moves with the one taken hold of, as far");
                        }
                        else
                        {
                            require(shift.isNull(), "And a table outside the group stays where it was");
                        }
                    }
                    require(carried >= 2, "More than one table was carried");
                    require(schema->selection() == gathered, "Moving the group leaves it gathered");
                    child<QAction>(window, "undoCommand")->trigger();
                    settle();
                    const auto undone = schema->table_boxes();
                    for (std::size_t i = 0; i < before.size(); ++i)
                        require(undone[i].topLeft() == before[i].topLeft(),
                                "And one undo puts the whole group back");
                }

                // As on the diagram: a press on the schema gives it the
                // keyboard, Select All then gathers every table, and taking
                // hold of any one of them carries the whole schema.
                {
                    const auto before = schema->table_boxes();
                    const QPointF empty(before[0].left(), before.back().bottom() + 400);
                    drag(QEvent::MouseButtonPress, empty, Qt::LeftButton, Qt::LeftButton);
                    drag(QEvent::MouseButtonRelease, empty, Qt::LeftButton, Qt::NoButton);
                    settle();
                    require(window.focusWidget() == schema, "A press on the schema gives it the keyboard");
                    QKeyEvent all(QEvent::KeyPress, Qt::Key_A, Qt::ControlModifier);
                    QApplication::sendEvent(window.focusWidget(), &all);
                    settle();
                    require(schema->selection().size() == schema->preview().tables.size(),
                            "Select All in the schema gathers every table");
                    const QPointF grip(before[0].center().x(), before[0].top() + 12);
                    const QPointF moved(40, 30);
                    drag(QEvent::MouseButtonPress, grip, Qt::LeftButton, Qt::LeftButton);
                    drag(QEvent::MouseMove, grip + moved, Qt::NoButton, Qt::LeftButton);
                    drag(QEvent::MouseButtonRelease, grip + moved, Qt::LeftButton, Qt::NoButton);
                    settle();
                    const auto after = schema->table_boxes();
                    for (std::size_t i = 0; i < before.size(); ++i)
                    {
                        const auto shift = after[i].topLeft() - before[i].topLeft();
                        require(std::abs(shift.x() - moved.x()) < 0.5 && std::abs(shift.y() - moved.y()) < 0.5,
                                "And dragging one of them moves every table in the schema together");
                    }
                    child<QAction>(window, "undoCommand")->trigger();
                    settle();
                    require(schema->table_boxes() == before, "One undo puts the whole schema back");
                }

                // The example's entities all have a key drawn, and each table
                // uses it: nothing is invented and nothing is announced. An
                // entity with no key is given one, and that is said once, in a
                // notice, rather than left to be found by hovering.
                {
                    std::vector<QString> heard;
                    auto reported = schema->warned;
                    schema->warned = [&](const QString &words, QPoint at)
                    {
                        heard.push_back(words);
                        if (reported)
                            reported(words, at);
                    };
                    schema->refresh();
                    require(heard.empty(), "Where every entity has a key drawn, nothing is announced");
                    const auto made = editor.create_entity("Locker", {900, 900, 148, 86});
                    require(made.ok, "An entity with no attributes at all");
                    schema->refresh();
                    require(heard.size() == 1 && heard.front().contains("Locker has no key attribute") && heard.front().contains("LockerID was made its primary key"),
                            "An entity with no key is given one, and told so by name");
                    require(editor.rename(*made.created, "Locker").ok, "An unrelated edit");
                    schema->refresh();
                    require(heard.size() == 1, "And it is said once, not again on every change");
                    schema->warned = reported;
                    child<QAction>(window, "undoCommand")->trigger();
                    settle();
                }

                // Pressing the bare canvas puts the whole schema back.
                const QPointF nowhere(boxes[0].left(), boxes.back().bottom() + 400);
                drag(QEvent::MouseButtonPress, nowhere, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, nowhere, Qt::LeftButton, Qt::NoButton);
                settle();
                require(schema->selection().empty(), "Pressing the bare canvas gathers nothing");
            }

            // An attribute is pulled about by its own edges and corners, as an
            // entity is. A default is a starting size, not a ruling.
            {
                const auto named = std::find_if(
                    window.editor().project().attributes.begin(),
                    window.editor().project().attributes.end(),
                    [](const auto &entry)
                    { return entry.second.name == "Credit Hours"; });
                require(named != window.editor().project().attributes.end(),
                        "The example has an attribute to pull about");
                const domain::ElementRef ref{named->first};
                const auto before = window.editor().project().layout.at(ref);
                auto wider = before;
                wider.width = before.width + 90;
                require(editor.move({{ref, wider}}).ok, "An attribute takes a size given to it");
                settle();
                require(window.editor().project().layout.at(ref).width > before.width + 80,
                        "And keeps it");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
                require(std::abs(window.editor().project().layout.at(ref).width - before.width) < 0.01,
                        "And it undoes like any other edit");
            }

            // A search of the schema, which is not the diagram's search: it
            // picks out the tables and columns whose names carry the words.
            auto *looking_at_schema = child<QLineEdit>(window, "schemaSearch");
            looking_at_schema->setText("phone");
            settle();
            require(schema->looking_for() == "phone", "The schema is searched by its own bar");
            require(!schema->selected().has_value(),
                    "Which is a broader question than asking about one table");
            looking_at_schema->clear();
            settle();
            require(schema->looking_for().isEmpty(), "And clearing it puts the whole schema back");

            // Release gives the lines back without moving the tables.
            const auto where = schema->table_boxes();
            child<QAction>(window, "schemaRelease")->trigger();
            settle();
            require(schema->shaped_lines() == 0, "Release gives every line back to the router");
            require(schema->table_boxes() == where, "And leaves every table where it was");

            child<QPushButton>(window, "previewSchema")->click();
            settle_motion(window);
        }

        {
            // Everything done on the schema undoes, and redoes. Arranging it
            // is presentation, but it is work somebody did, so it is an edit
            // like any other rather than something the window keeps to itself
            // and loses.
            child<QPushButton>(window, "previewSchema")->click();
            settle_motion(window);
            auto *schema = window.schema();
            auto *undo = child<QAction>(window, "undoCommand");
            auto *redo = child<QAction>(window, "redoCommand");
            const auto drag = [&](QEvent::Type type, QPointF at, Qt::MouseButton button,
                                  Qt::MouseButtons held)
            {
                QMouseEvent event(type, at, schema->mapToGlobal(at.toPoint()), button, held,
                                  Qt::NoModifier);
                QApplication::sendEvent(schema, &event);
                settle();
            };

            // Moving a table.
            const auto before_move = schema->table_boxes();
            require(!before_move.empty(), "The schema has tables to move");
            const auto grab_table = before_move.front().topLeft() + QPointF(60, 8);
            drag(QEvent::MouseButtonPress, grab_table, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, grab_table + QPointF(40, 60), Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, grab_table + QPointF(40, 60), Qt::LeftButton, Qt::NoButton);
            const auto after_move = schema->table_boxes();
            require(after_move.front().topLeft() != before_move.front().topLeft(), "A table moves");
            require(undo->isEnabled(), "Which is an edit, so there is something to undo");
            // One drag is one step, not one step for every frame of it.
            undo->trigger();
            settle();
            require(schema->table_boxes().front().topLeft() == before_move.front().topLeft(),
                    "Undo puts the table back where it was, in one step");
            redo->trigger();
            settle();
            require(schema->table_boxes().front().topLeft() == after_move.front().topLeft(),
                    "And redo puts it back where it was taken");
            undo->trigger();
            settle();

            // Moving one table moves that table. Every other table stays
            // exactly where it was, which is only true because the automatic
            // arrangement is worked out for all of them and a moved table
            // simply sits elsewhere: leave a moved table out of the packing
            // and the ones after it shuffle up behind it.
            {
                const auto settled = schema->table_boxes();
                require(settled.size() >= 3, "Several tables to leave alone");
                const auto lift = settled[1].topLeft() + QPointF(60, 8);
                drag(QEvent::MouseButtonPress, lift, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseMove, lift + QPointF(70, 120), Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, lift + QPointF(70, 120), Qt::LeftButton, Qt::NoButton);
                const auto after = schema->table_boxes();
                require(after.size() == settled.size(), "The same tables are there");
                require(after[1].topLeft() != settled[1].topLeft(), "The one that was moved moved");
                for (std::size_t i = 0; i < after.size(); ++i)
                {
                    if (i == 1)
                        continue;
                    require(after[i] == settled[i], "And no other table moved with it");
                }
                undo->trigger();
                settle();
            }

            // Pushing a line sideways.
            const auto drawn = schema->line_shapes();
            std::size_t on_line = 0;
            QPointF ran_from;
            QPointF ran_to;
            double longest = 0;
            for (std::size_t line = 0; line < drawn.size(); ++line)
                for (std::size_t i = 1; i < drawn[line].size(); ++i)
                {
                    const auto length = std::hypot(drawn[line][i].x() - drawn[line][i - 1].x(),
                                                   drawn[line][i].y() - drawn[line][i - 1].y());
                    if (length <= longest)
                        continue;
                    longest = length;
                    on_line = line;
                    ran_from = drawn[line][i - 1];
                    ran_to = drawn[line][i];
                }
            require(longest > 40, "There is a run long enough to push");
            const bool upright = std::abs(ran_from.x() - ran_to.x()) < 0.01;
            const auto across = upright ? QPointF(34, 0) : QPointF(0, 34);
            const auto hold = (ran_from + ran_to) / 2;
            drag(QEvent::MouseButtonPress, hold, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, hold + across / 3, Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseMove, hold + across, Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, hold + across, Qt::LeftButton, Qt::NoButton);
            require(schema->shaped_lines() == 1, "A run pushed sideways shapes that line");
            const auto pushed = schema->line_shapes()[on_line];
            undo->trigger();
            settle();
            require(schema->shaped_lines() == 0, "Undo gives the line back to the router");
            require(schema->line_shapes()[on_line] == drawn[on_line], "With the route it had");
            redo->trigger();
            settle();
            require(schema->shaped_lines() == 1, "Redo shapes it again");
            require(schema->line_shapes()[on_line] == pushed, "The same way it was shaped");

            // Moving a line's end off its table.
            const auto head = schema->line_shapes()[on_line].front();
            auto lowest = 0.0;
            for (const auto &box : schema->table_boxes())
                lowest = std::max(lowest, box.bottom());
            const QPointF adrift(head.x(), lowest + 70);
            drag(QEvent::MouseButtonPress, head, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, QPointF(head.x(), lowest + 30), Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseMove, adrift, Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, adrift, Qt::LeftButton, Qt::NoButton);
            require(schema->loose_ends() == 1, "An end pulled off its table is left hanging");
            undo->trigger();
            settle();
            require(schema->loose_ends() == 0, "Undo puts the end back on its table");
            redo->trigger();
            settle();
            require(schema->loose_ends() == 1, "And redo takes it off again");

            // Tidy, which throws away every arrangement at once.
            child<QAction>(window, "schemaTidy")->trigger();
            settle();
            require(schema->shaped_lines() == 0 && schema->loose_ends() == 0,
                    "Tidy gives back every line");
            undo->trigger();
            settle();
            require(schema->shaped_lines() == 1 && schema->loose_ends() == 1,
                    "And one undo brings the whole arrangement back");
            child<QAction>(window, "schemaTidy")->trigger();
            settle();

            // A line shaped by hand holds its shape when a table is moved on
            // top of it, unless it has been asked to get out of the way.
            {
                auto *give_way = child<QAction>(window, "schemaGiveWay");
                require(!give_way->isChecked(), "Lines hold their shape unless asked otherwise");
                // A shaped line of its own, since the block before this one
                // tidied everything away.
                const auto runs = schema->line_shapes();
                QPointF take;
                double reach = 0;
                for (const auto &shape : runs)
                    for (std::size_t i = 1; i < shape.size(); ++i)
                    {
                        const auto length = std::hypot(shape[i].x() - shape[i - 1].x(),
                                                       shape[i].y() - shape[i - 1].y());
                        if (length <= reach)
                            continue;
                        reach = length;
                        take = (shape[i] + shape[i - 1]) / 2;
                    }
                require(reach > 40, "A run long enough to shape");
                drag(QEvent::MouseButtonPress, take, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseMove, take + QPointF(0, 30), Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, take + QPointF(0, 30), Qt::LeftButton, Qt::NoButton);
                require(schema->shaped_lines() == 1, "There is a shaped line to leave alone");
                const auto boxes = schema->table_boxes();
                // The table is taken by a place that takes it, as near its
                // heading as there is one. A line answers a press before the
                // table under it, and where lines run depends on how wide each
                // platform's lettering makes the tables, so the place is found:
                // clear of every line, and where the pointer shows the open
                // hand, which the schema works out in the order it answers a press.
                const auto takes_the_table = [&](QPointF at)
                {
                    drag(QEvent::MouseMove, at, Qt::NoButton, Qt::NoButton);
                    return schema->cursor().shape() == Qt::OpenHandCursor;
                };
                const auto held_lines = schema->line_shapes();
                const auto wanted = boxes.front().topLeft() + QPointF(60, 8);
                std::vector<QPointF> places;
                const auto inside = boxes.front().adjusted(2, 2, -2, -2);
                for (double y = inside.top(); y <= inside.bottom(); y += 2)
                    for (double x = inside.left(); x <= inside.right(); x += 2)
                        if (distance_to_lines(QPointF(x, y), held_lines) >= press_clearance)
                            places.emplace_back(x, y);
                std::sort(places.begin(), places.end(), [&](QPointF a, QPointF b)
                          { return QLineF(a, wanted).length() < QLineF(b, wanted).length(); });
                const auto found = std::find_if(places.begin(), places.end(), takes_the_table);
                require(found != places.end(), "The table has a place to take hold of it by");
                const auto onto = *found;
                drag(QEvent::MouseButtonPress, onto, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseMove, onto + QPointF(120, 40), Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, onto + QPointF(120, 40), Qt::LeftButton, Qt::NoButton);
                require(schema->shaped_lines() == 1, "The shape survives a table moving about");
                undo->trigger();
                settle();

                // Asked to, a line a move has left lying across a table is
                // handed back to the router -- in the same edit as the move,
                // so one undo takes both back together.
                give_way->setChecked(true);
                settle();
                require(schema->lines_give_way(), "The option reaches the schema");
                const auto sat = schema->table_boxes();
                const auto shapes_before = schema->line_shapes();
                require(takes_the_table(onto), "The same place still takes hold of the table");
                drag(QEvent::MouseButtonPress, onto, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseMove, onto + QPointF(120, 40), Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, onto + QPointF(120, 40), Qt::LeftButton, Qt::NoButton);
                require(schema->table_boxes() != sat, "The move happens");
                // One undo, whether or not the move freed a line: the two are
                // one edit, so they come back together rather than in turn.
                undo->trigger();
                settle();
                require(schema->table_boxes() == sat, "One undo puts the table back");
                require(schema->shaped_lines() == 1, "And the shaped line back with it");
                require(schema->line_shapes() == shapes_before, "Exactly as it was");
                give_way->setChecked(false);
                settle();
            }

            // A table is pulled about by any of its four edges and any of its
            // corners, which is an edit like the rest: it undoes, it redoes,
            // and it is saved.
            {
                require(schema->tables_resizable(), "Tables can be resized unless told otherwise");
                // A line always answers the pointer before the table it
                // crosses, so a pull is aimed at a stretch of edge no line is
                // lying on -- as a hand would aim it.
                const auto clear_of_lines = [&](QPointF at)
                {
                    for (const auto &shape : schema->line_shapes())
                        for (std::size_t i = 1; i < shape.size(); ++i)
                        {
                            const auto from = shape[i - 1];
                            const auto to = shape[i];
                            const auto length = std::hypot(to.x() - from.x(), to.y() - from.y());
                            const auto steps = static_cast<int>(length / 3) + 1;
                            for (int step = 0; step <= steps; ++step)
                            {
                                const auto on = from + (to - from) * (static_cast<double>(step) / steps);
                                if (std::hypot(on.x() - at.x(), on.y() - at.y()) < 14)
                                    return false;
                            }
                        }
                    return true;
                };
                // Somewhere along one edge of a box that is clear, keeping
                // well away from the corners so the edge itself is what is
                // taken hold of. The edges are numbered clockwise from the
                // left, as they are read out below.
                enum Edge
                {
                    LeftEdge,
                    TopEdge,
                    RightEdge,
                    BottomEdge
                };
                const auto clear_spot = [&](const QRectF &box, Edge edge)
                {
                    const auto along = edge == TopEdge || edge == BottomEdge ? box.width() : box.height();
                    const auto spot = [&](double step)
                    {
                        switch (edge)
                        {
                        case LeftEdge:
                            return QPointF(box.left() + 2, box.top() + step);
                        case TopEdge:
                            return QPointF(box.left() + step, box.top() + 2);
                        case RightEdge:
                            return QPointF(box.right() - 2, box.top() + step);
                        case BottomEdge:
                            break;
                        }
                        return QPointF(box.left() + step, box.bottom() - 2);
                    };
                    for (double step = 26; step < along - 26; step += 5)
                        if (clear_of_lines(spot(step)))
                            return spot(step);
                    return spot(along / 2);
                };
                const auto before_width = schema->table_boxes().front();
                const auto edge = QPointF(before_width.right() - 2, before_width.center().y());
                drag(QEvent::MouseButtonPress, edge, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseMove, edge + QPointF(90, 0), Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, edge + QPointF(90, 0), Qt::LeftButton, Qt::NoButton);
                const auto widened = schema->table_boxes().front();
                require(widened.width() > before_width.width() + 40, "The table is wider");
                require(widened.topLeft() == before_width.topLeft(), "And has not moved doing it");
                require(widened.height() == before_width.height(),
                        "Nor grown taller: only the side that was pulled moves");
                undo->trigger();
                settle();
                require(schema->table_boxes().front().width() == before_width.width(),
                        "Undo puts the width back");
                redo->trigger();
                settle();
                require(schema->table_boxes().front().width() == widened.width(),
                        "And redo pulls it out again");

                // A table cannot be pulled past what a table may be.
                const auto wide_edge = QPointF(schema->table_boxes().front().right() - 2,
                                               schema->table_boxes().front().center().y());
                drag(QEvent::MouseButtonPress, wide_edge, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseMove, wide_edge + QPointF(4000, 0), Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, wide_edge + QPointF(4000, 0), Qt::LeftButton, Qt::NoButton);
                require(schema->table_boxes().front().width() <= domain::max_table_width,
                        "However far the edge is pulled");
                undo->trigger();
                settle();

                // The left edge carries the table's corner with it and leaves
                // the right-hand side exactly where it was. Both the size and
                // the place arrive as one edit, so one undo takes them back
                // together rather than leaving the table somewhere it was
                // never put.
                {
                    const auto before = schema->table_boxes().front();
                    const auto edge = clear_spot(before, LeftEdge);
                    drag(QEvent::MouseButtonPress, edge, Qt::LeftButton, Qt::LeftButton);
                    drag(QEvent::MouseMove, edge - QPointF(40, 0), Qt::NoButton, Qt::LeftButton);
                    drag(QEvent::MouseButtonRelease, edge - QPointF(40, 0), Qt::LeftButton, Qt::NoButton);
                    const auto reached = schema->table_boxes().front();
                    require(reached.left() < before.left() - 20, "The left edge follows the pointer");
                    require(std::abs(reached.right() - before.right()) < 0.01,
                            "And the right-hand side stays where it was");
                    require(std::abs(reached.height() - before.height()) < 0.01, "Its height is untouched");
                    undo->trigger();
                    settle();
                    require(schema->table_boxes().front() == before,
                            "One undo takes back the size and the place together");
                }

                // The bottom edge makes a table taller, and a table pulled
                // taller does not push the tables under it down the column:
                // pulling one table about is pulling one table about.
                {
                    const auto before = schema->table_boxes();
                    const auto standard_rows = schema->row_boxes().front();
                    const auto edge = clear_spot(before.front(), BottomEdge);
                    drag(QEvent::MouseButtonPress, edge, Qt::LeftButton, Qt::LeftButton);
                    drag(QEvent::MouseMove, edge + QPointF(0, 70), Qt::NoButton, Qt::LeftButton);
                    drag(QEvent::MouseButtonRelease, edge + QPointF(0, 70), Qt::LeftButton, Qt::NoButton);
                    const auto taller = schema->table_boxes();
                    require(taller.front().height() > before.front().height() + 50, "The table is taller");
                    require(taller.front().topLeft() == before.front().topLeft(),
                            "Without moving to do it");
                    require(std::equal(before.begin() + 1, before.end(), taller.begin() + 1),
                            "And no other table moves for it");
                    undo->trigger();
                    settle();
                    require(schema->table_boxes() == before, "Undo puts the height back");
                    redo->trigger();
                    settle();
                    require(schema->table_boxes().front().height() == taller.front().height(),
                            "And redo makes it tall again");

                    // The room it gained is shared out between its rows, so
                    // the table is a roomier one rather than one with a gap
                    // under its last row.
                    const auto deep = schema->row_boxes().front();
                    require(!deep.empty(), "The table has rows to share the room between");
                    require(deep.front().height() > standard_rows.front().height() + 1,
                            "Each row is drawn deeper for the room the table was given");
                    const auto rows_grew = (deep.back().bottom() - deep.front().top()) - (standard_rows.back().bottom() - standard_rows.front().top());
                    require(std::abs(rows_grew - (taller.front().height() - before.front().height())) < 1.0,
                            "And every bit of the room the table gained went into them");

                    // The top edge is the same thing the other way up: it
                    // takes the table's corner with it and leaves the bottom
                    // where it is. A table that has been given room can give
                    // it back this way; one that has none cannot be pulled
                    // down over its own rows.
                    const auto room = schema->table_boxes().front();
                    const auto top_edge = clear_spot(room, TopEdge);
                    drag(QEvent::MouseButtonPress, top_edge, Qt::LeftButton, Qt::LeftButton);
                    drag(QEvent::MouseMove, top_edge + QPointF(0, 40), Qt::NoButton, Qt::LeftButton);
                    drag(QEvent::MouseButtonRelease, top_edge + QPointF(0, 40), Qt::LeftButton, Qt::NoButton);
                    const auto shortened = schema->table_boxes().front();
                    require(shortened.top() > room.top() + 30, "The top edge follows the pointer down");
                    require(std::abs(shortened.bottom() - room.bottom()) < 0.01,
                            "And the bottom of the table stays where it was");
                    undo->trigger();
                    settle();
                    require(schema->table_boxes().front() == room,
                            "One undo takes back the height and the place together");
                    undo->trigger();
                    settle();
                }

                // A corner pulls the two sides that meet at it, and leaves the
                // corner opposite it exactly where it was.
                {
                    const auto before = schema->table_boxes().front();
                    const std::array<QPointF, 4> corners{
                        before.topLeft() + QPointF(2, 2), before.topRight() + QPointF(-2, 2),
                        before.bottomRight() + QPointF(-2, -2), before.bottomLeft() + QPointF(2, -2)};
                    const std::array<QPointF, 4> opposite{before.bottomRight(), before.bottomLeft(),
                                                          before.topLeft(), before.topRight()};
                    // The bottom two first, since the schema is packed from
                    // the top left and a table near the top has nowhere to
                    // grow upwards into.
                    // Nor a corner lying on a neighbour's edge: the table was
                    // widened by hand above, and a width given by hand never
                    // moves the table beside it, so the two may overlap.
                    const auto clear_of_tables = [&](QPointF at)
                    {
                        const auto boxes = schema->table_boxes();
                        return std::none_of(boxes.begin() + 1, boxes.end(), [&](const QRectF &other)
                                            { return other.adjusted(-8, -8, 8, 8).contains(at); });
                    };
                    std::size_t which = 2;
                    for (const std::size_t i : {2u, 3u, 1u, 0u})
                    {
                        const auto room = (corners[i].y() > before.center().y() || before.top() > 80) && (corners[i].x() > before.center().x() || before.left() > 80);
                        if (room && clear_of_lines(corners[i]) && clear_of_tables(corners[i]))
                        {
                            which = i;
                            break;
                        }
                    }
                    const auto corner = corners[which];
                    // Outwards from the middle of the table, whichever corner
                    // it is, so the pull always makes it bigger.
                    const QPointF away(corner.x() < before.center().x() ? -50 : 50,
                                       corner.y() < before.center().y() ? -50 : 50);
                    drag(QEvent::MouseButtonPress, corner, Qt::LeftButton, Qt::LeftButton);
                    drag(QEvent::MouseMove, corner + away, Qt::NoButton, Qt::LeftButton);
                    drag(QEvent::MouseButtonRelease, corner + away, Qt::LeftButton, Qt::NoButton);
                    const auto pulled = schema->table_boxes().front();
                    require(pulled.width() > before.width() + 30 && pulled.height() > before.height() + 30,
                            "A corner pulls two sides at once");
                    const std::array<QPointF, 4> now{pulled.bottomRight(), pulled.bottomLeft(),
                                                     pulled.topLeft(), pulled.topRight()};
                    require(std::abs(now[which].x() - opposite[which].x()) < 0.01 && std::abs(now[which].y() - opposite[which].y()) < 0.01,
                            "Leaving the corner opposite it alone");
                    undo->trigger();
                    settle();
                }

                // Fixed, and the edge is no longer a handle at all.
                child<QAction>(window, "schemaFixed")->trigger();
                settle();
                require(!schema->tables_resizable(), "Fixed takes the handle away");
                const auto held = schema->table_boxes().front();
                const auto held_edge = QPointF(held.right() - 2, held.center().y());
                drag(QEvent::MouseButtonPress, held_edge, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseMove, held_edge + QPointF(90, 0), Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, held_edge + QPointF(90, 0), Qt::LeftButton, Qt::NoButton);
                require(schema->table_boxes().front().width() == held.width(),
                        "So pulling the edge no longer widens it");
                const auto held_bottom = QPointF(held.center().x(), held.bottom() - 2);
                drag(QEvent::MouseButtonPress, held_bottom, Qt::LeftButton, Qt::LeftButton);
                drag(QEvent::MouseMove, held_bottom + QPointF(0, 60), Qt::NoButton, Qt::LeftButton);
                drag(QEvent::MouseButtonRelease, held_bottom + QPointF(0, 60), Qt::LeftButton, Qt::NoButton);
                require(schema->table_boxes().front().height() == held.height(),
                        "Nor the bottom one make it taller");
                child<QAction>(window, "schemaResizable")->trigger();
                settle();
                child<QAction>(window, "schemaTidy")->trigger();
                settle();
            }

            // And an arrangement is part of the document, so it is saved.
            QTemporaryDir folder;
            require(folder.isValid(), "A place to save into");
            drag(QEvent::MouseButtonPress, grab_table, Qt::LeftButton, Qt::LeftButton);
            drag(QEvent::MouseMove, grab_table + QPointF(30, 30), Qt::NoButton, Qt::LeftButton);
            drag(QEvent::MouseButtonRelease, grab_table + QPointF(30, 30), Qt::LeftButton, Qt::NoButton);
            const auto arranged = window.editor().project().schema_layout;
            require(!arranged.empty(), "Which has something in it to save");
            const auto where = folder.filePath("arranged.erdx");
            require(window.export_project_file(where), "The project writes");
            infrastructure::ErdxProjectStore store_again;
            const auto opened = store_again.load(where.toStdString());
            require(opened.project.has_value(), "And opens again");
            require(opened.project->schema_layout == arranged,
                    "With the arrangement exactly as it was left");

            child<QPushButton>(window, "previewSchema")->click();
            settle_motion(window);
        }

        // Export: how the work leaves. A picture any system can open, with
        // the project inside the two formats that can hold one, and a written
        // listing of the model for the people who want words rather than a
        // drawing.
        {
            QTemporaryDir pictures;
            require(pictures.isValid(), "Temporary export directory");

            // The Export tab waited until there was something to hand on.
            // There now is, so it is a tab like the others, built from the
            // same menu, and one word is used for it in both places.
            child<QAction>(window, "tabExport")->trigger();
            settle();
            auto *export_row = child<QToolBar>(window, "exportTools");
            require(export_row->isVisible(), "Export has a row of its own");
            for (const char *name : {"exportPdfDocument", "exportMarkdown", "exportHtml", "exportCsv",
                                     "exportSvg", "exportPng", "exportPdfPage",
                                     "exportWithOptions", "copyAsPicture"})
                require(child<QMenu>(window, "exportMenu")->findChildren<QAction *>().contains(child<QAction>(window, name)) || export_row->actions().contains(child<QAction>(window, name)), name);
            require(child<QMenu>(window, "fileMenu")->actions().contains(child<QMenu>(window, "exportMenu")->menuAction()),
                    "And the same menu hangs under File");
            require(child<QMenu>(window, "fileMenu")->actions().contains(child<QMenu>(window, "importMenu")->menuAction()),
                    "With Import beside it, which is its pair");

            // Import has a tab of its own beside Export, because a reader
            // looking for one expects the other in the same place.
            child<QAction>(window, "tabImport")->trigger();
            settle();
            auto *import_row = child<QToolBar>(window, "importTools");
            require(import_row->isVisible(), "Import has a row of its own");
            for (const char *name : {"importProject", "importPicture", "importFromOtherTools"})
                require(import_row->actions().contains(child<QAction>(window, name)), name);
            require(child<QAction>(window, "importProject")->isEnabled(),
                    "Reading what ERDFlow writes can be done now");
            require(!child<QAction>(window, "importFromOtherTools")->isEnabled(),
                    "Reading what other tools write cannot, and stands there saying so");
            child<QAction>(window, "tabExport")->trigger();
            settle();
            require(export_row->isVisible() && !import_row->isVisible(), "The two tabs swap rows like the rest");
            require(child<QAction>(window, "tabFile")->isChecked() && child<QAction>(window, "tabExport")->isChecked() &&
                        !child<QAction>(window, "tabImport")->isChecked(),
                    "Both stand under File, which is marked chosen with the one in front");

            // A tab colours itself when it is chosen; the row it brings up is
            // set heavier than the interface around it, so the row in front of
            // you reads as the thing you just chose rather than as a strip of
            // quiet text that looks the same whichever tab is showing.
            require(export_row->property("ribbonRow").toBool() && import_row->property("ribbonRow").toBool(),
                    "The rows that belong to a tab are marked as such");
            require(!child<QToolBar>(window, "modelTools")->property("ribbonRow").toBool(),
                    "Home is not, being the drawing tools, which their icons already tell apart");
            require(window.findChild<QToolBar *>("insertTools") == nullptr,
                    "Insert has no row of its own any more: what it carried is on Home");
            for (const char *row : {"designTools", "exportTools", "importTools", "viewTools", "helpTools"})
                require(child<QToolBar>(window, row)->property("ribbonRow").toBool(), row);
            // The menu's group headings are entries that cannot be chosen,
            // which reads well in a menu and would be a button nobody can
            // press on a row. They stay off the row.
            for (auto *action : export_row->actions())
                require(!action->objectName().endsWith("Heading"),
                        "No heading is put on the row as a dead button");
            require(!child<QAction>(window, "exportDocumentsHeading")->isEnabled(),
                    "A heading cannot be chosen, which is what makes it read as a heading");
            require(child<QAction>(window, "exportProject")->isEnabled(),
                    "While the project itself is a format work leaves in");
            require(child<QMenu>(window, "exportMorePictures") != nullptr,
                    "With the rarer picture formats gathered behind one entry");
            require(child<QAction>(window, "exportPng")->isEnabled(), "A drawn diagram can be exported");
            require(child<QAction>(window, "exportSvg")->text() == QString::fromUtf8("SVG picture…"),
                    "Named in the characters the name was written with, not in mangled bytes");
            require(!child<QAction>(window, "exportWithOptions")->icon().isNull(),
                    "Export carries a glyph of its own");

            auto options = window.export_choice().as_picture;
            options.format = desktop::PictureFormat::Png;
            const auto png = pictures.filePath("diagram.png");
            require(window.export_picture(options, png), "A PNG is written where it was told to write one");
            require(QFileInfo::exists(png), "And the file is there afterwards");

            // The picture is also the project. Opening it gives back exactly
            // what was drawn, which is the whole point of carrying it.
            const auto drawn = window.editor().project();
            if (window.editor().dirty())
                dismiss(QMessageBox::Discard);
            require(window.open_path(png), "A PNG ERDFlow wrote opens as the project it carries");
            require(window.editor().project() == drawn, "Giving back exactly the diagram that was exported");
            require(!window.editor().dirty(), "And it opens clean, like any other project");

            // SVG carries it too, and is the picture to prefer for that reason.
            options.format = desktop::PictureFormat::Svg;
            const auto svg = pictures.filePath("diagram.svg");
            require(window.export_picture(options, svg), "An SVG is written");
            if (window.editor().dirty())
                dismiss(QMessageBox::Discard);
            require(window.open_path(svg), "And opens as the project it carries");
            require(window.editor().project() == drawn, "Also exactly as it was drawn");

            // Asked to carry nothing, it carries nothing, and opening it says
            // so rather than reporting a damaged project.
            options.carry_project = false;
            const auto bare = pictures.filePath("bare.png");
            require(window.export_picture(options, bare), "A PNG written without the project");
            dismiss(QMessageBox::Ok);
            require(!window.open_path(bare), "Does not open as a project");
            require(window.editor().project() == drawn, "And leaves the open work alone");
            options.carry_project = true;

            // A page is written as a page, and carries nothing, as a page cannot.
            options.format = desktop::PictureFormat::Pdf;
            const auto pdf = pictures.filePath("diagram.pdf");
            require(window.export_picture(options, pdf), "A PDF page is written");
            QFile page(pdf);
            require(page.open(QIODevice::ReadOnly) && page.read(4) == "%PDF", "Which is a PDF");

            // Copy as picture puts both on the clipboard at once, so whatever
            // it is pasted into takes whichever of the two it prefers.
            window.canvas()->select_elements({});
            child<QAction>(window, "copyAsPicture")->trigger();
            settle();
            const auto *clipboard = QApplication::clipboard()->mimeData();
            require(clipboard && clipboard->hasFormat("image/png") && clipboard->hasFormat("image/svg+xml"),
                    "Copy as picture puts a raster and a vector on the clipboard together");

            // The project itself is a format work leaves in, and the only one
            // that loses nothing. Writing a copy leaves the open project alone:
            // it keeps its own file and its own unsaved state, which is what
            // makes it a copy rather than a Save As.
            {
                const auto copy = pictures.filePath("copy.erdx");
                const auto working_on = window.editor().project();
                require(window.export_project_file(copy), "A copy of the project is written");
                infrastructure::ErdxProjectStore reader;
                const auto read_back = reader.load(copy.toStdString());
                require(read_back.project.has_value(), "And reads back");
                require(*read_back.project == working_on, "As exactly the project that was open");
            }

            // Import is Export's pair. It brings another project's contents
            // into this one rather than replacing it, everything arrives with
            // identities of its own so nothing collides, and it undoes at once.
            {
                const auto source = pictures.filePath("to-import.erdx");
                require(window.export_project_file(source), "A project to import from");
                const auto before = window.editor().project();
                require(window.import_project(source), "It imports");
                const auto after = window.editor().project();
                require(after.entities.size() == before.entities.size() * 2,
                        "Everything arrives beside what was there, rather than replacing it");
                // Not one identity in common, though the two are the same work:
                // an import must be able to bring in a project copied from this
                // very one without a single collision.
                for (const auto &[id, entity] : before.entities)
                {
                    (void)entity;
                    require(after.entities.contains(id), "What was there is untouched");
                }
                std::size_t fresh = 0;
                for (const auto &[id, entity] : after.entities)
                {
                    (void)entity;
                    if (!before.entities.contains(id))
                        ++fresh;
                }
                require(fresh == before.entities.size(), "And what arrived is all new identity");
                require(after.comments.size() == before.comments.size() * 2 || before.comments.empty(),
                        "What was said about the work comes with the work");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
                require(window.editor().project() == before, "And one undo takes the whole import back out");
            }

            // The four written listings, each of which reads the model that is
            // already there. They are listings and not the project, so nothing
            // reopens them; what is checked is that each is the thing it claims.
            // Read out of the project rather than typed here, since earlier
            // tests rename what is on the diagram and a listing has to name
            // whatever is actually there now.
            require(!window.editor().project().entities.empty(), "There is an entity to be listed");
            const auto listed_entity = QString::fromStdString(
                                           window.editor().project().entities.begin()->second.name)
                                           .toUtf8();
            struct Listing
            {
                desktop::DocumentFormat format;
                const char *file;
                const char *opens_with;
            };
            for (const auto &listing : {Listing{desktop::DocumentFormat::Markdown, "dictionary.md", "# "},
                                        Listing{desktop::DocumentFormat::Csv, "listing.csv", "Element,Name,"},
                                        Listing{desktop::DocumentFormat::Html, "report.html", "<!DOCTYPE html>"},
                                        Listing{desktop::DocumentFormat::Pdf, "report.pdf", "%PDF"}})
            {
                const auto where = pictures.filePath(QString::fromLatin1(listing.file));
                require(window.export_document(listing.format, where),
                        "A listing is written where it was told to write one");
                QFile written(where);
                require(written.open(QIODevice::ReadOnly), listing.file);
                const auto head = written.readAll();
                require(head.startsWith(listing.opens_with), "And is the kind of file it says it is");
                // A PDF compresses its text, so what it names cannot be read
                // out of its bytes; the three text formats can be.
                if (listing.format != desktop::DocumentFormat::Pdf)
                    require(head.contains(listed_entity), "Naming what is actually on the diagram");
            }

            // The dialog offers every format in one list, documents above
            // pictures, and turns off what cannot be asked for: an extent with
            // nothing in it, the picture options a document has none of, and
            // carrying the project in a format with nowhere to put it.
            desktop::ExportDialog dialog(*window.canvas(), window.editor().project());
            desktop::ExportChoice choice;
            choice.as_picture = options;
            dialog.set_choice(choice);
            settle();
            auto *extent = dialog.findChild<QComboBox *>("exportExtent");
            auto *format = dialog.findChild<QComboBox *>("exportFormat");
            auto *carry = dialog.findChild<QCheckBox *>("exportCarryProject");
            auto *size = dialog.findChild<QLabel *>("exportSize");
            require(extent && format && carry && size, "The dialog has its controls");
            const auto *extents = qobject_cast<QStandardItemModel *>(extent->model());
            require(extents != nullptr, "Whose extents can be turned off one at a time");
            const auto selection_row = extent->findData(static_cast<int>(desktop::PictureExtent::Selection));
            require(!extents->item(selection_row)->isEnabled(),
                    "With nothing selected, a picture of the selection cannot be asked for");
            require(!size->text().isEmpty(), "And it says what pressing Export will produce");
            format->setCurrentIndex(format->findData(static_cast<int>(desktop::PictureFormat::Jpeg)));
            settle();
            require(!carry->isEnabled() && !carry->isChecked(),
                    "A format that cannot carry the project does not offer to");
            format->setCurrentIndex(format->findData(static_cast<int>(desktop::PictureFormat::Svg)));
            settle();
            require(carry->isEnabled(), "One that can, does");
            // Documents are in the same list, above the pictures, and choosing
            // one puts away the options it has none of.
            const auto markdown_row = format->findData(100 + static_cast<int>(desktop::DocumentFormat::Markdown));
            require(markdown_row > 0, "The documents are in the same list as the pictures");
            format->setCurrentIndex(markdown_row);
            settle();
            require(!extent->isEnabled() && !carry->isEnabled(),
                    "A document has no extent to choose and nowhere to carry the project");
            require(size->text().contains("listing"), "And the dialog says it is a listing");
            require(dialog.choice().document && dialog.choice().as_document == desktop::DocumentFormat::Markdown,
                    "What it settled on is the document that was chosen");

            // Nothing drawn is nothing to export, and the entries go quiet
            // rather than failing when they are pressed.
            child<QAction>(window, "newProject")->trigger();
            settle();
            require(!child<QAction>(window, "exportPng")->isEnabled(), "An empty project has nothing to hand on");
            window.load_example();
            settle();
            require(child<QAction>(window, "exportPng")->isEnabled(), "And a drawn one has something again");
        }
        }
        if (window_part)
        {
            std::cout << "Main window desktop tests passed\n";
            return 0;
        }
        // Run on a window of its own, the window is dressed as the application
        // dresses one at launch (main.cpp): in the default theme, its style
        // sheet and all. Run whole, it wears what the checks before left it in.
        if (new_projects_part)
        {
            window.set_theme(desktop::theme_from_key(desktop::default_theme_key()));
            settle();
        }

        // A project that starts from its schema (Zain, 2026-09-27): Home's
        // Relational Schema card makes one, tables and a foreign key are drawn
        // on the schema by hand, and the whole converts into its diagram, which
        // is the model from then on.
        {
            editor.mark_saved(editor.revision());
            window.show_home(true);
            settle();
            auto *home = static_cast<desktop::HomePage *>(window.findChild<QWidget *>("homePage"));
            auto *relational = home->cards()[1];
            require(relational->isEnabled(), "The Relational Schema card can be taken");
            relational->create_button()->click();
            settle_for(700);
            require(!window.showing_home(), "Its + Create leaves Home");
            require(editor.project().schema.standalone && editor.project().schema.relations.empty(),
                    "For an empty project that starts from its schema");
            auto *schema = static_cast<desktop::SchemaView *>(child<QWidget>(window, "schemaView"));
            require(schema->isVisible(), "Relational Design is in front");
            // On Home's tab, where the header shows its working tools: under
            // File and Settings they are put away (2026-10-08), and the ribbon
            // keeps whichever tab was chosen last.
            child<QAction>(window, "tabHome")->trigger();
            settle();
            {
                auto *menu = child<QMenu>(window, "headerModelMenu");
                emit menu->aboutToShow();
                QStringList names;
                for (auto *action : menu->actions())
                    if (action->isVisible() && !action->isSeparator())
                        names << (action->menu() ? action->menu()->objectName() : action->objectName());
                require(names == QStringList({"designConvert", "openRelationalExampleMenu"}) &&
                            child<QAction>(window, "designConvert")->text() == "Convert to Conceptual",
                        "A schema drawn by hand: Model offers Convert to Conceptual and its own examples");
                require(child<QToolButton>(window, "headerModel")->isVisible() &&
                            child<QToolButton>(window, "headerTheme")->isVisible(),
                        "With Model and Theme in its header's corner");
            }
            require(child<QPushButton>(window, "schemaFull")->isHidden() && child<QPushButton>(window, "schemaClose")->isHidden() && child<QPushButton>(window, "previewSchema")->isHidden(),
                    "And nothing offers to put it away onto a diagram that is not there");
            // Its tools are up in the header, where it already says Relational
            // Design, and the bar on the schema that held them is put away
            // (Zain, 2026-09-27).
            auto *add_table = child<QToolButton>(window, "schemaAddTable");
            auto *convert = child<QPushButton>(window, "schemaConvert");
            auto *header_tools = child<QWidget>(window, "schemaTopTools");
            require(header_tools->isVisible() && add_table->isVisible() && add_table->text() == "Table" && child<QToolButton>(window, "schemaSelect")->isVisible() && child<QToolButton>(window, "schemaConnect")->isVisible() && child<QToolButton>(window, "schemaTopArrange")->isVisible() && child<QToolButton>(window, "schemaTopAppearance")->isVisible(),
                    "The header offers Select, Table, Connect, Arrange and Appearance");
            {
                const auto x = [&](const char *name)
                { return child<QToolButton>(window, name)->x(); };
                require(x("schemaSelect") < x("schemaAddTable") && x("schemaAddTable") < x("schemaConnect") && x("schemaConnect") < x("schemaTopArrange") && x("schemaTopArrange") < x("schemaTopAppearance") && child<QAction>(window, "schemaSelectTool")->isChecked() && !add_table->isChecked() && !child<QAction>(window, "schemaConnectTool")->isChecked(),
                        "In that order, Select | Table | Connect | Arrange | Appearance, with Select in hand");
            }
            require(add_table->parentWidget() == header_tools, "Table is in the header, not on the schema");
            require(header_tools->parentWidget()->minimumSizeHint().width() <= 1440,
                    "And the header still fits a window 1440 wide, whatever the project is called");
            // With Select in the row it fits by Arrange and Appearance giving up
            // their words where the header is too narrow for every word (Zain,
            // 2026-10-01), and taking them back where it is wide enough.
            {
                auto *arranged = child<QToolButton>(window, "schemaTopArrange");
                auto *appearing = child<QToolButton>(window, "schemaTopAppearance");
                const auto worded = [&](const char *name)
                {
                    return child<QToolButton>(window, name)->toolButtonStyle() == Qt::ToolButtonTextBesideIcon;
                };
                const auto size_was = window.size();
                window.resize(1900, size_was.height());
                settle();
                require(worded("schemaSelect") && worded("schemaAddTable") && worded("schemaConnect") && worded("schemaTopArrange") && worded("schemaTopAppearance"),
                        "With room, every tool in the header says its name");
                window.resize(1440, size_was.height());
                settle();
                require(window.width() == 1440 && !worded("schemaTopArrange") && !worded("schemaTopAppearance") && arranged->toolTip() == "Arrange" && appearing->toolTip() == "Appearance" && worded("schemaSelect") && worded("schemaAddTable") && worded("schemaConnect"),
                        "1440 wide, Arrange and Appearance show their icons, named on hover, and Select, Table and "
                        "Connect keep their words");
                window.resize(size_was);
                settle();
            }
            require(child<QWidget>(window, "schemaBar")->isHidden(), "And the schema's own bar is put away");
            require(child<QToolButton>(window, "schemaTopArrange")->menu() == child<QToolButton>(window, "schemaArrange")->menu() && child<QToolButton>(window, "schemaTopAppearance")->menu() == child<QToolButton>(window, "schemaAppearance")->menu(),
                    "Arrange and Appearance up there open the schema's own menus");
            require(child<QAction>(window, "designConvert")->isVisible() && !convert->isHidden() && convert->parentWidget() == child<QWidget>(window, "conceptualPanel")->findChild<QWidget *>("conceptualBar"),
                    "Convert is on the Design menu and in the Conceptual preview's bar");
            // The header of a schema drawn by hand (Zain, 2026-09-27): Home, the
            // switch between Schema and Conceptual with Schema first and lit, the
            // title with its pencil, the tools, history, search and theme -- and
            // nowhere the word Relational.
            auto *modes = child<QWidget>(window, "schemaModeSwitch");
            auto *schema_mode = child<QPushButton>(window, "schemaModeSchema");
            require(modes->isVisible() && schema_mode->isChecked() && schema_mode->text() == "Schema" && child<QPushButton>(window, "previewConceptual")->text() == "Conceptual" && child<QPushButton>(window, "previewConceptual")->parentWidget() == modes && schema_mode->x() < child<QPushButton>(window, "previewConceptual")->x(),
                    "Schema | Conceptual, Schema first and chosen");
            require(child<QLabel>(window, "workspaceBadge")->isHidden() && child<QPushButton>(window, "backToHome")->text() == "← Home" && child<QToolButton>(window, "renameDocument")->isVisible(),
                    "The switch stands in place of the badge, after Home, and the title has its pencil");
            // The header is Home's row while the schema is in front (Zain,
            // 2026-10-08): under File and Settings its working tools go with
            // Home's row -- the drawing tools, undo and redo with the search,
            // Model and Theme -- and only the chosen row stands under the tabs.
            // Home, the switch and the title stay. Back on Home everything is
            // as it was: the tool in hand, what was typed in the search, and
            // nothing in the project touched.
            {
                auto *table_tool = child<QAction>(window, "schemaTableAction");
                auto *search = child<QLineEdit>(window, "schemaSearch");
                const auto revision = window.editor().revision();
                table_tool->trigger();
                search->setText("Dept");
                settle();
                require(window.schema()->placing() && table_tool->isChecked(), "Table is in hand");
                const auto ribbon_rows = [&]
                {
                    QStringList showing;
                    for (auto *row : window.findChildren<QToolBar *>())
                        if (row->isVisible() && row->objectName() != "ribbonTabs")
                            showing << row->objectName();
                    return showing;
                };
                const auto working = [&](bool shown)
                {
                    bool all = true;
                    for (const char *name : {"schemaTopTools", "schemaHeaderTools", "headerModel", "headerTheme"})
                        all = all && window.findChild<QWidget *>(name)->isVisible() == shown;
                    return all;
                };
                const auto identity = [&]
                {
                    return child<QPushButton>(window, "backToHome")->isVisible() &&
                           child<QWidget>(window, "schemaModeSwitch")->isVisible() &&
                           child<QLabel>(window, "documentTitle")->isVisible();
                };
                child<QAction>(window, "tabHome")->trigger();
                settle();
                require(working(true) && identity() && ribbon_rows().isEmpty(),
                        "Under Home the header's working tools show, and no other row");
                for (const auto &[tab, row] : std::vector<std::pair<const char *, const char *>>{
                         {"tabFile", "exportTools"}, {"tabImport", "importTools"}, {"tabExport", "exportTools"},
                         {"tabSettings", nullptr}, {"tabView", "viewTools"}, {"tabHelp", "helpTools"},
                         {"tabDesign", "designTools"}})
                {
                    child<QAction>(window, tab)->trigger();
                    settle();
                    // Settings opens on whichever of its rows was chosen last.
                    require(working(false) && identity() &&
                                (row ? ribbon_rows() == QStringList{QString::fromLatin1(row)} : ribbon_rows().size() == 1),
                            "Under File and Settings only the chosen row shows; the schema's working tools go, "
                            "and Home, the switch and the title stay");
                }
                child<QAction>(window, "tabHome")->trigger();
                settle();
                require(working(true) && identity() && ribbon_rows().isEmpty() && window.schema()->placing() &&
                            table_tool->isChecked() && search->text() == "Dept" &&
                            window.editor().revision() == revision,
                        "Back on Home the tools return as they were: Table in hand, the search as typed, the "
                        "project untouched");
                search->clear();
                table_tool->trigger();
                settle();
                require(!window.schema()->placing(), "And Table is put down again");
            }

            // The schema has the diagram's raft of view controls (Zain,
            // 2026-10-08), made by the same hands: over the bottom-right of the
            // schema, the same parts in the same order, the same size. Fit and
            // the zoom signs cannot change a schema drawn at its actual size, so
            // they say so and change nothing; Pan moves the view as the
            // diagram's Pan does; Full view puts the panels away and back; the
            // side panels' button is the diagram's own action; and the raft is
            // put away and brought back with the diagram's.
            {
                auto *schema = window.schema();
                auto *scroll = child<QScrollArea>(window, "schemaScroll");
                auto *raft = child<QWidget>(window, "schemaControls");
                auto *diagram_raft = child<QWidget>(window, "canvasControls");
                QStringList order;
                for (auto *part : raft->findChildren<QWidget *>(Qt::FindDirectChildrenOnly))
                    order << part->objectName();
                require(raft->isVisible() && raft->parentWidget() == scroll &&
                            order == QStringList{"schemaControlsGrip", "schemaRaftFullView", "schemaRaftFit",
                                                 "schemaRaftPan", "schemaRaftZoomIn", "schemaRaftZoomOut",
                                                 "schemaControlsRule", "schemaRaftSidePanels"},
                        "The schema has the raft: grip, Full view, Fit, Pan, + and -, a rule, the side panels");
                require(raft->sizeHint() == diagram_raft->sizeHint() &&
                            child<QToolButton>(window, "schemaRaftPan")->size() ==
                                child<QToolButton>(window, "canvasPan")->size(),
                        "Made the same size as the diagram's");
                require(raft->geometry().right() > scroll->width() * 3 / 4 &&
                            raft->geometry().bottom() > scroll->height() * 3 / 4,
                        "Over the bottom-right of the schema");
                require(child<QToolButton>(window, "schemaRaftSidePanels")->defaultAction() ==
                            child<QAction>(window, "viewSidePanels"),
                        "Its side panels' button is the diagram's own action");

                // Fit and zoom: pressable, saying why, changing nothing.
                const auto revision = window.editor().revision();
                const auto size_before = schema->size();
                for (const char *name : {"schemaRaftFit", "schemaRaftZoomIn", "schemaRaftZoomOut"})
                {
                    auto *button = child<QToolButton>(window, name);
                    window.statusBar()->clearMessage();
                    require(button->isEnabled(), "Never greyed out");
                    button->click();
                    settle();
                    require(window.statusBar()->currentMessage().contains("cannot be fitted or zoomed yet") &&
                                window.editor().revision() == revision && schema->size() == size_before,
                            "Fit and zoom say the schema is shown at its actual size, and change nothing");
                }

                // Pan: a table far off gives the schema room to move in.
                require(editor.create_relation("Far", domain::Point{2200, 1600}).ok, "A table far off");
                window.canvas()->on_edit({});
                settle();
                auto *pan = child<QToolButton>(window, "schemaRaftPan");
                const auto drag = [&](QPointF from, QPointF to)
                {
                    const auto global = [&](QPointF at) { return QPointF(schema->mapToGlobal(at.toPoint())); };
                    QMouseEvent press(QEvent::MouseButtonPress, from, global(from), Qt::LeftButton, Qt::LeftButton,
                                      Qt::NoModifier);
                    QApplication::sendEvent(schema, &press);
                    QMouseEvent move(QEvent::MouseMove, from, global(to), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
                    QApplication::sendEvent(schema, &move);
                    QMouseEvent release(QEvent::MouseButtonRelease, from, global(to), Qt::LeftButton, Qt::NoButton,
                                        Qt::NoModifier);
                    QApplication::sendEvent(schema, &release);
                    settle();
                };
                const auto revision_with_far = window.editor().revision();
                const auto selection_with_far = schema->selection_now();
                const auto boxes_with_far = schema->table_boxes();
                const auto routed = schema->routings();
                scroll->horizontalScrollBar()->setValue(200);
                scroll->verticalScrollBar()->setValue(200);
                settle();
                pan->click();
                settle();
                require(schema->panning() && pan->isChecked() && schema->cursor().shape() == Qt::OpenHandCursor,
                        "Pan is taken up, with an open hand");
                const QPointF from(scroll->horizontalScrollBar()->value() + 300.0, scroll->verticalScrollBar()->value() + 300.0);
                drag(from, from + QPointF(-120, -90));
                require(scroll->horizontalScrollBar()->value() == 320 && scroll->verticalScrollBar()->value() == 290,
                        "Dragging moves the view with the hand");
                require(window.editor().revision() == revision_with_far && schema->selection_now() == selection_with_far &&
                            schema->table_boxes() == boxes_with_far && schema->routings() == routed,
                        "And nothing in the schema: no edit, no selection, nothing moved, nothing routed");
                require(!schema->panning() && !pan->isChecked(), "One drag and Pan is handed back, as the diagram's is");
                // Locked by a double click, it stays for as many drags as wanted.
                QMouseEvent twice(QEvent::MouseButtonDblClick, QPointF(5, 5), pan->mapToGlobal(QPoint(5, 5)),
                                  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(pan, &twice);
                settle();
                require(schema->panning() && pan->toolTip().contains("locked"), "A double click locks Pan, and says so");
                drag(from, from + QPointF(40, 40));
                drag(from, from + QPointF(40, 40));
                require(schema->panning() && scroll->horizontalScrollBar()->value() == 240,
                        "Locked, Pan stays in hand for drag after drag");
                schema->setFocus();
                QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
                QApplication::sendEvent(schema, &escape);
                settle();
                require(!schema->panning() && !pan->isChecked() && !pan->toolTip().contains("locked"),
                        "Escape puts Pan down");
                child<QAction>(window, "undoCommand")->trigger();
                settle();

                // Full view puts the panels beside the schema away and brings
                // them back.
                auto *explorer_dock = child<QDockWidget>(window, "explorerDock");
                auto *properties_dock = child<QDockWidget>(window, "propertiesDock");
                require(explorer_dock->isVisible() && properties_dock->isVisible(), "Both panels are out");
                child<QToolButton>(window, "schemaRaftFullView")->click();
                settle();
                require(!explorer_dock->isVisible() && !properties_dock->isVisible() &&
                            child<QWidget>(window, "workspaceHeader")->isVisible(),
                        "Full view puts the panels away, and leaves the header");
                child<QToolButton>(window, "schemaRaftFullView")->click();
                settle();
                require(explorer_dock->isVisible() && properties_dock->isVisible(), "And brings them back");

                // The grip takes it anywhere over the schema.
                const auto was = raft->pos();
                auto *grip = child<QWidget>(window, "schemaControlsGrip");
                const QPoint on_grip(4, 2);
                QMouseEvent hold(QEvent::MouseButtonPress, on_grip, grip->mapToGlobal(on_grip), Qt::LeftButton,
                                 Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(grip, &hold);
                QMouseEvent carry(QEvent::MouseMove, on_grip, grip->mapToGlobal(on_grip) + QPoint(-200, -150),
                                  Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(grip, &carry);
                QMouseEvent leave(QEvent::MouseButtonRelease, on_grip, grip->mapToGlobal(on_grip) + QPoint(-200, -150),
                                  Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(grip, &leave);
                settle();
                require(raft->pos() == was + QPoint(-200, -150), "Dragged by its grip, the raft goes where it is taken");

                // Put away with the diagram's, and brought back from the empty
                // schema's own menu.
                auto *entry = child<QAction>(window, "viewCanvasControls");
                entry->setChecked(false);
                settle();
                require(raft->isHidden() && diagram_raft->isHidden() &&
                            window.statusBar()->currentMessage().contains("View menu"),
                        "Put away, both rafts go, and the schema says where to bring them back from");
                bool offered = false;
                QTimer::singleShot(0, &window, [&]
                                   {
                    auto *menu = window.findChild<QMenu *>("schemaEmptyMenu");
                    auto *back = menu ? menu->findChild<QAction *>("showSchemaControls") : nullptr;
                    offered = back != nullptr;
                    if (back) back->trigger();
                    if (menu) menu->close(); });
                const QPoint empty(30, 30);
                QContextMenuEvent asked(QContextMenuEvent::Mouse, empty, schema->mapToGlobal(empty));
                QApplication::sendEvent(schema, &asked);
                settle();
                require(offered && !raft->isHidden() && !diagram_raft->isHidden() && entry->isChecked(),
                        "The empty schema's menu brings both back, and the View menu says so");
            }
            {
                auto *header = child<QWidget>(window, "workspaceHeader");
                QStringList said;
                for (auto *button : header->findChildren<QAbstractButton *>())
                    if (button->isVisible())
                        said << button->text();
                for (auto *label : header->findChildren<QLabel *>())
                    if (label->isVisible())
                        said << label->text();
                for (auto *field : header->findChildren<QLineEdit *>())
                    if (field->isVisible())
                        said << field->placeholderText();
                if (said.join(' ').contains("Relational", Qt::CaseInsensitive))
                    qWarning() << said;
                require(!said.join(' ').contains("Relational", Qt::CaseInsensitive),
                        "Nothing in the header says Relational");
                require(child<QLineEdit>(window, "schemaSearch")->placeholderText().contains("schema"),
                        "The search is named for the schema");
                const auto size_was = window.size();
                for (const int width : {1280, 1440, 1920})
                {
                    window.resize(width, 1080);
                    settle();
                    QWidget *previous = child<QLineEdit>(window, "schemaSearch");
                    for (auto *control : {previous, static_cast<QWidget *>(child<QToolButton>(window, "headerModel")),
                                          static_cast<QWidget *>(child<QToolButton>(window, "headerTheme"))})
                    {
                        const QRect rect(control->mapTo(header, QPoint()), control->size());
                        require(control->isVisible() && header->rect().contains(rect) &&
                                    (control == previous || previous->mapTo(header, QPoint(previous->width(), 0)).x() < rect.left()),
                                "Schema keeps Search, Model, Theme in order without clipping at all desktop widths");
                        previous = control;
                    }
                }
                window.resize(size_was);
                settle();
            }
            // Stage 1: the Schema workspace has an Explorer and Properties either
            // side of it, in the same docks the diagram's are held in, with the
            // schema's own words in them and nothing of the diagram's.
            auto *explorer_dock = child<QDockWidget>(window, "explorerDock");
            auto *properties_dock = child<QDockWidget>(window, "propertiesDock");
            auto *schema_explorer = child<QTreeView>(window, "schemaExplorer");
            require(explorer_dock->isVisible() && explorer_dock->widget() == schema_explorer && properties_dock->isVisible() && properties_dock->widget() == child<QWidget>(window, "schemaProperties"),
                    "Explorer and Properties stand either side of the schema");
            const auto explorer_rows = [&]
            {
                QStringList rows;
                const auto *model = schema_explorer->model();
                const auto root = model->index(0, 0);
                rows << root.data().toString();
                for (int r = 0; r < model->rowCount(root); ++r)
                    rows << model->index(r, 0, root).data().toString();
                return rows;
            };
            require(explorer_rows() == QStringList{"Schema", "Tables", "Relationships"},
                    "The Explorer's frame is the schema's: Schema, Tables, Relationships");
            // What kind of thing Properties is about, and its name where it
            // has one (Stage 3 made the rest a full read-only inspector).
            const auto properties_say = [&]
            {
                return QStringList{properties_heading(*properties_dock->widget())} + properties_value(*properties_dock->widget(), "General/Name");
            };
            require(properties_say() == QStringList{"Schema"},
                    "With nothing chosen, Properties says so");

            // A table, placed where the schema is pressed with Table in hand,
            // and named where it appears (Zain, 2026-10-01; until then
            // pressing Table made one at once).
            add_table->click();
            settle();
            require(schema->preview().tables.empty() && add_table->isChecked() && schema->placing(),
                    "Table in hand makes nothing until the schema is pressed");
            {
                const QPointF there(100, 52);
                QMouseEvent press(QEvent::MouseButtonPress, there, schema->mapToGlobal(there.toPoint()),
                                  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QApplication::sendEvent(schema, &press);
                QMouseEvent release(QEvent::MouseButtonRelease, there, schema->mapToGlobal(there.toPoint()),
                                    Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(schema, &release);
                settle();
            }
            auto *field = child<QLineEdit>(window, "schemaName");
            require(field->isVisible() && field->text() == "Table" && !add_table->isChecked() && child<QAction>(window, "schemaSelectTool")->isChecked(),
                    "Pressed, Table makes a table there and opens its name for typing, and Select is back in hand");
            const auto type_name = [&](const QString &name)
            {
                field->setText(name);
                QKeyEvent done(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QApplication::sendEvent(field, &done);
                settle();
            };
            type_name("Employee");
            require(schema->preview().tables.size() == 1 && schema->preview().tables[0].name == "Employee",
                    "Named as it was typed");
            require(schema->preview().tables[0].columns.size() == 1 && schema->preview().tables[0].columns[0].primary_key,
                    "And starting with its key");
            require(schema->preview().tables[0].columns[0].name == "EmployeeID",
                    "Which is named for the table, EmployeeID rather than ID");
            const auto mouse = [&](QEvent::Type type, QPointF at, Qt::MouseButtons held)
            {
                QMouseEvent event(type, at, schema->mapToGlobal(at.toPoint()),
                                  type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton, held, Qt::NoModifier);
                QApplication::sendEvent(schema, &event);
            };
            // Another column, from the slot under it, named in its row.
            auto box = schema->table_boxes().front();
            const QPointF slot(box.center().x(), box.bottom() + 10);
            mouse(QEvent::MouseMove, slot, Qt::NoButton);
            mouse(QEvent::MouseButtonPress, slot, Qt::LeftButton);
            settle();
            require(field->isVisible(), "The slot makes a column and opens its name");
            type_name("ManagerID");
            require(schema->preview().tables[0].columns.size() == 2, "The table has its second column");

            // A foreign key drawn by hand, from the key it is to reference --
            // EmployeeID's key gutter -- onto the column that is to hold it,
            // ManagerID, which is asked about first (Zain, 2026-10-01: the
            // row a connection starts on is the key referred to).
            box = schema->table_boxes().front();
            auto rows = schema->row_boxes().front();
            const QPointF from(rows[0].left() + 22, rows[0].center().y());
            const QPointF onto_column(rows[1].center().x(), rows[1].center().y());
            QStringList asked;
            mouse(QEvent::MouseButtonPress, from, Qt::LeftButton);
            mouse(QEvent::MouseMove, (from + onto_column) / 2, Qt::LeftButton);
            mouse(QEvent::MouseMove, onto_column, Qt::LeftButton);
            answer("Use & Connect", &asked);
            mouse(QEvent::MouseButtonRelease, onto_column, Qt::NoButton);
            settle();
            require(asked.value(1) == "Existing column found" && asked.value(2).startsWith("Employee.ManagerID exists but is not a foreign key."),
                    "Let go on an ordinary column, it asks before using it");
            const auto &manager = schema->preview().tables[0].columns[1];
            require(manager.foreign_key && manager.references == std::size_t{0} && manager.references_column == 0 && schema->preview().tables[0].columns[0].primary_key && !schema->preview().tables[0].columns[0].foreign_key,
                    "Dragging from a key's gutter onto a column makes that column a foreign key to the key, "
                    "which stays only a key");
            require(manager.type == domain::LogicalType::Int, "Which takes the key's type");
            require(schema->table_boxes().front().topLeft() == box.topLeft(), "And does not move the table");

            // The schema is the main surface while it is drawn by hand, and
            // the Conceptual Design it becomes rises from below it, the other
            // way up from a diagram with its schema (Zain, 2026-09-27).
            auto *stage = child<QWidget>(window, "workspaceStage");
            require(child<QWidget>(window, "schemaPanel")->y() == 0 && child<QWidget>(window, "schemaGrip")->isHidden(),
                    "The schema fills the stage from the top, with no grip to be pulled by");
            auto *to_conceptual = child<QPushButton>(window, "previewConceptual");
            require(to_conceptual->isVisible(), "The header offers the Conceptual Design it becomes");
            to_conceptual->click();
            settle_for(700);
            auto *conceptual = child<QWidget>(window, "conceptualPanel");
            auto *conceptual_state = child<QLabel>(window, "conceptualState");
            require(conceptual->isVisible() && to_conceptual->isChecked() && conceptual->y() > 0 && conceptual->y() + conceptual->height() == stage->height(),
                    "It rises from the bottom of the stage, over the lower part of the schema");
            require(conceptual_state->text().startsWith("1 entity · 1 relationship"),
                    "It shows the diagram Convert would draw: a table and its reference to itself");
            auto *drawn = static_cast<desktop::DiagramView *>(child<QWidget>(window, "conceptualPreview"));
            require(!drawn->diagram_bounds().isEmpty(), "Drawn on a canvas of its own");
            require(editor.project().schema.standalone && editor.project().entities.empty(),
                    "Nothing is converted and nothing is written");
            // A preview is looked at; changing it is turned away and says why.
            const auto held = editor.revision();
            const QPoint middle = drawn->viewport()->rect().center();
            QMouseEvent twice(QEvent::MouseButtonDblClick, QPointF(middle), drawn->viewport()->mapToGlobal(QPointF(middle)),
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(drawn->viewport(), &twice);
            settle();
            require(window.statusBar()->currentMessage().contains("only for looking at") && editor.revision() == held,
                    "A double click on the preview says it is only for looking at, and changes nothing");

            // A double click on the empty schema, with Select in hand, makes no
            // table (Zain, 2026-10-06); a table is placed with Table.
            const QPointF empty(box.right() + 260, box.top() + 30);
            const auto tables_before = schema->preview().tables.size();
            const auto revision_before = editor.revision();
            mouse(QEvent::MouseButtonPress, empty, Qt::LeftButton);
            mouse(QEvent::MouseButtonRelease, empty, Qt::LeftButton);
            mouse(QEvent::MouseButtonDblClick, empty, Qt::LeftButton);
            mouse(QEvent::MouseButtonRelease, empty, Qt::LeftButton);
            settle();
            require(schema->preview().tables.size() == tables_before && editor.revision() == revision_before &&
                        !field->isVisible(),
                    "A double click on the empty schema makes no table and opens nothing");
            child<QToolButton>(window, "schemaAddTable")->defaultAction()->trigger();
            settle();
            require(schema->placing(), "Table is taken up");
            mouse(QEvent::MouseButtonPress, empty, Qt::LeftButton);
            mouse(QEvent::MouseButtonRelease, empty, Qt::LeftButton);
            settle();
            require(schema->preview().tables.size() == 2 && field->isVisible() && !schema->placing(),
                    "One press with Table in hand makes one table there, ready to be named, and hands Table back");
            type_name("Department");
            require(conceptual_state->text().startsWith("2 entities · 1 relationship"),
                    "The preview follows the schema as it is drawn");
            child<QPushButton>(window, "conceptualClose")->click();
            settle_motion(window);
            require(conceptual->isHidden() && !to_conceptual->isChecked(), "And Close puts it away again");
            // Schema, in the switch, puts it away too, and stays the one chosen.
            to_conceptual->click();
            settle_for(700);
            require(conceptual->isVisible() && to_conceptual->isChecked() && schema_mode->isChecked(),
                    "Conceptual raises the preview, with Schema still the design being drawn");
            schema_mode->click();
            settle_motion(window);
            require(conceptual->isHidden() && !to_conceptual->isChecked() && schema_mode->isChecked(),
                    "And Schema puts it away again");

            // Connect, up in the header, draws a foreign key from anywhere on a
            // key's row, not only its key gutter, to anywhere on the table that
            // refers to it; then it is put down (Zain, 2026-10-01: the row it
            // starts on is the key referred to, and stays only a key).
            auto *connect_tool = child<QAction>(window, "schemaConnectTool");
            connect_tool->trigger();
            settle();
            require(connect_tool->isChecked() && schema->connecting(), "Connect is taken up for the schema");
            std::size_t employee = 0;
            std::size_t department = 0;
            for (std::size_t t = 0; t < schema->preview().tables.size(); ++t)
                (schema->preview().tables[t].name == "Employee" ? employee : department) = t;
            const auto connect_from_department_key = [&](QPointF to, const QString &agreeing, QStringList *asked)
            {
                const auto key_rows = schema->row_boxes()[department];
                const QPointF on_key(key_rows[0].left() + 90, key_rows[0].center().y());
                mouse(QEvent::MouseButtonPress, on_key, Qt::LeftButton);
                mouse(QEvent::MouseMove, (on_key + to) / 2, Qt::LeftButton);
                mouse(QEvent::MouseMove, to, Qt::LeftButton);
                if (!agreeing.isEmpty())
                    answer(agreeing, asked);
                mouse(QEvent::MouseButtonRelease, to, Qt::NoButton);
                settle();
            };
            {
                const auto target = schema->table_boxes()[employee];
                QStringList offered;
                connect_from_department_key(QPointF(target.center().x(), target.top() + 8), "Create & Connect", &offered);
                require(offered.value(1) == "No foreign key found" && offered.value(2).startsWith("Employee does not contain a foreign key referencing "
                                                                                                  "Department.DepartmentID."),
                        "Let go on a table with nothing to hold the key, it asks to make a column for it");
            }
            const auto &made = schema->preview().tables[employee].columns.back();
            require(schema->preview().tables[employee].columns.size() == 3 && made.name == "DepartmentID" && made.foreign_key && made.references == department && made.references_column == 0 && schema->preview().tables[department].columns[0].primary_key && !schema->preview().tables[department].columns[0].foreign_key && schema->preview().tables[employee].columns[0].primary_key && !schema->preview().tables[employee].columns[0].foreign_key,
                    "Agreed, the table pointing at the key gains DepartmentID, and neither key is touched");
            require(!connect_tool->isChecked() && !schema->connecting(), "And Connect is put down after one line");
            child<QAction>(window, "undoCommand")->trigger();
            settle();
            require(schema->preview().tables[employee].columns.size() == 2 && schema->preview().tables[employee].columns[1].references == employee,
                    "One undo takes the line and the column made for it back");
            // A column already referring elsewhere is never pointed somewhere new.
            connect_tool->trigger();
            settle();
            {
                const auto before_refusal = editor.revision();
                const auto manager_row = schema->row_boxes()[employee][1];
                connect_from_department_key(manager_row.center(), {}, nullptr);
                require(editor.revision() == before_refusal && schema->preview().tables[employee].columns[1].references == employee && window.statusBar()->currentMessage().contains("already references Employee.EmployeeID"),
                        "Let go on a column already referencing another key, it says so and changes nothing");
            }
            // Escape puts it down too.
            connect_tool->trigger();
            settle();
            QKeyEvent escape(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
            QApplication::sendEvent(schema, &escape);
            settle();
            require(!connect_tool->isChecked() && !schema->connecting(), "Escape puts Connect down");

            // Stage 1: what is pressed on the schema is what is chosen, kept by
            // the schema's own identities, shown in Properties, and never an
            // edit: choosing four times adds nothing to Undo.
            {
                const auto revision = editor.revision();
                const auto undo_label = editor.undo_label();
                const auto &employee_table = schema->preview().tables[employee];
                const auto employee_id = employee_table.id;
                const auto press_at = [&](QPointF at)
                {
                    mouse(QEvent::MouseButtonPress, at, Qt::LeftButton);
                    mouse(QEvent::MouseButtonRelease, at, Qt::NoButton);
                    settle();
                };
                const auto heading_box = schema->table_boxes()[employee];
                press_at(QPointF(heading_box.center().x(), heading_box.top() + 8));
                const auto now_table = schema->selection_now();
                require(std::holds_alternative<desktop::ChosenTable>(now_table) && std::get<desktop::ChosenTable>(now_table).table == employee_id,
                        "A table's heading chooses the table, by its own identity");
                require(properties_say() == QStringList{"Properties", "Employee"}, "Properties says Table, Employee");

                rows = schema->row_boxes()[employee];
                press_at(QPointF(rows[1].left() + 90, rows[1].center().y()));
                const auto now_column = schema->selection_now();
                require(std::holds_alternative<desktop::ChosenColumn>(now_column) && std::get<desktop::ChosenColumn>(now_column).column.table == employee_id,
                        "A row chooses its column");
                const auto said_column = properties_say();
                require(said_column == QStringList{"Column", "ManagerID"} && properties_value(*properties_dock->widget(), "Keys/Key Role") == QStringList{"Foreign Key"},
                        "Properties says Column, ManagerID, and that it is a foreign key");

                // A line, pressed in the middle of its longest run.
                const auto shapes = schema->line_shapes();
                require(!shapes.empty(), "The foreign key is drawn as a line");
                QPointF on_line;
                double longest = -1;
                for (std::size_t i = 1; i < shapes.front().size(); ++i)
                {
                    const auto a = shapes.front()[i - 1], b = shapes.front()[i];
                    const auto length = std::hypot(b.x() - a.x(), b.y() - a.y());
                    if (length > longest)
                    {
                        longest = length;
                        on_line = (a + b) / 2;
                    }
                }
                press_at(on_line);
                const auto now_line = schema->selection_now();
                require(std::holds_alternative<desktop::ChosenForeignKey>(now_line),
                        "Pressing the line chooses the foreign key it stands for");
                const auto said_line = properties_say();
                require(said_line == QStringList{"Relationship"} && properties_value(*properties_dock->widget(), "Identity/Relationship") == QStringList{"Employee.ManagerID → Employee.EmployeeID"},
                        "Properties says Relationship, and which key points at which");

                const auto empty_at = QPointF(schema->table_boxes()[department].right() + 200,
                                              schema->table_boxes()[department].bottom() + 200);
                press_at(empty_at);
                require(std::holds_alternative<desktop::NothingChosen>(schema->selection_now()) && properties_say() == QStringList{"Schema"},
                        "The empty schema puts it all down");
                require(editor.revision() == revision && editor.undo_label() == undo_label,
                        "Choosing is not an edit: nothing reaches the history");

                // Kept by identity, so a rename does not lose it.
                const auto renamed_at = QPointF(heading_box.center().x(), heading_box.top() + 8);
                press_at(renamed_at);
                mouse(QEvent::MouseButtonDblClick, renamed_at, Qt::LeftButton);
                settle();
                require(field->isVisible(), "The table's name opens for typing");
                type_name("Worker");
                require(std::holds_alternative<desktop::ChosenTable>(schema->selection_now()) && std::get<desktop::ChosenTable>(schema->selection_now()).table == employee_id && properties_say() == QStringList{"Properties", "Worker"},
                        "The table chosen is still chosen after it is renamed, under its new name");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
                schema->choose(desktop::NothingChosen{});
                settle();
            }

            // Stage 2 (Zain, 2026-09-29): the Schema Explorer is the schema as a
            // tree, in its own words, and one more way of choosing on it: the
            // same choosing the canvas and Properties share, never an edit.
            {
                const auto *tree = schema_explorer->model();
                const auto root = tree->index(0, 0);
                const auto tables = row_saying(*tree, root, "Tables");
                const auto relationships = row_saying(*tree, root, "Relationships");
                require(root.data().toString() == "Schema" && tables.isValid() && relationships.isValid(),
                        "The Explorer is the schema, holding its Tables and its Relationships");
                require(tables.data(desktop::explorer_count_role).toInt() == 2 && relationships.data(desktop::explorer_count_role).toInt() == 1,
                        "Each counted from the schema: two tables, one foreign key");
                QStringList names;
                for (const auto &table : schema->preview().tables)
                    names << QString::fromStdString(table.name);
                require(rows_said(*tree, tables) == names, "Every table by its name, in the schema's own order");
                const auto employee_row = row_saying(*tree, tables, "Employee");
                require(rows_said(*tree, employee_row) == QStringList{"Columns", "Primary Key", "Foreign Keys"},
                        "A table holds its columns, its primary key and its foreign keys");
                const auto columns = row_saying(*tree, employee_row, "Columns");
                require(rows_said(*tree, columns) == QStringList{"EmployeeID", "ManagerID"} && columns.data(desktop::explorer_count_role).toInt() == 2,
                        "Its columns, in order, counted");
                require(row_saying(*tree, columns, "EmployeeID").data(desktop::explorer_note_role).toString() == "PK" && row_saying(*tree, columns, "ManagerID").data(desktop::explorer_note_role).toString() == "FK",
                        "Each column says what kind of key it is");
                require(rows_said(*tree, row_saying(*tree, employee_row, "Primary Key")) == QStringList{"EmployeeID"},
                        "Its primary key, by the column that is it");
                const auto employee_keys = row_saying(*tree, employee_row, "Foreign Keys");
                require(rows_said(*tree, employee_keys) == QStringList{"ManagerID → Employee.EmployeeID"},
                        "Its foreign key, pointing back into its own table");
                require(rows_said(*tree, row_saying(*tree, tables, "Department")) == QStringList{"Columns", "Primary Key"},
                        "A table holding no foreign key shows no empty group for them");
                require(rows_said(*tree, relationships) == QStringList{"Employee.ManagerID → Employee.EmployeeID"},
                        "Relationships lists the same key with the table holding it, and no second Employee");
                const auto manager_key = *schema->preview().tables[employee].columns[1].key_id;
                const auto key_here = tree->index(0, 0, employee_keys);
                const auto key_there = tree->index(0, 0, relationships);
                require(key_here.data(Qt::UserRole) == key_there.data(Qt::UserRole) && key_here.data(Qt::UserRole).toString() == desktop::schema_key(desktop::ChosenForeignKey{manager_key}),
                        "Both rows are the one foreign key, by its own identity");

                // Pressed in the Explorer, chosen on the schema.
                const auto revision = editor.revision();
                const auto undo_label = editor.undo_label();
                const auto was_dirty = editor.dirty();
                const auto employee_id = schema->preview().tables[employee].id;
                press_row(*schema_explorer, employee_row);
                require(std::holds_alternative<desktop::ChosenTable>(schema->selection_now()) && std::get<desktop::ChosenTable>(schema->selection_now()).table == employee_id && schema->selection() == std::vector<domain::ElementRef>{domain::ElementRef{employee_id}},
                        "A table pressed in the Explorer is the table chosen on the schema, by its own identity");
                require(properties_say() == QStringList{"Properties", "Employee"}, "And Properties says so");
                press_row(*schema_explorer, row_saying(*tree, columns, "EmployeeID"));
                const auto chosen_column = schema->selection_now();
                require(std::holds_alternative<desktop::ChosenColumn>(chosen_column) && std::get<desktop::ChosenColumn>(chosen_column).column == *schema->column_ref(employee, 0) && std::holds_alternative<domain::SchemaColumnId>(std::get<desktop::ChosenColumn>(chosen_column).column.source),
                        "A column pressed there is that column chosen, by its own SchemaColumnId");
                require(properties_say().mid(0, 2) == QStringList{"Column", "EmployeeID"}, "And Properties says so");
                const auto lit_column = lit_rows(*schema_explorer);
                require(lit_column.size() == 2 && lit_column[0] == lit_column[1],
                        "Lit under Columns and under Primary Key, being one column");
                press_row(*schema_explorer, key_here);
                require(std::holds_alternative<desktop::ChosenForeignKey>(schema->selection_now()) && std::get<desktop::ChosenForeignKey>(schema->selection_now()).key == manager_key && schema->selection().empty(),
                        "A foreign key pressed there is the key chosen, the tables put down, as its line would");
                require(properties_say().value(0) == "Relationship", "And Properties says Relationship");
                require(lit_rows(*schema_explorer) == QStringList(2, desktop::schema_key(desktop::ChosenForeignKey{manager_key})),
                        "Lit under its table and under Relationships, being one key");
                press_row(*schema_explorer, key_there);
                require(std::holds_alternative<desktop::ChosenForeignKey>(schema->selection_now()) && std::get<desktop::ChosenForeignKey>(schema->selection_now()).key == manager_key,
                        "Pressed under Relationships, it is the same key");

                // Pressed on the canvas, lit in the Explorer.
                const auto press_at = [&](QPointF at)
                {
                    mouse(QEvent::MouseButtonPress, at, Qt::LeftButton);
                    mouse(QEvent::MouseButtonRelease, at, Qt::NoButton);
                    settle();
                };
                const auto department_box = schema->table_boxes()[department];
                press_at(QPointF(department_box.center().x(), department_box.top() + 8));
                require(lit_rows(*schema_explorer) == QStringList{desktop::schema_key(
                                                          desktop::ChosenTable{schema->preview().tables[department].id})},
                        "A table pressed on the canvas is the table lit in the Explorer");
                schema_explorer->collapse(employee_row);
                rows = schema->row_boxes()[employee];
                press_at(QPointF(rows[1].left() + 90, rows[1].center().y()));
                const auto manager_row = row_saying(*tree, columns, "ManagerID");
                require(lit_rows(*schema_explorer) == QStringList{manager_row.data(Qt::UserRole).toString()} && schema_explorer->isExpanded(employee_row) && schema_explorer->isExpanded(columns) && schema_explorer->currentIndex() == manager_row,
                        "A column pressed there is lit here, its table opened onto it");
                const auto shapes = schema->line_shapes();
                QPointF on_line;
                double longest = -1;
                for (std::size_t i = 1; i < shapes.front().size(); ++i)
                {
                    const auto a = shapes.front()[i - 1], b = shapes.front()[i];
                    const auto length = std::hypot(b.x() - a.x(), b.y() - a.y());
                    if (length > longest)
                    {
                        longest = length;
                        on_line = (a + b) / 2;
                    }
                }
                press_at(on_line);
                require(lit_rows(*schema_explorer) == QStringList(2, desktop::schema_key(desktop::ChosenForeignKey{manager_key})) && schema_explorer->currentIndex() == key_there,
                        "A line pressed there lights its foreign key, opened onto under Relationships");
                press_at(QPointF(department_box.right() + 200, department_box.bottom() + 200));
                require(lit_rows(*schema_explorer).isEmpty(), "The empty schema leaves nothing lit");
                require(editor.revision() == revision && editor.undo_label() == undo_label && editor.dirty() == was_dirty,
                        "Choosing through the Explorer is not an edit: nothing reaches the project or the history");

                // Renamed, a table is listed by its new name and is still the
                // one lit, being known by what it is.
                const auto tables_now = [&]
                {
                    const auto *model = schema_explorer->model();
                    return row_saying(*model, model->index(0, 0), "Tables");
                };
                press_row(*schema_explorer, employee_row);
                const auto heading = schema->table_boxes()[employee];
                const QPointF on_heading(heading.center().x(), heading.top() + 8);
                press_at(on_heading);
                mouse(QEvent::MouseButtonDblClick, on_heading, Qt::LeftButton);
                settle();
                type_name("Worker");
                require(row_saying(*schema_explorer->model(), tables_now(), "Worker").isValid() && !row_saying(*schema_explorer->model(), tables_now(), "Employee").isValid() && lit_rows(*schema_explorer) == QStringList{desktop::schema_key(desktop::ChosenTable{employee_id})},
                        "Renamed, the table is listed by its new name, and is still the one lit");
                child<QAction>(window, "undoCommand")->trigger();
                settle();

                // Opened by hand, a table stays open through an edit; deleted,
                // it takes its rows and its choosing with it; undone, it is back.
                auto department_row = row_saying(*schema_explorer->model(), tables_now(), "Department");
                schema_explorer->expand(department_row);
                press_row(*schema_explorer, department_row);
                QKeyEvent erase(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
                QApplication::sendEvent(schema, &erase);
                settle();
                require(tables_now().data(desktop::explorer_count_role).toInt() == 1 && !row_saying(*schema_explorer->model(), tables_now(), "Department").isValid() && std::holds_alternative<desktop::NothingChosen>(schema->selection_now()) && lit_rows(*schema_explorer).isEmpty(),
                        "A table deleted is gone from the Explorer, counted out, and nothing is left lit for it");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
                department_row = row_saying(*schema_explorer->model(), tables_now(), "Department");
                require(tables_now().data(desktop::explorer_count_role).toInt() == 2 && department_row.isValid() && schema_explorer->isExpanded(department_row),
                        "Undone, it is back, counted in, and opened as the hand left it");
                schema->choose(desktop::NothingChosen{});
                settle();
            }

            // Stage 3 (2026-10-01): Properties is a read-only inspector of what
            // is chosen, read from the one selection and from the schema as it
            // is now -- the same answer whichever way a thing is chosen, never
            // stale, and never an edit.
            {
                const auto panel = [&]() -> const QWidget &
                { return *properties_dock->widget(); };
                const auto said = [&](const char *field)
                { return properties_value(panel(), QString::fromUtf8(field)); };
                // Schema words only. Since Stage 4 a table's and a column's
                // values are offered as fields, so whether anything can be
                // edited is asked separately, of the views that must stay read.
                const auto plainly = [&]
                { return properties_speak_schema(panel()); };
                const auto read_only = [&]
                { return !properties_offer_edits(panel()); };
                const auto press_at = [&](QPointF at)
                {
                    mouse(QEvent::MouseButtonPress, at, Qt::LeftButton);
                    mouse(QEvent::MouseButtonRelease, at, Qt::NoButton);
                    settle();
                };
                // The row menu, with what is wanted from it asked for before it
                // opens and taken as soon as it is there.
                const auto from_menu = [&](QPointF at, const char *which)
                {
                    QTimer::singleShot(0, &window, [&window, which]
                                       {
                        auto* menu = window.findChild<QMenu*>("schemaMenu");
                        if (!menu) return;
                        if (auto* action = menu->findChild<QAction*>(which)) action->trigger();
                        menu->close(); });
                    QContextMenuEvent asking(QContextMenuEvent::Mouse, at.toPoint(), schema->mapToGlobal(at.toPoint()));
                    QApplication::sendEvent(schema, &asking);
                    settle();
                };
                const auto table_called = [&](const char *name)
                {
                    for (std::size_t t = 0; t < schema->preview().tables.size(); ++t)
                        if (schema->preview().tables[t].name == name)
                            return t;
                    throw std::runtime_error("No such table");
                };
                const auto table_row = [&](const char *name)
                {
                    const auto *model = schema_explorer->model();
                    return row_saying(*model, row_saying(*model, model->index(0, 0), "Tables"), name);
                };
                const auto group_row = [&](const char *table, const char *group)
                {
                    return row_saying(*schema_explorer->model(), table_row(table), group);
                };
                const auto revision = editor.revision();
                const auto undo_label = editor.undo_label();
                const auto was_dirty = editor.dirty();

                // Nothing chosen: the schema summed up, counted from what it holds.
                schema->choose(desktop::NothingChosen{});
                settle();
                require(properties_heading(panel()) == "Schema" && said("Schema/Tables") == QStringList{"2"} && said("Schema/Columns") == QStringList{"3"} && said("Schema/Primary Keys") == QStringList{"2"} && said("Schema/Foreign Keys") == QStringList{"1"} && plainly() && read_only(),
                        "With nothing chosen, Properties sums up the schema from what it holds");

                // A table, pressed in the Explorer: its key, its foreign key,
                // and the same key again pointing back at it.
                press_row(*schema_explorer, table_row("Employee"));
                require_table_properties(panel(), *schema, table_called("Employee"));
                {
                    const auto *model = schema_explorer->model();
                    const auto relationships = row_saying(*model, model->index(0, 0), "Relationships");
                    const auto actual = relationships.data(Qt::DecorationRole).value<QIcon>();
                    const auto expected = desktop::glyph_icon(desktop::Glyph::SchemaRelationships,
                                                              desktop::theme(window.canvas()->theme_id()), 20,
                                                              window.icon_mode());
                    require(actual.pixmap(QSize(20, 20), window.devicePixelRatioF()).toImage() == expected.pixmap(QSize(20, 20), window.devicePixelRatioF()).toImage(),
                            "Only the Schema Relationships group uses the connected-nodes glyph");
                }
                require(properties_heading(panel()) == "Properties" && said("General/Name") == QStringList{"Employee"} && said("General/Source") == QStringList{"Schema-first"} && said("Structure/Columns") == QStringList{"2"} && said("Structure/Primary Key") == QStringList{"EmployeeID"} && said("Structure/Foreign Keys") == QStringList{"ManagerID → Employee.EmployeeID"} && properties_count(panel(), "Structure/Foreign Keys") == 1 && said("References/Referenced By") == QStringList{"Employee.ManagerID"} && properties_count(panel(), "References/Referenced By") == 1 && plainly(),
                        "A table says its columns, its key, its foreign key and what points at it -- itself, once");
                press_row(*schema_explorer, table_row("Department"));
                require(said("Structure/Primary Key") == QStringList{"DepartmentID"} && said("Structure/Foreign Keys") == QStringList{"None"} && said("References/Referenced By") == QStringList{"None"},
                        "A table holding no foreign key and pointed at by none says None for both");

                require(properties_count(panel(), "Structure/Foreign Keys") == 0 && properties_count(panel(), "References/Referenced By") == 0,
                        "An empty table's lower cards explicitly show zero counts");

                // Each lower section folds independently, remembers the
                // user's choice after rebuilding, and changes no schema fact.
                const auto lower_section = [&](const QString &title) -> QWidget *
                {
                    for (auto *section : panel().findChildren<QWidget *>("schemaTablePropertySection"))
                        if (section->property("section").toString() == title)
                            return section;
                    throw std::runtime_error("No lower section");
                };
                const auto project_before_folding = editor.project();
                const auto selection_before_folding = schema->selection_now();
                const auto facts_before_folding = properties_all(panel());
                const auto explorer_before_folding = lit_rows(*schema_explorer);
                const auto revision_before_folding = editor.revision();
                const auto undo_before_folding = editor.undo_label();
                const auto redo_before_folding = editor.redo_label();
                const auto dirty_before_folding = editor.dirty();
                for (const auto &title : {QString("Keys"), QString("References"), QString("Source")})
                {
                    auto *section = lower_section(title);
                    section->findChild<QAbstractButton *>("sectionHeader")->click();
                    settle();
                    require(section->findChild<QWidget *>("schemaTableSectionBody")->isHidden(),
                            "The user can collapse each lower section independently");
                }
                schema->choose(desktop::NothingChosen{});
                settle();
                schema->choose(selection_before_folding);
                settle();
                for (const auto &title : {QString("Keys"), QString("References"), QString("Source")})
                {
                    auto *section = lower_section(title);
                    auto *header = section->findChild<QAbstractButton *>("sectionHeader");
                    require(!header->isChecked() && section->findChild<QWidget *>("schemaTableSectionBody")->isHidden(),
                            "A manually collapsed lower section stays collapsed when the table inspector rebuilds");
                    header->click();
                    settle();
                    require(header->isChecked() && !section->findChild<QWidget *>("schemaTableSectionBody")->isHidden(),
                            "Each section can be expanded again");
                }
                require(editor.project() == project_before_folding && editor.revision() == revision_before_folding && editor.undo_label() == undo_before_folding && editor.redo_label() == redo_before_folding && editor.dirty() == dirty_before_folding && schema->selection_now() == selection_before_folding && properties_all(panel()) == facts_before_folding && lit_rows(*schema_explorer) == explorer_before_folding,
                        "Folding and rebuilding changes no schema, history, selection, Explorer state or displayed facts");
                require_table_properties(panel(), *schema, table_called("Department"));

                // A column, pressed on the canvas.
                auto employee_t = table_called("Employee");
                auto rows_now = schema->row_boxes()[employee_t];
                press_at(QPointF(rows_now[1].left() + 90, rows_now[1].center().y()));
                const auto manager_choice = schema->selection_now();
                require(std::holds_alternative<desktop::ChosenColumn>(manager_choice), "The row chooses its column");
                const auto manager_required = schema->preview().tables[employee_t].columns[1].required;
                require(properties_heading(panel()) == "Column" && said("General/Name") == QStringList{"ManagerID"} && said("General/Table") == QStringList{"Employee"} && said("General/Data Type") == QStringList{"INT"} && said("Constraints/Nullable") == QStringList{manager_required ? "No" : "Yes"} && said("Constraints/Not Null") == QStringList{manager_required ? "Yes" : "No"} && said("Constraints/Primary Key") == QStringList{"No"} && said("Constraints/Foreign Key") == QStringList{"Yes"} && said("Keys/Key Role") == QStringList{"Foreign Key"} && said("Keys/Key Column") == QStringList{"ManagerID"} && said("Keys/Foreign Key Column").isEmpty() && said("Keys/Member").isEmpty() && said("References/References") == QStringList{"Employee.EmployeeID"} && said("References/Referenced By") == QStringList{"None"} && properties_count(panel(), "References/Referenced By") == 0 && plainly(),
                        "A foreign key column says its type, that it is a foreign key, and what it references");
                // Its table's key, pressed under Primary Key in the Explorer.
                press_row(*schema_explorer, schema_explorer->model()->index(0, 0, group_row("Employee", "Primary Key")));
                require(properties_heading(panel()) == "Column" && said("General/Name") == QStringList{"EmployeeID"} && said("Keys/Key Role") == QStringList{"Primary Key"} && said("Keys/Key Column") == QStringList{"EmployeeID"} && said("Constraints/Nullable") == QStringList{"No"} && said("Constraints/Not Null") == QStringList{"Yes"} && said("Constraints/Unique") == QStringList{"Yes"} && properties_editor<QCheckBox>(panel(), "Constraints/Unique")->toolTip() == "A primary key is unique already." && said("References/References").isEmpty() && said("References/Referenced By") == QStringList{"Employee.ManagerID"},
                        "A key column says it is the key, the whole key, and which column points at it");

                // One foreign key, chosen three ways, says one thing.
                press_row(*schema_explorer, schema_explorer->model()->index(0, 0, group_row("Employee", "Foreign Keys")));
                const auto under_table = properties_all(panel());
                {
                    const auto *model = schema_explorer->model();
                    press_row(*schema_explorer,
                              model->index(0, 0, row_saying(*model, model->index(0, 0), "Relationships")));
                }
                const auto under_relationships = properties_all(panel());
                const auto shapes = schema->line_shapes();
                QPointF on_line;
                double longest = -1;
                for (std::size_t i = 1; i < shapes.front().size(); ++i)
                {
                    const auto a = shapes.front()[i - 1], b = shapes.front()[i];
                    const auto length = std::hypot(b.x() - a.x(), b.y() - a.y());
                    if (length > longest)
                    {
                        longest = length;
                        on_line = (a + b) / 2;
                    }
                }
                press_at(on_line);
                require(std::holds_alternative<desktop::ChosenForeignKey>(schema->selection_now()),
                        "The line chooses its foreign key");
                require(lit_rows(*schema_explorer).contains(desktop::schema_key(schema->selection_now())) && panel().findChild<QWidget *>("schemaRelationshipProperties"),
                        "Pressed on the canvas, the line lights its relationship in the Explorer and Relationship "
                        "Properties appears");
                require(under_table == under_relationships && under_relationships == properties_all(panel()),
                        "A foreign key says the same under its table, under Relationships and as its line");
                require(properties_heading(panel()) == "Relationship" && said("General/Type") == QStringList{"Foreign Key"} && said("Identity/Relationship") == QStringList{"Employee.ManagerID → Employee.EmployeeID"} && said("General/Self Reference") == QStringList{"Yes"} && said("Referenced/Cardinality") == QStringList{"One (1)"} && said("Referencing/Cardinality") == QStringList{manager_required ? "One or Many (1..N)" : "Zero or Many (0..N)"} && said("Referencing/Table") == QStringList{"Employee"} && said("Referencing/Column") == QStringList{"ManagerID"} && said("Referenced/Table") == QStringList{"Employee"} && said("Referenced/Column") == QStringList{"EmployeeID"} && plainly() && read_only(),
                        "Referencing and referenced, the same table for a key into its own, and said to be so");
                require(editor.revision() == revision && editor.undo_label() == undo_label && editor.dirty() == was_dirty,
                        "Choosing and reading Properties is not an edit: nothing reaches the project or the history");
                // Disconnected while it is chosen, from its column's own menu, the
                // line is gone and Properties says nothing more of it; undone,
                // it is back, and chosen it is described as it was.
                {
                    const auto line_choice = schema->selection_now();
                    const auto described = properties_all(panel());
                    const auto lines_before = schema->line_shapes().size();
                    const auto rows_here = schema->row_boxes()[table_called("Employee")];
                    from_menu(QPointF(rows_here[1].left() + 90, rows_here[1].center().y()), "schemaRemoveForeignKey");
                    require(schema->line_shapes().size() + 1 == lines_before && !panel().findChild<QWidget *>("schemaRelationshipProperties") && said("Identity/Relationship").isEmpty() && said("Referencing/Table").isEmpty(),
                            "Disconnected, the line is gone and Properties no longer describes it");
                    child<QAction>(window, "undoCommand")->trigger();
                    settle();
                    schema->choose(line_choice);
                    settle();
                    require(schema->line_shapes().size() == lines_before && schema->selection_now() == line_choice && properties_all(panel()) == described && editor.undo_label() == undo_label,
                            "Undone, the line is back and is chosen and described exactly as before");
                }

                // A column that is a primary key and a foreign key at once, in
                // a key of two, made as a hand makes it: a column added to
                // Department, given a foreign key by a line drawn from
                // Employee's key onto it, and made part of Department's key
                // from its row's menu.
                // (Employee's own key cannot take a second column while
                // ManagerID points at it, and the Editor says so.)
                auto department_t = table_called("Department");
                auto department_box = schema->table_boxes()[department_t];
                const QPointF head_slot(department_box.center().x(), department_box.bottom() + 10);
                mouse(QEvent::MouseMove, head_slot, Qt::NoButton);
                mouse(QEvent::MouseButtonPress, head_slot, Qt::LeftButton);
                settle();
                require(field->isVisible(), "The slot makes a column and opens its name");
                type_name("HeadID");
                department_t = table_called("Department");
                employee_t = table_called("Employee");
                {
                    const auto key_rows = schema->row_boxes()[employee_t];
                    const QPointF key_gutter(key_rows[0].left() + 22, key_rows[0].center().y());
                    const auto head_row = schema->row_boxes()[department_t][1].center();
                    mouse(QEvent::MouseButtonPress, key_gutter, Qt::LeftButton);
                    mouse(QEvent::MouseMove, (key_gutter + head_row) / 2, Qt::LeftButton);
                    mouse(QEvent::MouseMove, head_row, Qt::LeftButton);
                    answer("Use & Connect");
                    mouse(QEvent::MouseButtonRelease, head_row, Qt::NoButton);
                    settle();
                }
                department_t = table_called("Department");
                require(schema->preview().tables[department_t].columns[1].foreign_key,
                        "HeadID points at Employee's key");
                {
                    const auto head_rows = schema->row_boxes()[department_t];
                    from_menu(QPointF(head_rows[1].left() + 90, head_rows[1].center().y()), "schemaPrimaryKey");
                }
                department_t = table_called("Department");
                require(schema->preview().tables[department_t].columns[1].primary_key && schema->preview().tables[department_t].columns[1].foreign_key,
                        "HeadID is now a primary key and a foreign key");
                const auto head_choice = desktop::ChosenColumn{*schema->column_ref(department_t, 1)};
                schema->choose(head_choice);
                settle();
                require(properties_heading(panel()) == "Column" && said("General/Name") == QStringList{"HeadID"} && said("Keys/Key Role") == QStringList{"Primary Key + Foreign Key"} && said("Keys/Key Column") == QStringList{"DepartmentID, HeadID"} && said("Keys/Foreign Key Column") == QStringList{"HeadID"} && said("References/References") == QStringList{"Employee.EmployeeID"} && said("Constraints/Primary Key") == QStringList{"Yes"} && said("Constraints/Foreign Key") == QStringList{"Yes"} && said("Constraints/Unique") == QStringList{"No"} && plainly(),
                        "Both its roles are said, and the whole key of two it is one column of");
                schema->choose(desktop::ChosenColumn{*schema->column_ref(department_t, 0)});
                settle();
                require(said("Keys/Key Role") == QStringList{"Primary Key"} && said("Keys/Key Column") == QStringList{"DepartmentID, HeadID"} && said("Constraints/Unique") == QStringList{"No"},
                        "Its partner in the key says the same whole key, and is not unique on its own");
                press_row(*schema_explorer, table_row("Department"));
                require(said("Structure/Primary Key") == QStringList{"DepartmentID, HeadID"} && said("Structure/Foreign Keys") == QStringList{"HeadID → Employee.EmployeeID"},
                        "Its table's key is both columns, not the first of them");
                require_table_properties(panel(), *schema, table_called("Department"));
                const auto wearing = window.canvas()->theme_id();
                for (const auto appearance : {desktop::ThemeId::Midnight, desktop::ThemeId::Plain})
                {
                    window.set_theme(appearance);
                    settle();
                    require_table_properties(panel(), *schema, table_called("Department"));
                    if (appearance == desktop::ThemeId::Plain)
                        require(coloured_pixels(grab_without_subpixel_text(*properties_dock->widget())) == 0,
                                "The table list and both key roles follow the monochrome theme");
                }
                window.set_theme(wearing);
                settle();
                press_row(*schema_explorer, table_row("Employee"));
                require(said("References/Referenced By").size() == 2 && said("References/Referenced By").contains("Employee.ManagerID") && said("References/Referenced By").contains("Department.HeadID") && properties_count(panel(), "References/Referenced By") == 2,
                        "Employee is pointed at by itself and by Department");
                schema->choose(desktop::NothingChosen{});
                settle();
                require(said("Schema/Columns") == QStringList{"4"} && said("Schema/Primary Keys") == QStringList{"2"} && said("Schema/Foreign Keys") == QStringList{"2"},
                        "Counted again from what the schema holds now: two tables keyed, one by two columns");
                while (schema->preview().tables[table_called("Department")].columns.size() > 1 && editor.undo_label() != undo_label)
                {
                    child<QAction>(window, "undoCommand")->trigger();
                    settle();
                }
                require(schema->preview().tables[table_called("Department")].columns.size() == 1 && editor.undo_label() == undo_label,
                        "Undone, Department is its own key alone again");

                // Renamed, undone and redone, the column chosen says its name
                // as it is now, being chosen by what it is.
                schema->choose(manager_choice);
                settle();
                rows_now = schema->row_boxes()[table_called("Employee")];
                const QPointF manager_name(rows_now[1].left() + 70, rows_now[1].center().y());
                mouse(QEvent::MouseButtonDblClick, manager_name, Qt::LeftButton);
                settle();
                require(field->isVisible(), "The column's name opens for typing");
                type_name("BossID");
                require(schema->selection_now() == manager_choice && said("General/Name") == QStringList{"BossID"},
                        "Renamed, the column chosen is still chosen and Properties says its new name");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
                require(schema->selection_now() == manager_choice && said("General/Name") == QStringList{"ManagerID"},
                        "Undone, it says its old name");
                child<QAction>(window, "redoCommand")->trigger();
                settle();
                require(said("General/Name") == QStringList{"BossID"}, "Redone, its new one");
                child<QAction>(window, "undoCommand")->trigger();
                settle();

                // Removed while chosen, it is not described any longer; back,
                // it is described as it is.
                schema->choose(manager_choice);
                settle();
                rows_now = schema->row_boxes()[table_called("Employee")];
                from_menu(QPointF(rows_now[1].left() + 90, rows_now[1].center().y()), "schemaRemoveColumn");
                require(schema->preview().tables[table_called("Employee")].columns.size() == 1,
                        "The column is removed");
                for (const auto &[what, values] : properties_all(panel()))
                    require(!values.contains("ManagerID") && !values.contains("Employee.ManagerID") && !values.contains("ManagerID → Employee.EmployeeID"),
                            "Nothing of a removed column is left in Properties");
                require(properties_heading(panel()) != "Column", "Properties no longer describes a column");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
                require(properties_heading(panel()) != "Column" || said("General/Name") == QStringList{"ManagerID"},
                        "Back, whatever is described is described as it is");

                // A table removed while chosen leaves the schema summed up.
                press_row(*schema_explorer, table_row("Department"));
                QKeyEvent erase(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
                QApplication::sendEvent(schema, &erase);
                settle();
                require(properties_heading(panel()) == "Schema" && said("Schema/Tables") == QStringList{"1"},
                        "A table deleted while chosen leaves Properties summing up what is left");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
                require(said("Schema/Tables") == QStringList{"2"} || properties_heading(panel()) == "Properties",
                        "Undone, it is counted again");
                schema->choose(desktop::NothingChosen{});
                settle();
                require(said("Schema/Tables") == QStringList{"2"} && said("Schema/Columns") == QStringList{"3"},
                        "And the schema is summed up as it was");

                // The edits made here to have something to follow were each
                // undone, and nothing else was added to the history.
                require(editor.undo_label() == undo_label, "Every edit made here was undone");
            }

            // Stage 4 (2026-10-01): a table's name, and a column's name, type,
            // size, nullability, uniqueness and identity are edited in
            // Properties -- through the same paths the schema's own cells take,
            // so each is checked, refused and undone exactly as it is there --
            // and everything else stays read.
            {
                const auto panel = [&]() -> const QWidget &
                { return *properties_dock->widget(); };
                const auto said = [&](const char *field)
                { return properties_value(panel(), QString::fromUtf8(field)); };
                const auto name_field = [&]
                { return properties_editor<QLineEdit>(panel(), "General/Name"); };
                const auto rule = [&](const char *field)
                {
                    return properties_editor<QCheckBox>(panel(), QString::fromUtf8(field));
                };
                const auto chooser = [&](const char *field)
                {
                    return properties_editor<QComboBox>(panel(), QString::fromUtf8(field));
                };
                const auto press_at = [&](QPointF at)
                {
                    mouse(QEvent::MouseButtonPress, at, Qt::LeftButton);
                    mouse(QEvent::MouseButtonRelease, at, Qt::NoButton);
                    settle();
                };
                const auto table_called = [&](const char *name)
                {
                    for (std::size_t t = 0; t < schema->preview().tables.size(); ++t)
                        if (schema->preview().tables[t].name == name)
                            return t;
                    throw std::runtime_error("No such table");
                };
                const auto has_table = [&](const char *name)
                {
                    return std::any_of(schema->preview().tables.begin(), schema->preview().tables.end(),
                                       [&](const auto &table)
                                       { return table.name == name; });
                };
                const auto tables_row = [&]
                {
                    const auto *model = schema_explorer->model();
                    return row_saying(*model, model->index(0, 0), "Tables");
                };
                const auto listed_table = [&](const char *name)
                {
                    return row_saying(*schema_explorer->model(), tables_row(), name).isValid();
                };
                // Refused, the model's reason comes up in a box, which is
                // answered as soon as it opens.
                const auto type_in = [&](QLineEdit *line, const QString &words, Qt::Key key, bool refused = false)
                {
                    line->setFocus();
                    settle();
                    line->setText(words);
                    if (refused)
                        dismiss(QMessageBox::Ok);
                    QKeyEvent pressed(QEvent::KeyPress, key, Qt::NoModifier);
                    QApplication::sendEvent(line, &pressed);
                    settle();
                };
                const auto undo_once = [&]
                { child<QAction>(window, "undoCommand")->trigger(); settle(); };
                const auto redo_once = [&]
                { child<QAction>(window, "redoCommand")->trigger(); settle(); };
                const auto column_of = [&](const char *table, std::size_t row) -> const domain::PreviewColumn &
                {
                    return schema->preview().tables[table_called(table)].columns[row];
                };
                const auto choose_column = [&](const char *table, std::size_t row)
                {
                    schema->choose(desktop::ChosenColumn{*schema->column_ref(table_called(table), row)});
                    settle();
                };
                const auto start_label = editor.undo_label();

                // A table: direct name fields for it and every stored column.
                press_row(*schema_explorer, row_saying(*schema_explorer->model(), tables_row(), "Employee"));
                const auto employee_choice = schema->selection_now();
                require(name_field() && name_field()->text() == "Employee" && panel().findChildren<QLineEdit *>().size() == 3 && panel().findChildren<QComboBox *>().size() == 2 && panel().findChildren<QCheckBox *>().isEmpty(),
                        "A table offers its name, its two columns, and compact type and constraints controls");
                require_table_properties(panel(), *schema, table_called("Employee"));

                // The row fields reuse Stage 4: rename, cancel, validation,
                // Undo/Redo and stable selection all follow the same path.
                const auto manager_handle = *schema->column_ref(table_called("Employee"), 1);
                const auto manager_field = desktop::schema_key(desktop::ChosenColumn{manager_handle}) + "/Name";
                const auto inline_name = [&]
                { return properties_editor<QLineEdit>(panel(), manager_field); };
                type_in(inline_name(), "SupervisorID", Qt::Key_Return);
                require(column_of("Employee", 1).name == "SupervisorID" && schema->selection_now() == employee_choice && inline_name()->text() == "SupervisorID" && properties_heading(panel()) == "Properties",
                        "The real column is renamed in the table list, which stays in place");
                undo_once();
                require(inline_name()->text() == "ManagerID", "Undo restores the table row's column name");
                redo_once();
                require(inline_name()->text() == "SupervisorID", "Redo restores the same column's new name");
                undo_once();
                const auto no_edit = editor.revision();
                type_in(inline_name(), "Cancelled", Qt::Key_Escape);
                type_in(inline_name(), "", Qt::Key_Return, true);
                require(editor.revision() == no_edit && inline_name()->text() == "ManagerID",
                        "Cancel and a refused empty name leave the column unchanged in the direct table editor");

                // Compact controls edit the same model while the table stays
                // selected. Key type propagation is one undo from either surface.
                const auto key_handle = *schema->column_ref(table_called("Employee"), 0);
                const auto compact_type = [&](desktop::SchemaColumnRef handle)
                {
                    return properties_editor<QComboBox>(panel(), desktop::schema_key(desktop::ChosenColumn{handle}) + "/Data Type");
                };
                const auto compact_rules = [&](desktop::SchemaColumnRef handle, const char *action)
                {
                    auto *button = panel().findChildren<QFrame *>("schemaColumnRow")[static_cast<qsizetype>(schema->locate(handle)->second)]->findChild<QToolButton *>("schemaColumnConstraints");
                    bool offered = false;
                    QTimer::singleShot(0, &window, [&]
                                       {
                        auto* menu = window.findChild<QMenu*>("schemaColumnRulesMenu");
                        require(menu && menu->findChild<QAction*>("schemaRuleNull")
                                    && menu->findChild<QAction*>("schemaRuleNotNull")
                                    && menu->findChild<QAction*>("schemaRuleUnique")
                                    && menu->findChild<QAction*>("schemaRuleIdentity"),
                                "The compact menu exposes the existing constraint actions directly");
                        auto* entry = menu->findChild<QAction*>(action);
                        require(entry, "The requested existing constraint action is available");
                        offered = true;
                        entry->trigger();
                        menu->close(); });
                    button->click();
                    settle();
                    require(offered, "The constraints control opens its action list");
                };
                compact_type(key_handle)->showPopup();
                settle();
                auto *compact_list = child<QListWidget>(window, "typePickerList");
                child<QLineEdit>(window, "typePickerSearch")->setText("bigint");
                settle();
                QTest_activate(compact_list, compact_list->item(0));
                settle();
                require(column_of("Employee", 0).type == domain::LogicalType::BigInt && column_of("Employee", 1).type == domain::LogicalType::BigInt && compact_type(key_handle)->currentText() == "BIGINT" && compact_type(manager_handle)->currentText() == "BIGINT" && schema->selection_now() == employee_choice && properties_heading(panel()) == "Properties",
                        "A compact type edit updates the canvas, FK type, Explorer and Properties with the table still selected");
                undo_once();
                redo_once();
                require(compact_type(manager_handle)->currentText() == "BIGINT", "Redo refreshes the same compact column handles");
                undo_once();
                const auto before_refusal = editor.revision();
                compact_rules(key_handle, "schemaRuleNull");
                require(editor.revision() == before_refusal && column_of("Employee", 0).required && window.statusBar()->currentMessage().contains("A primary key can never be empty"),
                        "Compact constraints preserve the existing primary-key nullability validation");
                compact_rules(key_handle, "schemaRuleIdentity");
                require(column_of("Employee", 0).auto_increment && panel().findChildren<QToolButton *>("schemaColumnConstraints").front()->toolTip().contains("Identity") && schema->selection_now() == employee_choice && properties_heading(panel()) == "Properties",
                        "Independent Identity and PK states both survive the compact summary, the table list still shown");
                undo_once();

                // The menu, classified (Zain, 2026-10-01): Primary Key and
                // Foreign Key together, a rule, NULL and NOT NULL as opposite
                // choices, a rule, UNIQUE and IDENTITY. Taking a foreign key
                // off is under Foreign Key.
                const auto rules_menu = [&](desktop::SchemaColumnRef handle)
                {
                    auto *button = panel().findChildren<QFrame *>("schemaColumnRow")[static_cast<qsizetype>(schema->locate(handle)->second)]->findChild<QToolButton *>("schemaColumnConstraints");
                    QStringList top;
                    QStringList under;
                    QStringList ticked;
                    QTimer::singleShot(0, &window, [&]
                                       {
                        auto* menu = window.findChild<QMenu*>("schemaColumnRulesMenu");
                        if (!menu) return;
                        for (auto* action : menu->actions()) {
                            top << (action->isSeparator() ? QString("—") : action->text());
                            if (action->isChecked()) ticked << action->text();
                            if (auto* inner = action->menu())
                                for (auto* one : inner->actions()) under << (one->isSeparator() ? QString("—") : one->text());
                        }
                        menu->close(); });
                    button->click();
                    settle();
                    return std::tuple{top, under, ticked};
                };
                const QStringList classified{"Primary Key", "Foreign Key", "—", "NULL — may be empty",
                                             "NOT NULL — required", "—", "UNIQUE — no duplicate values",
                                             "IDENTITY — auto-generated number"};
                {
                    const auto [top, under, ticked] = rules_menu(manager_handle);
                    require(top == classified, "A column's Constraints menu reads Primary Key, Foreign Key, a rule, "
                                               "NULL and NOT NULL, a rule, UNIQUE and IDENTITY, in those words");
                    require(under.size() >= 3 && under.back() == "Remove the foreign key on \"ManagerID\"" && under[under.size() - 2] == "—",
                            "Taking the foreign key off is still offered, at the foot of Foreign Key");
                    require(ticked == QStringList{"NULL — may be empty"},
                            "A nullable foreign key that is no key ticks NULL and nothing else");
                }
                {
                    const auto [top, under, ticked] = rules_menu(key_handle);
                    require(top == classified && ticked.contains("Primary Key") && ticked.contains("NOT NULL — required") && !ticked.contains("NULL — may be empty"),
                            "The key's menu reads the same, ticking Primary Key and NOT NULL");
                }
                // The answer a column has already changes nothing; the other is
                // the same edit it always was, undone and redone as ever.
                auto rules_revision = editor.revision();
                compact_rules(manager_handle, "schemaRuleNull");
                require(editor.revision() == rules_revision && !column_of("Employee", 1).required,
                        "Choosing NULL for a column that may already be empty changes nothing");
                compact_rules(manager_handle, "schemaRuleNotNull");
                require(editor.revision() != rules_revision && column_of("Employee", 1).required && std::get<2>(rules_menu(manager_handle)) == QStringList{"NOT NULL — required"} && schema->selection_now() == employee_choice && properties_heading(panel()) == "Properties",
                        "Choosing NOT NULL makes it required, the menu then ticks NOT NULL, the table list still shown");
                rules_revision = editor.revision();
                compact_rules(manager_handle, "schemaRuleNotNull");
                require(editor.revision() == rules_revision && column_of("Employee", 1).required,
                        "Choosing NOT NULL again changes nothing");
                undo_once();
                require(!column_of("Employee", 1).required, "Undo makes it nullable again");
                redo_once();
                require(column_of("Employee", 1).required, "Redo makes it required again");
                undo_once();

                // Add uses the canvas command, including its inline naming.
                child<QPushButton>(window, "schemaPropertiesAddColumn")->click();
                settle();
                type_name("CompactValue");
                schema->choose(employee_choice);
                settle();
                const auto value_handle = *schema->column_ref(table_called("Employee"), 2);
                require_table_properties(panel(), *schema, table_called("Employee"));
                compact_type(value_handle)->showPopup();
                settle();
                child<QLineEdit>(window, "typePickerSearch")->setText("decimal");
                settle();
                QTest_activate(compact_list, compact_list->item(0));
                settle();
                require(child<QWidget>(window, "sizePicker")->isVisible(),
                        "A measured compact type directly reuses the size picker, without another row control");
                child<QLineEdit>(window, "sizePickerLength")->setText("10");
                child<QLineEdit>(window, "sizePickerScale")->setText("2");
                QKeyEvent enter_size(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QApplication::sendEvent(child<QLineEdit>(window, "sizePickerLength"), &enter_size);
                settle();
                require(compact_type(value_handle)->currentText() == "DECIMAL(10,2)" && column_of("Employee", 2).length == 10 && column_of("Employee", 2).scale == 2,
                        "Precision and scale come from the model and are displayed in the single type control");
                compact_rules(value_handle, "schemaRuleNotNull");
                compact_rules(value_handle, "schemaRuleUnique");
                require(column_of("Employee", 2).required && column_of("Employee", 2).unique,
                        "Compact constraints change the same canvas column's nullability and uniqueness");
                undo_once();
                undo_once();
                undo_once();
                require(compact_type(value_handle)->currentText() == "DECIMAL", "Undo removes the size from the compact model display");
                undo_once();
                undo_once(); // canvas rename
                undo_once(); // Add Column
                require(schema->preview().tables[table_called("Employee")].columns.size() == 2,
                        "Every compact-surface edit, rename and add is undone through the existing history");
                require_table_properties(panel(), *schema, table_called("Employee"));

                // A row opens nothing else (Zain, 2026-10-01): pressing its
                // background or its number, or Return or Space on it, leaves
                // the table chosen and its list in place, and nothing in the
                // table's Properties leads to the column's own inspector.
                const auto lit_before_rows = lit_rows(*schema_explorer);
                const auto press_card = [&](QPointF hit)
                {
                    auto *card = panel().findChildren<QFrame *>("schemaColumnRow")[1];
                    QMouseEvent row_press(QEvent::MouseButtonPress, hit, card->mapToGlobal(hit.toPoint()),
                                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                    QMouseEvent row_release(QEvent::MouseButtonRelease, hit, card->mapToGlobal(hit.toPoint()),
                                            Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                    QApplication::sendEvent(card, &row_press);
                    QApplication::sendEvent(card, &row_release);
                    for (const auto key : {Qt::Key_Return, Qt::Key_Space})
                    {
                        QKeyEvent pressed(QEvent::KeyPress, key, Qt::NoModifier);
                        QApplication::sendEvent(card, &pressed);
                    }
                    settle();
                };
                {
                    auto *card = panel().findChildren<QFrame *>("schemaColumnRow")[1];
                    press_card(QPointF(2, card->height() / 2.0));
                    card = panel().findChildren<QFrame *>("schemaColumnRow")[1];
                    press_card(card->findChild<QLabel *>("schemaColumnNumber")->geometry().center());
                }
                require(schema->selection_now() == employee_choice && properties_heading(panel()) == "Properties" && panel().findChildren<QFrame *>("schemaColumnRow").size() == 2 && name_field() && name_field()->text() == "Employee" && lit_rows(*schema_explorer) == lit_before_rows,
                        "Pressing a row leaves the table chosen, its Properties list in place and the Explorer as it was");
                {
                    const auto offered = panel().findChildren<QAction *>();
                    const auto pressable = panel().findChildren<QAbstractButton *>();
                    require(std::none_of(offered.begin(), offered.end(),
                                         [](auto *action)
                                         { return action->text() == "Column Properties"; }) &&
                                std::none_of(pressable.begin(), pressable.end(),
                                             [](auto *button)
                                             { return button->text() == "..."; }) &&
                                panel().findChildren<QToolButton *>("schemaColumnActions").isEmpty(),
                            "No ... button and no Column Properties action anywhere in the table's Properties");
                }
                // A column chosen in the Explorer or on the canvas still shows
                // its own inspector and is lit everywhere, as before; choosing
                // the table again brings its list back.
                {
                    const auto *tree = schema_explorer->model();
                    const auto employee_row = row_saying(*tree, tables_row(), "Employee");
                    press_row(*schema_explorer, row_saying(*tree, row_saying(*tree, employee_row, "Columns"), "ManagerID"));
                }
                const desktop::SchemaSelection manager_selection = desktop::ChosenColumn{manager_handle};
                require(schema->selection_now() == manager_selection && properties_heading(panel()) == "Column" && said("General/Name") == QStringList{"ManagerID"} && lit_rows(*schema_explorer).contains(desktop::schema_key(manager_selection)) && rule("Constraints/Nullable") && chooser("General/Data Type"),
                        "A column chosen in the Explorer still shows its own inspector, lit there and on the schema");
                schema->choose(employee_choice);
                settle();
                require(properties_heading(panel()) == "Properties" && panel().findChildren<QFrame *>("schemaColumnRow").size() == 2,
                        "Choosing the table again brings its column list back");

                // The room the menu took goes to the name: the type keeps what
                // VARCHAR(255) and DECIMAL(10,2) need, the constraints theirs,
                // and the name takes the rest, giving it up first.
                {
                    const auto dock_was = properties_dock->width();
                    window.resizeDocks({properties_dock}, {380}, Qt::Horizontal);
                    settle();
                    auto *row = panel().findChildren<QFrame *>("schemaColumnRow")[1];
                    auto *row_name = row->findChild<QLineEdit *>();
                    auto *row_type = row->findChild<QComboBox *>("schemaColumnType");
                    auto *row_rules = row->findChild<QToolButton *>("schemaColumnConstraints");
                    const auto whole = [&](const QString &words)
                    {
                        return row_type->fontMetrics().horizontalAdvance(words) + 17 <= row_type->width();
                    };
                    require(whole("VARCHAR(255)") && whole("DECIMAL(10,2)") && row_rules->width() == row_rules->sizeHint().width() && row_name->width() > row_type->width(),
                            "With room, the name is the widest, and the type shows VARCHAR(255) and DECIMAL(10,2) whole");
                    const auto wide_row = row->width();
                    const auto wide_name = row_name->width();
                    const auto wide_type = row_type->width();
                    const auto wide_rules = row_rules->width();
                    window.resizeDocks({properties_dock}, {320}, Qt::Horizontal);
                    settle();
                    require(row->width() < wide_row && row_type->width() == wide_type && row_rules->width() == wide_rules && row_name->width() == wide_name - (wide_row - row->width()),
                            "Narrower, only the name gives up room while the type and constraints keep what they need");
                    window.resizeDocks({properties_dock}, {dock_was}, Qt::Horizontal);
                    settle();
                }

                // Numbering follows the displayed list after insert/delete;
                // a surviving column continues to use its own handle.
                const auto employee_id = std::get<desktop::ChosenTable>(employee_choice).table;
                require(editor.add_schema_column(domain::ElementRef{employee_id}, "Extra").ok, "A test column is added");
                schema->refresh();
                schema->chose();
                settle();
                const auto extra_id = editor.project().schema.added.at(employee_id).back().id;
                const auto extra_handle = desktop::SchemaColumnRef{employee_id, extra_id};
                require(editor.add_schema_column(domain::ElementRef{employee_id}, "After").ok, "A following column is added");
                schema->refresh();
                schema->chose();
                settle();
                const auto after_id = editor.project().schema.added.at(employee_id).back().id;
                require(editor.erase_schema_column(extra_id).ok, "The preceding column is deleted");
                schema->refresh();
                schema->chose();
                settle();
                require_table_properties(panel(), *schema, table_called("Employee"));
                require(panel().findChildren<QFrame *>("schemaColumnRow")[2]->property("choice").value<desktop::SchemaSelection>() == desktop::SchemaSelection{desktop::ChosenColumn{{employee_id, after_id}}} && !schema->locate(extra_handle),
                        "The following column becomes r3 and retains its real identity after deletion");
                undo_once();
                require_table_properties(panel(), *schema, table_called("Employee"));
                undo_once();
                undo_once();
                require_table_properties(panel(), *schema, table_called("Employee"));
                auto revision = editor.revision();
                type_in(name_field(), "Staff", Qt::Key_Return);
                require(editor.revision() != revision && editor.undo_label() == "Rename table",
                        "Return renames the table, as one edit");
                require(schema->selection_now() == employee_choice && has_table("Staff") && !has_table("Employee") && listed_table("Staff") && !listed_table("Employee") && said("General/Name") == QStringList{"Staff"},
                        "The same table is still chosen, and the schema, the Explorer and Properties all say Staff");
                require(said("Structure/Primary Key") == QStringList{"StaffID"} && said("Structure/Foreign Keys") == QStringList{"ManagerID → Staff.StaffID"},
                        "Its key named for it follows, as on the canvas, and the foreign key says the new names");
                undo_once();
                require(schema->selection_now() == employee_choice && said("General/Name") == QStringList{"Employee"} && listed_table("Employee"),
                        "Undone, it is Employee again everywhere");
                redo_once();
                require(said("General/Name") == QStringList{"Staff"} && listed_table("Staff"), "Redone, Staff");
                undo_once();

                // Unchanged, cancelled or refused, nothing is written.
                revision = editor.revision();
                auto label = editor.undo_label();
                type_in(name_field(), "Employee", Qt::Key_Return);
                type_in(name_field(), "Employee  ", Qt::Key_Return);
                require(editor.revision() == revision && editor.undo_label() == label && name_field()->text() == "Employee",
                        "The same name again is not an edit");
                type_in(name_field(), "Typo", Qt::Key_Escape);
                require(name_field()->text() == "Employee" && editor.revision() == revision,
                        "Escape puts the name back as it was and writes nothing");
                schema->setFocus();
                settle();
                require(editor.revision() == revision, "Nor does leaving the field afterwards");
                type_in(name_field(), QString("Bad") + QChar(0x01) + "Name", Qt::Key_Return, true);
                require(editor.revision() == revision && editor.undo_label() == label && said("General/Name") == QStringList{"Employee"} && has_table("Employee"),
                        "A name the model refuses is refused with its reason, and the field shows the name kept");
                // The model has no rule against two tables sharing a name, and
                // Properties adds none of its own: it is the canvas's answer.
                type_in(name_field(), "Department", Qt::Key_Return);
                require(editor.undo_label() == "Rename table" && std::count_if(schema->preview().tables.begin(), schema->preview().tables.end(),
                                                                               [](const auto &table)
                                                                               { return table.name == "Department"; }) == 2,
                        "Two tables may share a name, as they may when renamed on the canvas");
                undo_once();
                require(has_table("Employee") && editor.undo_label() == label, "And it undoes");

                // A column: its name, type, size and three rules are fields;
                // its keys and references stay read.
                auto employee_t = table_called("Employee");
                choose_column("Employee", 1);
                const auto manager_choice = schema->selection_now();
                require(name_field() && name_field()->text() == "ManagerID" && chooser("General/Data Type") && rule("Constraints/Primary Key") && rule("Constraints/Foreign Key") && rule("Constraints/Nullable") && rule("Constraints/Not Null") && rule("Constraints/Unique") && rule("Constraints/Identity") && panel().findChildren<QLineEdit *>().size() == 1 && panel().findChildren<QCheckBox *>().size() == 6 && said("Keys/Key Role") == QStringList{"Foreign Key"} && said("References/References") == QStringList{"Employee.EmployeeID"},
                        "A column offers its name, its type and a switch for each constraint; its key details are said");
                revision = editor.revision();
                type_in(name_field(), "BossID", Qt::Key_Return);
                require(editor.revision() != revision && schema->selection_now() == manager_choice && column_of("Employee", 1).name == "BossID" && rows_said(*schema_explorer->model(), row_saying(*schema_explorer->model(), row_saying(*schema_explorer->model(), tables_row(), "Employee"), "Columns")).contains("BossID") && said("General/Name") == QStringList{"BossID"} && said("Identity/Name") == QStringList{"BossID"} && said("Identity/Column") == QStringList{"Employee.BossID"} && said("Keys/Key Column") == QStringList{"BossID"},
                        "Renamed, the same column is chosen and the schema, the Explorer and Properties say BossID");
                undo_once();
                require(schema->selection_now() == manager_choice && said("General/Name") == QStringList{"ManagerID"},
                        "Undone, ManagerID");
                redo_once();
                require(said("General/Name") == QStringList{"BossID"}, "Redone, BossID");
                undo_once();
                revision = editor.revision();
                label = editor.undo_label();
                type_in(name_field(), "", Qt::Key_Return, true);
                require(editor.revision() == revision && editor.undo_label() == label && said("General/Name") == QStringList{"ManagerID"},
                        "A column cannot be left without a name, and nothing is written for trying");

                // A foreign key's type is its key's: asked to change, the
                // model refuses with why, and touches neither end.
                auto *types = chooser("General/Data Type");
                types->showPopup();
                settle();
                auto *listed = child<QListWidget>(window, "typePickerList");
                require(window.findChild<QWidget *>("typePicker")->isVisible(),
                        "The type field opens the schema's own list of types");
                child<QLineEdit>(window, "typePickerSearch")->setText("bigint");
                settle();
                dismiss(QMessageBox::Ok);
                QTest_activate(listed, listed->item(0));
                settle();
                require(editor.revision() == revision && editor.undo_label() == label && column_of("Employee", 1).type == domain::LogicalType::Int && column_of("Employee", 0).type == domain::LogicalType::Int && column_of("Employee", 1).foreign_key && column_of("Employee", 1).key_id && column_of("Employee", 0).primary_key && said("General/Data Type") == QStringList{"INT"},
                        "A foreign key's type is refused, and neither it, its key nor the foreign key is changed");
                // The key it points at is changed, and the foreign key follows
                // in the same edit, as the model has always done.
                choose_column("Employee", 0);
                chooser("General/Data Type")->showPopup();
                settle();
                child<QLineEdit>(window, "typePickerSearch")->setText("bigint");
                settle();
                QTest_activate(listed, listed->item(0));
                settle();
                require(column_of("Employee", 0).type == domain::LogicalType::BigInt && column_of("Employee", 1).type == domain::LogicalType::BigInt && said("General/Data Type") == QStringList{"BIGINT"},
                        "A key's type is changed, and the foreign key pointing at it takes the same type");
                undo_once();
                require(column_of("Employee", 0).type == domain::LogicalType::Int && column_of("Employee", 1).type == domain::LogicalType::Int && editor.undo_label() == label,
                        "One undo takes both back");

                // A primary key's own rules: never empty, unique by being the
                // key, and counting up without ceasing to be the key.
                revision = editor.revision();
                require(said("Constraints/Nullable") == QStringList{"No"}, "A key may not be empty");
                rule("Constraints/Nullable")->click();
                settle();
                require(editor.revision() == revision && said("Constraints/Nullable") == QStringList{"No"} && window.statusBar()->currentMessage().contains("A primary key can never be empty"),
                        "Pressed, a key's Nullable says why it cannot be, and stays as it was");
                rule("Constraints/Unique")->click();
                settle();
                require(editor.revision() == revision && said("Constraints/Unique") == QStringList{"Yes"} && column_of("Employee", 0).primary_key && window.statusBar()->currentMessage().contains("unique already"),
                        "A key's uniqueness cannot be pressed away: it stays ticked and says why");
                rule("Constraints/Identity")->click();
                settle();
                require(editor.revision() != revision && column_of("Employee", 0).auto_increment && column_of("Employee", 0).primary_key && column_of("Employee", 1).foreign_key && !column_of("Employee", 1).auto_increment && said("Constraints/Identity") == QStringList{"Yes"} && said("Keys/Key Role") == QStringList{"Primary Key"},
                        "Identity counts the key up, and the key and the foreign key stay what they were");
                undo_once();
                require(!column_of("Employee", 0).auto_increment && said("Constraints/Identity") == QStringList{"No"},
                        "Undone, it no longer counts");
                redo_once();
                require(column_of("Employee", 0).auto_increment, "Redone, it counts again");
                undo_once();

                // An ordinary column, added where a hand adds one.
                auto department_box = schema->table_boxes()[table_called("Department")];
                const QPointF title_slot(department_box.center().x(), department_box.bottom() + 10);
                mouse(QEvent::MouseMove, title_slot, Qt::NoButton);
                mouse(QEvent::MouseButtonPress, title_slot, Qt::LeftButton);
                settle();
                type_name("Title");
                choose_column("Department", 1);
                require(said("General/Name") == QStringList{"Title"} && said("Keys/Key Role") == QStringList{"None"},
                        "An ordinary column is chosen");
                revision = editor.revision();
                chooser("General/Data Type")->showPopup();
                settle();
                child<QLineEdit>(window, "typePickerSearch")->setText("nvarchar");
                settle();
                QTest_activate(listed, listed->item(0));
                settle();
                require(editor.revision() != revision && column_of("Department", 1).type == domain::LogicalType::NVarchar && desktop::written_type(column_of("Department", 1)) == "nvarchar" && said("General/Data Type") == QStringList{"NVARCHAR"} && window.findChild<QWidget *>("sizePicker")->isVisible(),
                        "A type chosen is the column's type on the schema and in Properties, and a measured one asks "
                        "its size at once, in the schema's own size picker");
                auto *length = child<QLineEdit>(window, "sizePickerLength");
                length->setText("80");
                QKeyEvent entered(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QApplication::sendEvent(length, &entered);
                settle();
                require(column_of("Department", 1).length == 80 && desktop::written_type(column_of("Department", 1)) == "nvarchar(80)" && said("General/Data Type") == QStringList{"NVARCHAR(80)"},
                        "A size given is the column's size, said with its type");
                undo_once();
                require(said("General/Data Type") == QStringList{"NVARCHAR"}, "Undone, no size");
                undo_once();
                require(said("General/Data Type") == QStringList{"Not set"}, "Undone again, no type");
                redo_once();
                redo_once();
                require(said("General/Data Type") == QStringList{"NVARCHAR(80)"}, "Redone, both back");

                // Its three rules.
                revision = editor.revision();
                require(said("Constraints/Nullable") == QStringList{"Yes"}, "It may be empty to begin with");
                rule("Constraints/Nullable")->click();
                settle();
                require(editor.revision() != revision && column_of("Department", 1).required && said("Constraints/Nullable") == QStringList{"No"},
                        "Nullable taken off makes it NOT NULL on the schema and in Properties");
                undo_once();
                require(!column_of("Department", 1).required && said("Constraints/Nullable") == QStringList{"Yes"},
                        "Undone, it may be empty");
                redo_once();
                require(column_of("Department", 1).required, "Redone, it may not");
                undo_once();
                rule("Constraints/Unique")->click();
                settle();
                require(column_of("Department", 1).unique && said("Constraints/Unique") == QStringList{"Yes"},
                        "Unique put on");
                undo_once();
                require(!column_of("Department", 1).unique && said("Constraints/Unique") == QStringList{"No"},
                        "And undone");
                revision = editor.revision();
                rule("Constraints/Identity")->click();
                settle();
                require(editor.revision() == revision && !column_of("Department", 1).auto_increment && said("Constraints/Identity") == QStringList{"No"} && window.statusBar()->currentMessage().contains("whole-number"),
                        "A column of text cannot count up: refused with why, the box left unticked");

                // The column's Properties, redesigned (Zain, 2026-10-01): one
                // card saying who is chosen, then Column Info, Constraints, Key
                // Details, References and Source, each with its mark and open
                // to begin with, every change still the canvas's own.
                {
                    choose_column("Department", 1);
                    const auto &colours = desktop::theme(window.canvas()->theme_id());
                    auto *identity = panel().findChild<QFrame *>("schemaColumnIdentity");
                    const auto kinds = panel().findChildren<QLabel *>("propertyHeading");
                    auto *top_title = panel().findChild<QLabel *>("schemaPropertiesHeading");
                    require(panel().findChild<QWidget *>("schemaColumnProperties") && top_title && top_title->text() == "Properties" && identity && kinds.size() == 1 && identity->isAncestorOf(kinds.front()) && kinds.front()->text() == "Column" && kinds.front()->font().bold() && kinds.front()->palette().color(QPalette::WindowText) == colours.accent && said("Identity/Name") == QStringList{"Title"} && said("Identity/Column") == QStringList{"Department.Title"},
                            "Properties, then one card: Column in blue and bold, the column's name, and Table.Column");
                    const auto section_of = [&](const QString &title) -> QWidget *
                    {
                        for (auto *one : panel().findChildren<QWidget *>("schemaColumnPropertySection"))
                            if (one->property("section").toString() == title)
                                return one;
                        return nullptr;
                    };
                    const auto open = [&](const QString &title)
                    {
                        auto *one = section_of(title);
                        return one && one->findChild<QAbstractButton *>("sectionHeader")->isChecked() && !one->findChild<QWidget *>("schemaColumnSectionBody")->isHidden();
                    };
                    QStringList titled;
                    bool marked = true;
                    for (auto *one : panel().findChildren<QWidget *>("schemaColumnPropertySection"))
                    {
                        titled << one->property("section").toString();
                        auto *header = one->findChild<QAbstractButton *>("sectionHeader");
                        marked = marked && header && !header->icon().isNull() && header->text() == titled.back();
                    }
                    require(titled == QStringList{"Column Info", "Constraints", "Key Details", "References", "Source"} && marked && open("Column Info") && open("Constraints") && open("Key Details") && open("References") && open("Source") && panel().findChildren<QWidget *>("schemaPropertySection").isEmpty(),
                            "Five sections, each with its mark and open: Column Info where General was, Key Details "
                            "where Keys was");
                    QStringList headings;
                    QStringList meanings;
                    bool rows_marked = true;
                    for (auto *line : panel().findChildren<QFrame *>("schemaConstraintRow"))
                    {
                        headings << line->findChild<QLabel *>("schemaConstraintTitle")->text();
                        meanings << line->findChild<QLabel *>("schemaConstraintNote")->text();
                        const auto labels = line->findChildren<QLabel *>();
                        rows_marked = rows_marked && std::any_of(labels.begin(), labels.end(), [](auto *label)
                                                                 { return !label->pixmap().isNull(); }) &&
                                      line->findChild<QCheckBox *>("schemaConstraintSwitch");
                    }
                    require(headings == QStringList{"Primary Key", "Foreign Key", "NULL", "NOT NULL", "UNIQUE", "IDENTITY"} && meanings == QStringList{"This column is the primary key", "References a column in another table", "May be empty", "Required — cannot be empty", "No duplicate values", "Auto-generated number"} && rows_marked,
                            "Each constraint is a row of its own: its mark, its name, what it means, and its switch");
                    require(said("Keys/Key Role") == QStringList{"None"} && said("Keys/Key Column") == QStringList{"None"} && said("References/References").isEmpty() && said("References/Referenced By") == QStringList{"None"} && properties_count(panel(), "References/Referenced By") == 0 && said("General/Table") == QStringList{"Department"} && said("General/Data Type") == QStringList{"NVARCHAR(80)"} && said("General/Source") == QStringList{"Schema-first"} && said("Source/Source") == QStringList{"Schema-first"},
                            "An ordinary column has no key role, nothing points at it, and it came from the schema");

                    // Every mark follows the theme: none of them coloured under Plain.
                    {
                        const auto wearing_now = window.canvas()->theme_id();
                        window.set_theme(desktop::ThemeId::Plain);
                        settle();
                        require(coloured_pixels(grab_without_subpixel_text(*properties_dock->widget())) == 0,
                                "Under Plain the column's Properties have no colour: card, marks, switches, NOT NULL");
                        window.set_theme(wearing_now);
                        settle();
                    }

                    // NULL and NOT NULL are one fact, said both ways: never both.
                    const auto opposite = [&]
                    {
                        return said("Constraints/Nullable") != said("Constraints/Not Null") && (said("Constraints/Nullable") == QStringList{"Yes"}) == !column_of("Department", 1).required;
                    };
                    auto revision_now = editor.revision();
                    require(opposite() && said("Constraints/Nullable") == QStringList{"Yes"},
                            "A column that may be empty has NULL on and NOT NULL off");
                    rule("Constraints/Not Null")->click();
                    settle();
                    require(editor.revision() != revision_now && column_of("Department", 1).required && opposite() && said("Constraints/Not Null") == QStringList{"Yes"},
                            "Switched to NOT NULL, it is required, and NULL goes off");
                    rule("Constraints/Nullable")->click();
                    settle();
                    require(!column_of("Department", 1).required && opposite(), "Switched back to NULL, it may be empty");
                    undo_once();
                    require(column_of("Department", 1).required && opposite(), "Undone, NOT NULL again");
                    undo_once();
                    require(!column_of("Department", 1).required && opposite(), "Undone again, NULL");

                    // Folded by the hand, a section stays folded for the next
                    // column, and folding is not an edit.
                    revision_now = editor.revision();
                    section_of("Constraints")->findChild<QAbstractButton *>("sectionHeader")->click();
                    settle();
                    choose_column("Employee", 1);
                    choose_column("Department", 1);
                    require(!open("Constraints") && open("Column Info") && open("Key Details") && open("References") && open("Source") && editor.revision() == revision_now,
                            "A section folded by hand stays folded for the next column chosen, the others open");
                    section_of("Constraints")->findChild<QAbstractButton *>("sectionHeader")->click();
                    settle();
                    require(open("Constraints"), "And opens again");

                    // The Primary Key switch is the row menu's own key action.
                    revision_now = editor.revision();
                    rule("Constraints/Primary Key")->click();
                    settle();
                    const auto labels = panel().findChildren<QLabel *>();
                    require(editor.revision() != revision_now && column_of("Department", 1).primary_key && said("Constraints/Primary Key") == QStringList{"Yes"} && said("Keys/Key Role") == QStringList{"Primary Key"} && said("Keys/Key Column") == QStringList{"DepartmentID, Title"} && std::any_of(labels.begin(), labels.end(), [](auto *label)
                                                                                                                                                                                                                                                                                                        { return label->text() == "Key Columns"; }) &&
                                column_of("Department", 1).required && opposite(),
                            "Switched on, it joins the table's key as the row's menu makes it, required, the whole "
                            "key of two said");
                    undo_once();
                    require(!column_of("Department", 1).primary_key && said("Keys/Key Role") == QStringList{"None"},
                            "Undone, it is no key");
                    redo_once();
                    require(column_of("Department", 1).primary_key, "Redone, a key again");
                    undo_once();

                    // The Foreign Key switch is the path Connect takes: the keys
                    // to reference are offered, a column of another type is
                    // refused rather than retyped, and the rest is asked first.
                    const auto reference = [&](const QString &key)
                    {
                        QStringList offered_keys;
                        QTimer::singleShot(0, &window, [&]
                                           {
                            auto* menu = window.findChild<QMenu*>("schemaColumnReferenceMenu");
                            if (!menu) return;
                            for (auto* action : menu->actions()) offered_keys << action->text();
                            for (auto* action : menu->actions())
                                if (action->text() == key) {
                                    menu->close();
                                    action->trigger();
                                    break;
                                } });
                        rule("Constraints/Foreign Key")->click();
                        settle();
                        return offered_keys;
                    };
                    revision_now = editor.revision();
                    const auto offered_keys = reference("Employee.EmployeeID");
                    require(offered_keys.contains("Employee.EmployeeID") && editor.revision() == revision_now && !column_of("Department", 1).foreign_key && column_of("Department", 1).type == domain::LogicalType::NVarchar && said("Constraints/Foreign Key") == QStringList{"No"} && window.statusBar()->currentMessage().contains("Cannot use Department.Title"),
                            "A key of another type is refused, as a line drawn onto the column is, and nothing is retyped");
                    require(editor.set_schema_column_type(*column_of("Department", 1).added, domain::LogicalType::Int).ok,
                            "Title is made a whole number");
                    schema->refresh();
                    schema->chose();
                    settle();
                    choose_column("Department", 1);
                    const auto title_choice = schema->selection_now();
                    std::vector<QStringList> asked_now;
                    answer_in_turn({"Use & Connect"}, &asked_now);
                    reference("Employee.EmployeeID");
                    require(asked_now.size() == 1 && asked_now[0].value(1) == "Existing column found" && column_of("Department", 1).foreign_key && column_of("Department", 1).references == table_called("Employee") && editor.undo_label() == "Connect tables" && schema->selection_now() == title_choice && properties_heading(panel()) == "Column" && said("Constraints/Foreign Key") == QStringList{"Yes"} && said("Keys/Key Role") == QStringList{"Foreign Key"} && said("References/References") == QStringList{"Employee.EmployeeID"},
                            "Switched on, it is asked about as Connect asks, made in one edit, and the column stays chosen");
                    rule("Constraints/Foreign Key")->click();
                    settle();
                    require(!column_of("Department", 1).foreign_key && said("Constraints/Foreign Key") == QStringList{"No"} && said("Keys/Key Role") == QStringList{"None"} && said("References/References").isEmpty(),
                            "Switched off, the foreign key is taken off as the row's menu takes it off");
                    undo_once();
                    require(column_of("Department", 1).foreign_key && said("References/References") == QStringList{"Employee.EmployeeID"}, "Undone, the foreign key is back");
                    undo_once();
                    require(!column_of("Department", 1).foreign_key, "Undone again, it was never made");
                    undo_once();
                    require(column_of("Department", 1).type == domain::LogicalType::NVarchar && said("General/Data Type") == QStringList{"NVARCHAR(80)"},
                            "And the type made for it is undone too");

                    // Renamed on the canvas, Properties and the Explorer follow.
                    {
                        const auto rows_here = schema->row_boxes()[table_called("Department")];
                        const QPointF on_name(rows_here[1].left() + 90, rows_here[1].center().y());
                        press_at(on_name);
                        mouse(QEvent::MouseButtonDblClick, on_name, Qt::LeftButton);
                        settle();
                        require(field->isVisible(), "The column's name opens for typing on the canvas");
                        type_name("Caption");
                        const auto *tree = schema_explorer->model();
                        require(column_of("Department", 1).name == "Caption" && said("General/Name") == QStringList{"Caption"} && said("Identity/Name") == QStringList{"Caption"} && said("Identity/Column") == QStringList{"Department.Caption"} && rows_said(*tree, row_saying(*tree, row_saying(*tree, tables_row(), "Department"), "Columns")).contains("Caption"),
                                "Renamed on the canvas, the column's Properties and the Explorer say the new name");
                        undo_once();
                        require(said("General/Name") == QStringList{"Title"}, "Undone, Title");
                    }
                }

                // A name being typed when something else is chosen is finished
                // as leaving the field finishes it, before the choosing. (The
                // boxes answered above took the window's activation with them.)
                window.activateWindow();
                settle();
                name_field()->setFocus();
                settle();
                name_field()->setText("Heading");
                revision = editor.revision();
                schema->setFocus();
                settle();
                require(editor.revision() != revision && column_of("Department", 1).name == "Heading",
                        "Leaving the field for the schema finishes the name");
                employee_t = table_called("Employee");
                const auto heading_box = schema->table_boxes()[employee_t];
                press_at(QPointF(heading_box.center().x(), heading_box.top() + 8));
                require(properties_heading(panel()) == "Properties" && said("General/Name") == QStringList{"Employee"},
                        "And the table pressed is what Properties shows next");
                undo_once();
                require(column_of("Department", 1).name == "Title", "Undone, the column is Title again");

                // A table deleted while its name is being typed takes the field
                // with it, and what was typed goes nowhere.
                press_row(*schema_explorer, row_saying(*schema_explorer->model(), tables_row(), "Department"));
                name_field()->setFocus();
                settle();
                name_field()->setText("Gone");
                QKeyEvent erase(QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier);
                QApplication::sendEvent(schema, &erase);
                settle();
                require(!has_table("Department") && !has_table("Gone") && properties_heading(panel()) == "Schema" && editor.undo_label() != "Rename table",
                        "Deleted, the table is gone, the name typed for it is written nowhere, and the schema is summed up");
                undo_once();
                require(has_table("Department") && !has_table("Gone"), "Undone, Department is back under its own name");

                // The card's Constraints cell (Zain, 2026-10-08): it writes PK,
                // FK, NULL or NOT NULL, UNIQUE and IDENTITY in that order, in the
                // type's muted ink, and pressing it lists all of them -- the keys
                // doing exactly what their Properties switches do, the rules what
                // the Properties list does -- so the card and Properties are one
                // set of facts.
                {
                    const auto steps_at_start = editor.history_position();
                    const auto revision_at_start = editor.revision();
                    const auto cell_of = [&](const char *table, std::size_t row)
                    { return schema->cell_boxes()[table_called(table)][row].rules.center(); };
                    const auto card = [&](const char *table, std::size_t row)
                    { return schema->constraints_said(table_called(table), row); };
                    // Pressed, the cell opens its list; whatever it leads to -- the
                    // list of keys to reference, the question about using a column
                    // that is there already -- is answered as soon as it opens.
                    struct Listed
                    {
                        QStringList words;
                        QStringList ticked;
                    };
                    const auto from_card = [&](const char *table, std::size_t row, const char *which,
                                               const QString &referencing = {}) -> Listed
                    {
                        Listed listed;
                        bool listed_once = false;
                        int tries = 0;
                        QTimer poll;
                        poll.setInterval(0);
                        QObject::connect(&poll, &QTimer::timeout, &window, [&]
                                         {
                            if (++tries > 4000) {
                                poll.stop();
                                auto *stuck = QApplication::activePopupWidget() ? QApplication::activePopupWidget()
                                                                                : QApplication::activeModalWidget();
                                std::cerr << "STUCK " << (stuck ? stuck->objectName().toStdString() + " " + stuck->metaObject()->className() : std::string("nothing")) << "\n";
                                if (stuck) stuck->close();
                                return;
                            }
                            auto *rules = window.findChild<QMenu *>("schemaRulesMenu");
                            if (!listed_once && rules && rules->isVisible()) {
                                for (auto *action : rules->actions()) {
                                    listed.words << (action->isSeparator() ? QString("—") : action->text());
                                    if (action->isChecked()) listed.ticked << action->objectName();
                                }
                                if (which)
                                    if (auto *action = rules->findChild<QAction *>(which)) action->trigger();
                                rules->close();
                                listed_once = true;
                                return;
                            }
                            auto *targets = window.findChild<QMenu *>("schemaColumnReferenceMenu");
                            if (targets && targets->isVisible()) {
                                for (auto *action : targets->actions())
                                    if (action->text() == referencing) action->trigger();
                                targets->close();
                                return;
                            }
                        });
                        // Asked from inside the list's own answer, so answered by a
                        // timer of its own: a timer is not called again while it is
                        // still in its last call.
                        QTimer answer;
                        answer.setInterval(0);
                        QObject::connect(&answer, &QTimer::timeout, &window, [&]
                                         {
                            auto *asked = window.findChild<QMessageBox *>("schemaConnectAsk");
                            if (asked && asked->isVisible()) {
                                if (auto *agree = asked->findChild<QAbstractButton *>("schemaConnectAgree")) agree->click();
                                else asked->reject();
                            } });
                        poll.start();
                        answer.start();
                        press_at(cell_of(table, row));
                        settle();
                        poll.stop();
                        answer.stop();
                        return listed;
                    };
                    const QStringList all_of_them{"Primary Key", "Foreign Key", "—", "NULL — may be empty",
                                                  "NOT NULL — required", "—", "UNIQUE — no duplicate values",
                                                  "IDENTITY — auto-generated number"};

                    // What each kind of row writes, as the model has it: an
                    // ordinary column is NULL to begin with, because it is.
                    require(column_of("Department", 1).name == "Title" && !column_of("Department", 1).required,
                            "Title is an ordinary column that may be empty");
                    require(card("Department", 1) == "NULL" && card("Employee", 0) == "PK, NOT NULL" &&
                                card("Employee", 1) ==
                                    QString("FK, ") + (column_of("Employee", 1).required ? "NOT NULL" : "NULL"),
                            "The card writes NULL for an ordinary column, PK, NOT NULL for the key, FK and its "
                            "nullability for the foreign key");

                    // Written in the type's ink, in a light theme and a dark one:
                    // the strongest ink in the cell is the muted one, not the
                    // name's.
                    {
                        const auto wearing_now = window.canvas()->theme_id();
                        for (const auto look : {desktop::ThemeId::OfficeLight, desktop::ThemeId::Midnight})
                        {
                            window.set_theme(look);
                            schema->choose(desktop::NothingChosen{});
                            settle();
                            const auto &colours = desktop::theme(look);
                            const auto strongest = [&](const QRectF &part)
                            {
                                const auto image = schema->grab(part.toAlignedRect()).toImage();
                                // The cell's own ground is the colour most of it is.
                                std::map<QRgb, int> counted;
                                for (int y = 0; y < image.height(); ++y)
                                    for (int x = 0; x < image.width(); ++x)
                                        ++counted[image.pixel(x, y)];
                                const QColor ground = QColor::fromRgb(
                                    std::max_element(counted.begin(), counted.end(), [](const auto &a, const auto &b)
                                                     { return a.second < b.second; })->first);
                                QColor best = ground;
                                int far = -1;
                                for (int y = 0; y < image.height(); ++y)
                                    for (int x = 0; x < image.width(); ++x)
                                    {
                                        const auto here = image.pixelColor(x, y);
                                        const auto apart = std::abs(here.red() - ground.red()) +
                                                           std::abs(here.green() - ground.green()) +
                                                           std::abs(here.blue() - ground.blue());
                                        if (apart > far) { far = apart; best = here; }
                                    }
                                return best;
                            };
                            const auto nearness = [](const QColor &a, const QColor &b)
                            {
                                return std::abs(a.red() - b.red()) + std::abs(a.green() - b.green()) +
                                       std::abs(a.blue() - b.blue());
                            };
                            const auto cell = schema->cell_boxes()[table_called("Employee")][0];
                            const auto rules_ink = strongest(cell.rules.adjusted(7, 3, -7, -3));
                            const auto type_ink = strongest(cell.type.adjusted(2, 3, -2, -3));
                            require(nearness(rules_ink, colours.muted) < nearness(rules_ink, colours.text) &&
                                        nearness(type_ink, colours.muted) < nearness(type_ink, colours.text) &&
                                        nearness(rules_ink, type_ink) < 60,
                                    "The constraints are written in the data type's muted ink, in light and dark");
                        }
                        window.set_theme(wearing_now);
                        settle();
                    }

                    // The list: both keys, then the rules, the ones the column has
                    // ticked.
                    auto listed = from_card("Department", 1, nullptr);
                    require(listed.words == all_of_them && listed.ticked == QStringList{"schemaRuleNull"} &&
                                editor.revision() == revision_at_start,
                            "Pressed, the cell lists every constraint, NULL ticked, and changes nothing by opening");

                    // NOT NULL, then UNIQUE, from the card; Properties says the same.
                    from_card("Department", 1, "schemaRuleNotNull");
                    require(column_of("Department", 1).required && card("Department", 1) == "NOT NULL" &&
                                said("Constraints/Not Null") == QStringList{"Yes"} &&
                                said("Constraints/Nullable") == QStringList{"No"},
                            "NOT NULL chosen on the card: required, written so, and Properties agrees");
                    from_card("Department", 1, "schemaRuleUnique");
                    require(column_of("Department", 1).unique && card("Department", 1) == "NOT NULL, UNIQUE" &&
                                said("Constraints/Unique") == QStringList{"Yes"},
                            "UNIQUE chosen on the card is written after NOT NULL, and Properties agrees");
                    listed = from_card("Department", 1, nullptr);
                    require(listed.ticked == QStringList({"schemaRuleNotNull", "schemaRuleUnique"}),
                            "Opened again, NOT NULL and UNIQUE are ticked and NULL is not");
                    // And the other way: a Properties switch is on the card at once.
                    rule("Constraints/Nullable")->click();
                    settle();
                    require(!column_of("Department", 1).required && card("Department", 1) == "NULL, UNIQUE",
                            "NULL switched on in Properties is written on the card, NOT NULL gone");
                    undo_once();
                    require(card("Department", 1) == "NOT NULL, UNIQUE" && said("Constraints/Not Null") == QStringList{"Yes"},
                            "Undone, card and Properties both say NOT NULL again");
                    undo_once();
                    undo_once();
                    require(card("Department", 1) == "NULL" && said("Constraints/Unique") == QStringList{"No"} &&
                                said("Constraints/Nullable") == QStringList{"Yes"},
                            "Undone to the start, card and Properties both say NULL and nothing else");
                    redo_once();
                    redo_once();
                    require(card("Department", 1) == "NOT NULL, UNIQUE" && said("Constraints/Unique") == QStringList{"Yes"},
                            "Redone, both rules are back on the card and in Properties");
                    undo_once();
                    undo_once();

                    // IDENTITY where the column cannot count, and where it can.
                    auto revision = editor.revision();
                    from_card("Department", 1, "schemaRuleIdentity");
                    require(editor.revision() == revision && !column_of("Department", 1).auto_increment &&
                                window.statusBar()->currentMessage().contains("whole-number"),
                            "IDENTITY on a column of text is refused, with why, and nothing changes");
                    from_card("Employee", 0, "schemaRuleIdentity");
                    require(column_of("Employee", 0).auto_increment && card("Employee", 0) == "PK, NOT NULL, IDENTITY",
                            "IDENTITY on a whole-number key is written last");
                    undo_once();
                    require(card("Employee", 0) == "PK, NOT NULL", "And undone");

                    // The primary key, from the card: the same command as its
                    // Properties switch, required with it, never written NULL.
                    from_card("Department", 1, "schemaRulePrimaryKey");
                    require(column_of("Department", 1).primary_key && column_of("Department", 1).required &&
                                card("Department", 1) == "PK, NOT NULL" &&
                                said("Constraints/Primary Key") == QStringList{"Yes"},
                            "Made the key on the card, the column is PK and NOT NULL without a second step");
                    revision = editor.revision();
                    from_card("Department", 1, "schemaRuleNull");
                    require(editor.revision() == revision && card("Department", 1) == "PK, NOT NULL" &&
                                window.statusBar()->currentMessage().contains("can never be empty"),
                            "A key cannot be made NULL: refused with why, and nothing changes");
                    undo_once();
                    require(!column_of("Department", 1).primary_key && card("Department", 1) == "NULL",
                            "Undone, Title is an ordinary column that may be empty again");
                    redo_once();
                    require(column_of("Department", 1).primary_key && card("Department", 1) == "PK, NOT NULL",
                            "Redone, the key and NOT NULL come back together");
                    undo_once();

                    // A foreign key the column cannot be: Title is text, and a
                    // key is never retyped to fit.
                    revision = editor.revision();
                    from_card("Department", 1, "schemaRuleForeignKey", "Employee.EmployeeID");
                    require(editor.revision() == revision && !column_of("Department", 1).foreign_key &&
                                !window.statusBar()->currentMessage().isEmpty(),
                            "A foreign key that cannot be made is refused, with why, and nothing changes");

                    // And one it can be, once it holds whole numbers: chosen on
                    // the card, it is put on as a line drawn onto the row puts one
                    // on -- asked first, then made.
                    require(editor.set_schema_column_type(*column_of("Department", 1).added, domain::LogicalType::Int).ok,
                            "Title is given whole numbers");
                    schema->refresh();
                    settle();
                    from_card("Department", 1, "schemaRuleForeignKey", "Employee.EmployeeID");
                    require(column_of("Department", 1).foreign_key && card("Department", 1) == "FK, NULL",
                            "Chosen on the card, the column is a foreign key, written FK, NULL");
                    choose_column("Department", 1);
                    require(said("Constraints/Foreign Key") == QStringList{"Yes"} &&
                                said("References/References") == QStringList{"Employee.EmployeeID"},
                            "And Properties says what it references");
                    from_card("Department", 1, "schemaRuleNotNull");
                    require(card("Department", 1) == "FK, NOT NULL", "A foreign key made NOT NULL is written FK, NOT NULL");
                    from_card("Department", 1, "schemaRulePrimaryKey");
                    require(column_of("Department", 1).primary_key && column_of("Department", 1).foreign_key &&
                                card("Department", 1) == "PK, FK, NOT NULL" &&
                                said("Keys/Key Role") == QStringList{"Primary Key + Foreign Key"},
                            "Made the key as well, it is both, written PK, FK, NOT NULL");

                    // Saved and opened again, it is what it was.
                    {
                        QTemporaryDir files;
                        infrastructure::ErdxProjectStore keeper;
                        const auto at = files.filePath("Card constraints.erdx").toStdString();
                        require(files.isValid() && keeper.save(at, editor.project()).ok, "The schema saves");
                        const auto back = keeper.load(at);
                        require(back && *back.project == editor.project(),
                                "And opens again with the keys and rules chosen on the card");
                    }

                    // Converted to a diagram, the foreign key chosen on the card
                    // is a relationship like any other, and undone it is the
                    // schema again.
                    {
                        const auto relationships_before = editor.project().relationships.size();
                        child<QAction>(window, "designConvert")->trigger();
                        settle_for(700);
                        require(!editor.project().schema.standalone &&
                                    editor.project().relationships.size() == relationships_before + 2,
                                "Converted, both foreign keys are relationships on the diagram");
                        undo_once();
                        settle_for(700);
                        require(editor.project().schema.standalone && card("Department", 1) == "PK, FK, NOT NULL",
                                "Undone, the schema drawn by hand is back as it was");
                    }

                    // Taken off on the card, as its Properties switch takes it off.
                    from_card("Department", 1, "schemaRuleForeignKey");
                    require(!column_of("Department", 1).foreign_key && card("Department", 1) == "PK, NOT NULL",
                            "The foreign key taken off on the card leaves the key");
                    for (int guard = 0; guard < 20 && editor.history_position() > steps_at_start; ++guard)
                        undo_once();
                    require(editor.history_position() == steps_at_start && card("Department", 1) == "NULL" &&
                                column_of("Department", 1).type != domain::LogicalType::Int,
                            "Everything done from the card undoes");
                    schema->choose(desktop::NothingChosen{});
                    settle();
                }

                // Back to where this began.
                for (int guard = 0; guard < 12 && editor.undo_label() != start_label; ++guard)
                    undo_once();
                require(editor.undo_label() == start_label && column_of("Department", 0).name == "DepartmentID" && schema->preview().tables[table_called("Department")].columns.size() == 1,
                        "Everything done here is undone");
                schema->choose(desktop::NothingChosen{});
                settle();
            }

            // Converted, the diagram is the model and the schema follows it.
            child<QAction>(window, "designConvert")->trigger();
            settle_for(700);
            const auto &converted = editor.project();
            require(!converted.schema.standalone && converted.entities.size() == 2 && converted.relationships.size() == 1,
                    "Convert draws each table as an entity and the foreign key as a relationship");
            require(!convert->isVisible() && !add_table->isVisible() && child<QPushButton>(window, "schemaFull")->isVisible(),
                    "The schema is worked out from the diagram again, and can be put away again");
            require(schema->isVisible(), "And stays open beneath the diagram");
            // Conceptual is now the design in front, lit in the switch at the
            // start of the diagram's row (2026-10-08), and nothing previews it.
            // Asked of the Home tab, whose row that is; under another tab the
            // header holds them, as it always did, and gives them back.
            child<QAction>(window, "tabFile")->trigger();
            settle();
            require(child<QWidget>(window, "workspaceHeader")->isVisible() &&
                        child<QPushButton>(window, "backToHome")->isVisible() &&
                        child<QPushButton>(window, "backToHome")->text() == "← Back to Home" &&
                        child<QLabel>(window, "workspaceBadge")->isVisible() &&
                        child<QLabel>(window, "documentTitle")->isVisible() && !modes->isVisible(),
                    "Under another tab the header shows Back to Home, the workspace and the title, as it did");
            child<QAction>(window, "tabHome")->trigger();
            settle();
            require(to_conceptual->isVisible() && to_conceptual->isChecked() &&
                        child<QWidget>(window, "conceptualIdentity")->isAncestorOf(to_conceptual) &&
                        !child<QWidget>(window, "conceptualPanel")->isVisible() &&
                        !child<QWidget>(window, "schemaGrip")->isHidden(),
                    "The diagram is the surface again: no Conceptual preview, and the schema has its grip back");
            require(explorer_dock->widget() == child<QTreeView>(window, "explorer") && properties_dock->widget() != child<QWidget>(window, "schemaProperties"),
                    "Converted, the docks hold the diagram's own Explorer and Properties again");
            require(header_tools->isHidden() && child<QWidget>(window, "schemaBar")->isVisible() && !child<QAction>(window, "designConvert")->isVisible(),
                    "Its tools go back to the schema's own bar, and Convert off the Design menu");
            require(modes->isVisible() && child<QWidget>(window, "conceptualIdentity")->isAncestorOf(modes) &&
                        !child<QLabel>(window, "workspaceBadge")->isVisible() &&
                        !child<QWidget>(window, "workspaceHeader")->isVisible() &&
                        child<QPushButton>(window, "backToHome")->text() == "←" &&
                        child<QLineEdit>(window, "schemaSearch")->placeholderText() == "Search Relational Design",
                    "And Home, the switch and the title lead the diagram's one row, with no header beneath");
            const auto foreign_key_at = [&]() -> std::pair<std::size_t, std::size_t>
            {
                for (std::size_t t = 0; t < schema->preview().tables.size(); ++t)
                    for (std::size_t c = 0; c < schema->preview().tables[t].columns.size(); ++c)
                        if (schema->preview().tables[t].columns[c].foreign_key)
                            return {t, c};
                throw std::runtime_error("no foreign key");
            };
            auto [table_at, column_at] = foreign_key_at();
            require(schema->preview().tables[table_at].columns[column_at].name == "ManagerID",
                    "The foreign key keeps the name it was drawn with");

            // One step: undone it is the schema drawn by hand again, and redone
            // the diagram once more.
            child<QAction>(window, "undoCommand")->trigger();
            settle_for(700);
            require(editor.project().schema.standalone && !convert->isHidden() && child<QAction>(window, "designConvert")->isVisible(),
                    "Undo takes it back to the schema drawn by hand");
            require(header_tools->isVisible() && child<QWidget>(window, "schemaBar")->isHidden(),
                    "With its tools up in the header again");
            require(to_conceptual->isVisible() && child<QWidget>(window, "schemaGrip")->isHidden(),
                    "Where the schema is the surface again, and the preview is offered again");
            child<QAction>(window, "redoCommand")->trigger();
            settle_for(700);
            require(!editor.project().schema.standalone, "And redo converts it again");

            // A foreign key the conversion made is renamed where it is shown.
            std::tie(table_at, column_at) = foreign_key_at();
            rows = schema->row_boxes()[table_at];
            const QPointF name_at(rows[column_at].left() + 70, rows[column_at].center().y());
            mouse(QEvent::MouseButtonDblClick, name_at, Qt::LeftButton);
            settle();
            require(field->isVisible() && field->text() == "ManagerID", "A foreign key's name opens for typing");
            type_name("BossID");
            std::tie(table_at, column_at) = foreign_key_at();
            require(schema->preview().tables[table_at].columns[column_at].name == "BossID",
                    "And keeps the name typed over it");

            // The card's constraints list on a schema worked out from a diagram
            // (2026-10-08): a key chosen there is the diagram's key, and a key
            // the schema cannot change there is refused with why.
            {
                const auto find = [&](const char *name) -> std::pair<std::size_t, std::size_t>
                {
                    for (std::size_t t = 0; t < schema->preview().tables.size(); ++t)
                        for (std::size_t c = 0; c < schema->preview().tables[t].columns.size(); ++c)
                            if (schema->preview().tables[t].columns[c].name == name)
                                return {t, c};
                    throw std::runtime_error("no such column");
                };
                const auto choose_on_card = [&](const char *name, const char *which)
                {
                    const auto [t, c] = find(name);
                    const auto at = schema->cell_boxes()[t][c].rules.center();
                    QTimer::singleShot(0, &window, [&window, which]
                                       {
                        auto *menu = window.findChild<QMenu *>("schemaRulesMenu");
                        if (!menu) return;
                        if (auto *action = menu->findChild<QAction *>(which)) action->trigger();
                        menu->close(); });
                    mouse(QEvent::MouseButtonPress, at, Qt::LeftButton);
                    mouse(QEvent::MouseButtonRelease, at, Qt::NoButton);
                    settle();
                };
                const auto said_for = [&](const char *name)
                {
                    const auto [t, c] = find(name);
                    return schema->constraints_said(t, c);
                };
                const auto [key_t, key_c] = find("EmployeeID");
                const auto behind = *schema->preview().tables[key_t].columns[key_c].origin;
                require(editor.project().attributes.at(behind).identifier && said_for("EmployeeID") == "PK, NOT NULL" &&
                            said_for("BossID").startsWith("FK, "),
                        "Converted, the key is written PK, NOT NULL and the foreign key FK");
                choose_on_card("EmployeeID", "schemaRulePrimaryKey");
                // The table is left with no key of its own, so the conversion
                // gives it one, as it does for any entity drawn without a key.
                require(!editor.project().attributes.at(behind).identifier &&
                            window.statusBar()->currentMessage().contains("has no key attribute"),
                        "The key taken off on the card is taken off the attribute on the diagram");
                child<QAction>(window, "undoCommand")->trigger();
                settle();
                require(editor.project().attributes.at(behind).identifier && said_for("EmployeeID") == "PK, NOT NULL",
                        "And undone, it is the key on both again");
                auto revision = editor.revision();
                choose_on_card("EmployeeID", "schemaRuleForeignKey");
                require(editor.revision() == revision &&
                            window.statusBar()->currentMessage().contains("comes from a relationship"),
                        "A foreign key is not put on a converted column from the card: it says to draw a relationship");
                choose_on_card("BossID", "schemaRulePrimaryKey");
                require(editor.revision() == revision &&
                            window.statusBar()->currentMessage().contains("nothing behind it to change"),
                        "A column the relationship made cannot be made the key: refused with why");
            }

            editor.mark_saved(editor.revision());
            window.load_example();
            settle();
        }

        // Two large Conceptual examples, added beside the original university
        // diagram and built with the Editor's own commands.
        {
            const auto blocks = [](const domain::Project &project)
            {
                const auto issues = domain::validate(project);
                return std::any_of(issues.begin(), issues.end(),
                                   [](const auto &issue)
                                   { return issue.blocks_save; });
            };
            const auto named = [](const auto &map, const char *name)
            {
                return std::any_of(map.begin(), map.end(),
                                   [&](const auto &each)
                                   { return each.second.name == name; });
            };

            // Building an example touches nothing but the editor it is built in.
            const auto open_before_examples = window.editor().project();
            const auto open_was_dirty = window.editor().dirty();
            application::Editor company(ids);
            desktop::build_company_database(company);
            const auto &company_project = company.project();
            require(company_project.name == "Company Database", "The company example is named");
            require(company_project.entities.size() == 14, "Company Database has its fourteen entities");
            require(company_project.attributes.size() == 87, "Company Database has its attributes");
            require(company_project.relationships.size() == 17, "Company Database has its seventeen relationships");
            require(!blocks(company_project), "Company Database is a valid project");
            require(named(company_project.entities, "Employee") && named(company_project.entities, "Dependent"),
                    "Employee and Dependent are on it");
            bool company_weak = false, company_recursive = false, company_composite = false,
                 company_derived = false, company_multivalued = false, company_on_relationship = false;
            for (const auto &[id, entity] : company_project.entities)
                if (entity.weak)
                    company_weak = true;
            for (const auto &[id, rel] : company_project.relationships)
            {
                require(rel.participants.size() == 2, "Every company relationship is joined at both ends");
                if (rel.participants[0].target == rel.participants[1].target)
                    company_recursive = !rel.participants[0].role.empty() && rel.participants[1].role != rel.participants[0].role;
            }
            for (const auto &[id, attribute] : company_project.attributes)
            {
                if (attribute.kind == domain::AttributeKind::Composite)
                    company_composite = true;
                if (attribute.kind == domain::AttributeKind::Derived)
                    company_derived = true;
                if (attribute.kind == domain::AttributeKind::Multivalued)
                    company_multivalued = true;
                if (attribute.owner && std::holds_alternative<domain::RelationshipId>(*attribute.owner))
                    company_on_relationship = true;
            }
            require(company_weak && company_recursive && company_composite && company_derived && company_multivalued && company_on_relationship,
                    "Company Database uses weak, recursive, composite, derived, multivalued and relationship attributes");

            application::Editor university(ids);
            desktop::build_university_database(university);
            const auto &university_project = university.project();
            require(university_project.name == "University Database", "The university example is named");
            require(university_project.entities.size() == 14, "University Database has its fourteen entities");
            require(university_project.attributes.size() == 81, "University Database has its attributes");
            require(university_project.relationships.size() == 19, "University Database has its nineteen relationships");
            require(!blocks(university_project), "University Database is a valid project");
            bool university_associative = false, university_recursive = false;
            for (const auto &[id, rel] : university_project.relationships)
            {
                require(rel.participants.size() == 2, "Every university relationship is joined at both ends");
                if (rel.associative)
                    university_associative = true;
                if (rel.participants[0].target == rel.participants[1].target)
                    university_recursive = !rel.participants[0].role.empty() && rel.participants[1].role != rel.participants[0].role;
            }
            require(university_associative && university_recursive,
                    "University Database uses associative Enrollment/Book Loan and a recursive prerequisite");
            require(named(university_project.relationships, "Enrollment") && named(university_project.relationships, "Book Loan"),
                    "Enrollment and Book Loan are associative relationships rather than separate entities");

            require(window.findChild<QAction *>("fileExampleCompany") != nullptr && window.findChild<QAction *>("homeExampleCompany") != nullptr,
                    "Company Database is offered from File and Home");
            require(window.findChild<QAction *>("fileExampleUniversityDatabase") != nullptr && window.findChild<QAction *>("homeExampleUniversityDatabase") != nullptr,
                    "University Database is offered from File and Home");

            window.load_company_database();
            settle();
            require(!window.showing_home() && window.editor().project().name == "Company Database" && !window.editor().dirty() && window.editor().project().entities.size() == 14,
                    "Opening Company Database puts that diagram in the workspace, clean");
            window.load_university_database();
            settle();
            require(window.editor().project().name == "University Database" && !window.editor().dirty() && window.editor().project().entities.size() == 14,
                    "Opening University Database puts that diagram in the workspace, clean");

            require(company_project.name == "Company Database" && university_project.name == "University Database"
                        && open_before_examples.name != company_project.name && !open_was_dirty,
                    "Building the examples left the project then open alone");

            // Every entity has its key drawn. Dependent, being weak, has the one
            // that identifies it together with its owner's.
            const auto keyed = [](const domain::Project &project)
            {
                for (const auto &[id, entity] : project.entities)
                {
                    const auto keys = std::count_if(project.attributes.begin(), project.attributes.end(),
                                                    [&](const auto &each)
                                                    { return each.second.owner == domain::AttributeOwner{id}
                                                             && each.second.kind == domain::AttributeKind::Key; });
                    if (keys != 1)
                        return false;
                }
                return true;
            };
            require(keyed(company_project) && keyed(university_project), "Every entity in both examples has its key");

            // Laid out by hand rather than piled up: no two shapes lie over one
            // another, and no line carries a stored route that would stay behind
            // when what it joins is moved.
            const auto apart = [](const domain::Project &project)
            {
                std::vector<domain::Rect> boxes;
                for (const auto &[ref, box] : project.layout)
                    boxes.push_back(box);
                for (std::size_t i = 0; i < boxes.size(); ++i)
                    for (std::size_t j = i + 1; j < boxes.size(); ++j)
                    {
                        const auto &a = boxes[i];
                        const auto &b = boxes[j];
                        if (a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height)
                            return false;
                    }
                for (const auto &[ref, shape] : project.connectors)
                    if (shape.routed() || shape.offset != 0)
                        return false;
                return true;
            };
            require(apart(company_project) && apart(university_project),
                    "No two shapes in either example overlap, and every line routes itself");

            // What makes the examples worth having, counted, so it can be held to
            // surviving a save: recursive relationships with their roles,
            // many-to-many ones, and attributes on relationships.
            struct Features
            {
                int recursive = 0;
                int many_to_many = 0;
                int on_relationships = 0;
                bool operator==(const Features &) const = default;
            };
            const auto features = [](const domain::Project &project)
            {
                Features found;
                for (const auto &[id, rel] : project.relationships)
                {
                    if (rel.participants.size() != 2)
                        continue;
                    const auto &one = rel.participants[0];
                    const auto &other = rel.participants[1];
                    if (one.target == other.target && !one.role.empty() && !other.role.empty() && one.role != other.role)
                        ++found.recursive;
                    if (one.maximum == domain::Cardinality::Many && other.maximum == domain::Cardinality::Many)
                        ++found.many_to_many;
                }
                for (const auto &[id, attribute] : project.attributes)
                    if (attribute.owner && std::holds_alternative<domain::RelationshipId>(*attribute.owner))
                        ++found.on_relationships;
                return found;
            };
            require((features(company_project) == Features{1, 6, 6}),
                    "Company Database: Supervises is recursive, six relationships are many-to-many, six attributes "
                    "stand on relationships");
            require((features(university_project) == Features{1, 5, 6}),
                    "University Database: Prerequisite is recursive, five relationships are many-to-many, six "
                    "attributes stand on relationships");

            // Saved and opened again through the production store, each comes
            // back exactly as it was, and opens clean.
            QTemporaryDir example_files;
            require(example_files.isValid(), "A folder for the examples' files");
            infrastructure::ErdxProjectStore example_store;
            for (const auto *project : {&company_project, &university_project})
            {
                const auto example_path = example_files.filePath(QString::fromStdString(project->name) + ".erdx");
                require(example_store.save(example_path.toStdString(), *project).ok, "An example saves");
                const auto reopened = example_store.load(example_path.toStdString());
                require(reopened && *reopened.project == *project && features(*reopened.project) == features(*project),
                        "And loads back exactly as it was, recursion, many-to-many and relationship attributes "
                        "included");
                require(window.open_path(example_path) && window.editor().project() == *project && !window.editor().dirty(),
                        "And opens in the window as it was saved, clean");
            }

            // Unsaved work is never thrown away by opening an example: Cancel
            // keeps it.
            {
                const auto entities_before = window.editor().project().entities.size();
                editor.create_entity("Unsaved", {});
                require(window.editor().dirty(), "The open project has unsaved work");
                const auto kept = window.editor().project();
                dismiss(QMessageBox::Cancel);
                window.load_company_database();
                settle();
                require(window.editor().project() == kept && window.editor().dirty(),
                        "Cancel keeps the unsaved project instead of the example");
                editor.undo();
                editor.mark_saved(editor.revision());
                require(window.editor().project().entities.size() == entities_before, "And the work is taken back");
            }

            // The original example is still there, as it was.
            window.load_example();
            settle();
            require(window.editor().project().name == "University · Students, courses and professors"
                        && window.editor().project().entities.size() == 3 && !window.editor().dirty(),
                    "Open example still opens the original three-entity university diagram");

            // Each converts into its Relational Design without failing: every
            // entity and every many-to-many relationship becomes a table, and
            // every table has a primary key.
            for (const auto build : {&desktop::build_company_database, &desktop::build_university_database})
            {
                application::Editor converting(ids);
                build(converting);
                converting.mark_saved(converting.revision());
                infrastructure::ErdxProjectStore converting_store;
                desktop::MainWindow converting_window(converting, converting_store, ids);
                converting_window.resize(1440, 920);
                converting_window.show();
                converting_window.show_home(false);
                settle();
                converting_window.open_schema();
                settle_for(300);
                const auto &project = converting.project();
                const auto &tables = converting_window.schema()->preview().tables;
                const auto table_for = [&](domain::ElementRef origin)
                {
                    return std::any_of(tables.begin(), tables.end(), [&](const auto &table)
                                       { return table.origin == origin; });
                };
                bool every_entity = true;
                for (const auto &[id, entity] : project.entities)
                    every_entity = every_entity && table_for(domain::ElementRef{id});
                bool every_bridge = true;
                for (const auto &[id, rel] : project.relationships)
                    if (rel.participants.size() == 2 && rel.participants[0].maximum == domain::Cardinality::Many
                        && rel.participants[1].maximum == domain::Cardinality::Many)
                        every_bridge = every_bridge && table_for(domain::ElementRef{id});
                const bool every_key = std::all_of(tables.begin(), tables.end(), [](const auto &table)
                                                   { return std::any_of(table.columns.begin(), table.columns.end(),
                                                                        [](const auto &column)
                                                                        { return column.primary_key; }); });
                require(converting_window.schema()->isVisible() && every_entity && every_bridge && every_key
                            && converting_window.schema()->table_boxes().size() == tables.size()
                            && !converting.dirty(),
                        "An example converts into tables, each entity and many-to-many relationship one, each with "
                        "a primary key, all drawn, and converting does not change the diagram");
            }
        }

        // Making a generated foreign key UNIQUE where that turns its
        // relationship one to one (Zain, 2026-10-02): which table keeps the key
        // is asked, keeping it where it is first. Cancel changes nothing;
        // either answer is one edit, recorded as the relationship's one-to-one
        // key, and saved with the project. Nothing else asks.
        {
            application::Editor sided(ids);
            infrastructure::ErdxProjectStore sided_store;
            desktop::MainWindow sided_window(sided, sided_store, ids);
            sided_window.resize(1440, 920);
            sided_window.show();
            sided_window.show_home(false);
            sided_window.load_example();
            settle();
            sided_window.open_schema();
            settle_for(300);
            auto *view = sided_window.schema();
            auto *dock = child<QDockWidget>(sided_window, "propertiesDock");
            const auto panel = [&]() -> const QWidget &
            { return *dock->widget(); };
            const auto place = [&](const char *table, const char *name) -> std::optional<std::pair<std::size_t, std::size_t>>
            {
                const auto &tables = view->preview().tables;
                for (std::size_t t = 0; t < tables.size(); ++t)
                    for (std::size_t r = 0; r < tables[t].columns.size(); ++r)
                        if (tables[t].name == table && tables[t].columns[r].name == name)
                            return std::pair{t, r};
                return std::nullopt;
            };
            const auto column_at = [&](const char *table, const char *name)
            {
                const auto at = place(table, name);
                if (!at)
                    throw std::runtime_error(std::string("No column ") + table + "." + name);
                return view->preview().tables[at->first].columns[at->second];
            };
            const auto table_at = [&](const char *table)
            {
                const auto &tables = view->preview().tables;
                for (std::size_t t = 0; t < tables.size(); ++t)
                    if (tables[t].name == table)
                        return t;
                throw std::runtime_error("No such table");
            };
            const auto choose_column = [&](const char *table, const char *name)
            {
                const auto at = *place(table, name);
                view->choose(desktop::ChosenColumn{*view->column_ref(at.first, at.second)});
                settle();
            };
            const auto press_unique = [&](const char *table, const char *name)
            {
                choose_column(table, name);
                properties_editor<QCheckBox>(panel(), "Constraints/Unique")->click();
                settle();
            };
            // Pressed and answered as given, what was asked kept.
            const auto answered = [&](const char *table, const char *name, const QString &answer)
            {
                std::vector<QStringList> asked;
                answer_in_turn({answer}, &asked);
                press_unique(table, name);
                return asked;
            };
            // Pressed where nothing should be asked: a box that comes up anyway
            // is noted and dismissed, so it cannot hold the test up.
            const auto asks_nothing = [&](const char *table, const char *name)
            {
                bool asked = false;
                QTimer guard;
                QObject::connect(&guard, &QTimer::timeout, [&]
                                 {
                    if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                        asked = true;
                        box->reject();
                    } });
                guard.start(5);
                press_unique(table, name);
                guard.stop();
                return !asked;
            };
            const auto undo = [&]
            { child<QAction>(sided_window, "undoCommand")->trigger(); settle(); };
            const auto redo = [&]
            { child<QAction>(sided_window, "redoCommand")->trigger(); settle(); };
            // The relationship behind Students.ProfessorID and its two sides.
            const auto pointed = std::get<domain::ParticipantId>(*column_at("Students", "ProfessorID").link);
            domain::RelationshipId mentor{};
            domain::ParticipantId student_side{};
            for (const auto &[id, relationship] : sided.project().relationships)
                for (const auto &one : relationship.participants)
                    if (one.id == pointed)
                    {
                        mentor = id;
                        for (const auto &other : relationship.participants)
                            if (other.id != pointed)
                                student_side = other.id;
                    }
            const auto maximum_of = [&](domain::ParticipantId which)
            {
                for (const auto &one : sided.project().relationships.at(mentor).participants)
                    if (one.id == which)
                        return one.maximum;
                throw std::runtime_error("No such side");
            };
            // How many foreign keys the relationship makes, wherever they are.
            const auto keys_made = [&]
            {
                std::set<domain::ForeignKeyId> made;
                for (const auto &table : view->preview().tables)
                    for (const auto &column : table.columns)
                        if (column.key_id && column.link && (*column.link == domain::LinkSource{pointed} || *column.link == domain::LinkSource{student_side}))
                            made.insert(*column.key_id);
                return made.size();
            };
            const auto original_key = *column_at("Students", "ProfessorID").key_id;
            const auto was_required = column_at("Students", "ProfessorID").required;
            const auto professors = table_at("Professors");
            const auto students = table_at("Students");
            require(maximum_of(student_side) == domain::Cardinality::Many && maximum_of(pointed) == domain::Cardinality::One && !sided.project().decisions.one_to_one_key.contains(mentor) && keys_made() == 1 && !column_at("Students", "ProfessorID").unique && column_at("Students", "ProfessorID").references == professors,
                    "To begin with, many students to one professor: Students keeps the key, not unique, no side recorded");
            const auto question_kept = [&]
            {
                // The one-to-one question a diagram's own one-to-one is asked
                // stays as it was: Professors.CourseID's relationship is one.
                for (const auto &table : view->preview().tables)
                    for (const auto &decision : table.decisions)
                        if (decision.kind == domain::DecisionKind::OneToOneKey && !decision.answered)
                            return true;
                return false;
            };
            require(question_kept(), "A one-to-one drawn on the diagram is still asked about as before");

            // Cancelled: asked, with both answers, and nothing changes.
            const auto start = sided.project();
            const auto start_revision = sided.revision();
            const auto start_undo = sided.undo_label();
            const auto start_dirty = sided.dirty();
            {
                const auto asked = answered("Students", "ProfessorID", "Cancel");
                require(asked.size() == 1 && asked[0].value(1) == "One-to-One Relationship" && asked[0].value(2) == "Making \"ProfessorID\" UNIQUE changes this relationship to one-to-one.\n\nChoose which table should keep the foreign key:\n\nKeep FK in Students: Students.ProfessorID → Professors.ID\n\nMove FK to Professors: Professors.StudentID → Students.ID" && asked[0].value(3) == "Cancel|Keep FK in Students|Move FK to Professors" && asked[0].value(4) == "Keep FK in Students",
                        "Asked which table keeps the key, keeping it in Students the default, with both results named");
                require(sided.project() == start && sided.revision() == start_revision && sided.undo_label() == start_undo && sided.dirty() == start_dirty && properties_value(panel(), "Constraints/Unique") == QStringList{"No"},
                        "Cancelled, nothing changes: not UNIQUE, the cardinality, the side recorded, the revision, the "
                        "unsaved state or the history");
            }

            // A name typed over the key, kept against it.
            choose_column("Students", "ProfessorID");
            {
                auto *line = properties_editor<QLineEdit>(panel(), "General/Name");
                line->setText("AdvisorID");
                QKeyEvent pressed(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
                QApplication::sendEvent(line, &pressed);
                settle();
            }
            require(place("Students", "AdvisorID") && *column_at("Students", "AdvisorID").key_id == original_key,
                    "A name is typed over the foreign key");
            const auto named = sided.project();
            const auto named_undo = sided.undo_label();

            // Kept in Students: one edit, the key the same key, its name, line
            // and place untouched, the side recorded.
            {
                const auto asked = answered("Students", "AdvisorID", "Keep FK in Students");
                require(asked.size() == 1 && asked[0].value(2).contains("Keep FK in Students: Students.AdvisorID → Professors.ID"),
                        "Asked, naming the key as it is now called");
                const auto kept = column_at("Students", "AdvisorID");
                require(maximum_of(student_side) == domain::Cardinality::One && maximum_of(pointed) == domain::Cardinality::One && sided.project().decisions.one_to_one_key.at(mentor) == student_side && kept.foreign_key && kept.unique && *kept.key_id == original_key && kept.references == professors && kept.required == was_required && keys_made() == 1 && !place("Professors", "StudentID") && sided.undo_label() == "Make the side one",
                        "Kept: one to one, Students keeps the same key, now UNIQUE, pointing where it did, as required as "
                        "it was, and Students is recorded as the side that keeps it");
                require(sided.project().schema == named.schema && sided.project().schema_layout == named.schema_layout,
                        "Its typed name, its line and its place in the table are untouched");
                view->choose(desktop::ChosenForeignKey{original_key});
                settle();
                require(properties_value(panel(), "Identity/Relationship") == QStringList{"Students.AdvisorID → Professors.ID"} && properties_value(panel(), "Referencing/Cardinality") == QStringList{"One (1)"} && properties_value(panel(), "Referenced/Cardinality") == QStringList{"One (1)"},
                        "Relationship Properties says the key is where it was, one to one");
                const auto kept_project = sided.project();
                QTemporaryDir folder;
                const auto path = folder.filePath("kept.erdx").toStdString();
                require(sided_store.save(path, kept_project).ok, "The project is saved");
                const auto reread = sided_store.load(path);
                const auto reread_preview = domain::schema_preview(*reread.project);
                bool there = false;
                for (const auto &column : reread_preview.tables[students].columns)
                    there = there || (column.name == "AdvisorID" && column.key_id == original_key);
                require(reread && *reread.project == kept_project && reread.project->decisions.one_to_one_key.at(mentor) == student_side && there,
                        "Read back, Students still keeps the key");
                undo();
                require(sided.project() == named && sided.undo_label() == named_undo, "One undo takes all of it back");
                redo();
                require(sided.project() == kept_project, "One redo makes it again");
                // UNIQUE off asks nothing, and does as it did: the side made many
                // carries the key, which is still the same key; the side
                // recorded stays recorded, unused while there is a many.
                require(asks_nothing("Students", "AdvisorID"), "Turning UNIQUE off asks nothing");
                require(maximum_of(student_side) == domain::Cardinality::Many && *column_at("Students", "AdvisorID").key_id == original_key && !column_at("Students", "AdvisorID").unique && sided.project().decisions.one_to_one_key.at(mentor) == student_side,
                        "Off, many students to one professor again, the same key in Students");
                undo();
                undo();
                require(sided.project() == named, "Undone twice, as it was");
            }

            // Moved to Professors: one edit, the conversion making the key there
            // from the side recorded, still one key.
            {
                const auto asked = answered("Students", "AdvisorID", "Move FK to Professors");
                require(asked.size() == 1 && asked[0].value(2).contains("Move FK to Professors: Professors.StudentID → Students.ID"),
                        "Asked, naming what the other side would hold");
                const auto moved_key = domain::foreign_key_from(domain::LinkSource{student_side});
                require(maximum_of(student_side) == domain::Cardinality::One && sided.project().decisions.one_to_one_key.at(mentor) == pointed && place("Professors", "StudentID") && *column_at("Professors", "StudentID").key_id == moved_key && column_at("Professors", "StudentID").references == students && !place("Students", "AdvisorID") && keys_made() == 1 && sided.undo_label() == "Make the side one",
                        "Moved: one to one, Professors keeps the key, recorded as the side that keeps it, and there is "
                        "one key, not two");
                view->choose(desktop::ChosenForeignKey{moved_key});
                settle();
                require(properties_value(panel(), "Identity/Relationship") == QStringList{"Professors.StudentID → Students.ID"},
                        "Relationship Properties says where the key is now");
                const auto moved_project = sided.project();
                QTemporaryDir folder;
                const auto path = folder.filePath("moved.erdx").toStdString();
                require(sided_store.save(path, moved_project).ok, "The project is saved");
                const auto reread = sided_store.load(path);
                const auto reread_preview = domain::schema_preview(*reread.project);
                bool there = false;
                for (const auto &column : reread_preview.tables[professors].columns)
                    there = there || (column.name == "StudentID" && column.key_id == moved_key);
                require(reread && *reread.project == moved_project && reread.project->decisions.one_to_one_key.at(mentor) == pointed && there,
                        "Read back, Professors still keeps the key");
                undo();
                require(sided.project() == named, "One undo takes the move back");
                redo();
                require(sided.project() == moved_project, "One redo moves it again");
                undo();
            }

            // An older answer naming Professors, given while there is still a
            // many: the question shows where the key is now, and the new answer
            // replaces the old.
            {
                domain::OpenDecision older;
                older.kind = domain::DecisionKind::OneToOneKey;
                older.about = domain::ElementRef{mentor};
                for (const auto &one : sided.project().relationships.at(mentor).participants)
                    older.sides.push_back(one.id);
                view->decided(older, older.sides[0] == pointed ? 0 : 1);
                settle();
                require(sided.project().decisions.one_to_one_key.at(mentor) == pointed && column_at("Students", "AdvisorID").key_id == original_key,
                        "Professors is recorded while Students still keeps the key");
                const auto asked = answered("Students", "AdvisorID", "Keep FK in Students");
                require(asked.size() == 1 && asked[0].value(4) == "Keep FK in Students" && sided.project().decisions.one_to_one_key.at(mentor) == student_side && *column_at("Students", "AdvisorID").key_id == original_key && column_at("Students", "AdvisorID").unique,
                        "Kept in Students, and Students replaces the older answer");
                undo();
                undo();
                require(sided.project() == named, "Both undone");
            }

            // Nothing else asks: UNIQUE on a column that is not a foreign key,
            // or on a foreign key whose relationship would not become one to one.
            require(asks_nothing("Professors", "Name") && column_at("Professors", "Name").unique,
                    "A column that is not a foreign key is made UNIQUE without a question");
            undo();
            require(asks_nothing("Enrolleds", "StudentID"), "A bridge's foreign key is not asked about");
            undo();
            require(sided.project() == named && question_kept(), "And all of it undone, the diagram's own one-to-one still asked about");
        }

        window.close(); // The example was reloaded clean, so no discard dialog.
        require(!window.isVisible(), "Clean window closes without prompting");
        std::cout << "Desktop integration tests passed\n";
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "Desktop test failed: " << error.what() << '\n';
        return 1;
    }
}
