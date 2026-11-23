#include "TestHelpers.h"

#include "multipack/config/ConfigDefaults.h"
#include "multipack/config/SettingsManager.h"
#include "multipack/core/GlobalState.h"
#include "multipack/database/DatabaseManager.h"
#include "multipack/network/XmlRpcServer.h"
#include "multipack/ui/MainWindow.h"
#include "multipack/ui/Theme.h"
#include "multipack/ui/VisualizationWidget.h"

#include <QAbstractButton>
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QLibraryInfo>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest/QtTest>

#include <memory>

using multipack::config::SettingsManager;
using multipack::core::GlobalState;
using multipack::database::DatabaseManager;
using multipack::database::SaveResult;
using multipack::network::XmlRpcServer;
using multipack::ui::MainWindow;

class MainWindowIntegrationTest : public QObject
{
    Q_OBJECT

private slots:
    void startAndStopServerFromUi();
    void themeIsAppliedToTheWindow();
    void experimentalButtonIsReplacedByPlansTab();
    void planListFollowsDatabaseChanges();
    void navigationSwitchesPagesAndLocksRobotUntilPlanLoaded();
    void qrCodeTabIsHiddenWithoutPlanner();
    void operatorTextsAvoidPlannerJargon();
    void ur20NoticeBarOffersPaletteRelease();
};

namespace {

/// A main window on a temporary database with one plan ("sample").
struct UiFixture {
    QTemporaryDir dir;
    DatabaseManager database;
    SettingsManager settings;
    std::unique_ptr<MainWindow> window;

    bool start(const QString& urModel = QStringLiteral("UR10"))
    {
        qputenv("MULTIPACK_DISABLE_ROBOT_STATUS_MONITOR", "1");
        GlobalState::instance().clear();
        if (!dir.isValid() || !database.open(dir.filePath("palettes.db"))
            || database.savePaletteData(testhelpers::makeSamplePaletteData()) != SaveResult::Inserted) {
            return false;
        }
        settings.resetToDefaults();
        settings.setValue(multipack::config::Keys::INFO_UR_MODEL, urModel);
        window = std::make_unique<MainWindow>();
        window->setSettingsManager(&settings);
        window->setDatabaseManager(&database);
        window->setGlobalState(&GlobalState::instance());
        window->show();
        return true;
    }

    template <typename T>
    T* find(const char* name) const { return window->findChild<T*>(QString::fromLatin1(name)); }

    QString currentPage() const
    {
        auto* stack = find<QStackedWidget>("stackedWidget");
        return stack && stack->currentWidget() ? stack->currentWidget()->objectName() : QString();
    }

    void loadSamplePlan() const
    {
        find<QLineEdit>("EingabePallettenplan")->setText("sample");
        QTest::mouseClick(find<QPushButton>("LadePallettenplan"), Qt::LeftButton);
    }
};

/// Press @p buttonText in the next modal dialog. The dialog runs a nested
/// event loop (exec()), so the answer has to be armed before it opens.
std::shared_ptr<bool> answerNextModalDialog(const QString& buttonText)
{
    auto answered = std::make_shared<bool>(false);
    auto* timer = new QTimer(qApp);
    timer->setInterval(20);
    QObject::connect(timer, &QTimer::timeout, timer, [timer, buttonText, answered]() {
        QWidget* modal = QApplication::activeModalWidget();
        if (!modal) {
            return;
        }
        for (QAbstractButton* button : modal->findChildren<QAbstractButton*>()) {
            if (button->text() == buttonText) {
                timer->stop();
                timer->deleteLater();
                *answered = true;
                button->click();
                return;
            }
        }
    });
    timer->start();
    return answered;
}

} // namespace

