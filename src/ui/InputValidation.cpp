/**
 * @file InputValidation.cpp
 * @brief Implementation of input validation utilities
 */

#include "multipack/ui/InputValidation.h"

#include <QDebug>
#include <QFile>
#include <QDir>
#include <QRegularExpression>
#include <QTimer>
#include <limits>

namespace multipack {
namespace ui {

namespace InputValidation {

ValidationResult notEmpty(const QString& value)
{
    ValidationResult result;
    if (value.trimmed().isEmpty()) {
        result.valid = false;
        result.errorMessage = QObject::tr("This field cannot be empty");
    }
    return result;
}

ValidationResult isInteger(const QString& value, int min, int max)
{
    ValidationResult result;
    bool ok = false;
    int intValue = value.toInt(&ok);

    if (!ok) {
        result.valid = false;
        result.errorMessage = QObject::tr("Please enter a valid integer");
        return result;
    }

    if (intValue < min || intValue > max) {
        result.valid = false;
        result.errorMessage = QObject::tr("Value must be between %1 and %2")
                                  .arg(min).arg(max);
    }

    return result;
}

ValidationResult isDouble(const QString& value, double min, double max)
{
    ValidationResult result;
    bool ok = false;
    double doubleValue = value.toDouble(&ok);

    if (!ok) {
        result.valid = false;
        result.errorMessage = QObject::tr("Please enter a valid number");
        return result;
    }

    if (doubleValue < min || doubleValue > max) {
        result.valid = false;
        result.errorMessage = QObject::tr("Value must be between %1 and %2")
                                  .arg(min).arg(max);
    }

    return result;
}

ValidationResult isPositive(const QString& value)
{
    ValidationResult result;
    bool ok = false;
    double doubleValue = value.toDouble(&ok);

    if (!ok) {
        result.valid = false;
        result.errorMessage = QObject::tr("Please enter a valid number");
        return result;
    }

    if (doubleValue <= 0) {
        result.valid = false;
        result.errorMessage = QObject::tr("Value must be positive");
    }

    return result;
}

ValidationResult isIpAddress(const QString& value)
{
    ValidationResult result;

    // IPv4 pattern
    QRegularExpression ipv4Pattern(
        R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)"
    );

    if (!ipv4Pattern.match(value).hasMatch()) {
        result.valid = false;
        result.errorMessage = QObject::tr("Please enter a valid IP address (e.g., 192.168.0.1)");
    }

    return result;
}

ValidationResult isPort(const QString& value)
{
    ValidationResult result;
    bool ok = false;
    int port = value.toInt(&ok);

    if (!ok || port < 1 || port > 65535) {
        result.valid = false;
        result.errorMessage = QObject::tr("Port must be between 1 and 65535");
    }

    return result;
}

ValidationResult fileExists(const QString& value)
{
    ValidationResult result;

    if (!QFile::exists(value)) {
        result.valid = false;
        result.errorMessage = QObject::tr("File does not exist");
    }

    return result;
}

ValidationResult directoryExists(const QString& value)
{
    ValidationResult result;

    QDir dir(value);
    if (!dir.exists()) {
        result.valid = false;
        result.errorMessage = QObject::tr("Directory does not exist");
    }

    return result;
}

ValidationResult passwordStrength(const QString& value, int minLength)
{
    ValidationResult result;

    if (value.length() < minLength) {
        result.valid = false;
        result.errorMessage = QObject::tr("Password must be at least %1 characters")
                                  .arg(minLength);
        return result;
    }

    // Check for at least one letter and one number
    bool hasLetter = false;
    bool hasNumber = false;

    for (const QChar& c : value) {
        if (c.isLetter()) hasLetter = true;
        if (c.isDigit()) hasNumber = true;
    }

    if (!hasLetter || !hasNumber) {
        result.valid = false;
        result.errorMessage = QObject::tr("Password must contain both letters and numbers");
    }

    return result;
}

ValidationResult isValidBoxHeight(const QString& value)
{
    ValidationResult result;
    bool ok = false;
    int height = value.toInt(&ok);

    if (!ok) {
        result.valid = false;
        result.errorMessage = QObject::tr("Please enter a valid height");
        return result;
    }

    // Height range: 0-600mm (matching Python implementation)
    if (height < 0 || height > 600) {
        result.valid = false;
        result.errorMessage = QObject::tr("Height must be between 0 and 600 mm");
    }

    return result;
}

ValidationResult isValidBoxWeight(const QString& value)
{
    ValidationResult result;
    bool ok = false;
    double weight = value.toDouble(&ok);

    if (!ok) {
        result.valid = false;
        result.errorMessage = QObject::tr("Please enter a valid weight");
        return result;
    }

    // Weight range: 0-15kg (matching Python implementation)
    if (weight < 0.0 || weight > 15.0) {
        result.valid = false;
        result.errorMessage = QObject::tr("Weight must be between 0 and 15 kg");
    }

    return result;
}

double calculateEstimatedWeight(int length, int width, int height)
{
    // Empirical formula from Python implementation
    // Based on volume with density factor
    double volumeCm3 = (length / 10.0) * (width / 10.0) * (height / 10.0);
    // Assuming average density of ~0.3 g/cm³ for cardboard boxes
    double estimatedKg = volumeCm3 * 0.0003;
    // Clamp to reasonable range
    return qBound(0.1, estimatedKg, 15.0);
}

} // namespace InputValidation

// =============================================================================
// DimensionInputHandler Implementation
// =============================================================================

class DimensionInputHandler::Private
{
public:
    QTimer* debounceTimer = nullptr;
    QTimer* revertTimer = nullptr;

    int debounceDelayMs = 2000;
    int revertTimeoutMs = 6900;

