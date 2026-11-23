/**
 * @file Theme.cpp
 * @brief Shared look of the HMI
 */
#include "multipack/ui/Theme.h"

#include <QApplication>
#include <QColor>
#include <QDebug>
#include <QFile>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QStyle>
#include <QStyleFactory>
#include <QWidget>

// Q_INIT_RESOURCE has to be used outside of any namespace.
static void initThemeResources()
{
    Q_INIT_RESOURCE(multipack);
}

namespace multipack {
namespace ui {
namespace Theme {

namespace {

void repolish(QWidget* widget)
{
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}

} // namespace

QString toneColor(Tone tone)
{
    switch (tone) {
        case Tone::Info:
            return QString::fromLatin1(INFO);
        case Tone::Success:
            return QString::fromLatin1(SUCCESS);
        case Tone::Warning:
            return QString::fromLatin1(WARNING);
        case Tone::Danger:
            return QString::fromLatin1(DANGER);
        case Tone::Neutral:
            break;
    }
    return QString::fromLatin1(TEXT);
}

QString styleSheet()
{
    // The resources are linked from a static library: register them when the
    // host (a test, a tool) has not done so yet.
    if (!QFile::exists(QStringLiteral(":/styles/hmi.qss"))) {
        initThemeResources();
    }

    QFile file(QStringLiteral(":/styles/hmi.qss"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Theme - stylesheet resource missing: :/styles/hmi.qss";
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

void applyApplicationLook(QApplication& app)
{
    // Fusion renders the same on the Pi, in the containers and on a dev machine.
    if (QStyle* fusion = QStyleFactory::create(QStringLiteral("Fusion"))) {
        app.setStyle(fusion);
    }

    QPalette palette = app.palette();
    const QColor text(QString::fromLatin1(TEXT));
    const QColor disabled(QString::fromLatin1(TEXT_DISABLED));
    const QColor primary(QString::fromLatin1(PRIMARY));
    palette.setColor(QPalette::Window, QColor(QString::fromLatin1(BACKGROUND)));
    palette.setColor(QPalette::WindowText, text);
    palette.setColor(QPalette::Base, QColor(QString::fromLatin1(SURFACE)));
    palette.setColor(QPalette::AlternateBase, QColor(QString::fromLatin1(BACKGROUND)));
    palette.setColor(QPalette::Text, text);
    palette.setColor(QPalette::Button, QColor(QString::fromLatin1(SURFACE)));
    palette.setColor(QPalette::ButtonText, text);
    palette.setColor(QPalette::ToolTipBase, QColor(QString::fromLatin1(SURFACE)));
    palette.setColor(QPalette::ToolTipText, text);
    palette.setColor(QPalette::Highlight, primary);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Link, primary);
    palette.setColor(QPalette::PlaceholderText, QColor(QString::fromLatin1(TEXT_MUTED)));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, disabled);
    palette.setColor(QPalette::Disabled, QPalette::Text, disabled);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, disabled);
    app.setPalette(palette);
}

void applyTo(QWidget* window)
{
    if (!window) {
        return;
    }
    const QString sheet = styleSheet();
    if (!sheet.isEmpty()) {
        window->setStyleSheet(sheet);
    }
}

void setRole(QWidget* widget, const char* role)
{
    if (!widget) {
        return;
    }
    widget->setProperty("role", QString::fromLatin1(role));
    repolish(widget);
}

void setScale(QWidget* widget, const char* scale)
{
    if (!widget) {
        return;
    }
    widget->setProperty("scale", QString::fromLatin1(scale));
    repolish(widget);
}

void setCard(QWidget* widget)
{
    if (!widget) {
        return;
    }
    widget->setProperty("card", true);
    repolish(widget);
}

void setCompact(QWidget* widget)
{
    if (!widget) {
        return;
    }
    widget->setProperty("density", QStringLiteral("compact"));
    repolish(widget);
}

void setTone(QLabel* label, Tone tone)
{
    if (!label) {
        return;
    }
    label->setStyleSheet(QStringLiteral("color: %1;").arg(toneColor(tone)));
}

bool confirm(QWidget* parent, const QString& title, const QString& text)
{
    QMessageBox box(QMessageBox::Question, title, text, QMessageBox::NoButton, parent);
    QPushButton* yes = box.addButton(QStringLiteral("Ja"), QMessageBox::YesRole);
    QPushButton* no = box.addButton(QStringLiteral("Nein"), QMessageBox::NoRole);
    box.setDefaultButton(no);
    box.setEscapeButton(no);
    box.exec();
    return box.clickedButton() == yes;
}

} // namespace Theme
} // namespace ui
} // namespace multipack