void MainWindowIntegrationTest::startAndStopServerFromUi()
{
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());

    qputenv("MULTIPACK_DISABLE_ROBOT_STATUS_MONITOR", "1");

    GlobalState::instance().clear();

    DatabaseManager database;
    QVERIFY(database.open(tempDir.filePath("palettes.db")));
    QCOMPARE(database.savePaletteData(testhelpers::makeSamplePaletteData()), SaveResult::Inserted);

    SettingsManager settings;
    settings.resetToDefaults();
    settings.setValue(multipack::config::Keys::SERVER_PORT, static_cast<int>(testhelpers::findFreePort()));

    XmlRpcServer server;
    server.setDatabaseManager(&database);
    server.setGlobalState(&GlobalState::instance());
    server.registerStandardMethods();

    MainWindow window;
    window.setSettingsManager(&settings);
    window.setDatabaseManager(&database);
    window.setGlobalState(&GlobalState::instance());
    window.setXmlRpcServer(&server);
    window.show();

    auto* paletteInput = window.findChild<QLineEdit*>("EingabePallettenplan");
    auto* loadButton = window.findChild<QPushButton*>("LadePallettenplan");
    auto* startButton = window.findChild<QPushButton*>("ButtonDatenSenden");
    auto* stopButton = window.findChild<QPushButton*>("ButtonStopRPCServer");

    QVERIFY(paletteInput != nullptr);
    QVERIFY(loadButton != nullptr);
    QVERIFY(startButton != nullptr);
    QVERIFY(stopButton != nullptr);

    paletteInput->setText("sample");
    QTest::mouseClick(loadButton, Qt::LeftButton);
    QTRY_VERIFY(startButton->isEnabled());

    QTest::mouseClick(startButton, Qt::LeftButton);
    QTRY_VERIFY(server.isRunning());

    QTcpSocket socket;
    socket.connectToHost(QHostAddress::LocalHost, static_cast<quint16>(server.port()));
    QVERIFY(socket.waitForConnected(5000));
    socket.disconnectFromHost();

    QTest::mouseClick(stopButton, Qt::LeftButton);
    QTRY_VERIFY(!server.isRunning());

    window.close();
    QTRY_VERIFY(!window.isVisible());
}

void MainWindowIntegrationTest::themeIsAppliedToTheWindow()
{
    UiFixture f;
    QVERIFY(f.start());

    // One shared stylesheet for the window (and the dialogs parented to it).
    const QString sheet = multipack::ui::Theme::styleSheet();
    QVERIFY(!sheet.isEmpty());
    QCOMPARE(f.window->styleSheet(), sheet);

    // Touch targets: the operator controls of the main page are finger-sized.
    for (const char* name : {"LadePallettenplan", "ButtonDatenSenden", "ButtonOpenParameterRoboter",
                             "navBetrieb", "navPlaene", "navRoboter", "ButtonSettings",
                             "pushButtonVolumeOnOff"}) {
        auto* button = f.find<QPushButton>(name);
        QVERIFY2(button, name);
        QVERIFY2(button->height() >= 48, name);
    }
    QVERIFY(f.find<QLineEdit>("EingabePallettenplan")->height() >= 48);
}

void MainWindowIntegrationTest::experimentalButtonIsReplacedByPlansTab()
{
    UiFixture f;
    QVERIFY(f.start());

    // No "Experimental" button anywhere, and no page-local back buttons.
    QVERIFY(!f.window->findChild<QWidget*>(QStringLiteral("openExperimentalTab")));
    for (QAbstractButton* button : f.window->findChildren<QAbstractButton*>()) {
        QVERIFY2(!button->text().contains(QStringLiteral("experimental"), Qt::CaseInsensitive),
                 qPrintable(button->objectName()));
        QVERIFY2(!button->objectName().startsWith(QStringLiteral("ButtonZurueck")),
                 qPrintable(button->objectName()));
    }

    // What the button opened is the "Pläne" tab: list, filters, preview, load.
    auto* plans = f.find<QPushButton>("navPlaene");
    auto* list = f.find<QListWidget>("plannerFinalsList");
    auto* load = f.find<QPushButton>("plannerLoadButton");
    QVERIFY(plans && list && load);
    QVERIFY(plans->isVisible());
    QVERIFY(!list->isVisible());
    QTest::mouseClick(plans, Qt::LeftButton);
    QCOMPARE(f.currentPage(), QStringLiteral("PlanLibrary"));
    QVERIFY(plans->isChecked());
    QVERIFY(list->isVisible());
    QCOMPARE(list->count(), 1);
    QVERIFY(f.find<QLineEdit>("lineEditFilterLength")->isVisible());
    QVERIFY(f.find<QWidget>("planPreviewFrame")->isVisible());

    // Nothing selected: nothing to load. Selecting a plan enables the button.
    QVERIFY(!load->isEnabled());
    list->setCurrentRow(0);
    QVERIFY(load->isEnabled());
    QVERIFY(load->height() >= 48);
    f.find<QPushButton>("deselectRobFile")->click();
    QVERIFY(list->selectedItems().isEmpty());
    QVERIFY(!load->isEnabled());

    // Loading the selected plan still returns to the operating page with it loaded.
    list->setCurrentRow(0);
    QTest::mouseClick(load, Qt::LeftButton);
    QCOMPARE(f.currentPage(), QStringLiteral("MainMenu"));
    QCOMPARE(GlobalState::instance().currentFileName(), QStringLiteral("sample.rob"));
}

