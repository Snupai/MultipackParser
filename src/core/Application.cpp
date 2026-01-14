/**
 * @file Application.cpp
 * @brief Implementation of main application class
 */

#include "multipack/core/Application.h"
#include "multipack/core/AppInitializer.h"
#include "multipack/core/GlobalState.h"
#include "multipack/ui/MainWindow.h"
#include "multipack/config/SettingsManager.h"
#include "multipack/database/DatabaseManager.h"
#include "multipack/robot/RobotController.h"

#include <QDebug>

namespace multipack {
namespace core {

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    qDebug() << "Application - constructor";

    // Set application info
    setApplicationName("MultipackParser");
    setApplicationVersion("1.0.0");
    setOrganizationName("Szaidel Cosmetic GmbH");
    setOrganizationDomain("szaidel.com");
}

Application::~Application()
{
    qDebug() << "Application - destructor";
}

bool Application::initialize()
{
    qDebug() << "Application - initializing";

    // Create and run initializer
    m_initializer = std::make_unique<AppInitializer>();

    if (!m_initializer->initialize()) {
        qCritical() << "Application - initialization failed";
        return false;
    }

    // Create main window
    qDebug() << "Application - creating main window";
    m_mainWindow = std::make_unique<ui::MainWindow>();

    // Wire up components to main window
    if (m_initializer->settingsManager()) {
        m_mainWindow->setSettingsManager(m_initializer->settingsManager());
    }

    if (m_initializer->databaseManager()) {
        m_mainWindow->setDatabaseManager(m_initializer->databaseManager());
    }

    if (m_initializer->robotController()) {
        m_mainWindow->setRobotController(m_initializer->robotController());

        // Connect robot signals to main window
        QObject::connect(m_initializer->robotController(), &robot::RobotController::connected,
            m_mainWindow.get(), &ui::MainWindow::updateRobotStatus);
        QObject::connect(m_initializer->robotController(), &robot::RobotController::disconnected,
            m_mainWindow.get(), &ui::MainWindow::updateRobotStatus);
    }

    // Connect to global state
    m_mainWindow->setGlobalState(&GlobalState::instance());

    qDebug() << "Application - initialized successfully";
    return true;
}

int Application::run()
{
    qDebug() << "Application - starting";

    if (!m_mainWindow) {
        qCritical() << "Application - no main window";
        return -1;
    }

    // Show the main window
    m_mainWindow->show();

    qDebug() << "Application - entering event loop";
    return exec();
}

} // namespace core
} // namespace multipack
