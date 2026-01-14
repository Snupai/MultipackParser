/**
 * @file UR20ServerFunctions.cpp
 * @brief UR20-specific XML-RPC server functions
 *
 * Implements palette management and scanner status for UR20 robot.
 */

#include "multipack/network/UR20ServerFunctions.h"
#include "multipack/network/RpcMethodRegistry.h"
#include "multipack/core/GlobalState.h"
#include "multipack/audio/AudioManager.h"

#include <QDebug>
#include <QDateTime>

namespace multipack {
namespace network {
namespace UR20ServerFunctions {

// Helper function to mark palette as not empty and record timestamp
static void markPaletteNotEmpty(int paletteNumber)
{
    auto& state = core::GlobalState::instance();
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

    qDebug() << "UR20: Marking palette" << paletteNumber << "as not empty";

    if (paletteNumber == 1) {
        state.setUr20Palette1Empty(false);
        if (state.palette1NonEmptyTimestamp() == 0) {
            state.setPalette1NonEmptyTimestamp(currentTime);
        }
    } else if (paletteNumber == 2) {
        state.setUr20Palette2Empty(false);
        if (state.palette2NonEmptyTimestamp() == 0) {
            state.setPalette2NonEmptyTimestamp(currentTime);
        }
    } else {
        qWarning() << "UR20: Invalid palette number:" << paletteNumber;
    }
}

void registerMethods(RpcMethodRegistry* registry)
{
    if (!registry) {
        qWarning() << "UR20ServerFunctions: Registry is null";
        return;
    }

    qDebug() << "UR20ServerFunctions::registerMethods - registering UR20 methods";

    auto& state = core::GlobalState::instance();

    // UR20_scannerStatus - Set scanner status and update UI
    registry->registerMethod("UR20_scannerStatus",
        [&state](const QVector<RpcValue>& params) -> RpcValue {
            if (params.isEmpty()) {
                qWarning() << "RPC: UR20_scannerStatus - missing status parameter";
                return RpcValue(-1);
            }

            QString status = params[0].toString();
            qDebug() << "RPC: UR20_scannerStatus -" << status;

            QString previousStatus = state.previousScannerStatus();
            qint64 currentTime = QDateTime::currentMSecsSinceEpoch();

            // Update scanner status in state (this emits signal for UI update)
            state.setScannerStatus(status);

            // Track when entering safe state
            if (status == "True,True,True") {
                state.setTimestampScannerSafe(currentTime);

                // Clear scanner fault timestamp when all safe
                if (state.timestampScannerFault() != 0) {
                    qDebug() << "UR20: Scanner fault cleared";
                    state.setTimestampScannerFault(0);
                }
            } else {
                // Handle scanner fault detection - play warning if changed from safe to unsafe
                if (previousStatus == "True,True,True") {
                    qint64 lastWarning = state.lastScannerWarningTime();
                    // Only play warning if not played in last 15 seconds
                    if (lastWarning == 0 || (currentTime - lastWarning) >= 15000) {
                        qDebug() << "UR20: Scanner status changed to unsafe, would play warning";
                        // Audio playback would be triggered here via signal
                        state.setLastScannerWarningTime(currentTime);
                    }
                }
            }

            return RpcValue(0);
        },
        "Set scanner status (True,True,True format)",
        "int, string status"
    );

    // UR20_SetActivePalette - Set the active palette
    registry->registerMethod("UR20_SetActivePalette",
        [&state](const QVector<RpcValue>& params) -> RpcValue {
            if (params.isEmpty()) {
                qWarning() << "RPC: UR20_SetActivePalette - missing palette number";
                return RpcValue(404);
            }

            int paletteNumber = params[0].toInt();
            qDebug() << "RPC: UR20_SetActivePalette -" << paletteNumber;

            // Validate palette number
            if (paletteNumber != 1 && paletteNumber != 2) {
                qWarning() << "UR20: Invalid palette number:" << paletteNumber;
                return RpcValue(404);
            }

            // Check if palette is empty
            bool isEmpty = (paletteNumber == 1) ? state.ur20Palette1Empty()
                                                 : state.ur20Palette2Empty();

            if (!isEmpty) {
                qWarning() << "UR20: Cannot set palette" << paletteNumber << "as active - not empty";
                return RpcValue(503);
            }

            // Set active palette
            state.setUr20ActivePalette(paletteNumber);

            // Mark as not empty since it will be used
            markPaletteNotEmpty(paletteNumber);

            qDebug() << "UR20: Active palette set to" << paletteNumber;
            return RpcValue(paletteNumber);
        },
        "Set active palette (1 or 2)",
        "int, int palette_number"
    );

    // UR20_RequestPaletteChange - Request palette change from old to new
    registry->registerMethod("UR20_RequestPaletteChange",
        [&state](const QVector<RpcValue>& params) -> RpcValue {
            if (params.size() < 2) {
                qWarning() << "RPC: UR20_RequestPaletteChange - missing parameters";
                return RpcValue(404);
            }

            int oldPalette = params[0].toInt();
            int newPalette = params[1].toInt();
            qDebug() << "RPC: UR20_RequestPaletteChange -" << oldPalette << "->" << newPalette;

            Q_UNUSED(oldPalette);  // Only check new palette status

            // Validate new palette number
            if (newPalette != 1 && newPalette != 2) {
                qWarning() << "UR20: Invalid new palette number:" << newPalette;
                return RpcValue(404);
            }

            // Check if new palette is empty
            bool newEmpty = (newPalette == 1) ? state.ur20Palette1Empty()
                                               : state.ur20Palette2Empty();

            if (!newEmpty) {
                qWarning() << "UR20: New palette" << newPalette << "is not empty - cannot change";
                return RpcValue(0);
            }

            // Set active palette and mark as not empty
            state.setUr20ActivePalette(newPalette);
            markPaletteNotEmpty(newPalette);

            qDebug() << "UR20: Palette change approved to" << newPalette;
            return RpcValue(1);
        },
        "Request palette change",
        "int, int old_palette, int new_palette"
    );

    // UR20_GetActivePaletteNumber - Get active palette number
    registry->registerMethod("UR20_GetActivePaletteNumber",
        [&state](const QVector<RpcValue>& params) -> RpcValue {
            Q_UNUSED(params);
            qDebug() << "RPC: UR20_GetActivePaletteNumber called";

            int activePalette = state.ur20ActivePalette();

            if (activePalette == 1) {
                if (state.ur20Palette1Empty()) {
                    qDebug() << "UR20: Palette 1 is empty - returning palette 1";
                    markPaletteNotEmpty(1);
                    return RpcValue(1);
                } else {
                    qDebug() << "UR20: Palette 1 is not empty - returning 0";
                    return RpcValue(0);
                }
            } else if (activePalette == 2) {
                if (state.ur20Palette2Empty()) {
                    qDebug() << "UR20: Palette 2 is empty - returning palette 2";
                    markPaletteNotEmpty(2);
                    return RpcValue(2);
                } else {
                    qDebug() << "UR20: Palette 2 is not empty - returning 0";
                    return RpcValue(0);
                }
            }

            return RpcValue(0);
        },
        "Get active palette number",
        "int"
    );

    // UR20_GetPaletteStatus - Get palette empty status
    registry->registerMethod("UR20_GetPaletteStatus",
        [&state](const QVector<RpcValue>& params) -> RpcValue {
            if (params.isEmpty()) {
                qWarning() << "RPC: UR20_GetPaletteStatus - missing palette number";
                return RpcValue(-1);
            }

            int paletteNumber = params[0].toInt();
            qDebug() << "RPC: UR20_GetPaletteStatus -" << paletteNumber;

            if (paletteNumber == 1) {
                int status = state.ur20Palette1Empty() ? 1 : 0;
                qDebug() << "UR20: Palette 1 status:" << (status ? "empty" : "not empty");
                return RpcValue(status);
            } else if (paletteNumber == 2) {
                int status = state.ur20Palette2Empty() ? 1 : 0;
                qDebug() << "UR20: Palette 2 status:" << (status ? "empty" : "not empty");
                return RpcValue(status);
            }

            qWarning() << "UR20: Invalid palette number:" << paletteNumber;
            return RpcValue(-1);
        },
        "Get palette status (1=empty, 0=not empty, -1=invalid)",
        "int, int palette_number"
    );

    // UR20_SetZwischenLageLegen - Set intermediate layer flag
    registry->registerMethod("UR20_SetZwischenLageLegen",
        [&state](const QVector<RpcValue>& params) -> RpcValue {
            if (params.isEmpty()) {
                qWarning() << "RPC: UR20_SetZwischenLageLegen - missing parameter";
                return RpcValue(0);
            }

            bool aktiv = params[0].toBool();
            qDebug() << "RPC: UR20_SetZwischenLageLegen -" << aktiv;

            state.setUr20Zwischenlage(aktiv);

            if (aktiv) {
                qDebug() << "UR20: Zwischenlage legen und mit Reset bestätigen";
            } else {
                qDebug() << "UR20: Zwischenlage reset confirmed";
            }

            return RpcValue(1);
        },
        "Set intermediate layer flag",
        "int, bool aktiv"
    );

    // UR20_GetKlemmungAktiv - Check if clamping is active
    registry->registerMethod("UR20_GetKlemmungAktiv",
        [&state](const QVector<RpcValue>& params) -> RpcValue {
            Q_UNUSED(params);
            qDebug() << "RPC: UR20_GetKlemmungAktiv called";
            return RpcValue(state.klemmungAktiv());
        },
        "Check if clamping is active",
        "bool"
    );

    // UR20_GetScannerOverride - Get scanner override status
    registry->registerMethod("UR20_GetScannerOverride",
        [&state](const QVector<RpcValue>& params) -> RpcValue {
            Q_UNUSED(params);
            qDebug() << "RPC: UR20_GetScannerOverride called";

            QVector<bool> override = state.scannerOverride();

            // Return as array of booleans
            QVector<RpcValue> result;
            for (bool val : override) {
                result.append(RpcValue(val));
            }

            return RpcValue::fromArray(result);
        },
        "Get scanner override status [scanner1, scanner2, scanner3]",
        "array[bool]"
    );

    qDebug() << "UR20ServerFunctions: Registered 8 methods";
}

} // namespace UR20ServerFunctions
} // namespace network
} // namespace multipack