void MainWindowIntegrationTest::planListFollowsDatabaseChanges()
{
    UiFixture f;
    QVERIFY(f.start());
    QTest::mouseClick(f.find<QPushButton>("navPlaene"), Qt::LeftButton);
    auto* list = f.find<QListWidget>("plannerFinalsList");
    auto* preview = f.window->findChild<multipack::ui::VisualizationWidget*>();
    auto* filterLength = f.find<QLineEdit>("lineEditFilterLength");
    QVERIFY(list && preview && filterLength);

    // A selected plan that gets new content is previewed again.
    list->setCurrentRow(0);
    const QImage before = preview->grab().toImage();
    auto changed = testhelpers::makeSamplePaletteData(QStringLiteral("sample.rob"), 1735689700000);
    changed.packageDimensions.length = 400;
    changed.packageDimensions.width = 300;
    changed.rawData[1] = {400, 300, 50, 0};
    QCOMPARE(f.database.savePaletteData(changed), SaveResult::Updated);
    QCOMPARE(list->count(), 1);
    QVERIFY(list->currentItem());
    QVERIFY(preview->grab().toImage() != before);

    // An active package size filter is answered again when the stored plans change.
    filterLength->setText(QStringLiteral("200"));
    QVERIFY(list->item(0)->isHidden());
    QCOMPARE(f.database.savePaletteData(testhelpers::makeSamplePaletteData(QStringLiteral("other.rob"))),
             SaveResult::Inserted);
    QCOMPARE(list->count(), 2);
    for (int i = 0; i < list->count(); ++i) {
        const bool isOther = list->item(i)->text().startsWith(QStringLiteral("other"));
        QCOMPARE(list->item(i)->isHidden(), !isOther);
    }
}

void MainWindowIntegrationTest::navigationSwitchesPagesAndLocksRobotUntilPlanLoaded()
{
    UiFixture f;
    QVERIFY(f.start());

    auto* betrieb = f.find<QPushButton>("navBetrieb");
    auto* plaene = f.find<QPushButton>("navPlaene");
    auto* roboter = f.find<QPushButton>("navRoboter");
    auto* parameterButton = f.find<QPushButton>("ButtonOpenParameterRoboter");
    QVERIFY(betrieb && plaene && roboter && parameterButton);

    QCOMPARE(f.currentPage(), QStringLiteral("MainMenu"));
    QVERIFY(betrieb->isChecked());

    // Robot parameters stay locked without a plan, tab and button alike.
    QVERIFY(!roboter->isEnabled());
    QVERIFY(!parameterButton->isEnabled());
    roboter->click();
    QCOMPARE(f.currentPage(), QStringLiteral("MainMenu"));

    f.loadSamplePlan();
    QTRY_VERIFY(roboter->isEnabled());
    QVERIFY(parameterButton->isEnabled());

    QTest::mouseClick(roboter, Qt::LeftButton);
    QCOMPARE(f.currentPage(), QStringLiteral("RoboParameter"));
    QVERIFY(roboter->isChecked());
    QVERIFY(!betrieb->isChecked());

    // Tapping the active tab again keeps it active.
    QTest::mouseClick(roboter, Qt::LeftButton);
    QVERIFY(roboter->isChecked());
    QCOMPARE(f.currentPage(), QStringLiteral("RoboParameter"));

    QTest::mouseClick(plaene, Qt::LeftButton);
    QCOMPARE(f.currentPage(), QStringLiteral("PlanLibrary"));
    QVERIFY(plaene->isChecked());
    QVERIFY(!roboter->isChecked());

    QTest::mouseClick(betrieb, Qt::LeftButton);
    QCOMPARE(f.currentPage(), QStringLiteral("MainMenu"));
    QTest::mouseClick(parameterButton, Qt::LeftButton);
    QCOMPARE(f.currentPage(), QStringLiteral("RoboParameter"));
    QVERIFY(roboter->isChecked());
}

