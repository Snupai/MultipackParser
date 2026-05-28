/**
 * @file DatabaseModels.h
 * @brief Database table model definitions
 *
 * Defines the structure of database tables and
 * provides SQL generation utilities.
 */

#ifndef MULTIPACK_DATABASE_DATABASEMODELS_H
#define MULTIPACK_DATABASE_DATABASEMODELS_H

#include <QString>
#include <QStringList>

namespace multipack {
namespace database {

/**
 * @namespace Tables
 * @brief Database table name constants
 */
namespace Tables {
    constexpr const char* PALETTEN_METADATA = "paletten_metadata";
    constexpr const char* PALETTE_DIMENSIONS = "palette_dimensions";
    constexpr const char* PACKAGE_DIMENSIONS = "package_dimensions";
    constexpr const char* LAYER_TYPES = "layer_types";
    constexpr const char* LAYER_POSITIONS = "layer_positions";
    constexpr const char* SCHEMA_VERSION = "schema_version";
}

/**
 * @namespace Columns
 * @brief Common column name constants
 */
namespace Columns {
    constexpr const char* ID = "id";
    constexpr const char* METADATA_ID = "metadata_id";
    constexpr const char* FILE_NAME = "file_name";
    constexpr const char* CREATED_AT = "created_at";
    constexpr const char* UPDATED_AT = "updated_at";
}

/**
 * @namespace Schema
 * @brief SQL schema definitions
 */
namespace Schema {

/**
 * @brief Get CREATE TABLE statement for paletten_metadata
 */
inline QString createPalettenMetadata()
{
    return R"(
        CREATE TABLE IF NOT EXISTS paletten_metadata (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            file_name TEXT NOT NULL UNIQUE,
            number_of_layers INTEGER DEFAULT 0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
}

/**
 * @brief Get CREATE TABLE statement for palette_dimensions
 */
inline QString createPaletteDimensions()
{
    return R"(
        CREATE TABLE IF NOT EXISTS palette_dimensions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            metadata_id INTEGER NOT NULL,
            length REAL DEFAULT 0,
            width REAL DEFAULT 0,
            height REAL DEFAULT 0,
            overhang REAL DEFAULT 0,
            FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
        )
    )";
}

/**
 * @brief Get CREATE TABLE statement for package_dimensions
 */
inline QString createPackageDimensions()
{
    return R"(
        CREATE TABLE IF NOT EXISTS package_dimensions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            metadata_id INTEGER NOT NULL,
            length REAL DEFAULT 0,
            width REAL DEFAULT 0,
            height REAL DEFAULT 0,
            weight REAL DEFAULT 0,
            FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
        )
    )";
}

/**
 * @brief Get CREATE TABLE statement for layer_types
 */
inline QString createLayerTypes()
{
    return R"(
        CREATE TABLE IF NOT EXISTS layer_types (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            metadata_id INTEGER NOT NULL,
            layer_number INTEGER NOT NULL,
            layer_type INTEGER DEFAULT 0,
            FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
        )
    )";
}

/**
 * @brief Get CREATE TABLE statement for layer_positions
 */
inline QString createLayerPositions()
{
    return R"(
        CREATE TABLE IF NOT EXISTS layer_positions (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            metadata_id INTEGER NOT NULL,
            layer_number INTEGER NOT NULL,
            position_index INTEGER NOT NULL,
            x_pick REAL DEFAULT 0,
            y_pick REAL DEFAULT 0,
            angle_pick REAL DEFAULT 0,
            x_drop REAL DEFAULT 0,
            y_drop REAL DEFAULT 0,
            angle_drop REAL DEFAULT 0,
            package_count INTEGER DEFAULT 1,
            x_vector REAL DEFAULT 0,
            y_vector REAL DEFAULT 0,
            FOREIGN KEY (metadata_id) REFERENCES paletten_metadata(id) ON DELETE CASCADE
        )
    )";
}

/**
 * @brief Get CREATE TABLE statement for schema_version
 */
inline QString createSchemaVersion()
{
    return R"(
        CREATE TABLE IF NOT EXISTS schema_version (
            id INTEGER PRIMARY KEY CHECK (id = 1),
            version INTEGER NOT NULL DEFAULT 0,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
}

/**
 * @brief Get all CREATE TABLE statements
 */
inline QStringList allCreateStatements()
{
    return {
        createPalettenMetadata(),
        createPaletteDimensions(),
        createPackageDimensions(),
        createLayerTypes(),
        createLayerPositions(),
        createSchemaVersion()
    };
}

/**
 * @brief Create index for file_name lookups
 */
inline QString createFileNameIndex()
{
    return R"(
        CREATE INDEX IF NOT EXISTS idx_paletten_file_name
        ON paletten_metadata(file_name)
    )";
}

/**
 * @brief Create index for layer lookups
 */
inline QString createLayerIndex()
{
    return R"(
        CREATE INDEX IF NOT EXISTS idx_layer_positions_layer
        ON layer_positions(metadata_id, layer_number)
    )";
}

} // namespace Schema

} // namespace database
} // namespace multipack

#endif // MULTIPACK_DATABASE_DATABASEMODELS_H
