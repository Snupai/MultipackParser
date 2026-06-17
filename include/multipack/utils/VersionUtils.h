/**
 * @file VersionUtils.h
 * @brief Semantic version helpers
 */

#ifndef MULTIPACK_UTILS_VERSIONUTILS_H
#define MULTIPACK_UTILS_VERSIONUTILS_H

#include <QString>

namespace multipack {
namespace utils {

namespace VersionUtils {

/**
 * @brief Normalize a version or tag string for display and comparison.
 *
 * Removes a leading v/V tag prefix and surrounding whitespace. Semantic
 * prerelease suffixes are preserved.
 */
QString normalize(const QString& version);

/**
 * @brief Compare two semantic version strings.
 *
 * Build metadata is ignored. Prerelease versions sort before their matching
 * final release, following semver precedence rules.
 *
 * @return -1 if v1 < v2, 0 if equal, 1 if v1 > v2
 */
int compare(const QString& v1, const QString& v2);

} // namespace VersionUtils

} // namespace utils
} // namespace multipack

#endif // MULTIPACK_UTILS_VERSIONUTILS_H
