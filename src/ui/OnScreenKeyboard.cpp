/**
 * @file OnScreenKeyboard.cpp
 * @brief Shared QWidget on-screen keyboard implementation.
 */
#include "multipack/ui/OnScreenKeyboard.h"

#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCompleter>
#include <QEvent>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScreen>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

namespace multipack {
namespace ui {

namespace {
constexpr int kKeyboardHeight = 320;
constexpr int kHeaderHeight = 40;

struct KeySpec {
    QString label;
    QString action;
    QString text;
    int span = 1;
    bool function = false;
};

KeySpec textKey(const QString& label, const QString& text = QString(), int span = 1)
{
    return {label, QStringLiteral("text"), text.isEmpty() ? label : text, span, false};
}

KeySpec actionKey(const QString& label, const QString& action, int span = 1)
{
    return {label, action, QString(), span, true};
}
} // namespace

OnScreenKeyboard::OnScreenKeyboard(QWidget* parent)
    : QFrame(parent)
    , m_hostWidget(parent)
{
    Q_ASSERT(parent != nullptr);

    setObjectName(QStringLiteral("OnScreenKeyboardOverlay"));
    setAutoFillBackground(true);
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::NoFocus);
    setStyleSheet(QStringLiteral(
        "QFrame#OnScreenKeyboardOverlay { background-color: #202832; "
        "border-top: 1px solid #46515c; }"
        "QWidget#OnScreenKeyboardHeader { background-color: #2c2c33; }"
        "QPushButton { color: white; background-color: #020a10; border: none; "
        "font-size: 25px; min-height: 50px; }"
        "QPushButton:pressed { background-color: #1f6feb; }"
        "QPushButton#FunctionKey { background-color: #56626a; font-size: 18px; "
        "font-weight: bold; }"
        "QPushButton#FunctionKey:pressed { background-color: #1f6feb; }"
        "QPushButton#KeyboardCloseButton { background-color: #56626a; "
        "font-size: 18px; font-weight: bold; min-height: 30px; max-height: 30px; "
        "padding: 0 14px; }"
        "QPushButton#KeyboardCloseButton:pressed { background-color: #1f6feb; }"));

    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("OnScreenKeyboardHeader"));
    header->setFixedHeight(kHeaderHeight);

    m_closeButton = new QPushButton(tr("Tastatur ausblenden  x"), header);
    m_closeButton->setObjectName(QStringLiteral("KeyboardCloseButton"));
    m_closeButton->setFocusPolicy(Qt::NoFocus);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setFixedHeight(kHeaderHeight - 10);
    connect(m_closeButton, &QPushButton::clicked, this, [this]() {
        m_dismissed = true;
        if (QWidget* fw = QApplication::focusWidget()) {
            fw->clearFocus();
        }
        hideOverlay();
    });

    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 4, 8, 4);
    headerLayout->addStretch(1);
    headerLayout->addWidget(m_closeButton);

    m_keyContainer = new QWidget(this);
    m_keyContainer->setObjectName(QStringLiteral("OnScreenKeyboardKeys"));
    m_keyContainer->setFocusPolicy(Qt::NoFocus);
    m_keyLayout = new QGridLayout(m_keyContainer);
    m_keyLayout->setContentsMargins(18, 10, 18, 10);
    m_keyLayout->setHorizontalSpacing(8);
    m_keyLayout->setVerticalSpacing(8);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header, 0);
    layout->addWidget(m_keyContainer, 1);

    rebuildKeyboard();

    qApp->installEventFilter(this);
    parent->installEventFilter(this);

    hide();
    reposition();
    QTimer::singleShot(0, this, [this]() { syncFromFocus(); });
}

OnScreenKeyboard::~OnScreenKeyboard() = default;

bool OnScreenKeyboard::isReady() const
{
    return true;
}

void OnScreenKeyboard::reposition()
{
    QWidget* p = m_hostWidget ? m_hostWidget.data() : parentWidget();
    if (!p) {
        return;
    }

    if (m_floatingOverlay) {
        QScreen* screen = p->screen();
        if (!screen) {
            screen = QGuiApplication::primaryScreen();
        }
        if (!screen) {
            return;
        }

        const QRect area = screen->availableGeometry();
        const int h = qMin(kKeyboardHeight, area.height() / 2);
        setGeometry(area.x(), area.y() + area.height() - h, area.width(), h);
        return;
    }

    const int h = qMin(kKeyboardHeight, p->height() / 2);
    setGeometry(0, p->height() - h, p->width(), h);
}

