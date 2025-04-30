#pragma once

#include "traffic/traffic_info.hpp"

#include "drape_frontend/drape_engine_safe_ptr.hpp"
#include "drape_frontend/traffic_generator.hpp"

#include "drape/pointers.hpp"

#include "indexer/mwm_set.hpp"

#include "openlr/openlr_decoder.hpp"

#include "geometry/point2d.hpp"
#include "geometry/polyline2d.hpp"
#include "geometry/screenbase.hpp"

#include "base/thread.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

class TrafficManager final
{
public:
  using CountryParentNameGetterFn = std::function<std::string(std::string const &)>;

  /**
   * @brief Global state of traffic information.
   */
  enum class TrafficState
  {
    /** Traffic is disabled, no traffic data will be retrieved or considered for routing. */
    Disabled,
    /** Traffic is enabled and working normally (the first request may not have been scheduled yet). */
    Enabled,
    /** At least one request is currently pending. */
    WaitingData,
    /** At least one MWM has stale traffic data. */
    Outdated,
    /** Traffic data for at least one MWM was invalid or not found on the server. */
    NoData,
    /** At least one request failed or timed out. */
    NetworkError,
    /** Traffic data could not be retrieved because the map data is outdated. */
    ExpiredData,
    /** Traffic data could not be retrieved because the app version is outdated. */
    ExpiredApp
  };

  struct MyPosition
  {
    m2::PointD m_position = m2::PointD(0.0, 0.0);
    bool m_knownPosition = false;

    MyPosition() = default;
    MyPosition(m2::PointD const & position)
      : m_position(position),
        m_knownPosition(true)
    {}
  };

  using TrafficStateChangedFn = std::function<void(TrafficState)>;
  using GetMwmsByRectFn = std::function<std::vector<MwmSet::MwmId>(m2::RectD const &)>;

  TrafficManager(CountryParentNameGetterFn const & countryParentNameGetter,
                 GetMwmsByRectFn const & getMwmsByRectFn, size_t maxCacheSizeBytes,
                 traffic::TrafficObserver & observer);
  ~TrafficManager();

  void Teardown();

  TrafficState GetState() const;
  void SetStateListener(TrafficStateChangedFn const & onStateChangedFn);

  void SetDrapeEngine(ref_ptr<df::DrapeEngine> engine);
  /**
   * @brief Sets the version of the MWM used locally.
   */
  void SetCurrentDataVersion(int64_t dataVersion);

  /**
   * @brief Enables or disables the traffic manager.
   *
   * This sets the internal state and notifies the drape engine. Enabling the traffic manager will
   * invalidate its data, disabling it will notify the observer that traffic data has been cleared.
   *
   * Calling this function with `enabled` identical to the current state is a no-op.
   *
   * @param enabled True to enable, false to disable
   */
  void SetEnabled(bool enabled);
  /**
   * @brief Whether the traffic manager is enabled.
   *
   * @return True if enabled, false if not
   */
  bool IsEnabled() const;

  void UpdateViewport(ScreenBase const & screen);
  void UpdateMyPosition(MyPosition const & myPosition);

  void Invalidate();

  void OnDestroySurface();
  void OnRecoverSurface();
  void OnMwmDeregistered(platform::LocalCountryFile const & countryFile);

  void OnEnterForeground();
  void OnEnterBackground();

  void SetSimplifiedColorScheme(bool simplified);
  bool HasSimplifiedColorScheme() const { return m_hasSimplifiedColorScheme; }

private:
  /**
   * @brief Holds information about pending or previous traffic requests pertaining to an MWM.
   */
  struct CacheEntry
  {
    CacheEntry();
    explicit CacheEntry(std::chrono::time_point<std::chrono::steady_clock> const & requestTime);

    /**
     * @brief Whether we have traffic data for this MWM.
     */
    bool m_isLoaded;

    /**
     * @brief The amount of memory occupied by the coloring for this MWM.
     */
    size_t m_dataSize;

    /**
     * @brief When the last update request occurred, not including forced updates.
     *
     * This timestamp is the basis for eliminating the oldest entries from the cache.
     */
    std::chrono::time_point<std::chrono::steady_clock> m_lastActiveTime;

    /**
     * @brief When the last update request occurred, including forced updates.
     *
     * This timestamp is the basis for determining whether an update is needed.
     */
    std::chrono::time_point<std::chrono::steady_clock> m_lastRequestTime;

    /**
     * @brief When the last response was received.
     *
     * This timestamp is the basis for determining whether a network request timed out, or if data is outdated.
     */
    std::chrono::time_point<std::chrono::steady_clock> m_lastResponseTime;

