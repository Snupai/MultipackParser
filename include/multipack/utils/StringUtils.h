/**
 * @file StringUtils.h
 * @brief String manipulation utilities
 */

#ifndef MULTIPACK_UTILS_STRINGUTILS_H
#define MULTIPACK_UTILS_STRINGUTILS_H

#include <QString>
#include <QStringList>

namespace multipack {
namespace utils {

/**
 * @namespace StringUtils
 * @brief String manipulation utilities
 */
namespace StringUtils {

/**
 * @brief Trim whitespace from string
 * @param str Input string
 * @return Trimmed string
 */
QString trim(const QString& str);

/**
 * @brief Convert to lowercase
 * @param str Input string
 * @return Lowercase string
 */
QString toLower(const QString& str);

/**
 * @brief Convert to uppercase
 * @param str Input string
 * @return Uppercase string
 */
QString toUpper(const QString& str);

/**
 * @brief Check if string is numeric
 * @param str String to check
 * @return true if numeric
 */
bool isNumeric(const QString& str);

/**
 * @brief Check if string is integer
 * @param str String to check
 * @return true if integer
 */
bool isInteger(const QString& str);

/**
 * @brief Parse boolean from string
 * @param str String to parse
 * @param defaultValue Default if unparseable
 * @return Parsed boolean
 */
bool parseBool(const QString& str, bool defaultValue = false);

/**
 * @brief Join strings with separator
 * @param list Strings to join
 * @param separator Separator
 * @return Joined string
 */
QString join(const QStringList& list, const QString& separator = ", ");

/**
 * @brief Split string by separator
 * @param str String to split
 * @param separator Separator
 * @param skipEmpty Skip empty parts
 * @return List of parts
 */
QStringList split(const QString& str, const QString& separator,
                  bool skipEmpty = true);

/**
 * @brief Format number with fixed decimals
 * @param value Number value
 * @param decimals Decimal places
 * @return Formatted string
 */
QString formatNumber(double value, int decimals = 2);

/**
 * @brief Format file size
 * @param bytes Size in bytes
 * @return Human-readable size
 */
QString formatFileSize(qint64 bytes);

/**
 * @brief Format duration
 * @param milliseconds Duration in ms
 * @return Human-readable duration
 */
QString formatDuration(qint64 milliseconds);

/**
 * @brief Truncate string with ellipsis
 * @param str Input string
 * @param maxLength Maximum length
 * @return Truncated string
 */
QString truncate(const QString& str, int maxLength);

/**
 * @brief Remove non-printable characters
 * @param str Input string
 * @return Cleaned string
 */
QString removeNonPrintable(const QString& str);

/**
 * @brief Escape string for XML
 * @param str Input string
 * @return XML-escaped string
 */
QString escapeXml(const QString& str);

/**
 * @brief Unescape XML string
 * @param str XML-escaped string
 * @return Unescaped string
 */
QString unescapeXml(const QString& str);

} // namespace StringUtils

} // namespace utils
} // namespace multipack

#endif // MULTIPACK_UTILS_STRINGUTILS_H
