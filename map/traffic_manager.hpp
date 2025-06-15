#pragma once

#include "traffic/traffic_info.hpp"

#include "drape_frontend/drape_engine_safe_ptr.hpp"
#include "drape_frontend/traffic_generator.hpp"

#include "drape/pointers.hpp"

#include "indexer/mwm_set.hpp"

#include "storage/country_info_getter.hpp"

#include "traffxml/traff_decoder.hpp"
#include "traffxml/traff_model.hpp"

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
  using CountryInfoGetterFn = std::function<storage::CountryInfoGetter const &()>;
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

  /**
   * @brief The mode for the traffic manager.
   *
   * Future versions may introduce further test modes. Therefore, always use `TrafficManager::IsTestMode()`
   * to verify if the traffic manager is running in test mode.
   */
  enum class Mode
  {
    /**
     * Traffic manager mode for normal operation.
     *
     * This is the default mode unless something else is explicitly set.
     */
    Normal,
    /**
     * Test mode.
     *
     * This mode will prevent the traffic manager from automatically subscribing to sources and
     * polling them. It will still receive and process push feeds.
     *
     * Future versions may introduce further behavior changes, and/or introduce more test modes.
     */
    Test
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

  TrafficManager(DataSource & dataSource,
                 CountryInfoGetterFn countryInfoGetter,
                 CountryParentNameGetterFn const & countryParentNameGetter,
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
   * This sets the internal state and notifies the drape engine.
   *
   * Upon creation, the traffic manager is disabled and will not poll any sources or process any
   * feeds until enabled. Feeds received through `Push()` will be added to the queue before the
   * traffic manager is started, but will not be processed any further until the traffic manager is
   * started.
   *
   * MWMs must be loaded before first enabling the traffic manager.
   *
   * Calling this function with `enabled` identical to the current state is a no-op.
   *
   * @todo Currently, all MWMs must be loaded before calling `SetEnabled()`, as MWMs loaded after
   * that will not get picked up. We need to extend `TrafficManager` to react to MWMs being added
   * (and removed) – note that this affects the `DataSource`, not the set of active MWMs.
   *
   * @todo Enabling the traffic manager will invalidate its data, disabling it will notify the
   * observer that traffic data has been cleared. This is old logic, to be reviewed/removed.
   *
   * @todo State/pause/resume logic is not fully implemented ATM and needs to be revisited.
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

  /**
   * @brief Starts the traffic manager.
   *
   */
  void Start();

  void UpdateViewport(ScreenBase const & screen);
  void UpdateMyPosition(MyPosition const & myPosition);

  /**
   * @brief Invalidates traffic information.
   *
   * Invalidating causes traffic data to be re-requested.
   *
   * This happens when a new MWM file is downloaded, the traffic manager is enabled after being
   * disabled or resumed after being paused.
   *
   * @todo this goes for the old MWM arch. For TraFF we need to refresh the MWM set for the decoder
   * and possibly decode locations again (MWMs might have changed, or new ones added).
   */
  void Invalidate();

  void OnDestroySurface();
  void OnRecoverSurface();
  void OnMwmDeregistered(platform::LocalCountryFile const & countryFile);

  void OnEnterForeground();
  void OnEnterBackground();

  void SetSimplifiedColorScheme(bool simplified);
  bool HasSimplifiedColorScheme() const { return m_hasSimplifiedColorScheme; }

  /**
   * @brief Whether the traffic manager is operating in test mode.
   */
  bool IsTestMode() { return m_mode != Mode::Normal; }

  /**
   * @brief Switches the traffic manager into test mode.
   *
   * The mode can only be set before the traffic manager is first enabled. After that, this method
   * will log a warning but otherwise do nothing.
   *
   * In test mode, the traffic manager will not subscribe to sources or poll them automatically.
   * Expired messages will not get purged automatically, but `PurgeExpiredMessages()` can be called
   * to purge expired messages once. The traffic manager will still receive and process push feeds.
   *
   * Future versions may introduce further behavior changes.
   */
  void SetTestMode();

  /**
   * @brief Processes a traffic feed received through a push operation.
   *
   * Push is safe to call from any thread.
   *
   * Push operations are not supported on all platforms.
   *
   * @param feed The traffic feed.
   */
  void Push(traffxml::TraffFeed feed);

  /**
   * @brief Purges expired messages from the cache.
   *
   * This method is safe to call from any thread.
   */
  void PurgeExpiredMessages();

  /**
   * @brief Clears the entire traffic cache.
   *
   * This is currently called when the traffic manager is enabled or disabled.
   *
   * The old MWM traffic architecture was somewhat liberal in clearing its cache and re-fetching
   * traffic data. This was possible because data was pre-processed and required no processing
   * beyond deserialization, whereas TraFF data is more expensive to recreate. Also, the old
   * architecture lacked any explicit notion of expiration; the app decided that data was to be
   * considered stale after a certain period of time. TraFF, in contrast, has an explicit expiration
   * time for each message, which can be anywhere from a few minutes to several weeks or months.
   * Messages that have expired get deleted individually.
   * For this reason, the TraFF message cache should not be cleared out under normal conditions
   * (the main exception being tests).
   *
   * @todo Currently not implemented for TraFF; implement it for test purposes but do not call when
   * the enabled state changes.
   */
  void Clear();

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
   * @brief Returns a TraFF filter list for a set of MWMs.
   *
   * @param mwms The MWMs for which a filter list is to be created.
   * @return A `filter_list` in XML format.
   */
  std::string GetMwmFilters(std::set<MwmSet::MwmId> & mwms);

  /**
   * @brief Subscribes to a traffic service.
   *
   * @param mwms The MWMs for which data is needed.
   * @return true on success, false on failure.
   */
  bool Subscribe(std::set<MwmSet::MwmId> & mwms);

  /**
   * @brief Changes an existing traffic subscription.
   *
   * @param mwms The new set of MWMs for which data is needed.
   * @return true on success, false on failure.
   */
  bool ChangeSubscription(std::set<MwmSet::MwmId> & mwms);

  /**
   * @brief Ensures we have a subscription covering all currently active MWMs.
   *
   * This method subscribes to a traffic service if not already subscribed, or changes the existing
   * subscription otherwise.
   *
   * @return true on success, false on failure.
   */
  bool SetSubscriptionArea();

  /**
   * @brief Unsubscribes from a traffic service we are subscribed to.
   */
  void Unsubscribe();

  /**
   * @brief Whether we are currently subscribed to a traffic service.
   * @return
   */
  bool IsSubscribed();

  /**
   * @brief Polls the traffic service for updates.
   *
   * @return true on success, false on failure.
   */
  bool Poll();

  /**
   * @brief Consolidates the feed queue.
   *
   * If multiple feeds in the queue have the same message ID, only the message with the newest
   * update time is kept (if two messages have the same ID and update time, the one in the feed
   * with the higher index is kept); other messages with the same ID are discarded. Empty feeds
   * are discarded.
   */
  void ConsolidateFeedQueue();

  /**
   * @brief Removes the first message from the first feed and decodes it.
   */
  void DecodeFirstMessage();

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
  // TODO mwms argument is no longer needed
  bool WaitForRequest();

  /**
   * @brief Processes new traffic data.
   *
   * The new per-MWM colorings (preprocessed traffic information) are taken from `m_allMmColoring`.
   */
  void OnTrafficDataUpdate();

// TODO no longer needed
#ifdef traffic_dead_code
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
#endif

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

  // TODO no longer needed
#ifdef traffic_dead_code
  /**
   * @brief Removes traffic data for one specific MWM from the cache.
   *
   * This would be used when an MWM file gets deregistered and its traffic data is no longer needed.
   * With the old MWM traffic architecture (pre-processed sets of segments), this method was also
   * used to shrink the cache to stay below a certain size (no longer possible with TraFF, due to
   * the data structures being more complex, and also due to re-fetching data being expensive in
   * terms of computing time).
   *
   * @param mwmId The mwmId for which to remove traffic data.
   */
  void ClearCache(MwmSet::MwmId const & mwmId);
  void ShrinkCacheToAllowableSize();
#endif

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
  void ForEachMwm(F && f) const
  {
    std::vector<std::shared_ptr<MwmInfo>> allMwmInfo;
    m_dataSource.GetMwmsInfo(allMwmInfo);
    std::for_each(allMwmInfo.begin(), allMwmInfo.end(), std::forward<F>(f));
  }

  template <class F>
  void ForEachActiveMwm(F && f) const
  {
    std::set<MwmSet::MwmId> activeMwms;
    UniteActiveMwms(activeMwms);
    std::for_each(activeMwms.begin(), activeMwms.end(), std::forward<F>(f));
  }

  DataSource & m_dataSource;
  CountryInfoGetterFn m_countryInfoGetterFn;
  CountryParentNameGetterFn m_countryParentNameGetterFn;
  GetMwmsByRectFn m_getMwmsByRectFn;
  traffic::TrafficObserver & m_observer;

  df::DrapeEngineSafePtr m_drapeEngine;
  std::atomic<int64_t> m_currentDataVersion;

  // These fields have a flag of their initialization.
  std::pair<MyPosition, bool> m_currentPosition = {MyPosition(), false};
  std::pair<ScreenBase, bool> m_currentModelView = {ScreenBase(), false};

  /**
   * The mode in which the traffic manager is running.
   */
  Mode m_mode = Mode::Normal;

  /**
   * Whether the traffic manager accepts mode changes.
   *
   * Mode cannot be set after the traffic manager has been enabled for the first time.
   */
  bool m_canSetMode = true;

  std::atomic<TrafficState> m_state;
  TrafficStateChangedFn m_onStateChangedFn;

  bool m_hasSimplifiedColorScheme = true;

// TODO no longer needed
#ifdef traffic_dead_code
  size_t m_maxCacheSizeBytes;
  size_t m_currentCacheSizeBytes = 0;
#endif

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
   * Clear() clears both the set and the vector. (Clearing the set is currently disabled as it breaks ForEachActiveMwm.)
   */
  std::vector<MwmSet::MwmId> m_lastDrapeMwmsByRect;
  std::set<MwmSet::MwmId> m_activeDrapeMwms;
  std::vector<MwmSet::MwmId> m_lastRoutingMwmsByRect;
  std::set<MwmSet::MwmId> m_activeRoutingMwms;

// TODO no longer needed
#ifdef traffic_dead_code
  // The ETag or entity tag is part of HTTP, the protocol for the World Wide Web.
  // It is one of several mechanisms that HTTP provides for web cache validation,
  // which allows a client to make conditional requests.
  std::map<MwmSet::MwmId, std::string> m_trafficETags;
#endif

  std::atomic<bool> m_isPaused;

// TODO no longer needed
#ifdef traffic_dead_code
  /**
   * @brief MWMs for which to retrieve traffic data.
   */
  std::vector<MwmSet::MwmId> m_requestedMwms;
#endif

  /**
   * @brief Mutex for access to shared members.
   *
   * Threads which access shared members (see documentation) must lock this mutex while doing so.
   */
  std::mutex m_mutex;

  /**
   * @brief Worker thread which fetches traffic updates.
   */
  threads::SimpleThread m_thread;

  /**
   * @brief When the last response was received.
   */
  std::chrono::time_point<std::chrono::steady_clock> m_lastResponseTime;

  /**
   * @brief When the last update notification to the Drape engine was posted.
   */
  std::chrono::time_point<std::chrono::steady_clock> m_lastDrapeUpdate;

  /**
   * @brief When the last update notification to the traffic observer was posted.
   */
  std::chrono::time_point<std::chrono::steady_clock> m_lastObserverUpdate;

  /**
   * @brief Whether active MWMs have changed since the last request.
   */
  bool m_activeMwmsChanged = false;

  /**
   * @brief The subscription ID received from the traffic server.
   *
   * An empty subscription ID means no subscription.
   */
  std::string m_subscriptionId;

  /**
   * @brief Whether a poll operation is needed.
   *
   * Used in the worker thread. A poll operation is needed unless a subscription (or subscription
   * change) operation was performed before and a feed was received as part of it.
   */
  bool m_isPollNeeded;

  /**
   * @brief Queue of feeds waiting to be processed.
   *
   * Threads must lock `m_mutex` before accessing `m_feedQueue`, as some platforms may receive feeds
   * on multiple threads.
   */
  std::vector<traffxml::TraffFeed> m_feedQueue;

  /**
   * @brief Cache of all currently active TraFF messages.
   *
   * Keys are message IDs, values are messages.
   *
   * Threads must lock `m_mutex` before accessing `m_messageCache`, as access can happen from
   * multiple threads (messages are added by the worker thread, `Clear()` can be called from the UI
   * thread).
   */
  std::map<std::string, traffxml::TraffMessage> m_messageCache;

  /**
   * @brief The TraFF decoder instance.
   *
   * Used to decode TraFF locations into road segments on the map.
   */
  std::unique_ptr<traffxml::DefaultTraffDecoder> m_traffDecoder;

  /**
   * @brief Map between MWM IDs and their colorings.
   *
   * Threads must lock `m_mutex` before accessing `m_allMwmColoring`, as access can happen from
   * multiple threads (messages are added by the worker thread, `Clear()` can be called from the UI
   * thread).
   */
  std::map<MwmSet::MwmId, traffic::TrafficInfo::Coloring> m_allMwmColoring;
};

extern std::string DebugPrint(TrafficManager::TrafficState state);