    /**
     * @brief The number of failed traffic requests for this MWM.
     *
     * Reset when the MWM becomes inactive.
     */
    int m_retriesCount;

    /**
     * @brief Whether a request is currently pending for this MWM.
     *
     * Set to `true` when a request is scheduled, reverted to `false` when a response is received or the request fails.
     */
    bool m_isWaitingForResponse;

    traffic::TrafficInfo::Availability m_lastAvailability;
  };

  /**
   * @brief Event loop for the traffic worker thread.
   *
   * This method runs an event loop, which blocks until woken up or a timeout equivalent to the
   * update interval elapses. It cycles through the list of MWMs for which updates have been
   * scheduled, triggering a network request for each and processing the result.
   */
  void ThreadRoutine();

  /**
   * @brief Blocks until a request for traffic data is received or a timeout expires.
   *
   * This method acts as the loop condition for `ThreadRoutine()`. It blocks until woken up or the
   * update interval expires. In the latter case, it calls `RequestTrafficData()` to insert all
   * currently active MWMs into the list of MWMs to update; otherwise, it leaves the list as it is.
   * In either case, it populates `mwms` with the list and returns.
   *
   * @param mwms Receives a list of MWMs for which to update traffic data.
   * @return `true` during normal operation, `false` during teardown (signaling the event loop to exit).
   */
  bool WaitForRequest(std::vector<MwmSet::MwmId> & mwms);

  void OnTrafficDataResponse(traffic::TrafficInfo && info);
  /**
   * @brief Processes a failed traffic request.
   *
   * This method gets called when a traffic request has failed.
   *
   * It updates the `m_isWaitingForResponse` and `m_lastAvailability` of `info.
   *
   * If the MWM is no longer active, this method returns immediately after that.
   *
   * If the retry limit has not been reached, the MWM is re-inserted into the list by calling
   * `RequestTrafficData(MwmSet::MwmId, bool)` with `force` set to true. Otherwise, the retry count
   * is reset and the state updated accordingly.
   *
   * @param info
   */
  void OnTrafficRequestFailed(traffic::TrafficInfo && info);

  /**
   * @brief Updates `activeMwms` and requests traffic data.
   *
   * The old and new list of active MWMs may refer either to those used by the rendering engine
   * (`m_lastDrapeMwmsByRect`/`m_activeDrapeMwms`) or to those used by the routing engine
   * (`m_lastRoutingMwmsByRect`/`m_activeRoutingMwms`).
   *
   * The method first determines the list of MWMs overlapping with `rect`. If it is identical to
   * `lastMwmsByRect`, the method returns immediately. Otherwise, it stores the new set in
   * `lastMwmsByRect` and populates `activeMwms` with the elements.
   *
   * This method locks `m_mutex` while populating `activeMwms`. There is no need for the caller to
   * do that.
   *
   * @param rect Rectangle covering the new active MWM set.
   * @param lastMwmsByRect Set of active MWMs, see description.
   * @param activeMwms Vector of active MWMs, see description.
   */
  void UpdateActiveMwms(m2::RectD const & rect, std::vector<MwmSet::MwmId> & lastMwmsByRect,
                        std::set<MwmSet::MwmId> & activeMwms);

  // This is a group of methods that haven't their own synchronization inside.

  /**
   * @brief Requests a refresh of traffic data for all currently active MWMs.
   *
   * This method is the entry point for periodic traffic data refresh operations. It cycles through
   * all active MWMs and calls `RequestTrafficData(MwmSet::MwmId, bool)` on each `MwmId`,
   * scheduling a refresh if needed. The actual network operation is performed asynchronously on a
   * separate thread.
   *
   * The method does nothing if the `TrafficManager` instance is disabled, paused, in an invalid
   * state (`NetworkError`) or if neither the rendering engine nor the routing engine have any
   * active MWMs.
   *
   * This method is unsynchronized; the caller must lock `m_mutex` prior to calling it.
   */
  void RequestTrafficData();

  /**
   * @brief Requests a refresh of traffic data for a single MWM.
   *
   * This method first checks if traffic data for the given MWM needs to be refreshed, which is the
   * case if no traffic data has ever been fetched for the given MWM, the update interval has
   * expired or `force` is true. In that case, the method inserts the `mwmId` into the list of MWMs
   * for which to update traffic and wakes up the worker thread.
   *
   * This method is unsynchronized; the caller must lock `m_mutex` prior to calling it.
   *
   * @param mwmId Describes the MWM for which traffic data is to be refreshed.
   * @param force If true, a refresh is requested even if the update interval has not expired.
   */
  void RequestTrafficData(MwmSet::MwmId const & mwmId, bool force);

