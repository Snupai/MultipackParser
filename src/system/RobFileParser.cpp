/**
 * @file RobFileParser.cpp
 * @brief Implementation of .rob file parser
 */

#include "multipack/system/RobFileParser.h"
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QTextCodec>
#include <QRegularExpression>

namespace multipack {

RobFileParser::RobFileParser(const QString& usbPath)
    : m_usbPath(usbPath)
{
}

RobFileData RobFileParser::parseFile(const QString& filename)
{
    QString fullPath = QDir(m_usbPath).filePath(filename);
    return parseFileFullPath(fullPath);
}

RobFileData RobFileParser::parseFileFullPath(const QString& fullPath)
{
    RobFileData data;
    data.filePath = fullPath;
    data.fileTimestamp = getFileTimestamp(fullPath);
    
    // Check if file exists
    if (!QFile::exists(fullPath)) {
        data.isValid = false;
        data.errorMessage = QString("File does not exist: %1").arg(fullPath);
        return data;
    }
    
    // Parse the file into 2D integer array
    QVector<QVector<int>> raw_data = readFileWithEncodings(fullPath);
    if (raw_data.isEmpty()) {
        data.isValid = false;
        data.errorMessage = QString("Failed to parse file or file is empty: %1").arg(fullPath);
        return data;
    }
    
    // Parse the structured data
    return parseDataArray(raw_data, fullPath);
}

bool RobFileParser::isValidRobFile(const QString& filename)
{
    if (!filename.endsWith(".rob", Qt::CaseInsensitive)) {
        return false;
    }
    
    QString fullPath = QDir(m_usbPath).filePath(filename);
    return QFile::exists(fullPath);
}

QStringList RobFileParser::getAvailableRobFiles()
{
    QStringList robFiles;
    QDir usbDir(m_usbPath);
    
    if (!usbDir.exists()) {
        qWarning() << "USB directory does not exist:" << m_usbPath;
        return robFiles;
    }
    
    QStringList filters;
    filters << "*.rob";
    usbDir.setNameFilters(filters);
    
    QFileInfoList fileList = usbDir.entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo& fileInfo : fileList) {
        robFiles.append(fileInfo.fileName());
    }
    
    return robFiles;
}

void RobFileParser::setUsbPath(const QString& path)
{
    m_usbPath = path;
}

QString RobFileParser::getUsbPath() const
{
    return m_usbPath;
}

QVector<QVector<int>> RobFileParser::readFileWithEncodings(const QString& filePath)
{
    QVector<QVector<int>> data;
    
    // Try different encodings in order of likelihood
    QStringList encodings = {"UTF-8", "Latin-1", "Windows-1252", "System"};
    
    for (const QString& encoding : encodings) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }
        
        QTextStream stream(&file);
        stream.setEncoding(QStringConverter::encodingForName(encoding.toUtf8()));
        
        data.clear();
        bool parseSuccess = true;
        
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (line.isEmpty()) {
                continue;
            }
            
            QVector<int> lineData = lineToIntVector(line);
            if (lineData.isEmpty()) {
                parseSuccess = false;
                break;
            }
            
            data.append(lineData);
        }
        
        file.close();
        
        if (parseSuccess && !data.isEmpty()) {
            qDebug() << "Successfully parsed file with encoding:" << encoding;
            return data;
        }
    }
    
    qWarning() << "Failed to parse file with any encoding:" << filePath;
    return QVector<QVector<int>>();
}

