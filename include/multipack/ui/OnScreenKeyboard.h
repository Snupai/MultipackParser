/**
 * @file OnScreenKeyboard.h
 * @brief Shared QWidget on-screen keyboard for all touchscreen text input.
 */
#ifndef MULTIPACK_UI_ONSCREENKEYBOARD_H
#define MULTIPACK_UI_ONSCREENKEYBOARD_H

#include <QFrame>
#include <QPointer>

QT_BEGIN_NAMESPACE
class QGridLayout;
class QPushButton;
class QWidget;
QT_END_NAMESPACE

namespace multipack {
namespace ui {

class OnScreenKeyboard : public QFrame
{
    Q_OBJECT

public:
    explicit OnScreenKeyboard(QWidget* parent);
    ~OnScreenKeyboard() override;

    bool isReady() const;
    void reposition();
    void setHostWidget(QWidget* host, bool floatingOverlay = false);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    enum class KeyboardMode {
        Letters,
        Numbers
    };

    void showOverlay();
    void hideOverlay();
    void syncFromFocus();
    void rememberTextInput(QWidget* widget);
    void rebuildKeyboard();
    void handleKey(const QString& action, const QString& text = QString());
    void insertText(const QString& text);
    void backspace();
    void clearTarget();
    void pressEnter();
    void sendKey(int key, const QString& text = QString());
    void updateCompleter();
    QWidget* currentTextTarget() const;
    bool currentFocusIsKeyboard() const;
    static bool focusRequestsTextInput(QWidget* widget);
    static bool shouldUseNumericMode(QWidget* widget);
    static bool hasSpinBoxAncestor(QWidget* widget);
    bool isOwnDescendant(QObject* object) const;

    QPointer<QWidget> m_hostWidget;
    QWidget* m_keyContainer = nullptr;
    QGridLayout* m_keyLayout = nullptr;
    QPushButton* m_closeButton = nullptr;
    QPointer<QWidget> m_lastTextInput;
    KeyboardMode m_mode = KeyboardMode::Letters;
    bool m_shift = false;
    bool m_floatingOverlay = false;
    bool m_dismissed = false;
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_ONSCREENKEYBOARD_H
