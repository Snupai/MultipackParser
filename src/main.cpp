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
#include <QEvent>
#include <QGuiApplication>
#include <QInputMethod>
#include <QLineEdit>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QAbstractSpinBox>
#include <cstring>

#include "multipack/core/Application.h"
#include "multipack/core/GlobalState.h"
#include "multipack/config/ConfigDefaults.h"

class VirtualKeyboardFocusFilter final : public QObject
{
public:
    explicit VirtualKeyboardFocusFilter(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event && event->type() == QEvent::FocusIn) {
            QWidget* widget = qobject_cast<QWidget*>(watched);
            if (!widget) {
                return QObject::eventFilter(watched, event);
            }

            const bool isTextInput =
                qobject_cast<QLineEdit*>(widget) != nullptr ||
                qobject_cast<QTextEdit*>(widget) != nullptr ||
                qobject_cast<QPlainTextEdit*>(widget) != nullptr ||
                qobject_cast<QAbstractSpinBox*>(widget) != nullptr;

            if (isTextInput) {
                widget->setAttribute(Qt::WA_InputMethodEnabled, true);
                if (QInputMethod* inputMethod = QGuiApplication::inputMethod()) {
                    inputMethod->show();
                }
            }
        }

        return QObject::eventFilter(watched, event);
    }
};

/**
 * @brief Set environment variables for platform compatibility
 */
void setupEnvironment()
{
#if defined(Q_OS_LINUX)
    // Force software rendering for Raspberry Pi
    qputenv("QT_X11_NO_MITSHM", "1");
    qputenv("LIBGL_ALWAYS_SOFTWARE", "1");
    qputenv("QT_OPENGL", "software");
    qputenv("QT_QUICK_BACKEND", "software");
    qputenv("QSG_RHI_BACKEND", "software");
    qputenv("QT_XCB_GL_INTEGRATION", "none");
    qputenv("QT_QPA_PLATFORM", "xcb");
    qputenv("QT_VIRTUALKEYBOARD_DESKTOP_DISABLE", "0");
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
        QLoggingCategory::setFilterRules("*.debug=true\nqt.*.debug=false");
        qInfo() << "Verbose logging enabled";
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
    // Setup environment before creating QApplication
    setupEnvironment();

    if (shouldEnableVirtualKeyboard(argc, argv)) {
        qputenv("QT_IM_MODULE", "qtvirtualkeyboard");
    }

    // Create application
    multipack::core::Application app(argc, argv);

    // For QWidget apps on touch devices, explicitly show VK on focus.
    if (qEnvironmentVariable("QT_IM_MODULE") == "qtvirtualkeyboard") {
        app.installEventFilter(new VirtualKeyboardFocusFilter(&app));
    }

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
