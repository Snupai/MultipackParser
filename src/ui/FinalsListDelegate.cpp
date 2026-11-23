/**
 * @file FinalsListDelegate.cpp
 * @brief Draws the planner's Fertigstände as touch-sized cards
 */
#include "multipack/ui/FinalsListDelegate.h"

#include "multipack/ui/Theme.h"

#include <QColor>
#include <QFontMetrics>
#include <QPainter>
#include <QPen>
#include <QStringList>

namespace multipack {
namespace ui {

namespace {

constexpr int CARD_GAP = 6;        // vertical space between two cards
constexpr int CARD_RADIUS = 8;
constexpr int CARD_PADDING = 14;
constexpr int MARK_SIZE = 26;      // selection mark
constexpr int OLDER_INDENT = 30;   // older versions sit under their plan
constexpr int CHIP_HEIGHT = 28;
constexpr int CHIP_PADDING = 11;
constexpr int CHIP_SPACING = 6;
constexpr int NAME_PIXEL_SIZE = 20;
constexpr int DETAIL_PIXEL_SIZE = 16;
constexpr int CHIP_PIXEL_SIZE = 15;

struct ChipColors {
    QColor background;
    QColor text;
};

ChipColors chipColors(const QString& tone)
{
    if (tone == QLatin1String(FinalsChip::Success)) {
        return {QColor(QStringLiteral("#dff1e2")), QColor(QString::fromLatin1(Theme::SUCCESS))};
    }
    if (tone == QLatin1String(FinalsChip::Warning)) {
        return {QColor(QString::fromLatin1(Theme::HIGHLIGHT_ROW)), QColor(QStringLiteral("#8a4500"))};
    }
    if (tone == QLatin1String(FinalsChip::Danger)) {
        return {QColor(QStringLiteral("#fbe1df")), QColor(QString::fromLatin1(Theme::DANGER))};
    }
    if (tone == QLatin1String(FinalsChip::Info)) {
        return {QColor(QStringLiteral("#e1ebfb")), QColor(QString::fromLatin1(Theme::INFO))};
    }
    return {QColor(QStringLiteral("#e9edf2")), QColor(QString::fromLatin1(Theme::TEXT_MUTED))};
}

QFont sized(const QFont& base, int pixelSize, bool bold)
{
    QFont font(base);
    font.setPixelSize(pixelSize);
    font.setBold(bold);
    return font;
}

} // namespace

namespace FinalsChip {

QString encode(const char* tone, const QString& text)
{
    return QString::fromLatin1(tone) + QLatin1Char('|') + text;
}

QString text(const QString& encoded)
{
    return encoded.section(QLatin1Char('|'), 1);
}

} // namespace FinalsChip

FinalsListDelegate::FinalsListDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

QSize FinalsListDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    Q_UNUSED(index);
    return QSize(option.rect.width(), ROW_HEIGHT);
}

void FinalsListDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                               const QModelIndex& index) const
{
    const bool selected = option.state.testFlag(QStyle::State_Selected);
    const bool olderVersion = index.data(FinalsRole::OlderVersion).toBool();
    const bool loadable = index.data(FinalsRole::Downloaded).toBool();
    const QColor primary(QString::fromLatin1(Theme::PRIMARY));
    const QColor border(QString::fromLatin1(Theme::BORDER));
    const QColor muted(QString::fromLatin1(Theme::TEXT_MUTED));

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QRectF card = QRectF(option.rect).adjusted(1, 1, -1, -1 - CARD_GAP);
    if (olderVersion) {
        // Elbow from the plan above to its older version.
        const qreal x = card.left() + OLDER_INDENT / 2.0;
        painter->setPen(QPen(QColor(QStringLiteral("#b9c2ce")), 2));
        painter->drawLine(QPointF(x, card.top() - CARD_GAP), QPointF(x, card.center().y()));
        painter->drawLine(QPointF(x, card.center().y()), QPointF(card.left() + OLDER_INDENT - 4, card.center().y()));
        card.setLeft(card.left() + OLDER_INDENT);
    }

    // Card
    painter->setPen(selected ? QPen(primary, 2) : QPen(border, 1));
    painter->setBrush(selected ? QColor(QStringLiteral("#eef2fa")) : QColor(QString::fromLatin1(Theme::SURFACE)));
    painter->drawRoundedRect(card, CARD_RADIUS, CARD_RADIUS);

    // Selection mark: ring, filled with a dot when selected
    const QRectF mark(card.left() + CARD_PADDING, card.center().y() - MARK_SIZE / 2.0, MARK_SIZE, MARK_SIZE);
    painter->setPen(QPen(selected ? primary : QColor(QStringLiteral("#7c8796")), 2));
    painter->setBrush(QColor(QString::fromLatin1(Theme::SURFACE)));
    painter->drawEllipse(mark);
    if (selected) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(primary);
        painter->drawEllipse(mark.adjusted(6, 6, -6, -6));
    }

    const qreal textLeft = mark.right() + 12;
    qreal textRight = card.right() - CARD_PADDING;
    const qreal lineOneCenter = card.top() + 25;
    const qreal lineTwoCenter = card.bottom() - 20;

    // State chips, right-aligned on the first line
    const QFont chipFont = sized(option.font, CHIP_PIXEL_SIZE, true);
    const QFontMetrics chipMetrics(chipFont);
    const QStringList chips = index.data(FinalsRole::Chips).toStringList();
    painter->setFont(chipFont);
    for (int i = chips.size() - 1; i >= 0; --i) {
        const QString tone = chips.at(i).section(QLatin1Char('|'), 0, 0);
        const QString label = FinalsChip::text(chips.at(i));
        const qreal width = chipMetrics.horizontalAdvance(label) + 2 * CHIP_PADDING;
        const QRectF chip(textRight - width, lineOneCenter - CHIP_HEIGHT / 2.0, width, CHIP_HEIGHT);
        const ChipColors colors = chipColors(tone);
        painter->setPen(Qt::NoPen);
        painter->setBrush(colors.background);
        painter->drawRoundedRect(chip, CHIP_HEIGHT / 2.0, CHIP_HEIGHT / 2.0);
        painter->setPen(colors.text);
        painter->drawText(chip, Qt::AlignCenter, label);
        textRight = chip.left() - CHIP_SPACING;
    }

    // Name
    const QFont nameFont = sized(option.font, NAME_PIXEL_SIZE, true);
    const QFontMetrics nameMetrics(nameFont);
    const qreal nameWidth = qMax<qreal>(0, textRight - 6 - textLeft);
    painter->setFont(nameFont);
    painter->setPen(loadable ? QColor(QString::fromLatin1(Theme::TEXT)) : muted);
    painter->drawText(QRectF(textLeft, lineOneCenter - 15, nameWidth, 30), Qt::AlignLeft | Qt::AlignVCenter,
                      nameMetrics.elidedText(index.data(FinalsRole::Name).toString(), Qt::ElideRight,
                                             static_cast<int>(nameWidth)));

    // Detail line: date, version, or why it cannot be loaded
    const QFont detailFont = sized(option.font, DETAIL_PIXEL_SIZE, false);
    const QFontMetrics detailMetrics(detailFont);
    const qreal detailWidth = qMax<qreal>(0, card.right() - CARD_PADDING - textLeft);
    painter->setFont(detailFont);
    painter->setPen(index.data(FinalsRole::DetailIsError).toBool() ? QColor(QString::fromLatin1(Theme::DANGER))
                                                                   : muted);
    painter->drawText(QRectF(textLeft, lineTwoCenter - 12, detailWidth, 24), Qt::AlignLeft | Qt::AlignVCenter,
                      detailMetrics.elidedText(index.data(FinalsRole::Detail).toString(), Qt::ElideRight,
                                               static_cast<int>(detailWidth)));

    painter->restore();
}

} // namespace ui
} // namespace multipack