void OnScreenKeyboard::setHostWidget(QWidget* host, bool floatingOverlay)
{
    if (!host) {
        return;
    }

    QWidget* oldHost = parentWidget();
    if (oldHost == host && m_floatingOverlay == floatingOverlay) {
        reposition();
        raise();
        return;
    }

    hideOverlay();
    m_lastTextInput.clear();
    m_dismissed = false;

    if (oldHost) {
        oldHost->removeEventFilter(this);
    }

    m_floatingOverlay = floatingOverlay;
    if (m_floatingOverlay) {
        setParent(host, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                           | Qt::WindowDoesNotAcceptFocus);
        setAttribute(Qt::WA_ShowWithoutActivating, true);
    } else {
        setAttribute(Qt::WA_ShowWithoutActivating, false);
        setParent(host, Qt::Widget);
    }

    m_hostWidget = host;
    host->installEventFilter(this);

    reposition();
    raise();
}

bool OnScreenKeyboard::eventFilter(QObject* watched, QEvent* event)
{
    if (!event) {
        return QFrame::eventFilter(watched, event);
    }

    if (watched == m_hostWidget.data()
        && (event->type() == QEvent::Resize || event->type() == QEvent::Move
            || event->type() == QEvent::Show)) {
        reposition();
        if (isVisible()) {
            raise();
        }
    }

    if (event->type() == QEvent::FocusIn) {
        QWidget* widget = qobject_cast<QWidget*>(watched);
        if (focusRequestsTextInput(widget)) {
            rememberTextInput(widget);
            m_dismissed = false;
            showOverlay();
        }
    }

    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::TouchBegin) {
        if (isOwnDescendant(watched)) {
            if (QWidget* target = currentTextTarget()) {
                target->setFocus(Qt::OtherFocusReason);
            }
            m_dismissed = false;
            showOverlay();
            return QFrame::eventFilter(watched, event);
        }

        QWidget* widget = qobject_cast<QWidget*>(watched);
        if (focusRequestsTextInput(widget)) {
            rememberTextInput(widget);
            m_dismissed = false;
            showOverlay();
        } else if (widget && isVisible()) {
            hideOverlay();
            m_dismissed = false;
        }
    }

    return QFrame::eventFilter(watched, event);
}

void OnScreenKeyboard::showOverlay()
{
    if (m_dismissed) {
        return;
    }
    reposition();
    if (!isVisible()) {
        show();
    }
    raise();
}

void OnScreenKeyboard::hideOverlay()
{
    if (isVisible()) {
        hide();
    }
}

void OnScreenKeyboard::syncFromFocus()
{
    QWidget* focusWidget = QApplication::focusWidget();
    if (focusRequestsTextInput(focusWidget)) {
        rememberTextInput(focusWidget);
        m_dismissed = false;
        showOverlay();
    }
}

void OnScreenKeyboard::rememberTextInput(QWidget* widget)
{
    if (!widget) {
        return;
    }

    m_lastTextInput = widget;
    const KeyboardMode nextMode = shouldUseNumericMode(widget)
        ? KeyboardMode::Numbers
        : KeyboardMode::Letters;
    if (m_mode != nextMode) {
        m_mode = nextMode;
        m_shift = false;
        rebuildKeyboard();
    }
}

