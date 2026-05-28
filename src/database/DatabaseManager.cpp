/**
 * @file DatabaseManager.cpp
 * @brief Full SQLite database implementation for palette storage
 */
#include "multipack/database/DatabaseManager.h"

#include <QDebug>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QUuid>

namespace multipack {
namespace database {

namespace {

QString formatTimestamp(qint64 timestampMs)
{
    return QDateTime::fromMSecsSinceEpoch(timestampMs).toString("yyyy-MM-dd hh:mm:ss");
}

}

DatabaseManager::DatabaseManager(QObject* parent)
    : QObject(parent)
    , m_connectionName(QUuid::createUuid().toString())
{
    qDebug() << "DatabaseManager - initialized";
}

DatabaseManager::~DatabaseManager()
{
    close();
}

bool DatabaseManager::open(const QString& path)
{
    if (m_open) {
        close();
    }

    qDebug() << "DatabaseManager::open -" << path;

    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connectionName);
    m_db.setDatabaseName(path);

    if (!m_db.open()) {
        qCritical() << "Failed to open database:" << m_db.lastError().text();
        return false;
    }

    m_open = true;

    // Enable foreign keys
    QSqlQuery query(m_db);
    query.exec("PRAGMA foreign_keys = ON");

    // Create tables and run migrations
    if (!createTables()) {
        qCritical() << "Failed to create database tables";
        close();
        return false;
    }

    runMigrations();

    emit databaseOpened();
    qDebug() << "Database opened successfully";
    return true;
}

void DatabaseManager::close()
{
    if (m_open) {
        m_db.close();
        m_open = false;
        emit databaseClosed();
    }
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool DatabaseManager::isOpen() const
{
    return m_open;
}

bool DatabaseManager::createTables()
{
    QSqlQuery query(m_db);

    // Main metadata table
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS paletten_metadata (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            paket_quer INTEGER,
            center_of_gravity_x REAL,
            center_of_gravity_y REAL,
            center_of_gravity_z REAL,
            lage_arten INTEGER,
            anz_lagen INTEGER,
            anzahl_pakete INTEGER,
            file_timestamp REAL,
            file_name TEXT
        )
    )")) {
        qCritical() << "Failed to create paletten_metadata:" << query.lastError().text();
        return false;
    }

    // Index on file_name
    query.exec("CREATE INDEX IF NOT EXISTS idx_file_name ON paletten_metadata(file_name)");

    // Raw data table
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS daten (
            id INTEGER PRIMARY KEY,
            metadata_id INTEGER,
            row_index INTEGER,
            col_index INTEGER,
            value INTEGER,
            FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
        )
    )")) {
        qCritical() << "Failed to create daten:" << query.lastError().text();
        return false;
    }

    // Pallet dimensions
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS paletten_dim (
            id INTEGER PRIMARY KEY,
            metadata_id INTEGER,
            length INTEGER,
            width INTEGER,
            height INTEGER,
            FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
        )
    )")) {
        qCritical() << "Failed to create paletten_dim:" << query.lastError().text();
        return false;
    }

    // Package dimensions
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS paket_dim (
            id INTEGER PRIMARY KEY,
            metadata_id INTEGER,
            length INTEGER,
            width INTEGER,
            height INTEGER,
            gap INTEGER,
            weight REAL,
            einzelpaket_laengs INTEGER,
            FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
        )
    )")) {
        qCritical() << "Failed to create paket_dim:" << query.lastError().text();
        return false;
    }

    // Layer assignments
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS lage_zuordnung (
            id INTEGER PRIMARY KEY,
            metadata_id INTEGER,
            lage_index INTEGER,
            value INTEGER,
            FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
        )
    )")) {
        qCritical() << "Failed to create lage_zuordnung:" << query.lastError().text();
        return false;
    }

    // Intermediary layers
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS zwischenlagen (
            id INTEGER PRIMARY KEY,
            metadata_id INTEGER,
            lage_index INTEGER,
            value INTEGER,
            FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
        )
    )")) {
        qCritical() << "Failed to create zwischenlagen:" << query.lastError().text();
        return false;
    }

    // Packages per layer type
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS pakete_zuordnung (
            id INTEGER PRIMARY KEY,
            metadata_id INTEGER,
            lage_index INTEGER,
            value INTEGER,
            FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
        )
    )")) {
        qCritical() << "Failed to create pakete_zuordnung:" << query.lastError().text();
        return false;
    }

    // Package positions
    if (!query.exec(R"(
        CREATE TABLE IF NOT EXISTS paket_pos (
            id INTEGER PRIMARY KEY,
            metadata_id INTEGER,
            paket_index INTEGER,
            xp INTEGER,
            yp INTEGER,
            ap INTEGER,
            xd INTEGER,
            yd INTEGER,
            ad INTEGER,
            nop INTEGER,
            xvec INTEGER,
            yvec INTEGER,
            FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
        )
    )")) {
        qCritical() << "Failed to create paket_pos:" << query.lastError().text();
        return false;
    }

    return true;
}

