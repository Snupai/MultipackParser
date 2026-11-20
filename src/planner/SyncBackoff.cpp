/**
 * @file SyncBackoff.cpp
 * @brief Poll scheduling for the planner sync (pure logic)
 */
#include "multipack/planner/SyncBackoff.h"

#include <QRandomGenerator>

#include <algorithm>
#include <cmath>

namespace multipack {
namespace planner {

SyncBackoff::SyncBackoff()
    : SyncBackoff([]() { return 0.5 + QRandomGenerator::global()->generateDouble(); })
{
}

SyncBackoff::SyncBackoff(JitterSource jitter)
    : m_jitter(std::move(jitter))
{
}

qint64 SyncBackoff::onSuccess()
{
    m_failures = 0;
    m_currentDelayMs = POLL_INTERVAL_MS;
    return m_currentDelayMs;
}

qint64 SyncBackoff::exponentialDelay()
{
    // 10 s, 20 s, 40 s, ... for failure 1, 2, 3, ...; cap the exponent so the
    // shift never overflows during a long outage.
    const int exponent = std::min(m_failures - 1, 16);
    const qint64 base = std::min<qint64>(POLL_INTERVAL_MS << exponent, MAX_DELAY_MS);
    const double factor = std::clamp(m_jitter ? m_jitter() : 1.0, 0.5, 1.5);
    const auto jittered = static_cast<qint64>(std::llround(static_cast<double>(base) * factor));
    return std::clamp<qint64>(jittered, 1'000, MAX_DELAY_MS);
}

qint64 SyncBackoff::onTransientFailure()
{
    ++m_failures;
    m_currentDelayMs = exponentialDelay();
    return m_currentDelayMs;
}

qint64 SyncBackoff::onRateLimited(std::optional<qint64> retryAfterMs)
{
    ++m_failures;
    if (retryAfterMs.has_value()) {
        // The server knows when its window resets; honour it (at least 1 s).
        m_currentDelayMs = std::max<qint64>(*retryAfterMs, 1'000);
    } else {
        m_currentDelayMs = exponentialDelay();
    }
    return m_currentDelayMs;
}

qint64 SyncBackoff::onNotAuthorized()
{
    ++m_failures;
    m_currentDelayMs = NOT_AUTHORIZED_DELAY_MS;
    return m_currentDelayMs;
}

} // namespace planner
} // namespace multipack