RobFileData RobFileParser::parseDataArray(const QVector<QVector<int>>& data, const QString& filePath)
{
    RobFileData result;
    result.filePath = filePath;
    result.fileTimestamp = getFileTimestamp(filePath);
    
    if (data.size() < 4) {
        result.isValid = false;
        result.errorMessage = "File has too few lines (minimum 4 required)";
        return result;
    }
    
    try {
        // Parse pallet dimensions from first line
        if (data[LI_PALETTE_DATA].size() < 3) {
            result.isValid = false;
            result.errorMessage = "Invalid pallet data format";
            return result;
        }
        
        int pl = data[LI_PALETTE_DATA][LI_PALETTE_DATA_LENGTH];  // Length
        int pw = data[LI_PALETTE_DATA][LI_PALETTE_DATA_WIDTH];   // Width
        int ph = data[LI_PALETTE_DATA][LI_PALETTE_DATA_HEIGHT];  // Height
        result.g_PalettenDim = {pl, pw, ph};
        
        // Parse package dimensions from second line
        if (data[LI_PACKAGE_DATA].size() < 4) {
            result.isValid = false;
            result.errorMessage = "Invalid package data format";
            return result;
        }
        
        pl = data[LI_PACKAGE_DATA][LI_PACKAGE_DATA_LENGTH];  // Length
        pw = data[LI_PACKAGE_DATA][LI_PACKAGE_DATA_WIDTH];   // Width
        ph = data[LI_PACKAGE_DATA][LI_PACKAGE_DATA_HEIGHT];  // Height
        int pr = data[LI_PACKAGE_DATA][LI_PACKAGE_DATA_GAP];     // Gap
        result.g_PaketDim = {pl, pw, ph, pr};
        
        // Get number of layer types from third line
        if (data[LI_LAYERTYPES].isEmpty()) {
            result.isValid = false;
            result.errorMessage = "Invalid layer types format";
            return result;
        }
        result.g_LageArten = data[LI_LAYERTYPES][0];
        
        // Get number of layers from fourth line
        if (data[LI_NUMBER_OF_LAYERS].isEmpty()) {
            result.isValid = false;
            result.errorMessage = "Invalid number of layers format";
            return result;
        }
        result.g_AnzLagen = data[LI_NUMBER_OF_LAYERS][0];
        
        // Parse layer assignments and intermediate layers
        // Python: index = LI_NUMBER_OF_LAYERS + 2 (skip header lines)
        int index = LI_NUMBER_OF_LAYERS + 2;  // Skip header lines (line 4 is blank/header)
        int end_index = index + result.g_AnzLagen;
        
        if (data.size() < end_index) {
            result.isValid = false;
            result.errorMessage = QString("File has insufficient data for layers (need %1, have %2)")
                                    .arg(end_index).arg(data.size());
            return result;
        }
        
        while (index < end_index && index < data.size()) {
            if (data[index].size() < 2) {
                result.isValid = false;
                result.errorMessage = QString("Invalid layer assignment at line %1").arg(index + 1);
                return result;
            }
            
            int lagenart = data[index][0];     // Layer type
            int zwischenlagen = data[index][1]; // Intermediate layer flag
            
            result.g_LageZuordnung.append(lagenart);
            result.g_Zwischenlagen.append(zwischenlagen);
            index++;
        }
        
        // Parse package positions
        // Python: ersteLage = 4 + (anzLagen + 1)
        int ersteLage = 4 + (result.g_AnzLagen + 1);  // Skip layer data
        if (data.size() <= ersteLage) {
            result.isValid = false;
            result.errorMessage = "File ends before package position data";
            return result;
        }
        
        int anzahlPaket = data[ersteLage][0];  // Total packages
        result.g_AnzahlPakete = anzahlPaket;    // Note: Deprecated - number of picks for multipick
        int index_paketZuordnung = ersteLage;
        
        // Get number of packages per layer type
        if (result.g_LageArten > 0) {
            for (int i = 0; i < result.g_LageArten; ++i) {
                if (index_paketZuordnung >= data.size()) {
                    result.isValid = false;
                    result.errorMessage = QString("Insufficient data for package count at layer type %1").arg(i);
                    return result;
                }
                
                if (data[index_paketZuordnung].isEmpty()) {
                    result.isValid = false;
                    result.errorMessage = QString("Empty package count line at layer type %1").arg(i);
                    return result;
                }
                
                int anzahlPick = data[index_paketZuordnung][0];
                result.g_PaketeZuordnung.append(anzahlPick);
                index_paketZuordnung = index_paketZuordnung + anzahlPick + 1;
            }
        }
        
        // Parse individual package positions for each layer type
        // Python: starts at ersteLage, increments after each layer type header
        index = ersteLage;
        for (int i = 0; i < result.g_LageArten; ++i) {
            index++; // Skip count line (number of packages in this layer type)
            
            if (i >= result.g_PaketeZuordnung.size()) {
                break; // Safety check
            }
            
            int anzahlPaket = result.g_PaketeZuordnung[i];
            
            for (int j = 0; j < anzahlPaket; ++j) {
                if (index >= data.size()) {
                    result.isValid = false;
                    result.errorMessage = QString("Insufficient package position data at layer %1, package %2")
                                          .arg(i).arg(j);
                    return result;
                }
                
                if (data[index].size() < 9) {
                    result.isValid = false;
                    result.errorMessage = QString("Invalid package position data at line %1 (need 9 values)")
                                          .arg(index + 1);
                    return result;
                }
                
                // Extract position data (9 values per package)
                int xp = data[index][LI_POSITION_XP];    // X pick position
                int yp = data[index][LI_POSITION_YP];    // Y pick position
                int ap = data[index][LI_POSITION_AP];    // Pick angle
                int xd = data[index][LI_POSITION_XD];    // X drop position
                int yd = data[index][LI_POSITION_YD];    // Y drop position
                int ad = data[index][LI_POSITION_AD];    // Drop angle
                int nop = data[index][LI_POSITION_NOP];  // Number of packages
                int xvec = data[index][LI_POSITION_XVEC]; // X vector
                int yvec = data[index][LI_POSITION_YVEC]; // Y vector
                
                QVector<int> packagePos = {xp, yp, ap, xd, yd, ad, nop, xvec, yvec};
                result.g_PaketPos.append(packagePos);
                index++;
            }
        }
        
        // Set defaults for remaining fields
        result.g_paket_quer = 1;
        result.g_CenterOfGravity = {0, 0, 0};
        result.g_Daten = data;
        
        result.isValid = validateParsedData(result);
        
    } catch (const std::exception& e) {
        result.isValid = false;
        result.errorMessage = QString("Exception during parsing: %1").arg(e.what());
    }
    
    return result;
}

