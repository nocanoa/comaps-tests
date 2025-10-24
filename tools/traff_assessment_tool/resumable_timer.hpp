#pragma once

#include <chrono>

namespace base
{
/**
 * @brief A timer which can be paused and resumed.
 *
 * On creation, the timer is in paused state.
 *
 * Elapsed time can be queried in any state (running or paused). In running state, it will return
 * the currently elapsed time, and the result will increase with each subsequent call. In paused
 * state, it will return the time elapsed when the timer was last paused, and the result is stable
 * between calls as long as the timer remains paused.
 */
class ResumableTimer
{
public:
  std::chrono::steady_clock::duration TimeElapsed() const;

  template <typename Duration>
  Duration TimeElapsedAs() const
  {
    return std::chrono::duration_cast<Duration>(TimeElapsed());
  }

  double ElapsedSeconds() const { return TimeElapsedAs<std::chrono::duration<double>>().count(); }
  uint64_t ElapsedMilliseconds() const { return TimeElapsedAs<std::chrono::milliseconds>().count(); }
  uint64_t ElapsedNanoseconds() const { return TimeElapsedAs<std::chrono::nanoseconds>().count(); }

  /**
   * @brief Pauses the timer, i.e. freezes its current value and stops it from advancing further.
   *
   * Pausing an already paused timer is a no-op.
   */
  void Pause();

  /**
   * @brief Resumes the timer, i.e. causes it to advance further from its previous value.
   *
   * Resuming a running timer is a no-op.
   */
  void Resume();

  /**
   * @brief Resets the timer to zero.
   *
   * Resetting the timer will not change its paused/running state. That is, resetting a running
   * timer will cause it to count upward from zero immediately, whereas resetting a paused timer
   * will set its value to zero and keep it there until resumed.
   */
  void Reset();
private:
  /**
   * @brief The time at which the timer was last started.
   *
   * Valid only if the timer is running.
   *
   * When first starting a timer after it has been created or reset, this value is set to current
   * time. When resuming a timer, this value us set to current time minus `m_prevTimeElapsed`, so
   * that the timer value for a currently running timer is always the difference between this value
   * and current time.
   */
  std::chrono::steady_clock::time_point m_startTime;

  /**
   * @brief Whether the timer is currently running.
   */
  bool m_isRunning = false;

  /**
   * @brief The time elapsed when the timer was last paused.
   *
   * Valid only if the timer is paused.
   *
   * Initially zero, reset to zero when the timer is reset or resumed.
   */
  std::chrono::steady_clock::duration m_prevTimeElapsed = std::chrono::steady_clock::duration::zero();
};
}  // namespace base
