/**
 * @file DisplayScale.h
 * @brief Fits the 1280x720 design to the panel the HMI runs on
 */
#ifndef MULTIPACK_UI_DISPLAYSCALE_H
#define MULTIPACK_UI_DISPLAYSCALE_H

#include <QSize>

namespace multipack {
namespace ui {

/**
 * @brief One scale factor for the whole UI
 *
 * The HMI is designed for a 1280x720 panel. Sizes in the .ui file, the
 * stylesheet and the code are pixels of that design. On another panel Qt
 * scales them (QT_SCALE_FACTOR): 1920x1080 shows the same layout at 1.5.
 */
namespace DisplayScale {

constexpr int DESIGN_WIDTH = 1280;
constexpr int DESIGN_HEIGHT = 720;

/**
 * @brief Scale factor at which the design fills a screen
 * @param screenSize Screen size in pixels
 * @return Factor in steps of 0.05, at least 1.0; 1.0 for an unknown size
 *
 * The design has to fit in both directions: a screen that is not 16:9 gets
 * the smaller factor and the layouts take up the rest (see windowSizeFor()).
 * A screen smaller than the design is not scaled down and shows a part of it.
 */
[[nodiscard]] double factorFor(const QSize& screenSize);

/**
 * @brief Size of the kiosk window on a screen
 * @param scaledScreenSize Screen size in scaled (design) pixels
 * @return The screen size, never smaller than the design
 */
[[nodiscard]] QSize windowSizeFor(const QSize& scaledScreenSize);

/**
 * @brief Set the scale factor for the primary screen
 *
 * Call before the application object is created. Leaves a QT_SCALE_FACTOR
 * set by the launcher alone.
 */
void applyForPrimaryScreen(int argc, char** argv);

} // namespace DisplayScale

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_DISPLAYSCALE_H
