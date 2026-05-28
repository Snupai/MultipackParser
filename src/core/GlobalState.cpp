/**
 * @file GlobalState.cpp
 * @brief Implementation of centralized state management
 */

#include "multipack/core/GlobalState.h"
#include "multipack/database/DatabaseManager.h"
#include "multipack/system/FileOperations.h"
#include <QMutexLocker>
#include <QDebug>

namespace multipack {
namespace core {

GlobalState& GlobalState::instance()
{
    static GlobalState instance;
    return instance;
}

GlobalState::GlobalState() : QObject(nullptr)
{
    qDebug() << "GlobalState::GlobalState - singleton created";
    m_centerOfGravity = {0.0, 0.0, 0.0};
}

GlobalState::~GlobalState()
{
    qDebug() << "GlobalState::~GlobalState - singleton destroyed";
}

// =============================================================================
// Palette Data
// =============================================================================

QVector<int> GlobalState::paletteDimensions() const
{
    QMutexLocker locker(&m_mutex);
    return {m_paletteLength, m_paletteWidth, m_paletteHeight};
}

void GlobalState::setPaletteDimensions(const QVector<int>& dims)
{
    if (dims.size() >= 3) {
        setPaletteDimensions(dims[0], dims[1], dims[2]);
    }
}

void GlobalState::setPaletteDimensions(int length, int width, int height)
{
    {
        QMutexLocker locker(&m_mutex);
        m_paletteLength = length;
        m_paletteWidth = width;
        m_paletteHeight = height;
    }
    emit paletteDataChanged();
}

int GlobalState::paletteLength() const
{
    QMutexLocker locker(&m_mutex);
    return m_paletteLength;
}

int GlobalState::paletteWidth() const
{
    QMutexLocker locker(&m_mutex);
    return m_paletteWidth;
}

int GlobalState::paletteHeight() const
{
    QMutexLocker locker(&m_mutex);
    return m_paletteHeight;
}

// =============================================================================
// Package Data
// =============================================================================

QVector<int> GlobalState::packageDimensions() const
{
    QMutexLocker locker(&m_mutex);
    return {m_packageLength, m_packageWidth, m_packageHeight, m_packageGap};
}

void GlobalState::setPackageDimensions(const QVector<int>& dims)
{
    if (dims.size() >= 4) {
        setPackageDimensions(dims[0], dims[1], dims[2], dims[3]);
    } else if (dims.size() >= 3) {
        setPackageDimensions(dims[0], dims[1], dims[2], 0);
    }
}

void GlobalState::setPackageDimensions(int length, int width, int height, int gap)
{
    {
        QMutexLocker locker(&m_mutex);
        m_packageLength = length;
        m_packageWidth = width;
        m_packageHeight = height;
        m_packageGap = gap;
    }
    emit packageDataChanged();
}

int GlobalState::packageLength() const
{
    QMutexLocker locker(&m_mutex);
    return m_packageLength;
}

int GlobalState::packageWidth() const
{
    QMutexLocker locker(&m_mutex);
    return m_packageWidth;
}

int GlobalState::packageHeight() const
{
    QMutexLocker locker(&m_mutex);
    return m_packageHeight;
}

int GlobalState::packageGap() const
{
    QMutexLocker locker(&m_mutex);
    return m_packageGap;
}

double GlobalState::packageWeight() const
{
    QMutexLocker locker(&m_mutex);
    return m_packageWeight;
}

void GlobalState::setPackageWeight(double weight)
{
    {
        QMutexLocker locker(&m_mutex);
        m_packageWeight = weight;
    }
    emit packageDataChanged();
}

bool GlobalState::einzelpaketLaengs() const
{
    QMutexLocker locker(&m_mutex);
    return m_einzelpaketLaengs;
}

void GlobalState::setEinzelpaketLaengs(bool value)
{
    QMutexLocker locker(&m_mutex);
    m_einzelpaketLaengs = value;
}

// =============================================================================
// Layer Data
// =============================================================================

int GlobalState::layerTypeCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_layerTypeCount;
}

void GlobalState::setLayerTypeCount(int count)
{
    {
        QMutexLocker locker(&m_mutex);
        m_layerTypeCount = count;
    }
    emit layerDataChanged();
}

int GlobalState::numberOfLayers() const
{
    QMutexLocker locker(&m_mutex);
    return m_numberOfLayers;
}

void GlobalState::setNumberOfLayers(int count)
{
    {
        QMutexLocker locker(&m_mutex);
        m_numberOfLayers = count;
    }
    emit layerDataChanged();
}

QVector<int> GlobalState::layerAssignments() const
{
    QMutexLocker locker(&m_mutex);
    return m_layerAssignments;
}

void GlobalState::setLayerAssignments(const QVector<int>& assignments)
{
    {
        QMutexLocker locker(&m_mutex);
        m_layerAssignments = assignments;
    }
    emit layerDataChanged();
}

QVector<int> GlobalState::intermediateLayers() const
{
    QMutexLocker locker(&m_mutex);
    return m_intermediateLayers;
}

void GlobalState::setIntermediateLayers(const QVector<int>& layers)
{
    {
        QMutexLocker locker(&m_mutex);
        m_intermediateLayers = layers;
    }
    emit layerDataChanged();
}

QVector<int> GlobalState::packagesPerLayerType() const
{
    QMutexLocker locker(&m_mutex);
    return m_packagesPerLayerType;
}

void GlobalState::setPackagesPerLayerType(const QVector<int>& counts)
{
    {
        QMutexLocker locker(&m_mutex);
        m_packagesPerLayerType = counts;
    }
    emit layerDataChanged();
}

// =============================================================================
// Package Positions
// =============================================================================

QVector<QVector<int>> GlobalState::packagePositions() const
{
    QMutexLocker locker(&m_mutex);
    return m_packagePositions;
}

void GlobalState::setPackagePositions(const QVector<QVector<int>>& positions)
{
    {
        QMutexLocker locker(&m_mutex);
        m_packagePositions = positions;
    }
    emit positionsChanged();
}

QVector<int> GlobalState::packagePosition(int index) const
{
    QMutexLocker locker(&m_mutex);
    if (index >= 0 && index < m_packagePositions.size()) {
        return m_packagePositions[index];
    }
    return QVector<int>();
}

int GlobalState::totalPackages() const
{
    QMutexLocker locker(&m_mutex);
    return m_totalPackages;
}

void GlobalState::setTotalPackages(int count)
{
    QMutexLocker locker(&m_mutex);
    m_totalPackages = count;
}

// =============================================================================
// Raw Data
// =============================================================================

QVector<QVector<int>> GlobalState::rawData() const
{
    QMutexLocker locker(&m_mutex);
    return m_rawData;
}

void GlobalState::setRawData(const QVector<QVector<int>>& data)
{
    QMutexLocker locker(&m_mutex);
    m_rawData = data;
}

// =============================================================================
// Metadata
// =============================================================================

bool GlobalState::paketQuer() const
{
    QMutexLocker locker(&m_mutex);
    return m_paketQuer;
}

void GlobalState::setPaketQuer(bool value)
{
    QMutexLocker locker(&m_mutex);
    m_paketQuer = value;
}

QVector<double> GlobalState::centerOfGravity() const
{
    QMutexLocker locker(&m_mutex);
    return m_centerOfGravity;
}

void GlobalState::setCenterOfGravity(const QVector<double>& cog)
{
    QMutexLocker locker(&m_mutex);
    m_centerOfGravity = cog;
}



// =============================================================================
// Current File
// =============================================================================

QString GlobalState::currentFileName() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentFileName;
}