QDateTime RobFileParser::getFileTimestamp(const QString& filePath)
{
    QFileInfo fileInfo(filePath);
    if (fileInfo.exists()) {
        return fileInfo.lastModified();
    }
    return QDateTime();
}

bool RobFileParser::validateParsedData(RobFileData& data)
{
    // Basic validation
    if (data.g_PalettenDim.size() != 3) {
        data.errorMessage = "Invalid pallet dimensions (should have 3 values)";
        return false;
    }
    
    if (data.g_PaketDim.size() != 4) {
        data.errorMessage = "Invalid package dimensions (should have 4 values)";
        return false;
    }
    
    if (data.g_LageArten < 0 || data.g_LageArten > 10) {
        data.errorMessage = "Invalid number of layer types";
        return false;
    }
    
    if (data.g_AnzLagen < 0 || data.g_AnzLagen > 100) {
        data.errorMessage = "Invalid number of layers";
        return false;
    }
    
    if (data.g_LageZuordnung.size() != data.g_AnzLagen) {
        data.errorMessage = "Layer assignment count doesn't match number of layers";
        return false;
    }
    
    if (data.g_Zwischenlagen.size() != data.g_AnzLagen) {
        data.errorMessage = "Intermediate layer count doesn't match number of layers";
        return false;
    }
    
    if (data.g_PaketeZuordnung.size() != data.g_LageArten) {
        data.errorMessage = "Package count per layer type doesn't match layer types";
        return false;
    }
    
    // Validate dimensions are positive
    for (int dim : data.g_PalettenDim) {
        if (dim <= 0) {
            data.errorMessage = "Pallet dimensions must be positive";
            return false;
        }
    }
    
    for (int dim : data.g_PaketDim) {
        if (dim < 0) { // Gap can be 0
            data.errorMessage = "Package dimensions must be non-negative";
            return false;
        }
    }
    
    return true;
}

QVector<int> RobFileParser::lineToIntVector(const QString& line)
{
    QVector<int> result;
    
    // Split by tab characters
    QStringList parts = line.split('\t', Qt::SkipEmptyParts);
    
    for (const QString& part : parts) {
        bool ok;
        int value = part.toInt(&ok);
        if (!ok) {
            qWarning() << "Failed to convert to integer:" << part;
            return QVector<int>(); // Return empty vector on error
        }
        result.append(value);
    }
    
    return result;
}

} // namespace multipack