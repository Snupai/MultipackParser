/**
 * @file InputValidation.h
 * @brief Input validation utilities for UI fields
 *
 * Provides validation functions and validators
 * for form input fields.
 */

#ifndef MULTIPACK_UI_INPUTVALIDATION_H
#define MULTIPACK_UI_INPUTVALIDATION_H

#include <QString>
#include <QValidator>
#include <functional>

namespace multipack {
namespace ui {

/**
 * @brief Validation result
 */
struct ValidationResult {
    bool valid = true;           ///< Whether validation passed
    QString errorMessage;        ///< Error message if invalid
};

/**
 * @brief Validation callback type
 */
using ValidationCallback = std::function<ValidationResult(const QString&)>;

/**
 * @namespace InputValidation
 * @brief Input validation utilities
 */
namespace InputValidation {

/**
 * @brief Validate that a string is not empty
 * @param value Value to check
 * @return Validation result
 */
ValidationResult notEmpty(const QString& value);

/**
 * @brief Validate that a string is a valid integer
 * @param value Value to check
 * @param min Minimum value (optional)
 * @param max Maximum value (optional)
 * @return Validation result
 */
ValidationResult isInteger(const QString& value, int min = INT_MIN, int max = INT_MAX);

/**
 * @brief Validate that a string is a valid double
 * @param value Value to check
 * @param min Minimum value
 * @param max Maximum value
 * @return Validation result
 */
ValidationResult isDouble(const QString& value, double min = -1e308, double max = 1e308);

/**
 * @brief Validate that a string is a valid positive number
 * @param value Value to check
 * @return Validation result
 */
ValidationResult isPositive(const QString& value);

/**
 * @brief Validate that a string is a valid IP address
 * @param value Value to check
 * @return Validation result
 */
ValidationResult isIpAddress(const QString& value);

/**
 * @brief Validate that a string is a valid port number
 * @param value Value to check
 * @return Validation result
 */
ValidationResult isPort(const QString& value);

/**
 * @brief Validate a file path exists
 * @param value Path to check
 * @return Validation result
 */
ValidationResult fileExists(const QString& value);

/**
 * @brief Validate a directory path exists
 * @param value Path to check
 * @return Validation result
 */
ValidationResult directoryExists(const QString& value);

/**
 * @brief Validate password strength
 * @param value Password to check
 * @param minLength Minimum length required
 * @return Validation result
 */
ValidationResult passwordStrength(const QString& value, int minLength = 8);

/**
 * @brief Validate box height (0-600mm)
 * @param value Height value
 * @return Validation result
 */
ValidationResult isValidBoxHeight(const QString& value);

/**
 * @brief Validate box weight (0-15kg)
 * @param value Weight value
 * @return Validation result
 */
ValidationResult isValidBoxWeight(const QString& value);

/**
 * @brief Calculate estimated weight from dimensions
 * @param length Package length in mm
 * @param width Package width in mm
 * @param height Package height in mm
 * @return Estimated weight in kg
 */
double calculateEstimatedWeight(int length, int width, int height);

} // namespace InputValidation

/**
 * @class DimensionInputHandler
 * @brief Handles debounced input changes for box dimensions
 *
 * Provides 2-second debouncing for dimension changes with
 * optional confirmation dialogs and automatic revert timers.
 */
class DimensionInputHandler : public QObject
{
    Q_OBJECT

public:
    explicit DimensionInputHandler(QObject* parent = nullptr);
    ~DimensionInputHandler() override;

    /**
     * @brief Set debounce delay in milliseconds
     * @param ms Delay in milliseconds (default 2000)
     */
    void setDebounceDelay(int ms);

    /**
     * @brief Set revert timeout in milliseconds
     * @param ms Timeout in milliseconds (default 6900)
     */
    void setRevertTimeout(int ms);

    /**
     * @brief Handle height value change
     * @param newValue New height value
     */
    void onHeightChanged(const QString& newValue);

    /**
     * @brief Handle weight value change
     * @param newValue New weight value
     */
    void onWeightChanged(const QString& newValue);

    /**
     * @brief Confirm pending changes
     */
    void confirmChanges();

    /**
     * @brief Cancel pending changes
     */
    void cancelChanges();

    /**
     * @brief Check if changes are pending confirmation
     */
    bool hasPendingChanges() const;

signals:
    /**
     * @brief Emitted when height should be applied
     * @param height New height value
     */
    void heightConfirmed(int height);

    /**
     * @brief Emitted when weight should be applied
     * @param weight New weight value
     */
    void weightConfirmed(double weight);

    /**
     * @brief Emitted when changes should be reverted
     */
    void changesReverted();

    /**
     * @brief Emitted when confirmation is needed
     * @param message Message to display
     */
    void confirmationNeeded(const QString& message);

    /**
     * @brief Emitted on validation error
     * @param message Error message
     */
    void validationError(const QString& message);

private slots:
    void onDebounceTimeout();
    void onRevertTimeout();

private:
    class Private;
    std::unique_ptr<Private> d;
};

/**
 * @class NumericValidator
 * @brief QValidator for numeric input with range checking
 */
class NumericValidator : public QDoubleValidator
{
    Q_OBJECT

public:
    /**
     * @brief Construct a numeric validator
     * @param bottom Minimum value
     * @param top Maximum value
     * @param decimals Decimal places
     * @param parent Parent object
     */
    NumericValidator(double bottom, double top, int decimals,
                    QObject* parent = nullptr);

    /**
     * @brief Validate input
     * @param input Input string (modified in place)
     * @param pos Cursor position
     * @return Validation state
     */
    State validate(QString& input, int& pos) const override;
};

/**
 * @class IpAddressValidator
 * @brief QValidator for IP address input
 */
class IpAddressValidator : public QValidator
{
    Q_OBJECT

public:
    /**
     * @brief Construct an IP address validator
     * @param parent Parent object
     */
    explicit IpAddressValidator(QObject* parent = nullptr);

    /**
     * @brief Validate input
     * @param input Input string
     * @param pos Cursor position
     * @return Validation state
     */
    State validate(QString& input, int& pos) const override;
};

} // namespace ui
} // namespace multipack

#endif // MULTIPACK_UI_INPUTVALIDATION_H