void GlobalState::setCurrentFileName(const QString& name)
{
    {
        QMutexLocker locker(&m_mutex);
        m_currentFileName = name;
    }
    emit fileChanged(name);
}

QString GlobalState::currentFilePath() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentFilePath;
}

void GlobalState::setCurrentFilePath(const QString& path)
{
    QMutexLocker locker(&m_mutex);
    m_currentFilePath = path;
}

qint64 GlobalState::currentFileTimestamp() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentFileTimestamp;
}

void GlobalState::setCurrentFileTimestamp(qint64 timestamp)
{
    QMutexLocker locker(&m_mutex);
    m_currentFileTimestamp = timestamp;
}

// =============================================================================
// Robot State
// =============================================================================

bool GlobalState::robotConnected() const
{
    QMutexLocker locker(&m_mutex);
    return m_robotConnected;
}

void GlobalState::setRobotConnected(bool connected)
{
    {
        QMutexLocker locker(&m_mutex);
        m_robotConnected = connected;
    }
    emit robotStatusChanged(connected);
}

QString GlobalState::robotIp() const
{
    QMutexLocker locker(&m_mutex);
    return m_robotIp;
}

void GlobalState::setRobotIp(const QString& ip)
{
    QMutexLocker locker(&m_mutex);
    m_robotIp = ip;
}