    QString pendingHeight;
    QString pendingWeight;
    bool heightPending = false;
    bool weightPending = false;

    int originalHeight = 0;
    double originalWeight = 0.0;
};

DimensionInputHandler::DimensionInputHandler(QObject* parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->debounceTimer = new QTimer(this);
    d->debounceTimer->setSingleShot(true);
    connect(d->debounceTimer, &QTimer::timeout, this, &DimensionInputHandler::onDebounceTimeout);

    d->revertTimer = new QTimer(this);
    d->revertTimer->setSingleShot(true);
    connect(d->revertTimer, &QTimer::timeout, this, &DimensionInputHandler::onRevertTimeout);
}

DimensionInputHandler::~DimensionInputHandler() = default;

void DimensionInputHandler::setDebounceDelay(int ms)
{
    d->debounceDelayMs = ms;
}

void DimensionInputHandler::setRevertTimeout(int ms)
{
    d->revertTimeoutMs = ms;
}

void DimensionInputHandler::onHeightChanged(const QString& newValue)
{
    // Validate input
    auto result = InputValidation::isValidBoxHeight(newValue);
    if (!result.valid) {
        emit validationError(result.errorMessage);
        return;
    }

    // Store original value on first change
    if (!d->heightPending) {
        d->originalHeight = newValue.toInt();
    }

    d->pendingHeight = newValue;
    d->heightPending = true;

    // Restart debounce timer
    d->debounceTimer->start(d->debounceDelayMs);
}

void DimensionInputHandler::onWeightChanged(const QString& newValue)
{
    // Validate input
    auto result = InputValidation::isValidBoxWeight(newValue);
    if (!result.valid) {
        emit validationError(result.errorMessage);
        return;
    }

    // Store original value on first change
    if (!d->weightPending) {
        d->originalWeight = newValue.toDouble();
    }

    d->pendingWeight = newValue;
    d->weightPending = true;

    // Restart debounce timer
    d->debounceTimer->start(d->debounceDelayMs);
}

void DimensionInputHandler::confirmChanges()
{
    d->revertTimer->stop();

    if (d->heightPending) {
        int height = d->pendingHeight.toInt();
        emit heightConfirmed(height);
        d->heightPending = false;
    }

    if (d->weightPending) {
        double weight = d->pendingWeight.toDouble();
        emit weightConfirmed(weight);
        d->weightPending = false;
    }

    d->pendingHeight.clear();
    d->pendingWeight.clear();
}

void DimensionInputHandler::cancelChanges()
{
    d->debounceTimer->stop();
    d->revertTimer->stop();

    d->heightPending = false;
    d->weightPending = false;
    d->pendingHeight.clear();
    d->pendingWeight.clear();

    emit changesReverted();
}

bool DimensionInputHandler::hasPendingChanges() const
{
    return d->heightPending || d->weightPending;
}

void DimensionInputHandler::onDebounceTimeout()
{
    // Debounce period completed, request confirmation
    QString message;

    if (d->heightPending && d->weightPending) {
        message = QObject::tr("Confirm height (%1 mm) and weight (%2 kg) changes?")
                      .arg(d->pendingHeight)
                      .arg(d->pendingWeight);
    } else if (d->heightPending) {
        message = QObject::tr("Confirm height change to %1 mm?")
                      .arg(d->pendingHeight);
    } else if (d->weightPending) {
        message = QObject::tr("Confirm weight change to %1 kg?")
                      .arg(d->pendingWeight);
    }

    if (!message.isEmpty()) {
        emit confirmationNeeded(message);
        // Start revert timer
        d->revertTimer->start(d->revertTimeoutMs);
    }
}

void DimensionInputHandler::onRevertTimeout()
{
    // Revert timeout - cancel changes
    qDebug() << "DimensionInputHandler: Revert timeout - canceling changes";
    cancelChanges();
}

// NumericValidator implementation
NumericValidator::NumericValidator(double bottom, double top, int decimals,
                                   QObject* parent)
    : QDoubleValidator(bottom, top, decimals, parent)
{
    setNotation(QDoubleValidator::StandardNotation);
}

QValidator::State NumericValidator::validate(QString& input, int& pos) const
{
    // Allow empty input and minus sign during typing
    if (input.isEmpty() || input == "-") {
        return QValidator::Intermediate;
    }

    return QDoubleValidator::validate(input, pos);
}

// IpAddressValidator implementation
IpAddressValidator::IpAddressValidator(QObject* parent)
    : QValidator(parent)
{
}

QValidator::State IpAddressValidator::validate(QString& input, int& pos) const
{
    Q_UNUSED(pos);

    if (input.isEmpty()) {
        return QValidator::Intermediate;
    }

    // Check for valid characters
    QRegularExpression validChars("^[0-9.]*$");
    if (!validChars.match(input).hasMatch()) {
        return QValidator::Invalid;
    }

    // Split by dots
    QStringList parts = input.split('.');

    if (parts.size() > 4) {
        return QValidator::Invalid;
    }

    for (const QString& part : parts) {
        if (part.isEmpty()) {
            continue;  // Allow intermediate state with trailing dot
        }

        bool ok = false;
        int value = part.toInt(&ok);

        if (!ok || value < 0 || value > 255) {
            return QValidator::Invalid;
        }

        // No leading zeros (except for "0" itself)
        if (part.length() > 1 && part.startsWith('0')) {
            return QValidator::Invalid;
        }
    }

    // Check if complete and valid
    if (parts.size() == 4) {
        bool allComplete = true;
        for (const QString& part : parts) {
            if (part.isEmpty()) {
                allComplete = false;
                break;
            }
        }
        if (allComplete) {
            return QValidator::Acceptable;
        }
    }

    return QValidator::Intermediate;
}

} // namespace ui
} // namespace multipack
