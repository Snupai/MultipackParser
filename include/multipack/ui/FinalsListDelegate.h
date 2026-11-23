/**
 * @file FinalsListDelegate.h
 * @brief Draws the rows of the plan list on "Pläne" as touch-sized cards
 */
#ifndef MULTIPACK_UI_FINALSLISTDELEGATE_H
#define MULTIPACK_UI_FINALSLISTDELEGATE_H

#include <QString>
#include <QStyledItemDelegate>

namespace multipack {
namespace ui {

/**
 * @brief Item data of one row of the plan list
 *
 * Local (USB) plans and plans synced from the Palettenplaner (Fertigstände)
 * are rows of the same kind. Qt::DisplayRole keeps a plain one-line summary
 * (name, date, states); the card is drawn from the roles below.
 */
namespace FinalsRole {
constexpr int FinalId = Qt::UserRole;        ///< QString: Fertigstand id, empty for a local plan
constexpr int Downloaded = Qt::UserRole + 1; ///< bool: stored locally, can be previewed and loaded
constexpr int Name = Qt::UserRole + 2;       ///< QString
constexpr int Chips = Qt::UserRole + 3;      ///< QStringList of FinalsChip::encode()
constexpr int SourceTag = Qt::UserRole + 4;  ///< QString: small source hint after the name ("Planer"), may be empty
constexpr int FileName = Qt::UserRole + 5;   ///< QString: palette file name in the database
constexpr int MarkedAt = Qt::UserRole + 6;   ///< QString: "dd.MM.yy HH:mm" of a planner plan, empty when unknown
constexpr int LoadError = Qt::UserRole + 7;  ///< QString: why a planner plan cannot be loaded, empty if it can
} // namespace FinalsRole

/// State chip of a row ("läuft", "Neue Version", "Fehler", ...).
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
 * @brief One card per plan: selection mark, name, source hint, state chips
 *
 * Rows are ROW_HEIGHT high so that a gloved finger hits them. The delegate
 * paints everything itself and ignores the list's item stylesheet rules.
 */
class FinalsListDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    static constexpr int ROW_HEIGHT = 64;

    explicit FinalsListDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_FINALSLISTDELEGATE_H