void OnScreenKeyboard::rebuildKeyboard()
{
    while (QLayoutItem* item = m_keyLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }

    for (int col = 0; col < 12; ++col) {
        m_keyLayout->setColumnStretch(col, 1);
    }

    QVector<QVector<KeySpec>> rows;
    if (m_mode == KeyboardMode::Numbers) {
        rows = {
            {textKey(QStringLiteral("1"), QString(), 2), textKey(QStringLiteral("2"), QString(), 2),
             textKey(QStringLiteral("3"), QString(), 2),
             actionKey(QStringLiteral("<-"), QStringLiteral("backspace"), 2)},
            {textKey(QStringLiteral("4"), QString(), 2), textKey(QStringLiteral("5"), QString(), 2),
             textKey(QStringLiteral("6"), QString(), 2),
             actionKey(QStringLiteral("Clear"), QStringLiteral("clear"), 2)},
            {textKey(QStringLiteral("7"), QString(), 2), textKey(QStringLiteral("8"), QString(), 2),
             textKey(QStringLiteral("9"), QString(), 2),
             actionKey(QStringLiteral("Enter"), QStringLiteral("enter"), 2)},
            {actionKey(QStringLiteral("ABC"), QStringLiteral("letters"), 2),
             textKey(QStringLiteral("0"), QString(), 2), textKey(QStringLiteral("-"), QString(), 2),
             textKey(QStringLiteral("."), QString(), 2)}
        };
    } else {
        const QStringList row1 = {QStringLiteral("q"), QStringLiteral("w"), QStringLiteral("e"),
                                  QStringLiteral("r"), QStringLiteral("t"), QStringLiteral("z"),
                                  QStringLiteral("u"), QStringLiteral("i"), QStringLiteral("o"),
                                  QStringLiteral("p")};
        const QStringList row2 = {QStringLiteral("a"), QStringLiteral("s"), QStringLiteral("d"),
                                  QStringLiteral("f"), QStringLiteral("g"), QStringLiteral("h"),
                                  QStringLiteral("j"), QStringLiteral("k"), QStringLiteral("l"),
                                  QStringLiteral("!")};
        QVector<KeySpec> r1;
        QVector<KeySpec> r2;
        for (const QString& key : row1) {
            r1.append(textKey(m_shift ? key.toUpper() : key, key));
        }
        for (const QString& key : row2) {
            r2.append(textKey(m_shift && key.at(0).isLetter() ? key.toUpper() : key, key));
        }

        rows = {
            r1,
            r2,
            {actionKey(QStringLiteral("Shift"), QStringLiteral("shift"), 2),
             textKey(m_shift ? QStringLiteral("Y") : QStringLiteral("y"), QStringLiteral("y")),
             textKey(m_shift ? QStringLiteral("X") : QStringLiteral("x"), QStringLiteral("x")),
             textKey(m_shift ? QStringLiteral("C") : QStringLiteral("c"), QStringLiteral("c")),
             textKey(m_shift ? QStringLiteral("V") : QStringLiteral("v"), QStringLiteral("v")),
             textKey(m_shift ? QStringLiteral("B") : QStringLiteral("b"), QStringLiteral("b")),
             textKey(m_shift ? QStringLiteral("N") : QStringLiteral("n"), QStringLiteral("n")),
             textKey(m_shift ? QStringLiteral("M") : QStringLiteral("m"), QStringLiteral("m")),
             actionKey(QStringLiteral("<-"), QStringLiteral("backspace"), 2)},
            {actionKey(QStringLiteral("123"), QStringLiteral("numbers"), 2),
             actionKey(QStringLiteral("Space"), QStringLiteral("space"), 5),
             textKey(QStringLiteral("-")), textKey(QStringLiteral("_")), textKey(QStringLiteral("#")),
             actionKey(QStringLiteral("Enter"), QStringLiteral("enter"), 2)}
        };
    }

    for (int row = 0; row < rows.size(); ++row) {
        int totalSpan = 0;
        for (const KeySpec& spec : rows.at(row)) {
            totalSpan += qMax(1, spec.span);
        }

        int col = qMax(0, (12 - totalSpan) / 2);
        for (const KeySpec& spec : rows.at(row)) {
            auto* button = new QPushButton(spec.label, m_keyContainer);
            button->setFocusPolicy(Qt::NoFocus);
            button->setCursor(Qt::PointingHandCursor);
            if (spec.function) {
                button->setObjectName(QStringLiteral("FunctionKey"));
            }
            connect(button, &QPushButton::clicked, this, [this, spec]() {
                handleKey(spec.action, spec.text);
            });

            const int span = qMax(1, spec.span);
            m_keyLayout->addWidget(button, row, col, 1, span);
            col += span;
        }
    }
}

void OnScreenKeyboard::handleKey(const QString& action, const QString& text)
{
    if (QWidget* target = currentTextTarget()) {
        target->setFocus(Qt::OtherFocusReason);
    }

    if (action == QStringLiteral("text")) {
        QString value = text;
        if (m_mode == KeyboardMode::Letters && m_shift && value.size() == 1 && value.at(0).isLetter()) {
            value = value.toUpper();
        }
        insertText(value);
    } else if (action == QStringLiteral("space")) {
        insertText(QStringLiteral(" "));
    } else if (action == QStringLiteral("backspace")) {
        backspace();
    } else if (action == QStringLiteral("clear")) {
        clearTarget();
    } else if (action == QStringLiteral("enter")) {
        pressEnter();
    } else if (action == QStringLiteral("shift")) {
        m_shift = !m_shift;
        rebuildKeyboard();
    } else if (action == QStringLiteral("letters")) {
        m_mode = KeyboardMode::Letters;
        m_shift = false;
        rebuildKeyboard();
    } else if (action == QStringLiteral("numbers")) {
        m_mode = KeyboardMode::Numbers;
        m_shift = false;
        rebuildKeyboard();
    }
}

void OnScreenKeyboard::insertText(const QString& text)
{
    QWidget* target = currentTextTarget();
    if (!target || text.isEmpty()) {
        return;
    }

    if (auto* lineEdit = qobject_cast<QLineEdit*>(target)) {
        lineEdit->insert(text);
        updateCompleter();
        return;
    }
    if (auto* textEdit = qobject_cast<QTextEdit*>(target)) {
        textEdit->insertPlainText(text);
        return;
    }
    if (auto* plainTextEdit = qobject_cast<QPlainTextEdit*>(target)) {
        plainTextEdit->insertPlainText(text);
        return;
    }

    sendKey(0, text);
}

