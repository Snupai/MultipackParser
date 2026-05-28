/**
 * @file ValidationUtils.cpp
 * @brief Implementation of validation utilities
 */

#include "multipack/utils/ValidationUtils.h"

#include <QFileInfo>
#include <QRegularExpression>

namespace multipack {
namespace utils {

namespace ValidationUtils {

bool isNotEmpty(const QString& str)
{
    return !str.trimmed().isEmpty();
}

bool lengthInRange(const QString& str, int minLength, int maxLength)
{
    int len = str.length();
    return len >= minLength && len <= maxLength;
}

bool isValidEmail(const QString& email)
{
    // Basic email validation
    QRegularExpression regex(
        R"(^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$)"
    );
    return regex.match(email).hasMatch();
}

bool isValidPath(const QString& path)
{
    if (path.isEmpty()) {
        return false;
    }

    // Check for invalid characters
    QRegularExpression invalidChars;
#ifdef Q_OS_WIN
    invalidChars.setPattern(R"([<>:"|?*])");
#else
    invalidChars.setPattern(R"([\0])");  // Only null character is invalid on Unix
#endif

    return !invalidChars.match(path).hasMatch();
}

bool hasExtension(const QString& path, const QString& extension)
{
    QFileInfo info(path);
    return info.suffix().compare(extension, Qt::CaseInsensitive) == 0;
}

bool validatePaletteDimensions(const QVector<double>& dimensions)
{
    // Expected: [length, width, height, overhang]
    if (dimensions.size() != 4) {
        return false;
    }

    // Length and width must be positive
    if (dimensions[0] <= 0 || dimensions[1] <= 0) {
        return false;
    }

    // Height must be non-negative (0 is allowed for empty palette)
    if (dimensions[2] < 0) {
        return false;
    }

    // Overhang must be non-negative
    if (dimensions[3] < 0) {
        return false;
    }

    // Sanity checks for reasonable values (in mm)
    if (dimensions[0] > 5000 || dimensions[1] > 5000) {
        return false;  // Palette too large
    }

    if (dimensions[2] > 3000) {
        return false;  // Height too tall
    }

    return true;
}

bool validatePackageDimensions(const QVector<double>& dimensions)
{
    // Expected: [length, width, height, weight]
    if (dimensions.size() != 4) {
        return false;
    }

    // All dimensions must be positive
    if (dimensions[0] <= 0 || dimensions[1] <= 0 || dimensions[2] <= 0) {
        return false;
    }

    // Weight must be non-negative
    if (dimensions[3] < 0) {
        return false;
    }

    // Sanity checks for reasonable values
    if (dimensions[0] > 2000 || dimensions[1] > 2000 || dimensions[2] > 2000) {
        return false;  // Package too large
    }

    if (dimensions[3] > 100) {
        return false;  // Package too heavy (100 kg max)
    }

    return true;
}

bool validateLayerCount(int count)
{
    return count >= 1 && count <= 100;
}

bool validatePositions(const QVector<double>& positions)
{
    // Positions should be in groups of 9:
    // [x_pick, y_pick, angle_pick, x_drop, y_drop, angle_drop, count, x_vec, y_vec]
    if (positions.size() % 9 != 0) {
        return false;
    }

    // Check each position group
    for (int i = 0; i < positions.size(); i += 9) {
        // Angles should be normalized (-360 to 360)
        if (positions[i + 2] < -360 || positions[i + 2] > 360) {
            return false;
        }
        if (positions[i + 5] < -360 || positions[i + 5] > 360) {
            return false;
        }

        // Package count should be positive
        if (positions[i + 6] < 1) {
            return false;
        }
    }

    return true;
}

} // namespace ValidationUtils

} // namespace utils
} // namespace multipack