void MainWindowIntegrationTest::qrCodeTabIsHiddenWithoutPlanner()
{
    UiFixture f;
    QVERIFY(f.start());

    // No planner configured: no QR tab, nothing to scan, no planner notices.
    auto* qrTab = f.find<QPushButton>("navQrCode");
    auto* qrCode = f.find<QWidget>("plannerQrCode");
    QVERIFY(qrTab && qrCode);
    QVERIFY(!qrTab->isVisible());
    QVERIFY(!qrCode->isVisible());
    QVERIFY(!f.find<QPushButton>("plannerNewVersionNotice")->isVisible());
    QVERIFY(!f.find<QWidget>("navPlaeneBadge")->isVisible());

    // "Betrieb" is the operating form, the scanner image and the two actions.
    QWidget* mainPage = f.find<QWidget>("MainMenu");
    QVERIFY(!mainPage->isAncestorOf(f.find<QWidget>("plannerFinalsList")));
    for (const char* name : {"EingabePallettenplan", "label_7", "ButtonOpenParameterRoboter", "ButtonDatenSenden"}) {
        QVERIFY2(mainPage->isAncestorOf(f.find<QWidget>(name)), name);
        QVERIFY2(f.find<QWidget>(name)->isVisible(), name);
    }

    // The QR code is not part of the operating page.
    QVERIFY(!f.find<QWidget>("MainMenu")->isAncestorOf(qrCode));
    QVERIFY(f.find<QWidget>("QrCodePage")->isAncestorOf(qrCode));
}

void MainWindowIntegrationTest::operatorTextsAvoidPlannerJargon()
{
    UiFixture f;
    QVERIFY(f.start());

    // Operators do not know the planner's word "Fertigstand": planner plans are
    // plans in "Pläne".
    QVERIFY(f.find<QPushButton>("plannerNewVersionNotice")->text().contains(QStringLiteral("im Tab Pläne")));
    for (QLabel* label : f.window->findChildren<QLabel*>()) {
        QVERIFY2(!label->text().contains(QStringLiteral("Fertigst"), Qt::CaseInsensitive),
                 qPrintable(label->objectName() + ": " + label->text()));
    }
    for (QAbstractButton* button : f.window->findChildren<QAbstractButton*>()) {
        QVERIFY2(!button->text().contains(QStringLiteral("Fertigst"), Qt::CaseInsensitive),
                 qPrintable(button->objectName() + ": " + button->text()));
    }
}

void MainWindowIntegrationTest::ur20NoticeBarOffersPaletteRelease()
{
    GlobalState& state = GlobalState::instance();
    state.setUr20Palette1Empty(true);
    state.setUr20Palette2Empty(true);
    state.setPalette1NonEmptyTimestamp(0);
    state.setPalette2NonEmptyTimestamp(0);

    UiFixture f;
    QVERIFY(f.start(QStringLiteral("UR20")));

    // A UR20 asks for the palette state at startup; confirm the defaults.
    const auto startupConfirmed = answerNextModalDialog(QStringLiteral("Bestätigen"));
    QTRY_VERIFY(*startupConfirmed);
    QTRY_VERIFY(QApplication::activeModalWidget() == nullptr);

    auto* bar = f.find<QWidget>("noticeBar");
    auto* release1 = f.find<QPushButton>("palette1ClearButton");
    auto* release2 = f.find<QPushButton>("palette2ClearButton");
    QVERIFY(bar && release1 && release2);
    QVERIFY(!bar->isVisible());

    // Palette 1 has been full for longer than the waiting time.
    state.setPalette1NonEmptyTimestamp(QDateTime::currentMSecsSinceEpoch() - 11000);
    state.setUr20Palette1Empty(false);
    QTRY_VERIFY(bar->isVisible());
    QVERIFY(release1->isVisible());
    QVERIFY(!release2->isVisible());

    // The bar stays on every page.
    QTest::mouseClick(f.find<QPushButton>("navPlaene"), Qt::LeftButton);
    QVERIFY(bar->isVisible());

    // Releasing needs a confirmation: "Nein" keeps the palette blocked.
    const auto declined = answerNextModalDialog(QStringLiteral("Nein"));
    QTest::mouseClick(release1, Qt::LeftButton);
    QVERIFY(*declined);
    QVERIFY(!state.ur20Palette1Empty());
    QVERIFY(bar->isVisible());

    const auto confirmed = answerNextModalDialog(QStringLiteral("Ja"));
    QTest::mouseClick(release1, Qt::LeftButton);
    QVERIFY(*confirmed);
    QVERIFY(state.ur20Palette1Empty());
    QCOMPARE(state.palette1NonEmptyTimestamp(), qint64(0));
    QTRY_VERIFY(!bar->isVisible());

    f.window.reset();
    state.setUr20Palette1Empty(true);
    state.setUr20Palette2Empty(true);
}

int main(int argc, char** argv)
{
#ifdef Q_OS_WIN
    qputenv("QT_QPA_PLATFORM", "windows");
#else
    qputenv("QT_QPA_PLATFORM", "offscreen");
#endif
    qputenv("QT_PLUGIN_PATH", QDir::toNativeSeparators(QLibraryInfo::path(QLibraryInfo::PluginsPath)).toUtf8());

    QApplication app(argc, argv);
    MainWindowIntegrationTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "test_mainwindow_integration.moc"
