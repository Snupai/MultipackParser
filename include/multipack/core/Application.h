/**
 * @file Application.h
 * @brief Main application class
 */

#ifndef MULTIPACK_CORE_APPLICATION_H
#define MULTIPACK_CORE_APPLICATION_H

#include <QApplication>
#include <memory>

namespace multipack {

namespace ui { class MainWindow; }

namespace core {

class AppInitializer;

/**
 * @class Application
 * @brief Main application class extending QApplication
 */
class Application : public QApplication
{
    Q_OBJECT

public:
    Application(int& argc, char** argv);
    ~Application() override;

    bool initialize();
    int run();

    // Accessor for main window
    ui::MainWindow* mainWindow() const { return m_mainWindow.get(); }
    AppInitializer* initializer() const { return m_initializer.get(); }

private:
    std::unique_ptr<AppInitializer> m_initializer;
    std::unique_ptr<ui::MainWindow> m_mainWindow;
};

} // namespace core
} // namespace multipack

#endif // MULTIPACK_CORE_APPLICATION_H