// =============================================================================
// UR20 Specific State
// =============================================================================

int GlobalState::ur20ActivePalette() const
{
    QMutexLocker locker(&m_mutex);
    return m_ur20ActivePalette;
}

void GlobalState::setUr20ActivePalette(int palette)
{
    {
        QMutexLocker locker(&m_mutex);
        m_ur20ActivePalette = palette;
    }
    emit ur20StateChanged();
}

bool GlobalState::ur20Palette1Empty() const
{
    QMutexLocker locker(&m_mutex);
    return m_ur20Palette1Empty;
}

void GlobalState::setUr20Palette1Empty(bool empty)
{
    {
        QMutexLocker locker(&m_mutex);
        m_ur20Palette1Empty = empty;
    }
    emit ur20StateChanged();
}

bool GlobalState::ur20Palette2Empty() const
{
    QMutexLocker locker(&m_mutex);
    return m_ur20Palette2Empty;
}

void GlobalState::setUr20Palette2Empty(bool empty)
{
    {
        QMutexLocker locker(&m_mutex);
        m_ur20Palette2Empty = empty;
    }
    emit ur20StateChanged();
}

bool GlobalState::ur20Zwischenlage() const
{
    QMutexLocker locker(&m_mutex);
    return m_ur20Zwischenlage;
}

void GlobalState::setUr20Zwischenlage(bool value)
{
    {
        QMutexLocker locker(&m_mutex);
        m_ur20Zwischenlage = value;
    }
    emit ur20StateChanged();
}

qint64 GlobalState::palette1NonEmptyTimestamp() const
{
    QMutexLocker locker(&m_mutex);
    return m_palette1NonEmptyTimestamp;
}

void GlobalState::setPalette1NonEmptyTimestamp(qint64 timestamp)
{
    QMutexLocker locker(&m_mutex);
    m_palette1NonEmptyTimestamp = timestamp;
}

qint64 GlobalState::palette2NonEmptyTimestamp() const
{
    QMutexLocker locker(&m_mutex);
    return m_palette2NonEmptyTimestamp;
}

void GlobalState::setPalette2NonEmptyTimestamp(qint64 timestamp)
{
    QMutexLocker locker(&m_mutex);
    m_palette2NonEmptyTimestamp = timestamp;
}

// =============================================================================
// Scanner State
// =============================================================================

QString GlobalState::scannerStatus() const
{
    QMutexLocker locker(&m_mutex);
    return m_scannerStatus;
}

void GlobalState::setScannerStatus(const QString& status)
{
    QString imagePath;
    {
        QMutexLocker locker(&m_mutex);
        m_previousScannerStatus = m_scannerStatus;
        m_scannerStatus = status;

        // Determine image path based on status
        if (status == "True,True,True") {
            imagePath = ":/icons/UR20/scanner123io.png";
        } else if (status == "False,False,False") {
            imagePath = ":/icons/UR20/scanner123nio.png";
        } else if (status == "True,False,False") {
            imagePath = ":/icons/UR20/scanner1io.png";
        } else if (status == "False,True,False") {
            imagePath = ":/icons/UR20/scanner2io.png";
        } else if (status == "False,False,True") {
            imagePath = ":/icons/UR20/scanner3io.png";
        } else if (status == "True,True,False") {
            imagePath = ":/icons/UR20/scanner3nio.png";
        } else if (status == "True,False,True") {
            imagePath = ":/icons/UR20/scanner2nio.png";
        } else if (status == "False,True,True") {
            imagePath = ":/icons/UR20/scanner1nio.png";
        }
    }

    emit scannerStatusChanged(status, imagePath);
}