void DatabaseManager::runMigrations()
{
    QSqlQuery query(m_db);

    // Add weight column if missing
    query.exec("ALTER TABLE paket_dim ADD COLUMN weight REAL");

    // Add einzelpaket_laengs column if missing
    query.exec("ALTER TABLE paket_dim ADD COLUMN einzelpaket_laengs INTEGER");
}

SaveResult DatabaseManager::savePaletteData(const PaletteData& data)
{
    if (!m_open) {
        qWarning() << "Database not open";
        return SaveResult::Error;
    }

    QSqlQuery query(m_db);

    // Check if file already exists
    int existingId = getMetadataId(data.metadata.fileName);
    const bool hadExistingData = (existingId > 0);
    if (existingId > 0) {
        // Check timestamps
        query.prepare("SELECT file_timestamp FROM paletten_metadata WHERE id = ?");
        query.addBindValue(existingId);
        if (query.exec() && query.next()) {
            qint64 existingTimestamp = query.value(0).toLongLong();
            if (existingTimestamp >= data.metadata.fileTimestamp) {
                qDebug() << "Skipping save - existing data is newer";
                return SaveResult::Unchanged;
            }
        } else {
            qCritical() << "Failed to read existing metadata timestamp:" << query.lastError().text();
            return SaveResult::Error;
        }
    }

    if (!m_db.transaction()) {
        qCritical() << "Failed to start database transaction:" << m_db.lastError().text();
        return SaveResult::Error;
    }

    auto rollbackWithError = [this](const QString& message) {
        qCritical() << message;
        m_db.rollback();
        return SaveResult::Error;
    };

    if (existingId > 0) {
        // Delete existing data (CASCADE will clean up related tables)
        query.prepare("DELETE FROM paletten_metadata WHERE id = ?");
        query.addBindValue(existingId);
        if (!query.exec()) {
            return rollbackWithError(QString("Failed to delete existing metadata: %1").arg(query.lastError().text()));
        }
    }

    // Insert metadata
    query.prepare(R"(
        INSERT INTO paletten_metadata (
            paket_quer, center_of_gravity_x, center_of_gravity_y, center_of_gravity_z,
            lage_arten, anz_lagen, anzahl_pakete, file_timestamp, file_name
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    query.addBindValue(data.metadata.paketQuer);
    query.addBindValue(data.metadata.centerOfGravity.value(0, 0.0));
    query.addBindValue(data.metadata.centerOfGravity.value(1, 0.0));
    query.addBindValue(data.metadata.centerOfGravity.value(2, 0.0));
    query.addBindValue(data.metadata.lageArten);
    query.addBindValue(data.metadata.anzLagen);
    query.addBindValue(data.metadata.anzahlPakete);
    query.addBindValue(data.metadata.fileTimestamp);
    query.addBindValue(data.metadata.fileName);

    if (!query.exec()) {
        return rollbackWithError(QString("Failed to insert metadata: %1").arg(query.lastError().text()));
    }

    int metadataId = query.lastInsertId().toInt();

    // Insert raw data
    for (int i = 0; i < data.rawData.size(); ++i) {
        for (int j = 0; j < data.rawData[i].size(); ++j) {
            query.prepare("INSERT INTO daten (metadata_id, row_index, col_index, value) VALUES (?, ?, ?, ?)");
            query.addBindValue(metadataId);
            query.addBindValue(i);
            query.addBindValue(j);
            query.addBindValue(data.rawData[i][j]);
            if (!query.exec()) {
                return rollbackWithError(QString("Failed to insert raw data: %1").arg(query.lastError().text()));
            }
        }
    }

    // Insert pallet dimensions
    query.prepare("INSERT INTO paletten_dim (metadata_id, length, width, height) VALUES (?, ?, ?, ?)");
    query.addBindValue(metadataId);
    query.addBindValue(data.paletteDimensions.length);
    query.addBindValue(data.paletteDimensions.width);
    query.addBindValue(data.paletteDimensions.height);
    if (!query.exec()) {
        return rollbackWithError(QString("Failed to insert pallet dimensions: %1").arg(query.lastError().text()));
    }

    // Insert package dimensions
    query.prepare("INSERT INTO paket_dim (metadata_id, length, width, height, gap, weight, einzelpaket_laengs) VALUES (?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(metadataId);
    query.addBindValue(data.packageDimensions.length);
    query.addBindValue(data.packageDimensions.width);
    query.addBindValue(data.packageDimensions.height);
    query.addBindValue(data.packageDimensions.gap);
    query.addBindValue(data.packageDimensions.weight);
    query.addBindValue(data.packageDimensions.einzelpaketLaengs ? 1 : 0);
    if (!query.exec()) {
        return rollbackWithError(QString("Failed to insert package dimensions: %1").arg(query.lastError().text()));
    }

    // Insert layer assignments
    for (int i = 0; i < data.layerAssignments.size(); ++i) {
        query.prepare("INSERT INTO lage_zuordnung (metadata_id, lage_index, value) VALUES (?, ?, ?)");
        query.addBindValue(metadataId);
        query.addBindValue(i);
        query.addBindValue(data.layerAssignments[i]);
        if (!query.exec()) {
            return rollbackWithError(QString("Failed to insert layer assignments: %1").arg(query.lastError().text()));
        }
    }

    // Insert intermediary layers
    for (int i = 0; i < data.intermediaryLayers.size(); ++i) {
        query.prepare("INSERT INTO zwischenlagen (metadata_id, lage_index, value) VALUES (?, ?, ?)");
        query.addBindValue(metadataId);
        query.addBindValue(i);
        query.addBindValue(data.intermediaryLayers[i]);
        if (!query.exec()) {
            return rollbackWithError(QString("Failed to insert intermediary layers: %1").arg(query.lastError().text()));
        }
    }

    // Insert packages per layer type
    for (int i = 0; i < data.packagesPerLayerType.size(); ++i) {
        query.prepare("INSERT INTO pakete_zuordnung (metadata_id, lage_index, value) VALUES (?, ?, ?)");
        query.addBindValue(metadataId);
        query.addBindValue(i);
        query.addBindValue(data.packagesPerLayerType[i]);
        if (!query.exec()) {
            return rollbackWithError(QString("Failed to insert packages per layer type: %1").arg(query.lastError().text()));
        }
    }

    // Insert package positions
    for (int i = 0; i < data.packagePositions.size(); ++i) {
        const auto& pos = data.packagePositions[i];
        query.prepare(R"(
            INSERT INTO paket_pos (metadata_id, paket_index, xp, yp, ap, xd, yd, ad, nop, xvec, yvec)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )");
        query.addBindValue(metadataId);
        query.addBindValue(i);
        query.addBindValue(pos.xp);
        query.addBindValue(pos.yp);
        query.addBindValue(pos.ap);
        query.addBindValue(pos.xd);
        query.addBindValue(pos.yd);
        query.addBindValue(pos.ad);
        query.addBindValue(pos.nop);
        query.addBindValue(pos.xvec);
        query.addBindValue(pos.yvec);
        if (!query.exec()) {
            return rollbackWithError(QString("Failed to insert package positions: %1").arg(query.lastError().text()));
        }
    }

    if (!m_db.commit()) {
        return rollbackWithError(QString("Failed to commit palette transaction: %1").arg(m_db.lastError().text()));
    }

    qDebug() << "Saved palette data:" << data.metadata.fileName;
    emit dataChanged();
    return hadExistingData ? SaveResult::Updated : SaveResult::Inserted;
}

std::optional<PaletteData> DatabaseManager::loadPaletteData(const QString& fileName, int metadataId)
{
    if (!m_open) {
        qWarning() << "Database not open";
        return std::nullopt;
    }

    QSqlQuery query(m_db);

    // Find the metadata record
    if (metadataId > 0) {
        query.prepare(R"(
            SELECT id, paket_quer, center_of_gravity_x, center_of_gravity_y, center_of_gravity_z,
                   lage_arten, anz_lagen, anzahl_pakete, file_timestamp, file_name
            FROM paletten_metadata WHERE id = ?
        )");
        query.addBindValue(metadataId);
    } else if (!fileName.isEmpty()) {
        query.prepare(R"(
            SELECT id, paket_quer, center_of_gravity_x, center_of_gravity_y, center_of_gravity_z,
                   lage_arten, anz_lagen, anzahl_pakete, file_timestamp, file_name
            FROM paletten_metadata WHERE file_name LIKE ?
        )");
        query.addBindValue("%" + fileName + "%");
    } else {
        query.prepare(R"(
            SELECT id, paket_quer, center_of_gravity_x, center_of_gravity_y, center_of_gravity_z,
                   lage_arten, anz_lagen, anzahl_pakete, file_timestamp, file_name
            FROM paletten_metadata ORDER BY file_timestamp DESC LIMIT 1
        )");
    }

    if (!query.exec() || !query.next()) {
        qWarning() << "Palette data not found";
        return std::nullopt;
    }

    PaletteData data;

    // Load metadata
    data.metadata.id = query.value(0).toInt();
    data.metadata.paketQuer = query.value(1).toInt();
    data.metadata.centerOfGravity = {
        query.value(2).toDouble(),
        query.value(3).toDouble(),
        query.value(4).toDouble()
    };
    data.metadata.lageArten = query.value(5).toInt();
    data.metadata.anzLagen = query.value(6).toInt();
    data.metadata.anzahlPakete = query.value(7).toInt();
    data.metadata.fileTimestamp = query.value(8).toLongLong();
    data.metadata.fileName = query.value(9).toString();

    int id = data.metadata.id;

    // Load raw data
    query.prepare("SELECT row_index, col_index, value FROM daten WHERE metadata_id = ? ORDER BY row_index, col_index");
    query.addBindValue(id);
    if (query.exec()) {
        int maxRow = -1;
        QVector<QPair<int, QPair<int, int>>> values;
        while (query.next()) {
            int row = query.value(0).toInt();
            int col = query.value(1).toInt();
            int val = query.value(2).toInt();
            values.append({row, {col, val}});
            if (row > maxRow) maxRow = row;
        }
        data.rawData.resize(maxRow + 1);
        for (const auto& v : values) {
            int row = v.first;
            int col = v.second.first;
            int val = v.second.second;
            while (data.rawData[row].size() <= col) {
                data.rawData[row].append(0);
            }
            data.rawData[row][col] = val;
        }
    }

    // Load pallet dimensions
    query.prepare("SELECT length, width, height FROM paletten_dim WHERE metadata_id = ?");
    query.addBindValue(id);
    if (query.exec() && query.next()) {
        data.paletteDimensions.length = query.value(0).toInt();
        data.paletteDimensions.width = query.value(1).toInt();
        data.paletteDimensions.height = query.value(2).toInt();
    }

    // Load package dimensions
    query.prepare("SELECT length, width, height, gap, weight, einzelpaket_laengs FROM paket_dim WHERE metadata_id = ?");
    query.addBindValue(id);
    if (query.exec() && query.next()) {
        data.packageDimensions.length = query.value(0).toInt();
        data.packageDimensions.width = query.value(1).toInt();
        data.packageDimensions.height = query.value(2).toInt();
        data.packageDimensions.gap = query.value(3).toInt();
        data.packageDimensions.weight = query.value(4).toDouble();
        data.packageDimensions.einzelpaketLaengs = query.value(5).toInt() != 0;
    }

    // Load layer assignments
    query.prepare("SELECT value FROM lage_zuordnung WHERE metadata_id = ? ORDER BY lage_index");
    query.addBindValue(id);
    if (query.exec()) {
        while (query.next()) {
            data.layerAssignments.append(query.value(0).toInt());
        }
    }

    // Load intermediary layers
    query.prepare("SELECT value FROM zwischenlagen WHERE metadata_id = ? ORDER BY lage_index");
    query.addBindValue(id);
    if (query.exec()) {
        while (query.next()) {
            data.intermediaryLayers.append(query.value(0).toInt());
        }
    }

    // Load packages per layer type
    query.prepare("SELECT value FROM pakete_zuordnung WHERE metadata_id = ? ORDER BY lage_index");
    query.addBindValue(id);
    if (query.exec()) {
        while (query.next()) {
            data.packagesPerLayerType.append(query.value(0).toInt());
        }
    }

    // Load package positions
    query.prepare("SELECT xp, yp, ap, xd, yd, ad, nop, xvec, yvec FROM paket_pos WHERE metadata_id = ? ORDER BY paket_index");
    query.addBindValue(id);
    if (query.exec()) {
        while (query.next()) {
            PackagePosition pos;
            pos.xp = query.value(0).toInt();
            pos.yp = query.value(1).toInt();
            pos.ap = query.value(2).toInt();
            pos.xd = query.value(3).toInt();
            pos.yd = query.value(4).toInt();
            pos.ad = query.value(5).toInt();
            pos.nop = query.value(6).toInt();
            pos.xvec = query.value(7).toInt();
            pos.yvec = query.value(8).toInt();
            data.packagePositions.append(pos);
        }
    }

    qDebug() << "Loaded palette data:" << data.metadata.fileName;
    return data;
}

QVector<FileInfo> DatabaseManager::listAvailableFiles()
{
    QVector<FileInfo> files;

    if (!m_open) {
        return files;
    }

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT id, file_name, file_timestamp FROM paletten_metadata
        WHERE file_name IS NOT NULL AND file_name LIKE '%.rob'
        ORDER BY file_timestamp DESC
    )");

    if (query.exec()) {
        while (query.next()) {
            FileInfo info;
            info.id = query.value(0).toInt();
            info.fileName = query.value(1).toString();
            info.timestamp = query.value(2).toLongLong();
            info.timestampStr = formatTimestamp(info.timestamp);
            files.append(info);
        }
    }

    return files;
}

std::optional<FileInfo> DatabaseManager::findFile(const QString& fileName)
{
    if (!m_open) {
        return std::nullopt;
    }

    QSqlQuery query(m_db);
    query.prepare("SELECT id, file_name, file_timestamp FROM paletten_metadata WHERE file_name LIKE ?");
    query.addBindValue("%" + fileName + "%");

    if (query.exec() && query.next()) {
        FileInfo info;
        info.id = query.value(0).toInt();
        info.fileName = query.value(1).toString();
        info.timestamp = query.value(2).toLongLong();
        info.timestampStr = formatTimestamp(info.timestamp);
        return info;
    }

    return std::nullopt;
}

QStringList DatabaseManager::findByPackageDimensions(int length, int width, int height)
{
    QStringList fileNames;

    if (!m_open || (length == 0 && width == 0 && height == 0)) {
        return fileNames;
    }

    QString sql = "SELECT metadata_id FROM paket_dim WHERE 1=1";
    QVariantList params;

    if (length != 0) {
        sql += " AND length = ?";
        params.append(length);
    }
    if (width != 0) {
        sql += " AND width = ?";
        params.append(width);
    }
    if (height != 0) {
        sql += " AND height = ?";
        params.append(height);
    }

    QSqlQuery query(m_db);
    query.prepare(sql);
    for (const auto& param : params) {
        query.addBindValue(param);
    }

    if (query.exec()) {
        while (query.next()) {
            int metadataId = query.value(0).toInt();

            QSqlQuery nameQuery(m_db);
            nameQuery.prepare("SELECT file_name FROM paletten_metadata WHERE id = ?");
            nameQuery.addBindValue(metadataId);
            if (nameQuery.exec() && nameQuery.next()) {
                QString name = nameQuery.value(0).toString();
                name.replace(".rob", "");
                fileNames.append(name);
            }
        }
    }

    return fileNames;
}

bool DatabaseManager::updateBoxDimensions(const QString& fileName, int height, double weight, int einzelpaketLaengs)
{
    if (!m_open || fileName.isEmpty()) {
        return false;
    }

    QString name = fileName;
    if (!name.endsWith(".rob")) {
        name += ".rob";
    }

    int metadataId = getMetadataId(name);
    if (metadataId <= 0) {
        return false;
    }

    QStringList updates;
    QVariantList params;

    if (height >= 0) {
        updates.append("height = ?");
        params.append(height);
    }
    if (weight >= 0) {
        updates.append("weight = ?");
        params.append(weight);
    }
    if (einzelpaketLaengs >= 0) {
        updates.append("einzelpaket_laengs = ?");
        params.append(einzelpaketLaengs);
    }

    if (updates.isEmpty()) {
        return true;
    }

    params.append(metadataId);

    QSqlQuery query(m_db);
    query.prepare("UPDATE paket_dim SET " + updates.join(", ") + " WHERE metadata_id = ?");
    for (const auto& param : params) {
        query.addBindValue(param);
    }

    bool success = query.exec();
    if (success) {
        emit dataChanged();
    }
    return success;
}

std::optional<double> DatabaseManager::getBoxWeight(const QString& fileName)
{
    if (!m_open || fileName.isEmpty()) {
        return std::nullopt;
    }

    QString name = fileName;
    if (!name.endsWith(".rob")) {
        name += ".rob";
    }

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT pd.weight FROM paket_dim pd
        JOIN paletten_metadata pm ON pd.metadata_id = pm.id
        WHERE pm.file_name = ? OR pm.file_name LIKE ?
    )");
    query.addBindValue(name);
    query.addBindValue("%" + name + "%");

    if (query.exec() && query.next()) {
        QVariant val = query.value(0);
        if (!val.isNull()) {
            return val.toDouble();
        }
    }

    return std::nullopt;
}

std::optional<int> DatabaseManager::getBoxHeight(const QString& fileName)
{
    if (!m_open || fileName.isEmpty()) {
        return std::nullopt;
    }

    QString name = fileName;
    if (!name.endsWith(".rob")) {
        name += ".rob";
    }

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT pd.height FROM paket_dim pd
        JOIN paletten_metadata pm ON pd.metadata_id = pm.id
        WHERE pm.file_name = ? OR pm.file_name LIKE ?
    )");
    query.addBindValue(name);
    query.addBindValue("%" + name + "%");

    if (query.exec() && query.next()) {
        QVariant val = query.value(0);
        if (!val.isNull()) {
            return val.toInt();
        }
    }

    return std::nullopt;
}

