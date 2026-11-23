/**
 * @file SyncBackoff.h
 * @brief Poll scheduling for the planner sync (pure logic)
 *
 * Normal polling every 10 s. On network errors, 5xx or 429 the delay doubles
 * (10 s, 20 s, 40 s, ...) and is multiplied by a random factor in
 * [0.5, 1.5], never exceeding 5 min. A 429 Retry-After overrides the delay.
 * After "HMI nicht freigeschaltet" (401 / server_storage_disabled) the HMI
 * retries at most every 5 min. Any success resets to 10 s.
 */
#ifndef MULTIPACK_PLANNER_SYNCBACKOFF_H
#define MULTIPACK_PLANNER_SYNCBACKOFF_H

#include <QtGlobal>

#include <functional>
#include <optional>

namespace multipack {
namespace planner {

class SyncBackoff
{
public:
    static constexpr qint64 POLL_INTERVAL_MS = 10'000;
    static constexpr qint64 MAX_DELAY_MS = 5 * 60'000;
    static constexpr qint64 NOT_AUTHORIZED_DELAY_MS = 5 * 60'000;

    /// Returns a factor in [0.5, 1.5]. Injectable for tests.
    using JitterSource = std::function<double()>;

    SyncBackoff();
    explicit SyncBackoff(JitterSource jitter);

    /// Successful cycle: back to the normal interval. Returns the next delay.
    qint64 onSuccess();

    /// Network error / 5xx: exponential backoff with jitter. Returns the next delay.
    qint64 onTransientFailure();

    /// 429: Retry-After overrides the delay when present. Returns the next delay.
    qint64 onRateLimited(std::optional<qint64> retryAfterMs);

    /// 401 / server_storage_disabled: fixed 5 min. Returns the next delay.
    qint64 onNotAuthorized();

    /// Consecutive failures since the last success.
    int failureCount() const { return m_failures; }

    /// Delay computed by the last call (POLL_INTERVAL_MS initially).
    qint64 currentDelayMs() const { return m_currentDelayMs; }

private:
    qint64 exponentialDelay();

    JitterSource m_jitter;
    int m_failures = 0;
    qint64 m_currentDelayMs = POLL_INTERVAL_MS;
};

} // namespace planner
} // namespace multipack

#endif // MULTIPACK_PLANNER_SYNCBACKOFF_H
