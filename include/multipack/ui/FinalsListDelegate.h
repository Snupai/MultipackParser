/**
 * @file FinalsListDelegate.h
 * @brief Draws the planner's Fertigstände as touch-sized cards
 */
#ifndef MULTIPACK_UI_FINALSLISTDELEGATE_H
#define MULTIPACK_UI_FINALSLISTDELEGATE_H

#include <QString>
#include <QStyledItemDelegate>

namespace multipack {
namespace ui {

/**
 * @brief Item data of one Fertigstand row
 *
 * Qt::DisplayRole keeps a plain one-line summary (name, date, states); the
 * card is drawn from the roles below.
 */
namespace FinalsRole {
constexpr int FinalId = Qt::UserRole;          ///< QString
constexpr int Downloaded = Qt::UserRole + 1;   ///< bool: validated and stored, can be loaded
constexpr int Name = Qt::UserRole + 2;         ///< QString
constexpr int Detail = Qt::UserRole + 3;       ///< QString: second line (date, version, error reason)
constexpr int DetailIsError = Qt::UserRole + 4; ///< bool: draw the second line as an error
constexpr int Chips = Qt::UserRole + 5;        ///< QStringList of FinalsChip::encode()
constexpr int OlderVersion = Qt::UserRole + 6; ///< bool: older Fertigstand of the plan above it
constexpr int MarkedAt = Qt::UserRole + 7;     ///< QString: "dd.MM.yy HH:mm", empty when unknown
} // namespace FinalsRole

/// State chip of a Fertigstand ("läuft", "Neue Version", "Fehler", ...).
namespace FinalsChip {
constexpr const char* Success = "success";
constexpr const char* Warning = "warning";
constexpr const char* Danger = "danger";
constexpr const char* Info = "info";
constexpr const char* Neutral = "neutral";

/// "<tone>|<text>" for FinalsRole::Chips
[[nodiscard]] QString encode(const char* tone, const QString& text);
/// Text of an encoded chip
[[nodiscard]] QString text(const QString& encoded);
} // namespace FinalsChip

/**
 * @class FinalsListDelegate
 * @brief One card per Fertigstand: selection mark, name, state chips, detail line
 *
 * Rows are ROW_HEIGHT high so that a gloved finger hits them; the delegate
 * paints everything itself and ignores the list's item stylesheet rules.
 */
class FinalsListDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    static constexpr int ROW_HEIGHT = 78;

    explicit FinalsListDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_FINALSLISTDELEGATE_H