std::optional<bool> DatabaseManager::getEinzelpaketLaengs(const QString& fileName)
{
    if (!m_open || fileName.isEmpty()) {
        return std::nullopt;
    }

    QString name = fileName;
    if (!name.endsWith(".rob")) {
        name += ".rob";
    }

    QSqlQuery query(m_db);
    query.prepare(R"(
        SELECT pd.einzelpaket_laengs FROM paket_dim pd
        JOIN paletten_metadata pm ON pd.metadata_id = pm.id
        WHERE pm.file_name = ? OR pm.file_name LIKE ?
    )");
    query.addBindValue(name);
    query.addBindValue("%" + name + "%");

    if (query.exec() && query.next()) {
        QVariant val = query.value(0);
        if (!val.isNull()) {
            return val.toInt() != 0;
        }
    }

    return std::nullopt;
}

bool DatabaseManager::deletePalette(const QString& fileName)
{
    if (!m_open || fileName.isEmpty()) {
        return false;
    }

    int metadataId = getMetadataId(fileName);
    if (metadataId <= 0) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare("DELETE FROM paletten_metadata WHERE id = ?");
    query.addBindValue(metadataId);

    bool success = query.exec();
    if (success) {
        emit dataChanged();
    }
    return success;
}

int DatabaseManager::getMetadataId(const QString& fileName)
{
    QSqlQuery query(m_db);
    query.prepare("SELECT id FROM paletten_metadata WHERE file_name = ? OR file_name LIKE ?");
    query.addBindValue(fileName);
    query.addBindValue("%" + fileName + "%");

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    return -1;
}

} // namespace database
} // namespace multipack
