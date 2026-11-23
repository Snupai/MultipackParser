/**
 * @file DisplayScale.cpp
 * @brief Fits the 1280x720 design to the panel the HMI runs on
 */
#include "multipack/ui/DisplayScale.h"

#include <QByteArray>
#include <QDebug>
#include <QGuiApplication>
#include <QScreen>
#include <QtGlobal>

#include <cmath>
#include <vector>

namespace multipack {
namespace ui {
namespace DisplayScale {

namespace {

constexpr double STEP = 0.05;
constexpr double MAX_FACTOR = 4.0;

QSize primaryScreenSize(int argc, char** argv)
{
    // Qt reads the scale factor when the application object is created, so a
    // short-lived one asks for the screen first. It gets its own argument list
    // because Qt removes the arguments it handles.
    std::vector<char*> arguments(argv, argv + argc);
    arguments.push_back(nullptr);
    int count = argc;
    QGuiApplication probe(count, arguments.data());
    const QScreen* screen = QGuiApplication::primaryScreen();
    return screen ? screen->geometry().size() : QSize();
}

} // namespace

double factorFor(const QSize& screenSize)
{
    if (screenSize.isEmpty()) {
        return 1.0;
    }
    const double fit = qMin(screenSize.width() / static_cast<double>(DESIGN_WIDTH),
                            screenSize.height() / static_cast<double>(DESIGN_HEIGHT));
    // Rounded down: the design never gets larger than the screen. Never below
    // 1: Qt 6.2 paints a window wrongly when it has to scale it down.
    const double stepped = std::floor(fit / STEP + 1e-6) * STEP;
    return qBound(1.0, stepped, MAX_FACTOR);
}

QSize windowSizeFor(const QSize& scaledScreenSize)
{
    return scaledScreenSize.expandedTo(QSize(DESIGN_WIDTH, DESIGN_HEIGHT));
}

void applyForPrimaryScreen(int argc, char** argv)
{
    if (!qEnvironmentVariableIsEmpty("QT_SCALE_FACTOR")) {
        return;
    }
    const QSize screenSize = primaryScreenSize(argc, argv);
    const double factor = factorFor(screenSize);
    qInfo() << "DisplayScale - screen" << screenSize << "scale factor" << factor;
    if (!qFuzzyCompare(factor, 1.0)) {
        qputenv("QT_SCALE_FACTOR", QByteArray::number(factor, 'f', 2));
    }
}

} // namespace DisplayScale
} // namespace ui
} // namespace multipack
