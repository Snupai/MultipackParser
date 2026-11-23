/**
 * @file Theme.h
 * @brief Shared look of the HMI: colours, stylesheet, small widget helpers
 *
 * The stylesheet lives in resources/styles/hmi.qss. Colours used from code
 * (state texts, list highlights) are defined here so both stay in sync.
 */
#ifndef MULTIPACK_UI_THEME_H
#define MULTIPACK_UI_THEME_H

#include <QString>

class QApplication;
class QLabel;
class QWidget;

namespace multipack {
namespace ui {
namespace Theme {

// Text
constexpr const char* TEXT = "#18212f";
constexpr const char* TEXT_MUTED = "#5b6676";
constexpr const char* TEXT_DISABLED = "#9aa3af";

// Surfaces
constexpr const char* BACKGROUND = "#eef1f5";
constexpr const char* SURFACE = "#ffffff";
constexpr const char* BORDER = "#d3d9e2";

// Brand and states
constexpr const char* PRIMARY = "#1d2f5e";
constexpr const char* INFO = "#1f5fbf";
constexpr const char* SUCCESS = "#1e7d32";
constexpr const char* WARNING = "#b85c00";
constexpr const char* DANGER = "#b3261e";
constexpr const char* HIGHLIGHT_ROW = "#ffe9c7";

/// Semantic state of a status text.
enum class Tone {
    Neutral,
    Info,
    Success,
    Warning,
    Danger
};

/// Colour (hex) for a status text of the given tone.
[[nodiscard]] QString toneColor(Tone tone);

/// The HMI stylesheet (resources/styles/hmi.qss); empty when the resource is missing.
[[nodiscard]] QString styleSheet();

/// Application-wide look: Fusion style and the HMI palette. Call once at startup.
void applyApplicationLook(QApplication& app);

/// Apply the HMI stylesheet to a top-level window; dialogs parented to it inherit it.
void applyTo(QWidget* window);

/// Pick a stylesheet variant, e.g. setRole(button, "primary"). Repolishes the widget.
void setRole(QWidget* widget, const char* role);

/// Pick a size variant, e.g. setScale(button, "medium"). Repolishes the widget.
void setScale(QWidget* widget, const char* scale);

/// Mark a container as a card or as a dense admin area. Repolishes the widget.
void setCard(QWidget* widget);
void setCompact(QWidget* widget);

/// Colour a status label by tone without touching its font.
void setTone(QLabel* label, Tone tone);

/**
 * @brief Yes/no confirmation with German, touch-sized buttons
 * @return true when the operator chose "Ja"; "Nein" is the default
 */
[[nodiscard]] bool confirm(QWidget* parent, const QString& title, const QString& text);

} // namespace Theme
} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_THEME_H
