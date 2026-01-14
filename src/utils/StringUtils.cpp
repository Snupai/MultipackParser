/**
 * @file StringUtils.cpp
 * @brief Implementation of string utilities
 */

#include "multipack/utils/StringUtils.h"

#include <QRegularExpression>
#include <QLocale>

namespace multipack {
namespace utils {

namespace StringUtils {

QString trim(const QString& str)
{
    return str.trimmed();
}

QString toLower(const QString& str)
{
    return str.toLower();
}

QString toUpper(const QString& str)
{
    return str.toUpper();
}

bool isNumeric(const QString& str)
{
    if (str.isEmpty()) {
        return false;
    }

    bool ok = false;
    str.toDouble(&ok);
    return ok;
}

bool isInteger(const QString& str)
{
    if (str.isEmpty()) {
        return false;
    }

    bool ok = false;
    str.toLongLong(&ok);
    return ok;
}

bool parseBool(const QString& str, bool defaultValue)
{
    QString lower = str.toLower().trimmed();

    if (lower == "true" || lower == "yes" || lower == "1" || lower == "on") {
        return true;
    }

    if (lower == "false" || lower == "no" || lower == "0" || lower == "off") {
        return false;
    }

    return defaultValue;
}

QString join(const QStringList& list, const QString& separator)
{
    return list.join(separator);
}

QStringList split(const QString& str, const QString& separator, bool skipEmpty)
{
    if (skipEmpty) {
        return str.split(separator, Qt::SkipEmptyParts);
    } else {
        return str.split(separator);
    }
}

QString formatNumber(double value, int decimals)
{
    return QString::number(value, 'f', decimals);
}

QString formatFileSize(qint64 bytes)
{
    if (bytes < 0) {
        return "Invalid";
    }

    const QStringList units = {"B", "KB", "MB", "GB", "TB"};
    int unitIndex = 0;
    double size = static_cast<double>(bytes);

    while (size >= 1024.0 && unitIndex < units.size() - 1) {
        size /= 1024.0;
        unitIndex++;
    }

    if (unitIndex == 0) {
        return QString("%1 %2").arg(static_cast<int>(size)).arg(units[unitIndex]);
    }

    return QString("%1 %2").arg(size, 0, 'f', 2).arg(units[unitIndex]);
}

QString formatDuration(qint64 milliseconds)
{
    if (milliseconds < 0) {
        return "Invalid";
    }

    if (milliseconds < 1000) {
        return QString("%1 ms").arg(milliseconds);
    }

    qint64 seconds = milliseconds / 1000;
    qint64 minutes = seconds / 60;
    qint64 hours = minutes / 60;

    seconds %= 60;
    minutes %= 60;

    if (hours > 0) {
        return QString("%1h %2m %3s").arg(hours).arg(minutes).arg(seconds);
    }

    if (minutes > 0) {
        return QString("%1m %2s").arg(minutes).arg(seconds);
    }

    return QString("%1s").arg(seconds);
}

QString truncate(const QString& str, int maxLength)
{
    if (str.length() <= maxLength) {
        return str;
    }

    if (maxLength <= 3) {
        return str.left(maxLength);
    }

    return str.left(maxLength - 3) + "...";
}

QString removeNonPrintable(const QString& str)
{
    QString result;
    result.reserve(str.length());

    for (const QChar& c : str) {
        if (c.isPrint() || c.isSpace()) {
            result.append(c);
        }
    }

    return result;
}

QString escapeXml(const QString& str)
{
    QString result;
    result.reserve(str.length() * 1.1);

    for (const QChar& c : str) {
        switch (c.unicode()) {
            case '&':
                result.append("&amp;");
                break;
            case '<':
                result.append("&lt;");
                break;
            case '>':
                result.append("&gt;");
                break;
            case '"':
                result.append("&quot;");
                break;
            case '\'':
                result.append("&apos;");
                break;
            default:
                result.append(c);
                break;
        }
    }

    return result;
}

QString unescapeXml(const QString& str)
{
    QString result = str;
    result.replace("&amp;", "&");
    result.replace("&lt;", "<");
    result.replace("&gt;", ">");
    result.replace("&quot;", "\"");
    result.replace("&apos;", "'");
    return result;
}

} // namespace StringUtils

} // namespace utils
} // namespace multipack