QString GlobalState::previousScannerStatus() const
{
    QMutexLocker locker(&m_mutex);
    return m_previousScannerStatus;
}

qint64 GlobalState::timestampScannerSafe() const
{
    QMutexLocker locker(&m_mutex);
    return m_timestampScannerSafe;
}

void GlobalState::setTimestampScannerSafe(qint64 timestamp)
{
    QMutexLocker locker(&m_mutex);
    m_timestampScannerSafe = timestamp;
}

qint64 GlobalState::timestampScannerFault() const
{
    QMutexLocker locker(&m_mutex);
    return m_timestampScannerFault;
}

void GlobalState::setTimestampScannerFault(qint64 timestamp)
{
    QMutexLocker locker(&m_mutex);
    m_timestampScannerFault = timestamp;
}

qint64 GlobalState::lastScannerWarningTime() const
{
    QMutexLocker locker(&m_mutex);
    return m_lastScannerWarningTime;
}

void GlobalState::setLastScannerWarningTime(qint64 timestamp)
{
    QMutexLocker locker(&m_mutex);
    m_lastScannerWarningTime = timestamp;
}

// =============================================================================
// UR10 Scanner Values
// =============================================================================

int GlobalState::scanner1and2NioValue() const
{
    QMutexLocker locker(&m_mutex);
    return m_scanner1and2NioValue;
}

void GlobalState::setScanner1and2NioValue(int value)
{
    QMutexLocker locker(&m_mutex);
    m_scanner1and2NioValue = value;
}

int GlobalState::scanner1Value() const
{
    QMutexLocker locker(&m_mutex);
    return m_scanner1Value;
}

void GlobalState::setScanner1Value(int value)
{
    QMutexLocker locker(&m_mutex);
    m_scanner1Value = value;
}

int GlobalState::scanner2Value() const
{
    QMutexLocker locker(&m_mutex);
    return m_scanner2Value;
}

void GlobalState::setScanner2Value(int value)
{
    QMutexLocker locker(&m_mutex);
    m_scanner2Value = value;
}

int GlobalState::scanner1and2IoValue() const
{
    QMutexLocker locker(&m_mutex);
    return m_scanner1and2IoValue;
}

void GlobalState::setScanner1and2IoValue(int value)
{
    QMutexLocker locker(&m_mutex);
    m_scanner1and2IoValue = value;
}

// =============================================================================
// UI State
// =============================================================================

bool GlobalState::klemmungAktiv() const
{
    QMutexLocker locker(&m_mutex);
    return m_klemmungAktiv;
}

void GlobalState::setKlemmungAktiv(bool active)
{
    QMutexLocker locker(&m_mutex);
    m_klemmungAktiv = active;
}

QVector<bool> GlobalState::scannerOverride() const
{
    QMutexLocker locker(&m_mutex);
    return m_scannerOverride;
}

void GlobalState::setScannerOverride(const QVector<bool>& override)
{
    QMutexLocker locker(&m_mutex);
    m_scannerOverride = override;
}

void GlobalState::setScannerOverride(int index, bool value)
{
    QMutexLocker locker(&m_mutex);
    if (index >= 0 && index < m_scannerOverride.size()) {
        m_scannerOverride[index] = value;
    }
}

bool GlobalState::labelInvert() const
{
    QMutexLocker locker(&m_mutex);
    return m_labelInvert;
}

void GlobalState::setLabelInvert(bool invert)
{
    QMutexLocker locker(&m_mutex);
    m_labelInvert = invert;
}

// =============================================================================
// Current Layer/Position Tracking
// =============================================================================

int GlobalState::currentLayer() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentLayer;
}

void GlobalState::setCurrentLayer(int layer)
{
    {
        QMutexLocker locker(&m_mutex);
        m_currentLayer = layer;
    }
    emit currentLayerChanged(layer);
}

int GlobalState::startLayer() const
{
    QMutexLocker locker(&m_mutex);
    return m_startLayer;
}

void GlobalState::setStartLayer(int layer)
{
    QMutexLocker locker(&m_mutex);
    m_startLayer = layer;
}

// =============================================================================
// Audio State
// =============================================================================