void OnScreenKeyboard::backspace()
{
    QWidget* target = currentTextTarget();
    if (!target) {
        return;
    }

    if (auto* lineEdit = qobject_cast<QLineEdit*>(target)) {
        lineEdit->backspace();
        updateCompleter();
        return;
    }
    sendKey(Qt::Key_Backspace);
}

void OnScreenKeyboard::clearTarget()
{
    QWidget* target = currentTextTarget();
    if (!target) {
        return;
    }

    if (auto* lineEdit = qobject_cast<QLineEdit*>(target)) {
        lineEdit->clear();
        updateCompleter();
    } else if (auto* textEdit = qobject_cast<QTextEdit*>(target)) {
        textEdit->clear();
    } else if (auto* plainTextEdit = qobject_cast<QPlainTextEdit*>(target)) {
        plainTextEdit->clear();
    }
}

void OnScreenKeyboard::pressEnter()
{
    auto* lineEdit = qobject_cast<QLineEdit*>(currentTextTarget());
    const bool passwordInput = lineEdit && lineEdit->echoMode() == QLineEdit::Password;

    sendKey(Qt::Key_Return);
    QTimer::singleShot(0, this, [this, passwordInput]() {
        if (passwordInput && focusRequestsTextInput(QApplication::focusWidget())) {
            m_dismissed = false;
            showOverlay();
            return;
        }

        m_dismissed = true;
        hideOverlay();
    });
}

void OnScreenKeyboard::sendKey(int key, const QString& text)
{
    QWidget* target = currentTextTarget();
    if (!target) {
        return;
    }

    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier, text);
    QCoreApplication::sendEvent(target, &press);
    QKeyEvent release(QEvent::KeyRelease, key, Qt::NoModifier, text);
    QCoreApplication::sendEvent(target, &release);
}

void OnScreenKeyboard::updateCompleter()
{
    auto* lineEdit = qobject_cast<QLineEdit*>(currentTextTarget());
    if (!lineEdit || !lineEdit->completer()) {
        return;
    }

    QCompleter* completer = lineEdit->completer();
    const QString prefix = lineEdit->text().trimmed();
    if (prefix.isEmpty()) {
        if (auto* popup = completer->popup()) {
            popup->hide();
        }
        return;
    }

    completer->setCompletionPrefix(prefix);
    if (completer->completionCount() > 0) {
        completer->complete();
    } else if (auto* popup = completer->popup()) {
        popup->hide();
    }
}

QWidget* OnScreenKeyboard::currentTextTarget() const
{
    if (m_lastTextInput && focusRequestsTextInput(m_lastTextInput.data())) {
        return m_lastTextInput.data();
    }

    QWidget* focused = QApplication::focusWidget();
    return focusRequestsTextInput(focused) ? focused : nullptr;
}

bool OnScreenKeyboard::currentFocusIsKeyboard() const
{
    return isOwnDescendant(qApp->focusObject()) || isOwnDescendant(QApplication::focusWidget());
}

bool OnScreenKeyboard::focusRequestsTextInput(QWidget* widget)
{
    if (!widget) {
        return false;
    }
    return qobject_cast<QLineEdit*>(widget) != nullptr
        || qobject_cast<QTextEdit*>(widget) != nullptr
        || qobject_cast<QPlainTextEdit*>(widget) != nullptr
        || qobject_cast<QAbstractSpinBox*>(widget) != nullptr
        || hasSpinBoxAncestor(widget);
}

bool OnScreenKeyboard::shouldUseNumericMode(QWidget* widget)
{
    if (!widget) {
        return false;
    }

    if (widget->objectName() == QStringLiteral("EingabePallettenplan")
        || qobject_cast<QAbstractSpinBox*>(widget) != nullptr
        || hasSpinBoxAncestor(widget)) {
        return true;
    }

    if (auto* lineEdit = qobject_cast<QLineEdit*>(widget)) {
        const Qt::InputMethodHints hints = lineEdit->inputMethodHints();
        return lineEdit->echoMode() == QLineEdit::Password
            || hints.testFlag(Qt::ImhDigitsOnly)
            || hints.testFlag(Qt::ImhFormattedNumbersOnly)
            || hints.testFlag(Qt::ImhPreferNumbers);
    }

    return false;
}

bool OnScreenKeyboard::hasSpinBoxAncestor(QWidget* widget)
{
    for (QWidget* p = widget ? widget->parentWidget() : nullptr; p != nullptr; p = p->parentWidget()) {
        if (qobject_cast<QAbstractSpinBox*>(p) != nullptr) {
            return true;
        }
    }
    return false;
}

bool OnScreenKeyboard::isOwnDescendant(QObject* object) const
{
    for (QObject* o = object; o != nullptr; o = o->parent()) {
        if (o == this) {
            return true;
        }
    }
    return false;
}

} // namespace ui
} // namespace multipack
