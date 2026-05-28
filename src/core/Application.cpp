/**
 * @file Application.cpp
 * @brief Implementation of main application class
 */

#include "multipack/core/Application.h"
#include "multipack/core/AppInitializer.h"
#include "multipack/core/GlobalState.h"
#include "multipack/ui/MainWindow.h"
#include "multipack/ui/SplashScreen.h"
#include "multipack/config/ConfigDefaults.h"
#include "multipack/config/SettingsManager.h"
#include "multipack/database/DatabaseManager.h"
#include "multipack/network/XmlRpcServer.h"
#include "multipack/robot/RobotController.h"

#include <QDebug>
#include <QCoreApplication>
#include <QSessionManager>
#include <QCloseEvent>

namespace multipack {
namespace core {

Application::Application(int& argc, char** argv)
    : QApplication(argc, argv)
{
    qDebug() << "Application - constructor";

    // Set application info
    setApplicationName(config::Defaults::APP_NAME);
    setApplicationVersion(config::Defaults::VERSION);
    setOrganizationName("Szaidel Cosmetic GmbH");
    setOrganizationDomain("szaidel.com");
}

Application::~Application()
{
    qDebug() << "Application - destructor";

    // Ensure clean shutdown
    cleanup();
}

void Application::cleanup()
{
    qDebug() << "Application - cleanup";

    // First close the main window to stop all UI interactions
    if (m_mainWindow) {
        m_mainWindow->close();
        m_mainWindow.reset();
    }

    // Then shutdown all subsystems
    if (m_initializer) {
        m_initializer->shutdown();
        m_initializer.reset();
    }

    qDebug() << "Application - cleanup complete";
}

bool Application::initialize()
{
    qDebug() << "Application - initializing";

    // Create and run initializer
    m_initializer = std::make_unique<AppInitializer>();

    ui::InstantSplashScreen instantSplash;
    instantSplash.show();
    processEvents();

    ui::SplashScreen splash;
    splash.show();
    processEvents();
    instantSplash.finish(&splash);

    auto progressCallback = [&splash](int percent, const QString& message) {
        splash.updateProgress(percent, message);
        QCoreApplication::processEvents();
    };

    if (!m_initializer->initialize(progressCallback)) {
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

    if (m_initializer->usbMonitor()) {
        m_mainWindow->setUsbMonitor(m_initializer->usbMonitor());
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
        QObject::connect(m_initializer->robotController(), &robot::RobotController::connected,
            &GlobalState::instance(), []() { GlobalState::instance().setRobotConnected(true); });
        QObject::connect(m_initializer->robotController(), &robot::RobotController::disconnected,
            &GlobalState::instance(), []() { GlobalState::instance().setRobotConnected(false); });
    }

    // Connect to global state
    m_mainWindow->setGlobalState(&GlobalState::instance());

    if (m_initializer->xmlRpcServer()) {
        m_mainWindow->setXmlRpcServer(m_initializer->xmlRpcServer());
    }

    if (m_initializer->audioManager()) {
        m_mainWindow->setAudioManager(m_initializer->audioManager());
    }

    if (m_initializer->autoUpdater()) {
        m_mainWindow->setAutoUpdater(m_initializer->autoUpdater());
    }

    if (m_initializer->xmlRpcServer()) {
        auto* server = m_initializer->xmlRpcServer();
        QObject::connect(m_mainWindow.get(), &ui::MainWindow::serverStartRequested,
            server, [this, server]() {
                if (server->isRunning()) {
                    return;
                }
                const int port = m_initializer && m_initializer->settingsManager()
                    ? m_initializer->settingsManager()->xmlRpcPort()
                    : network::XmlRpcServer::DEFAULT_PORT;
                const bool started = server->start(port);
                Q_UNUSED(started);
            });
        QObject::connect(m_mainWindow.get(), &ui::MainWindow::serverStopRequested,
            server, &network::XmlRpcServer::stop);
        QObject::connect(server, &network::XmlRpcServer::started,
            m_mainWindow.get(), [this]() { m_mainWindow->setServerRunning(true); });
        QObject::connect(server, &network::XmlRpcServer::stopped,
            m_mainWindow.get(), [this]() { m_mainWindow->setServerRunning(false); });
        QObject::connect(server, &network::XmlRpcServer::error,
            m_mainWindow.get(), [this](const QString& error) { m_mainWindow->setServerRunning(false, error); });

        if (server->isRunning()) {
            m_mainWindow->setServerRunning(true);
        }
    }

    splash.finish(m_mainWindow.get());

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

    // Kiosk mode can be controlled via environment:
    // MULTIPACK_FULLSCREEN=1 (default) -> frameless maximized (kiosk without WM title bar)
    // MULTIPACK_FULLSCREEN=0 -> regular windowed mode
    const bool fullscreenEnabled = qEnvironmentVariable("MULTIPACK_FULLSCREEN", "1") != "0";
    if (fullscreenEnabled) {
        m_mainWindow->setWindowFlag(Qt::FramelessWindowHint, true);
        m_mainWindow->showMaximized();
    } else {
        m_mainWindow->show();
    }

    qDebug() << "Application - entering event loop";
    return exec();
}

bool Application::event(QEvent* event)
{
    // Handle close events gracefully
    if (event->type() == QEvent::Close) {
        qDebug() << "Application - close event received";
        cleanup();
        event->accept();
        return true;
    }

    return QApplication::event(event);
}

void Application::commitData(QSessionManager& manager)
{
    // Save any pending session data
    Q_UNUSED(manager);

    qDebug() << "Application - committing session data";

    // Save settings if available
    if (m_initializer && m_initializer->settingsManager()) {
        if (!m_initializer->settingsManager()->save()) {
            qWarning() << "Failed to save settings during session commit";
        }
    }
}

} // namespace core
} // namespace multipack