bool GlobalState::audioMuted() const
{
    QMutexLocker locker(&m_mutex);
    return m_audioMuted;
}

void GlobalState::setAudioMuted(bool muted)
{
    {
        QMutexLocker locker(&m_mutex);
        m_audioMuted = muted;
    }
    emit audioMutedChanged(muted);
}

// =============================================================================
// Data Loading
// =============================================================================

void GlobalState::loadFromRobFileData(const system::RobFileData& data)
{
    qDebug() << "GlobalState::loadFromRobFileData - loading" << data.fileName;

    {
        QMutexLocker locker(&m_mutex);

        // File info
        m_currentFileName = data.fileName;
        m_currentFilePath = data.filePath;
        m_currentFileTimestamp = data.fileTimestamp;

        // Palette dimensions
        m_paletteLength = data.paletteLength;
        m_paletteWidth = data.paletteWidth;
        m_paletteHeight = data.paletteHeight;

        // Package dimensions
        m_packageLength = data.packageLength;
        m_packageWidth = data.packageWidth;
        m_packageHeight = data.packageHeight;
        m_packageGap = data.packageGap;
        m_packageWeight = data.packageWeight;
        m_einzelpaketLaengs = data.einzelpaketLaengs;

        // Layer data
        m_layerTypeCount = data.layerTypeCount;
        m_numberOfLayers = data.numberOfLayers;
        m_layerAssignments = data.layerAssignments;
        m_intermediateLayers = data.intermediateLayers;
        m_packagesPerLayerType = data.packagesPerLayerType;

        // Convert package positions to int vectors
        m_packagePositions.clear();
        for (const auto& pos : data.packagePositions) {
            QVector<int> posVec = {
                pos.xPick, pos.yPick, pos.anglePick,
                pos.xDrop, pos.yDrop, pos.angleDrop,
                pos.count, pos.xVector, pos.yVector
            };
            m_packagePositions.append(posVec);
        }

        m_totalPackages = data.totalPackages;

        // Raw data
        m_rawData = data.rawData;

        // Metadata
        m_paketQuer = data.paketQuer;
        m_centerOfGravity = data.centerOfGravity;

        // Reset layer tracking
        m_currentLayer = 1;
        m_startLayer = 1;
    }

    // Emit signals outside of lock
    emit fileChanged(data.fileName);
    emit paletteDataChanged();
    emit packageDataChanged();
    emit layerDataChanged();
    emit positionsChanged();

    qDebug() << "GlobalState::loadFromRobFileData - complete";
}

void GlobalState::applyPaletteData(const database::PaletteData& data)
{
    qDebug() << "GlobalState::applyPaletteData -" << data.metadata.fileName;

    QVector<QVector<int>> positions;
    positions.reserve(data.packagePositions.size());
    for (const auto& pos : data.packagePositions) {
        positions.append({
            pos.xp, pos.yp, pos.ap,
            pos.xd, pos.yd, pos.ad,
            pos.nop, pos.xvec, pos.yvec
        });
    }

    {
        QMutexLocker locker(&m_mutex);
        m_rawData = data.rawData;
        m_paletteLength = data.paletteDimensions.length;
        m_paletteWidth = data.paletteDimensions.width;
        m_paletteHeight = data.paletteDimensions.height;
        m_packageLength = data.packageDimensions.length;
        m_packageWidth = data.packageDimensions.width;
        m_packageHeight = data.packageDimensions.height;
        m_packageGap = data.packageDimensions.gap;
        m_packageWeight = data.packageDimensions.weight;
        m_einzelpaketLaengs = data.packageDimensions.einzelpaketLaengs;
        m_layerTypeCount = data.metadata.lageArten;
        m_numberOfLayers = data.metadata.anzLagen;
        m_layerAssignments = data.layerAssignments;
        m_intermediateLayers = data.intermediaryLayers;
        m_packagesPerLayerType = data.packagesPerLayerType;
        m_packagePositions = positions;
        m_totalPackages = data.metadata.anzahlPakete;
        m_currentFileName = data.metadata.fileName;
        m_currentFileTimestamp = data.metadata.fileTimestamp;
        m_currentFilePath.clear();
        m_paketQuer = (data.metadata.paketQuer != 0);
        m_centerOfGravity = data.metadata.centerOfGravity;
        m_currentLayer = 1;
        m_startLayer = 1;
    }

    emit fileChanged(data.metadata.fileName);
    emit paletteDataChanged();
    emit packageDataChanged();
    emit layerDataChanged();
    emit positionsChanged();
    emit additionalDataChanged();
}

