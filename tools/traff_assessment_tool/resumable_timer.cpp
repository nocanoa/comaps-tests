#include "traff_assessment_tool/resumable_timer.hpp"

namespace base
{
std::chrono::steady_clock::duration ResumableTimer::TimeElapsed() const
{
  if (m_isRunning)
    return std::chrono::steady_clock::now() - m_startTime;
  else
    return m_prevTimeElapsed;
}

void ResumableTimer::Pause()
{
  if (!m_isRunning)
    return;
  m_prevTimeElapsed = TimeElapsed();
  m_isRunning = false;
}

void ResumableTimer::Resume()
{
  if (m_isRunning)
    return;
  m_startTime = std::chrono::steady_clock::now() - m_prevTimeElapsed;
  m_prevTimeElapsed = std::chrono::steady_clock::duration::zero();
  m_isRunning = true;
}

void ResumableTimer::Reset()
{
  m_startTime = std::chrono::steady_clock::now();
  m_prevTimeElapsed = std::chrono::steady_clock::duration::zero();
}
}  // namespace base
