/**
 * @file ValidationUtils.h
 * @brief Data validation utilities
 */

#ifndef MULTIPACK_UTILS_VALIDATIONUTILS_H
#define MULTIPACK_UTILS_VALIDATIONUTILS_H

#include <QString>
#include <QVariant>
#include <functional>

namespace multipack {
namespace utils {

/**
 * @namespace ValidationUtils
 * @brief Data validation utilities
 */
namespace ValidationUtils {

/**
 * @brief Validate range
 * @param value Value to check
 * @param min Minimum value
 * @param max Maximum value
 * @return true if in range [min, max]
 */
template<typename T>
bool inRange(T value, T min, T max)
{
    return value >= min && value <= max;
}

/**
 * @brief Validate positive number
 * @param value Value to check
 * @return true if positive (> 0)
 */
template<typename T>
bool isPositive(T value)
{
    return value > 0;
}

/**
 * @brief Validate non-negative number
 * @param value Value to check
 * @return true if non-negative (>= 0)
 */
template<typename T>
bool isNonNegative(T value)
{
    return value >= 0;
}

/**
 * @brief Clamp value to range
 * @param value Value to clamp
 * @param min Minimum value
 * @param max Maximum value
 * @return Clamped value
 */
template<typename T>
T clamp(T value, T min, T max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

/**
 * @brief Validate string is not empty
 * @param str String to check
 * @return true if not empty after trimming
 */
bool isNotEmpty(const QString& str);

/**
 * @brief Validate string length
 * @param str String to check
 * @param minLength Minimum length
 * @param maxLength Maximum length
 * @return true if length in range
 */
bool lengthInRange(const QString& str, int minLength, int maxLength);

/**
 * @brief Validate email format
 * @param email Email to check
 * @return true if valid email format
 */
bool isValidEmail(const QString& email);

/**
 * @brief Validate file path format
 * @param path Path to check
 * @return true if valid path format
 */
bool isValidPath(const QString& path);

/**
 * @brief Validate file extension
 * @param path File path
 * @param extension Expected extension (without dot)
 * @return true if matches
 */
bool hasExtension(const QString& path, const QString& extension);

/**
 * @brief Validate array has expected size
 * @param array Array to check
 * @param expectedSize Expected size
 * @return true if size matches
 */
template<typename T>
bool hasSize(const QVector<T>& array, int expectedSize)
{
    return array.size() == expectedSize;
}

/**
 * @brief Validate all values in array are valid
 * @param array Array to check
 * @param validator Validation function
 * @return true if all valid
 */
template<typename T>
bool allValid(const QVector<T>& array, std::function<bool(const T&)> validator)
{
    for (const T& item : array) {
        if (!validator(item)) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Validate palette dimensions
 * @param dimensions [length, width, height, overhang]
 * @return true if valid
 */
bool validatePaletteDimensions(const QVector<double>& dimensions);

/**
 * @brief Validate package dimensions
 * @param dimensions [length, width, height, weight]
 * @return true if valid
 */
bool validatePackageDimensions(const QVector<double>& dimensions);

/**
 * @brief Validate layer count
 * @param count Number of layers
 * @return true if valid (1-100)
 */
bool validateLayerCount(int count);

/**
 * @brief Validate position data
 * @param positions Position array [x, y, angle, ...]
 * @return true if valid
 */
bool validatePositions(const QVector<double>& positions);

} // namespace ValidationUtils

} // namespace utils
} // namespace multipack

#endif // MULTIPACK_UTILS_VALIDATIONUTILS_H