  void Clear();
  void ClearCache(MwmSet::MwmId const & mwmId);
  void ShrinkCacheToAllowableSize();

  /**
   * @brief Updates the state of the traffic manager based on the state of all MWMs used by the renderer.
   *
   * This method cycles through the state of all MWMs used by the renderer (MWMs used by the
   * routing engine but not by the rendering engine are not considered), examines their traffic
   * state and sets the global state accordingly.
   *
   * For a description of states, see `TrafficState`. The order of states is as follows, the first
   * state whose conditions are fulfilled becomes the new state: `TrafficState::NetworkError`,
   * `TrafficState::WaitingData`, `TrafficState::ExpiredApp`, `TrafficState::ExpiredData`,
   * `TrafficState::NoData`, `TrafficState::Outdated`, `TrafficState::Enabled`.
   */
  void UpdateState();
  void ChangeState(TrafficState newState);

  bool IsInvalidState() const;

  void UniteActiveMwms(std::set<MwmSet::MwmId> & activeMwms) const;

  void Pause();
  void Resume();

  template <class F>
  void ForEachActiveMwm(F && f) const
  {
    std::set<MwmSet::MwmId> activeMwms;
    UniteActiveMwms(activeMwms);
    std::for_each(activeMwms.begin(), activeMwms.end(), std::forward<F>(f));
  }

  CountryParentNameGetterFn m_countryParentNameGetterFn;
  GetMwmsByRectFn m_getMwmsByRectFn;
  traffic::TrafficObserver & m_observer;

  df::DrapeEngineSafePtr m_drapeEngine;
  std::atomic<int64_t> m_currentDataVersion;

  // These fields have a flag of their initialization.
  std::pair<MyPosition, bool> m_currentPosition = {MyPosition(), false};
  std::pair<ScreenBase, bool> m_currentModelView = {ScreenBase(), false};

  std::atomic<TrafficState> m_state;
  TrafficStateChangedFn m_onStateChangedFn;

  bool m_hasSimplifiedColorScheme = true;

  size_t m_maxCacheSizeBytes;
  size_t m_currentCacheSizeBytes = 0;

  std::map<MwmSet::MwmId, CacheEntry> m_mwmCache;

  bool m_isRunning;
  std::condition_variable m_condition;

  /*
   * To determine for which MWMs we need traffic data, we need to keep track of two groups of MWMs:
   * those used by the renderer (i.e. in or just around the viewport) and those used by the routing
   * engine (i.e. those within a certain area around the route endpoints).
   *
   * Each group is stored twice: as a set and as a vector. The set always holds the MWMs which were
   * last seen in use. Both get updated together when active MWMs are added or removed. However,
   * the vector is used as a reference to detect changes. It may get cleared when the set is not,
   * which is used to invalidate the set without destroying its contents.
   *
   * Methods which use only the set:
   *
   * * RequestTrafficData(), exits if empty, otherwise cycles through the set.
   * * OnTrafficRequestFailed(), determines if an MWM is still active and the request should be retried.
   * * UniteActiveMwms(), build the list of active MWMs (used by RequestTrafficData() or to shrink the cache).
   * * UpdateState(), cycles through the set to determine the state of traffic requests (renderer only).
   *
   * Methods which use both, but in a different way:
   *
   * * ClearCache(), removes the requested MWM from the set but clears the vector completely.
   * * Invalidate(), clears the vector but not the set.
   * * UpdateActiveMwms(), uses the vector to detect changes. If so, it updates both vector and set.
   *
   * Clear() clears both the set and the vector.
   */
  std::vector<MwmSet::MwmId> m_lastDrapeMwmsByRect;
  std::set<MwmSet::MwmId> m_activeDrapeMwms;
  std::vector<MwmSet::MwmId> m_lastRoutingMwmsByRect;
  std::set<MwmSet::MwmId> m_activeRoutingMwms;

  // The ETag or entity tag is part of HTTP, the protocol for the World Wide Web.
  // It is one of several mechanisms that HTTP provides for web cache validation,
  // which allows a client to make conditional requests.
  std::map<MwmSet::MwmId, std::string> m_trafficETags;

  std::atomic<bool> m_isPaused;

  /**
   * @brief MWMs for which to retrieve traffic data.
   */
  std::vector<MwmSet::MwmId> m_requestedMwms;
  std::mutex m_mutex;

  /**
   * @brief Worker thread which fetches traffic updates.
   */
  threads::SimpleThread m_thread;
};

extern std::string DebugPrint(TrafficManager::TrafficState state);
