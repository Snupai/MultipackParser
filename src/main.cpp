/**
 * @file main.cpp
 * @brief Application entry point for MultipackParser C++ version
 *
 * Entry point that:
 * - Parses command line arguments
 * - Sets up environment for Raspberry Pi
 * - Initializes the application
 * - Shows splash screen with progress
 * - Starts the main event loop
 */

#include <QApplication>
#include <QDebug>
#include <QLoggingCategory>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QLocale>
#include <QtGlobal>
#include <cstring>

#include "multipack/core/Application.h"
#include "multipack/core/GlobalState.h"
#include "multipack/config/ConfigDefaults.h"

/**
 * @brief Set environment variables for platform compatibility
 */
void setupEnvironment()
{
#if defined(Q_OS_LINUX)
    // Use safe defaults for Raspberry Pi, but allow runtime scripts to override.
    if (qEnvironmentVariableIsEmpty("QT_X11_NO_MITSHM")) {
        qputenv("QT_X11_NO_MITSHM", "1");
    }
    if (qEnvironmentVariableIsEmpty("LIBGL_ALWAYS_SOFTWARE")) {
        qputenv("LIBGL_ALWAYS_SOFTWARE", "1");
    }
    if (qEnvironmentVariableIsEmpty("QT_OPENGL")) {
        qputenv("QT_OPENGL", "software");
    }
    if (qEnvironmentVariableIsEmpty("QT_QUICK_BACKEND")) {
        qputenv("QT_QUICK_BACKEND", "software");
    }
    if (qEnvironmentVariableIsEmpty("QSG_RHI_BACKEND")) {
        qputenv("QSG_RHI_BACKEND", "software");
    }
    if (qEnvironmentVariableIsEmpty("QT_XCB_GL_INTEGRATION")) {
        qputenv("QT_XCB_GL_INTEGRATION", "none");
    }
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "xcb");
    }
    qunsetenv("QT_IM_MODULE");
#elif defined(Q_OS_MACOS)
    // macOS uses cocoa platform (default)
    // No special environment setup needed
#elif defined(Q_OS_WIN)
    // Windows uses windows platform (default)
    // No special environment setup needed
#endif
}

bool shouldEnableVirtualKeyboard(int argc, char* argv[])
{
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--no-virtual-keyboard") == 0) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Parse command line arguments
 * @param app QCoreApplication reference
 * @return true if application should continue, false to exit
 */
bool parseArguments(QCoreApplication& app)
{
    QCommandLineParser parser;
    parser.setApplicationDescription("MultipackParser - Robot Palette System");
    parser.addHelpOption();

    // Custom options
    QCommandLineOption versionOption(
        QStringList() << "V" << "version",
        "Display version information and exit."
    );
    parser.addOption(versionOption);

    QCommandLineOption licenseOption(
        QStringList() << "license",
        "Display license information and exit."
    );
    parser.addOption(licenseOption);

    QCommandLineOption verboseOption(
        QStringList() << "v" << "verbose",
        "Enable verbose/debug logging."
    );
    parser.addOption(verboseOption);

    QCommandLineOption noKeyboardOption(
        QStringList() << "no-virtual-keyboard",
        "Disable virtual keyboard."
    );
    parser.addOption(noKeyboardOption);

    parser.process(app);

    // Handle version
    if (parser.isSet(versionOption)) {
        qInfo() << "MultipackParser version" << multipack::config::Defaults::VERSION;
        return false;
    }

    // Handle license
    if (parser.isSet(licenseOption)) {
        qInfo() << "MultipackParser - Proprietary Software";
        qInfo() << "Copyright (c) 2024 Szaidel Cosmetic GmbH";
        qInfo() << "All rights reserved.";
        return false;
    }

    // Handle verbose
    if (parser.isSet(verboseOption)) {
        // Enable debug logging
        qputenv("MULTIPACK_VERBOSE", "1");
        QLoggingCategory::setFilterRules("*.debug=true\nqt.*.debug=false");
        qInfo() << "Verbose logging enabled";
    } else {
        qputenv("MULTIPACK_VERBOSE", "0");
    }

    return true;
}

/**
 * @brief Main entry point
 * @param argc Argument count
 * @param argv Argument values
 * @return Exit code
 */
int main(int argc, char* argv[])
{
    Q_INIT_RESOURCE(multipack);
    Q_INIT_RESOURCE(MainWindowResources);

    // Setup environment before creating QApplication
    setupEnvironment();

    const bool vkEnabled = shouldEnableVirtualKeyboard(argc, argv);
    qputenv("MULTIPACK_VIRTUAL_KEYBOARD", vkEnabled ? "1" : "0");
    qunsetenv("QT_IM_MODULE");

    // HMI is operated in Germany. Keep the process locale stable for number
    // formatting and any locale-aware Qt widgets.
    if (qEnvironmentVariableIsEmpty("LANG") || qEnvironmentVariable("LANG") == "C"
        || qEnvironmentVariable("LANG") == "C.UTF-8" || qEnvironmentVariable("LANG") == "POSIX") {
        qputenv("LANG", "de_DE.UTF-8");
    }
    if (qEnvironmentVariable("LC_ALL") == "C" || qEnvironmentVariable("LC_ALL") == "C.UTF-8"
        || qEnvironmentVariable("LC_ALL") == "POSIX") {
        qunsetenv("LC_ALL");
    }
    QLocale::setDefault(QLocale(QLocale::German, QLocale::Germany));

    // Create application
    multipack::core::Application app(argc, argv);

    // Set application metadata
    app.setApplicationName(multipack::config::Defaults::APP_NAME);
    app.setApplicationVersion(multipack::config::Defaults::VERSION);
    app.setOrganizationName(multipack::config::Defaults::ORGANIZATION);

    qInfo() << "===========================================";
    qInfo() << "MultipackParser C++ Version" << multipack::config::Defaults::VERSION;
    qInfo() << "===========================================";

    // Parse command line arguments
    if (!parseArguments(app)) {
        return 0;  // Exit after showing version/license
    }

    // Initialize application
    if (!app.initialize()) {
        qCritical() << "Failed to initialize application";
        return 1;
    }

    // Run application
    return app.run();
}
