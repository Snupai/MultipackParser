/**
 * @file FinalsListDelegate.cpp
 * @brief Draws the rows of the plan list on "Pläne" as touch-sized cards
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
constexpr int CHIP_HEIGHT = 28;
constexpr int CHIP_PADDING = 11;
constexpr int CHIP_SPACING = 6;
constexpr int TAG_HEIGHT = 22;
constexpr int TAG_PADDING = 7;
constexpr int NAME_PIXEL_SIZE = 20;
constexpr int CHIP_PIXEL_SIZE = 15;
constexpr int TAG_PIXEL_SIZE = 13;

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
    const bool loadable = index.data(FinalsRole::Downloaded).toBool();
    const QColor primary(QString::fromLatin1(Theme::PRIMARY));
    const QColor border(QString::fromLatin1(Theme::BORDER));
    const QColor muted(QString::fromLatin1(Theme::TEXT_MUTED));

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // Card
    const QRectF card = QRectF(option.rect).adjusted(1, 1, -1, -1 - CARD_GAP);
    painter->setPen(selected ? QPen(primary, 2) : QPen(border, 1));
    painter->setBrush(selected ? QColor(QStringLiteral("#eef2fa")) : QColor(QString::fromLatin1(Theme::SURFACE)));
    painter->drawRoundedRect(card, CARD_RADIUS, CARD_RADIUS);
    const qreal centerY = card.center().y();

    // Selection mark: ring, filled with a dot when selected
    const QRectF mark(card.left() + CARD_PADDING, centerY - MARK_SIZE / 2.0, MARK_SIZE, MARK_SIZE);
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

    // State chips, right-aligned
    const QFont chipFont = sized(option.font, CHIP_PIXEL_SIZE, true);
    const QFontMetrics chipMetrics(chipFont);
    const QStringList chips = index.data(FinalsRole::Chips).toStringList();
    painter->setFont(chipFont);
    for (int i = chips.size() - 1; i >= 0; --i) {
        const QString tone = chips.at(i).section(QLatin1Char('|'), 0, 0);
        const QString label = FinalsChip::text(chips.at(i));
        const qreal width = chipMetrics.horizontalAdvance(label) + 2 * CHIP_PADDING;
        const QRectF chip(textRight - width, centerY - CHIP_HEIGHT / 2.0, width, CHIP_HEIGHT);
        const ChipColors colors = chipColors(tone);
        painter->setPen(Qt::NoPen);
        painter->setBrush(colors.background);
        painter->drawRoundedRect(chip, CHIP_HEIGHT / 2.0, CHIP_HEIGHT / 2.0);
        painter->setPen(colors.text);
        painter->drawText(chip, Qt::AlignCenter, label);
        textRight = chip.left() - CHIP_SPACING;
    }

    // Name, followed by the small source hint; the hint keeps its place when the name is cut.
    const QString tag = index.data(FinalsRole::SourceTag).toString();
    const QFont tagFont = sized(option.font, TAG_PIXEL_SIZE, false);
    const qreal tagWidth = tag.isEmpty() ? 0 : QFontMetrics(tagFont).horizontalAdvance(tag) + 2 * TAG_PADDING;
    const qreal tagGap = tag.isEmpty() ? 0 : 10;

    const QFont nameFont = sized(option.font, NAME_PIXEL_SIZE, true);
    const QFontMetrics nameMetrics(nameFont);
    const int nameSpace = static_cast<int>(qMax<qreal>(0, textRight - 6 - textLeft - tagWidth - tagGap));
    const QString name = nameMetrics.elidedText(index.data(FinalsRole::Name).toString(), Qt::ElideRight, nameSpace);
    painter->setFont(nameFont);
    painter->setPen(loadable ? QColor(QString::fromLatin1(Theme::TEXT)) : muted);
    painter->drawText(QRectF(textLeft, centerY - 16, nameSpace, 32), Qt::AlignLeft | Qt::AlignVCenter, name);

    if (!tag.isEmpty()) {
        const QRectF tagRect(textLeft + nameMetrics.horizontalAdvance(name) + tagGap, centerY - TAG_HEIGHT / 2.0 + 1,
                             tagWidth, TAG_HEIGHT);
        painter->setPen(QPen(QColor(QStringLiteral("#b9c2ce")), 1));
        painter->setBrush(Qt::NoBrush);
        painter->drawRoundedRect(tagRect, 5, 5);
        painter->setFont(tagFont);
        painter->setPen(muted);
        painter->drawText(tagRect, Qt::AlignCenter, tag);
    }

    painter->restore();
}

} // namespace ui
} // namespace multipack