void GlobalState::clear()
{
    qDebug() << "GlobalState::clear";

    {
        QMutexLocker locker(&m_mutex);

        // Palette
        m_paletteLength = 0;
        m_paletteWidth = 0;
        m_paletteHeight = 0;

        // Package
        m_packageLength = 0;
        m_packageWidth = 0;
        m_packageHeight = 0;
        m_packageGap = 0;
        m_packageWeight = 0.0;
        m_einzelpaketLaengs = false;

        // Layer
        m_layerTypeCount = 0;
        m_numberOfLayers = 0;
        m_layerAssignments.clear();
        m_intermediateLayers.clear();
        m_packagesPerLayerType.clear();

        // Positions
        m_packagePositions.clear();
        m_totalPackages = 0;

        // Raw data
        m_rawData.clear();

        // Metadata
        m_paketQuer = 1;
        m_centerOfGravity = {0.0, 0.0, 0.0};

        // File
        m_currentFileName.clear();
        m_currentFilePath.clear();
        m_currentFileTimestamp = 0;

        // Layer tracking
        m_currentLayer = 1;
        m_startLayer = 1;
    }

    emit fileChanged(QString());
    emit paletteDataChanged();
    emit packageDataChanged();
    emit layerDataChanged();
    emit positionsChanged();
}

bool GlobalState::hasLoadedData() const
{
    QMutexLocker locker(&m_mutex);
    return !m_currentFileName.isEmpty() && m_numberOfLayers > 0;
}

// =========================================================================
// Additional Data Variables Implementation
// =========================================================================

QVector<int> GlobalState::startlage() const
{
    QMutexLocker locker(&m_mutex);
    return m_startlage;
}

void GlobalState::setStartlage(const QVector<int>& startlage)
{
    QMutexLocker locker(&m_mutex);
    m_startlage = startlage;
    emit additionalDataChanged();
}



double GlobalState::massePaket() const
{
    QMutexLocker locker(&m_mutex);
    return m_massePaket;
}

void GlobalState::setMassePaket(double masse)
{
    QMutexLocker locker(&m_mutex);
    m_massePaket = masse;
    emit additionalDataChanged();
}

double GlobalState::pickOffsetX() const
{
    QMutexLocker locker(&m_mutex);
    return m_pickOffsetX;
}

void GlobalState::setPickOffsetX(double offset)
{
    QMutexLocker locker(&m_mutex);
    m_pickOffsetX = offset;
    emit additionalDataChanged();
}

double GlobalState::pickOffsetY() const
{
    QMutexLocker locker(&m_mutex);
    return m_pickOffsetY;
}

void GlobalState::setPickOffsetY(double offset)
{
    QMutexLocker locker(&m_mutex);
    m_pickOffsetY = offset;
    emit additionalDataChanged();
}

// =========================================================================
// Filter Variables Implementation
// =========================================================================

int GlobalState::filterLength() const
{
    QMutexLocker locker(&m_mutex);
    return m_filterLength;
}

void GlobalState::setFilterLength(int length)
{
    QMutexLocker locker(&m_mutex);
    m_filterLength = length;
    emit filterDimensionsChanged();
}

int GlobalState::filterWidth() const
{
    QMutexLocker locker(&m_mutex);
    return m_filterWidth;
}

void GlobalState::setFilterWidth(int width)
{
    QMutexLocker locker(&m_mutex);
    m_filterWidth = width;
    emit filterDimensionsChanged();
}

int GlobalState::filterHeight() const
{
    QMutexLocker locker(&m_mutex);
    return m_filterHeight;
}

void GlobalState::setFilterHeight(int height)
{
    QMutexLocker locker(&m_mutex);
    m_filterHeight = height;
    emit filterDimensionsChanged();
}

} // namespace core
} // namespace multipack
