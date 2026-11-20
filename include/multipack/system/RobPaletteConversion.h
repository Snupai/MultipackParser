/**
 * @file RobPaletteConversion.h
 * @brief Conversion from parsed .rob data to the database palette model
 */
#ifndef MULTIPACK_SYSTEM_ROBPALETTECONVERSION_H
#define MULTIPACK_SYSTEM_ROBPALETTECONVERSION_H

#include "multipack/database/DatabaseManager.h"
#include "multipack/system/RobFileParser.h"

namespace multipack {
namespace system {

/**
 * @brief Convert parsed .rob data into the structure stored in SQLite
 *
 * Shared by the USB import and the planner sync so both store plans the same
 * way. The metadata file name is the file name part of RobFileData::filePath
 * and the timestamp is RobFileData::fileTimestamp; callers may override both.
 */
database::PaletteData robFileToPaletteData(const RobFileData& fileData);

} // namespace system
} // namespace multipack

#endif // MULTIPACK_SYSTEM_ROBPALETTECONVERSION_H
