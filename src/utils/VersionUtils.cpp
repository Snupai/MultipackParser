/**
 * @file VersionUtils.cpp
 * @brief Implementation of semantic version helpers
 */

#include "multipack/utils/VersionUtils.h"

#include <QRegularExpression>
#include <QStringList>
#include <QVector>

namespace multipack {
namespace utils {

namespace VersionUtils {
namespace {

struct ParsedVersion {
    QVector<int> numbers;
    QStringList prerelease;
    bool hasPrerelease = false;
};

int parseNumberPart(const QString& value)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    if (ok) {
        return parsed;
    }

    const QRegularExpressionMatch match = QRegularExpression(QStringLiteral("^(\\d+)")).match(value);
    return match.hasMatch() ? match.captured(1).toInt() : 0;
}

bool isNumericIdentifier(const QString& value, int& parsed)
{
    if (value.isEmpty()) {
        return false;
    }

    bool ok = false;
    parsed = value.toInt(&ok);
    return ok;
}

ParsedVersion parse(const QString& version)
{
    QString cleaned = normalize(version);
    const int buildIndex = cleaned.indexOf('+');
    if (buildIndex >= 0) {
        cleaned = cleaned.left(buildIndex);
    }

    QString core = cleaned;
    QString prereleaseText;
    const int prereleaseIndex = cleaned.indexOf('-');
    if (prereleaseIndex >= 0) {
        core = cleaned.left(prereleaseIndex);
        prereleaseText = cleaned.mid(prereleaseIndex + 1);
    }

    ParsedVersion parsed;
    const QStringList numberParts = core.split('.', Qt::SkipEmptyParts);
    parsed.numbers.reserve(numberParts.size());
    for (const QString& part : numberParts) {
        parsed.numbers.append(parseNumberPart(part));
    }

    parsed.hasPrerelease = !prereleaseText.isEmpty();
    if (parsed.hasPrerelease) {
        parsed.prerelease = prereleaseText.split('.', Qt::SkipEmptyParts);
    }

    return parsed;
}

int comparePrerelease(const QStringList& left, const QStringList& right)
{
    const int maxLen = qMax(left.size(), right.size());
    for (int i = 0; i < maxLen; ++i) {
        if (i >= left.size()) {
            return -1;
        }
        if (i >= right.size()) {
            return 1;
        }

        int leftNumber = 0;
        int rightNumber = 0;
        const bool leftNumeric = isNumericIdentifier(left[i], leftNumber);
        const bool rightNumeric = isNumericIdentifier(right[i], rightNumber);

        if (leftNumeric && rightNumeric) {
            if (leftNumber < rightNumber) {
                return -1;
            }
            if (leftNumber > rightNumber) {
                return 1;
            }
            continue;
        }

        if (leftNumeric != rightNumeric) {
            return leftNumeric ? -1 : 1;
        }

        const int cmp = QString::compare(left[i], right[i], Qt::CaseInsensitive);
        if (cmp < 0) {
            return -1;
        }
        if (cmp > 0) {
            return 1;
        }
    }

    return 0;
}

} // namespace

QString normalize(const QString& version)
{
    QString normalized = version.trimmed();
    if (normalized.startsWith('v', Qt::CaseInsensitive)) {
        normalized = normalized.mid(1);
    }

    return normalized;
}

int compare(const QString& v1, const QString& v2)
{
    const ParsedVersion left = parse(v1);
    const ParsedVersion right = parse(v2);

    const int maxLen = qMax(left.numbers.size(), right.numbers.size());
    for (int i = 0; i < maxLen; ++i) {
        const int leftPart = (i < left.numbers.size()) ? left.numbers[i] : 0;
        const int rightPart = (i < right.numbers.size()) ? right.numbers[i] : 0;

        if (leftPart < rightPart) {
            return -1;
        }
        if (leftPart > rightPart) {
            return 1;
        }
    }

    if (left.hasPrerelease != right.hasPrerelease) {
        return left.hasPrerelease ? -1 : 1;
    }

    if (!left.hasPrerelease) {
        return 0;
    }

    return comparePrerelease(left.prerelease, right.prerelease);
}

} // namespace VersionUtils

} // namespace utils
} // namespace multipack
