#include "map/traffic_manager.hpp"

#include "routing/routing_helpers.hpp"

#include "drape_frontend/drape_engine.hpp"
#include "drape_frontend/visual_params.hpp"

#include "indexer/ftypes_matcher.hpp"
#include "indexer/scales.hpp"

#include "geometry/mercator.hpp"

#include "platform/platform.hpp"

#include "traffxml/traff_model_xml.hpp"
#include "traffxml/traff_storage.hpp"

using namespace std::chrono;

namespace
{
/**
 * Poll interval for traffic data
 */
auto constexpr kUpdateInterval = minutes(1);

/**
 * Purge interval for expired traffic messages
 */
auto constexpr kPurgeInterval = minutes(1);

auto constexpr kOutdatedDataTimeout = minutes(5) + kUpdateInterval;
auto constexpr kNetworkErrorTimeout = minutes(20);

auto constexpr kMaxRetriesCount = 5;

/**
 * Interval at which the Drape engine gets traffic updates while messages are being processed.
 */
auto constexpr kDrapeUpdateInterval = seconds(10);

/**
 * Interval at which the traffic observer gets traffic updates while messages are being processed.
 */
auto constexpr kObserverUpdateInterval = minutes(1);
} // namespace

TrafficManager::CacheEntry::CacheEntry()
  : m_isLoaded(false)
  , m_dataSize(0)
  , m_retriesCount(0)
  , m_isWaitingForResponse(false)
  , m_lastAvailability(traffic::TrafficInfo::Availability::Unknown)
{}

TrafficManager::CacheEntry::CacheEntry(time_point<steady_clock> const & requestTime)
  : m_isLoaded(false)
  , m_dataSize(0)
  , m_lastActiveTime(requestTime)
  , m_lastRequestTime(requestTime)
  , m_retriesCount(0)
  , m_isWaitingForResponse(true)
  , m_lastAvailability(traffic::TrafficInfo::Availability::Unknown)
{}

TrafficManager::TrafficManager(DataSource & dataSource, CountryInfoGetterFn countryInfoGetter,
                               const CountryParentNameGetterFn &countryParentNameGetter,
                               GetMwmsByRectFn const & getMwmsByRectFn, size_t maxCacheSizeBytes,
                               traffic::TrafficObserver & observer)
  : m_dataSource(dataSource)
  , m_countryInfoGetterFn(countryInfoGetter)
  , m_countryParentNameGetterFn(countryParentNameGetter)
  , m_getMwmsByRectFn(getMwmsByRectFn)
  , m_observer(observer)
  , m_currentDataVersion(0)
  , m_state(TrafficState::Disabled)
// TODO no longer needed
#ifdef traffic_dead_code
  , m_maxCacheSizeBytes(maxCacheSizeBytes)
#endif
  , m_isRunning(true)
  , m_isPaused(false)
  , m_thread(&TrafficManager::ThreadRoutine, this)
{
  CHECK(m_getMwmsByRectFn != nullptr, ());
}

TrafficManager::~TrafficManager()
{
#ifdef DEBUG
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    ASSERT(!m_isRunning, ());
  }
#endif
}

void TrafficManager::Teardown()
{
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_isRunning)
      return;
    m_isRunning = false;
  }
  m_condition.notify_one();
  m_thread.join();
}

TrafficManager::TrafficState TrafficManager::GetState() const
{
  return m_state;
}

void TrafficManager::SetStateListener(TrafficStateChangedFn const & onStateChangedFn)
{
  m_onStateChangedFn = onStateChangedFn;
}

void TrafficManager::SetEnabled(bool enabled)
{
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (enabled == IsEnabled())
       return;
    if (enabled && !m_traffDecoder)
      // deferred decoder initialization (requires maps to be loaded)
      m_traffDecoder = make_unique<traffxml::DefaultTraffDecoder>(m_dataSource, m_countryInfoGetterFn,
                                                                  m_countryParentNameGetterFn, m_messageCache);
    ChangeState(enabled ? TrafficState::Enabled : TrafficState::Disabled);
  }

  m_drapeEngine.SafeCall(&df::DrapeEngine::EnableTraffic, enabled);

  if (enabled)
  {
    Invalidate();
    m_canSetMode = false;
  }
  else
    m_observer.OnTrafficInfoClear();
}

void TrafficManager::Clear()
{
  // TODO what should we do about subscriptions?
  {
    std::lock_guard<std::mutex> lock(m_mutex);

    LOG(LINFO, ("Messages in cache:", m_messageCache.size()));
    LOG(LINFO, ("Feeds in queue:", m_feedQueue.size()));
    LOG(LINFO, ("MWMs with coloring:", m_allMwmColoring.size()));
    LOG(LINFO, ("MWM cache size:", m_mwmCache.size()));
    LOG(LINFO, ("Clearing..."));
    // TODO no longer needed
#ifdef traffic_dead_code
    m_currentCacheSizeBytes = 0;
#endif
    m_messageCache.clear();
    m_feedQueue.clear();
    m_mwmCache.clear();

    // TODO figure out which of the ones below we still need
    m_lastDrapeMwmsByRect.clear();
    m_lastRoutingMwmsByRect.clear();
#ifdef traffic_dead_code
    m_requestedMwms.clear();
    m_trafficETags.clear();
#endif

    LOG(LINFO, ("Messages in cache:", m_messageCache.size()));
    LOG(LINFO, ("Feeds in queue:", m_feedQueue.size()));
    LOG(LINFO, ("MWMs with coloring:", m_allMwmColoring.size()));
    LOG(LINFO, ("MWM cache size:", m_mwmCache.size()));
  }
  OnTrafficDataUpdate();
}

void TrafficManager::SetDrapeEngine(ref_ptr<df::DrapeEngine> engine)
{
  m_drapeEngine.Set(engine);
}

void TrafficManager::SetCurrentDataVersion(int64_t dataVersion)
{
  m_currentDataVersion = dataVersion;
}

void TrafficManager::OnMwmDeregistered(platform::LocalCountryFile const & countryFile)
{
  if (!IsEnabled())
    return;

// TODO no longer needed
#ifdef traffic_dead_code
  {
    std::lock_guard<std::mutex> lock(m_mutex);

    MwmSet::MwmId mwmId;
    for (auto const & cacheEntry : m_mwmCache)
    {
      if (cacheEntry.first.IsDeregistered(countryFile))
      {
        mwmId = cacheEntry.first;
        break;
      }
    }

    ClearCache(mwmId);
  }
#endif
}

void TrafficManager::OnDestroySurface()
{
  Pause();
}

void TrafficManager::OnRecoverSurface()
{
  Resume();
}

void TrafficManager::Invalidate()
{
  if (!IsEnabled())
    return;

  m_lastDrapeMwmsByRect.clear();
  m_lastRoutingMwmsByRect.clear();

  if (m_currentModelView.second)
    UpdateViewport(m_currentModelView.first);
  if (m_currentPosition.second)
    UpdateMyPosition(m_currentPosition.first);
}

void TrafficManager::UpdateActiveMwms(m2::RectD const & rect,
                                      std::vector<MwmSet::MwmId> & lastMwmsByRect,
                                      std::set<MwmSet::MwmId> & activeMwms)
{
  auto mwms = m_getMwmsByRectFn(rect);
  if (lastMwmsByRect == mwms)
    return;
  lastMwmsByRect = mwms;

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_activeMwmsChanged = true;
    activeMwms.clear();
    for (auto const & mwm : mwms)
    {
      if (mwm.IsAlive())
        activeMwms.insert(mwm);
    }
    RequestTrafficData();
  }
}

void TrafficManager::UpdateMyPosition(MyPosition const & myPosition)
{
  // Side of square around |myPosition|. Every mwm which is covered by the square
  // will get traffic info.
  double const kSquareSideM = 5000.0;
  m_currentPosition = {myPosition, true /* initialized */};

  if (!IsEnabled() || IsInvalidState() || m_isPaused)
    return;

  m2::RectD const rect =
      mercator::RectByCenterXYAndSizeInMeters(myPosition.m_position, kSquareSideM / 2.0);
  // Request traffic.
  UpdateActiveMwms(rect, m_lastRoutingMwmsByRect, m_activeRoutingMwms);

  // @TODO Do all routing stuff.
}

void TrafficManager::UpdateViewport(ScreenBase const & screen)
{
  m_currentModelView = {screen, true /* initialized */};

  if (!IsEnabled() || IsInvalidState() || m_isPaused)
    return;

  if (df::GetZoomLevel(screen.GetScale()) < df::kRoadClass0ZoomLevel)
    return;

  // Request traffic.
  UpdateActiveMwms(screen.ClipRect(), m_lastDrapeMwmsByRect, m_activeDrapeMwms);
}

std::string TrafficManager::GetMwmFilters(std::set<MwmSet::MwmId> & mwms)
{
  std::vector<m2::RectD> rects;
  for (auto mwmId : mwms)
    rects.push_back(mwmId.GetInfo()->m_bordersRect);
  return traffxml::FiltersToXml(rects);
}

// TODO make this work with multiple sources (e.g. Android)
bool TrafficManager::Subscribe(std::set<MwmSet::MwmId> & mwms)
{
  // TODO what if we’re subscribed already?
  std::string filterList = GetMwmFilters(mwms);
  // TODO
  LOG(LINFO, ("Would subscribe to:\n", filterList));
  m_subscriptionId = "placeholder_subscription_id";
  m_isPollNeeded = true; // would be false if we got a feed here
  return true;
}

// TODO make this work with multiple sources (e.g. Android)
bool TrafficManager::ChangeSubscription(std::set<MwmSet::MwmId> & mwms)
{
  if (!IsSubscribed())
    return false;
  std::string filterList = GetMwmFilters(mwms);
  // TODO
  LOG(LINFO, ("Would change subscription", m_subscriptionId, "to:\n", filterList));
  m_isPollNeeded = true; // would be false if we got a feed here
  return true;
}

bool TrafficManager::SetSubscriptionArea()
{
  std::set<MwmSet::MwmId> activeMwms;

  if (!IsSubscribed())
  {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_activeMwmsChanged = false;
      UniteActiveMwms(activeMwms);
    }
    if (!Subscribe(activeMwms))
      return false;
  }
  else if (m_activeMwmsChanged)
  {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_activeMwmsChanged = false;
      UniteActiveMwms(activeMwms);
    }
    if (!ChangeSubscription(activeMwms))
      return false;
  }
  return true;
}

// TODO make this work with multiple sources (e.g. Android)
void TrafficManager::Unsubscribe()
{
  if (!IsSubscribed())
    return;
  // TODO
  LOG(LINFO, ("Would unsubscribe from", m_subscriptionId));
  m_subscriptionId.clear();
}

bool TrafficManager::IsSubscribed()
{
  return !m_subscriptionId.empty();
}

// TODO make this work with multiple sources (e.g. Android)
// TODO deal with subscriptions rejected by the server (delete, resubscribe)
bool TrafficManager::Poll()
{
  // TODO
  //std::string fileName("test_data/traff/PL-A18-Krzyzowa-Lipiany.xml");
  std::string fileName("test_data/traff/PL-A18-Krzyzowa-Lipiany-bidir.xml");
  //std::string fileName("test_data/traff/LT-A1-Vezaiciai-Endriejavas.xml");
  traffxml::LocalStorage storage(fileName);
  pugi::xml_document document;
  auto const load_result = storage.Load(document);
  if (!load_result)
    return false;

  std::setlocale(LC_ALL, "en_US.UTF-8");
  traffxml::TraffFeed feed;
  if (traffxml::ParseTraff(document, feed))
  {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_feedQueue.push_back(feed);
    }
    m_lastResponseTime = steady_clock::now();
    m_isPollNeeded = false;
    return true;
  }
  else
  {
    LOG(LWARNING, ("An error occurred parsing the TraFF feed"));
    // TODO should we really reset m_isPollNeeded here?
    m_isPollNeeded = false;
    return false;
  }
}

void TrafficManager::Push(traffxml::TraffFeed feed)
{
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_feedQueue.push_back(feed);
    // TODO should we update m_lastResponseTime?
  }
  m_condition.notify_one();
}

void TrafficManager::PurgeExpiredMessages()
{
  PurgeExpiredMessagesImpl();
  OnTrafficDataUpdate();
}

void TrafficManager::PurgeExpiredMessagesImpl()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  LOG(LINFO, ("before:", m_messageCache.size(), "message(s)"));
  traffxml::IsoTime now = traffxml::IsoTime::Now();
  for (auto it = m_messageCache.begin(); it != m_messageCache.end(); )
  {
    if (it->second.IsExpired(now))
      it = m_messageCache.erase(it);
    else
      ++it;
  }
  LOG(LINFO, ("after:", m_messageCache.size(), "message(s)"));
}

void TrafficManager::ConsolidateFeedQueue()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (m_feedQueue.empty())
    return;
  for (size_t i = m_feedQueue.size() - 1; i <= 0; i--)
    for (size_t j = m_feedQueue.size() - 1; j <= 0; j--)
    {
      if (i == j)
        continue;
      for (auto it_i = m_feedQueue[i].begin(); it_i != m_feedQueue[i].end(); )
        for (auto it_j = m_feedQueue[j].end(); it_j != m_feedQueue[j].end(); )
          if (it_i->m_id == it_j->m_id)
          {
            // dupe, remove older
            if (it_i->m_updateTime < it_j->m_updateTime)
            {
              // standard case: i has the newer one
              ++it_i;
              it_j = m_feedQueue[j].erase(it_j);
            }
            else if (it_i->m_updateTime < it_j->m_updateTime)
            {
              // j has the newer one
              it_i = m_feedQueue[i].erase(it_i);
              ++it_j;
            }
            else if (i > j)
            {
              // same time, but feed i was received after j, keep i
              ++it_i;
              it_j = m_feedQueue[j].erase(it_j);
            }
            else
            {
              // same time, but feed j was received after i, keep j
              ASSERT(i != j, ());
              it_i = m_feedQueue[i].erase(it_i);
              ++it_j;
            }
          }
    }
  // remove empty feeds
  for (auto it = m_feedQueue.begin(); it != m_feedQueue.end(); )
    if (it->empty())
      it = m_feedQueue.erase(it);
    else
      ++it;
}

void TrafficManager::DecodeFirstMessage()
{
  traffxml::TraffMessage message;
  {
    // Lock the mutex while iterating over the feed queue
    std::lock_guard<std::mutex> lock(m_mutex);
    // remove empty feeds from the beginning of the queue
    while (!m_feedQueue.empty() && m_feedQueue.front().empty())
      m_feedQueue.erase(m_feedQueue.begin());
    // if we have no more feeds, return (nothing to do)
    if (m_feedQueue.empty())
      return;
    // retrieve the first message from the first feed, remove it from the feed
    std::swap(message, m_feedQueue.front().front());
    m_feedQueue.front().erase(m_feedQueue.front().begin());
    // if the feed has no more messages, erase it (eager erase, as an empty queue is used as a condition later)
    if (m_feedQueue.front().empty())
      m_feedQueue.erase(m_feedQueue.begin());
  }

  {
    std::lock_guard<std::mutex> lock(m_mutex);

    // check if message is actually newer
    auto it = m_messageCache.find(message.m_id);
    bool process = (it == m_messageCache.end());
    if (!process)
      process = (it->second.m_updateTime < message.m_updateTime);
    if (!process)
    {
      LOG(LINFO, ("message", message.m_id, "is already in cache, skipping"));
      return;
    }
  }

  LOG(LINFO, (" ", message.m_id, ":", message));
  m_traffDecoder->DecodeMessage(message);
  {
    std::lock_guard<std::mutex> lock(m_mutex);

    // store message in cache
    m_messageCache.insert_or_assign(message.m_id, message);
  }
  /*
   * TODO detect if we can do a quick update:
   *  - new message which does not replace any existing message
   *  - coloring “wins” over replaced message:
   *    - contains all the segments of the previous message (always true when location is the same)
   *    - speed groups are the same or lower as in previous message (always true when all members of
   *      traffic impact are unchanged or have worsened) – for this purpose, closure is considered
   *      lower than any other speed group
   *  In this case, run:
   *      traffxml::MergeMultiMwmColoring(message.m_decoded, m_allMwmColoring);
   *  Otherwise, set a flag indicating we need to process coloring in full.
   */
}

void TrafficManager::ThreadRoutine()
{
  // initially, treat last purge and drape/observer update as having just happened
  auto lastPurged = steady_clock::now();
  m_lastDrapeUpdate = steady_clock::now();
  m_lastObserverUpdate = steady_clock::now();

  while (WaitForRequest())
  {
    if (!IsEnabled())
      continue;

    if (!IsTestMode())
    {
      if (steady_clock::now() - lastPurged >= kPurgeInterval)
      {
        lastPurged == steady_clock::now();
        PurgeExpiredMessagesImpl();
      }

      LOG(LINFO, ("active MWMs changed:", m_activeMwmsChanged, ", poll needed:", m_isPollNeeded));

      // this is a no-op if active MWMs have not changed
      if (!SetSubscriptionArea())
      {
        LOG(LWARNING, ("SetSubscriptionArea failed."));
        if (!IsSubscribed())
          // do not skip out of the loop, we may need to process pushed feeds
          LOG(LWARNING, ("No subscription, no traffic data will be retrieved."));
      }

      /*
     * Fetch traffic data if needed and we have a subscription.
     * m_isPollNeeded may be set by WaitForRequest() and set/unset by SetSubscriptionArea().
     */
      if (m_isPollNeeded && IsSubscribed())
      {
        if (!Poll())
        {
          LOG(LWARNING, ("Poll failed."));
          // TODO set failed status somewhere and retry
        }
      }
    }
    LOG(LINFO, (m_feedQueue.size(), "feed(s) in queue"));

    // consolidate feed queue (remove older messages in favor of newer ones)
    ConsolidateFeedQueue();

    // decode one message and add it to the cache
    DecodeFirstMessage();

    // set new coloring for MWMs
    // `m_mutex` is obtained inside the method, no need to do it here
    // TODO drop the argument, use class member inside method
    OnTrafficDataUpdate();

// TODO no longer needed
#ifdef traffic_dead_code
    for (auto const & mwm : mwms)
    {
      if (!mwm.IsAlive())
        continue;

      traffic::TrafficInfo info(mwm, m_currentDataVersion);

      std::string tag;
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        tag = m_trafficETags[mwm];
      }

      if (info.ReceiveTrafficData(tag))
      {
        OnTrafficDataResponse(std::move(info));
      }
      else
      {
        LOG(LWARNING, ("Traffic request failed. Mwm =", mwm));
        OnTrafficRequestFailed(std::move(info));
      }

      {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_trafficETags[mwm] = tag;
      }
    }
    mwms.clear();
#endif
  }
  // Calling Unsubscribe() from the worker thread on exit makes thread synchronization easier
  Unsubscribe();
}

bool TrafficManager::WaitForRequest()
{
  std::unique_lock<std::mutex> lock(m_mutex);

  /*
   * if we got terminated, return false immediately
   * (don’t wait until sleep, we might not get much sleep if we’re busy processing a long feed)
   */
  if (!m_isRunning)
    return false;

  if (IsEnabled())
  {
    // if we have feeds in the queue, return immediately
    if (!m_feedQueue.empty())
    {
      LOG(LINFO, ("feed queue not empty, returning immediately"));
      return true;
    }

    if (!IsTestMode())
    {
      // if update interval has elapsed, return immediately
      auto const currentTime = steady_clock::now();
      auto const passedSeconds = currentTime - m_lastResponseTime;
      if (passedSeconds >= kUpdateInterval)
      {
        LOG(LINFO, ("last response was", passedSeconds, "ago, returning immediately"));
        m_isPollNeeded = true;
        return true;
      }
    }
  }

  LOG(LINFO, ("nothing to do for now, waiting for timeout or notification"));
  bool const timeout = !m_condition.wait_for(lock, kUpdateInterval, [this]
  {
    // return true for any condition we want to process immediately
    return !m_isRunning || (m_activeMwmsChanged && !IsTestMode()) || !m_feedQueue.empty();
  });

  // check again if we got terminated while waiting (or woken up because we got terminated)
  if (!m_isRunning)
    return false;

  // this works as long as wait timeout is at least equal to the poll interval
  if (IsEnabled())
    m_isPollNeeded |= timeout;

  LOG(LINFO, ("timeout:", timeout, "active MWMs changed:", m_activeMwmsChanged, "test mode:", IsTestMode()));
  return true;
}

void TrafficManager::RequestTrafficData(MwmSet::MwmId const & mwmId, bool force)
{
  bool needRequesting = false;
  auto const currentTime = steady_clock::now();
  auto const it = m_mwmCache.find(mwmId);
  if (it == m_mwmCache.end())
  {
    needRequesting = true;
    m_mwmCache.insert(std::make_pair(mwmId, CacheEntry(currentTime)));
  }
  else
  {
    auto const passedSeconds = currentTime - it->second.m_lastRequestTime;
    if (passedSeconds >= kUpdateInterval || force)
    {
      needRequesting = true;
      it->second.m_isWaitingForResponse = true;
      it->second.m_lastRequestTime = currentTime;
    }
    if (!force)
      it->second.m_lastActiveTime = currentTime;
  }

  if (needRequesting)
  {
// TODO no longer needed
#ifdef traffic_dead_code
    m_requestedMwms.push_back(mwmId);
#endif
    m_condition.notify_one();
  }
}

void TrafficManager::RequestTrafficData()
{
  if ((m_activeDrapeMwms.empty() && m_activeRoutingMwms.empty()) || !IsEnabled() ||
      IsInvalidState() || m_isPaused)
  {
    return;
  }

  ForEachActiveMwm([this](MwmSet::MwmId const & mwmId) {
    ASSERT(mwmId.IsAlive(), ());
    RequestTrafficData(mwmId, false /* force */);
  });
  UpdateState();
}

// TODO no longer needed
#ifdef traffic_dead_code
void TrafficManager::OnTrafficRequestFailed(traffic::TrafficInfo && info)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  auto it = m_mwmCache.find(info.GetMwmId());
  if (it == m_mwmCache.end())
    return;

  it->second.m_isWaitingForResponse = false;
  it->second.m_lastAvailability = info.GetAvailability();

  if (info.GetAvailability() == traffic::TrafficInfo::Availability::Unknown &&
      !it->second.m_isLoaded)
  {
    if (m_activeDrapeMwms.find(info.GetMwmId()) != m_activeDrapeMwms.cend() ||
        m_activeRoutingMwms.find(info.GetMwmId()) != m_activeRoutingMwms.cend())
    {
      if (it->second.m_retriesCount < kMaxRetriesCount)
        RequestTrafficData(info.GetMwmId(), true /* force */);
      ++it->second.m_retriesCount;
    }
    else
    {
      it->second.m_retriesCount = 0;
    }
  }

  UpdateState();
}
#endif

void TrafficManager::OnTrafficDataUpdate()
{
  bool feedQueueEmpty = false;

  {
    std::lock_guard<std::mutex> lock(m_mutex);
    feedQueueEmpty = m_feedQueue.empty();
  }
  // Whether to notify the Drape engine of the update.
  bool notifyDrape = (feedQueueEmpty);

  // Whether to notify the observer of the update.
  bool notifyObserver = (feedQueueEmpty);

  if (!feedQueueEmpty)
  {
    auto const currentTime = steady_clock::now();
    auto const drapeAge = currentTime - m_lastDrapeUpdate;
    auto const observerAge = currentTime - m_lastObserverUpdate;
    notifyDrape = (drapeAge >= kDrapeUpdateInterval);
    notifyObserver = (observerAge >= kObserverUpdateInterval);
  }

  if (!notifyDrape && !notifyObserver)
    return;

  LOG(LINFO, ("Announcing traffic update, notifyDrape:", notifyDrape, "notifyObserver:", notifyObserver));

  /*
   * TODO introduce a flag to indicate we need to fully reprocess coloring, skip if it is false.
   * The flag would get set when messages get deleted (including any clear/purge operations),
   * or when a new message is added without indicating a simplified update in `DecodeFirstMessage()`.
   * When we reprocess coloring in full (the block below), reset this flag.
   */
  {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_allMwmColoring.clear();
    for (const auto & [id, message] : m_messageCache)
      traffxml::MergeMultiMwmColoring(message.m_decoded, m_allMwmColoring);
  }

  /*
   * Much of this code is copied and pasted together from old MWM code, with some minor adaptations:
   *
   * ForEachActiveMwm and the assertion (not the rest of the body) is from RequestTrafficData();
   * modification: cycle over all MWMs (active or not).
   * trafficCache lookup is original code.
   * TrafficInfo construction is taken fron ThreadRoutine(), with modifications (different constructor).
   * Updating m_mwmCache is from RequestTrafficData(MwmSet::MwmId const &, bool), with modifications.
   * The remainder of the loop is from OnTrafficDataResponse(traffic::TrafficInfo &&), with some modifications
   * (deciding whether to notify a component and managing timestamps is original code).
   * Existing coloring deletion (if there is no new coloring) is original code.
   */
  ForEachMwm([this, notifyDrape, notifyObserver](std::shared_ptr<MwmInfo> info) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (info->GetCountryName().starts_with(WORLD_FILE_NAME))
      return;

    MwmSet::MwmId const mwmId(info);

    ASSERT(mwmId.IsAlive(), ());
    auto tcit = m_allMwmColoring.find(mwmId);
    if (tcit != m_allMwmColoring.end())
    {
      traffic::TrafficInfo::Coloring coloring = tcit->second;
      LOG(LINFO, ("Setting new coloring for", mwmId, "with", coloring.size(), "entries"));
      traffic::TrafficInfo info(mwmId, std::move(coloring));

      m_mwmCache.try_emplace(mwmId, CacheEntry(steady_clock::now()));

      auto it = m_mwmCache.find(mwmId);
      if (it != m_mwmCache.end())
      {
        it->second.m_isLoaded = true;
        it->second.m_lastResponseTime = steady_clock::now();
        it->second.m_isWaitingForResponse = false;
        it->second.m_lastAvailability = info.GetAvailability();

        UpdateState();

        if (notifyDrape)
        {
          /*
           * TODO calling ClearTrafficCache before UpdateTraffic is a workaround for a bug in the
           * Drape engine: some segments found in the old coloring but not in the new one may get
           * left behind. This was not a problem for MapsWithMe as the set of segments never
           * changed, but is an issue wherever the segment set is dynamic. Workaround is to clear
           * before sending an update. Ultimately, the processing logic for UpdateTraffic needs to
           * be fixed, but the code is hard to read (involves multiple messages getting thrown back
           * and forth between threads).
           */
          m_drapeEngine.SafeCall(&df::DrapeEngine::ClearTrafficCache,
                                 static_cast<MwmSet::MwmId const &>(mwmId));
          m_drapeEngine.SafeCall(&df::DrapeEngine::UpdateTraffic,
                                 static_cast<traffic::TrafficInfo const &>(info));
          m_lastDrapeUpdate = steady_clock::now();
        }

        if (notifyObserver)
        {
          // Update traffic colors for routing.
          m_observer.OnTrafficInfoAdded(std::move(info));
          m_lastObserverUpdate = steady_clock::now();
        }
      }
    }
    else
    {
      if (notifyDrape)
      {
        m_drapeEngine.SafeCall(&df::DrapeEngine::ClearTrafficCache,
                               static_cast<MwmSet::MwmId const &>(mwmId));
        m_lastDrapeUpdate = steady_clock::now();
      }

      if (notifyObserver)
      {
        // Update traffic colors for routing.
        m_observer.OnTrafficInfoRemoved(mwmId);
        m_lastObserverUpdate = steady_clock::now();
      }
    }
  });
}

// TODO no longer needed
#ifdef traffic_dead_code
void TrafficManager::OnTrafficDataResponse(traffic::TrafficInfo && info)
{
  {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_mwmCache.find(info.GetMwmId());
    if (it == m_mwmCache.end())
      return;

    it->second.m_isLoaded = true;
    it->second.m_lastResponseTime = steady_clock::now();
    it->second.m_isWaitingForResponse = false;
    it->second.m_lastAvailability = info.GetAvailability();

    if (!info.GetColoring().empty())
    {
      // Update cache.
      size_t constexpr kElementSize = sizeof(traffic::TrafficInfo::RoadSegmentId) + sizeof(traffic::SpeedGroup);
      size_t const dataSize = info.GetColoring().size() * kElementSize;
      m_currentCacheSizeBytes += (dataSize - it->second.m_dataSize);
      it->second.m_dataSize = dataSize;
      ShrinkCacheToAllowableSize();
    }

    UpdateState();
  }

  if (!info.GetColoring().empty())
  {
    m_drapeEngine.SafeCall(&df::DrapeEngine::UpdateTraffic,
                           static_cast<traffic::TrafficInfo const &>(info));

    // Update traffic colors for routing.
    m_observer.OnTrafficInfoAdded(std::move(info));
  }
}
#endif

void TrafficManager::UniteActiveMwms(std::set<MwmSet::MwmId> & activeMwms) const
{
  activeMwms.insert(m_activeDrapeMwms.cbegin(), m_activeDrapeMwms.cend());
  activeMwms.insert(m_activeRoutingMwms.cbegin(), m_activeRoutingMwms.cend());
}

// TODO no longer needed
#ifdef traffic_dead_code
void TrafficManager::ShrinkCacheToAllowableSize()
{
  // Calculating number of different active mwms.
  std::set<MwmSet::MwmId> activeMwms;
  UniteActiveMwms(activeMwms);
  size_t const numActiveMwms = activeMwms.size();

  if (m_currentCacheSizeBytes > m_maxCacheSizeBytes && m_mwmCache.size() > numActiveMwms)
  {
    std::multimap<time_point<steady_clock>, MwmSet::MwmId> seenTimings;
    for (auto const & mwmInfo : m_mwmCache)
      seenTimings.insert(std::make_pair(mwmInfo.second.m_lastActiveTime, mwmInfo.first));

    auto itSeen = seenTimings.begin();
    while (m_currentCacheSizeBytes > m_maxCacheSizeBytes && m_mwmCache.size() > numActiveMwms)
    {
      ClearCache(itSeen->second);
      ++itSeen;
    }
  }
}

void TrafficManager::ClearCache(MwmSet::MwmId const & mwmId)
{
  auto const it = m_mwmCache.find(mwmId);
  if (it == m_mwmCache.end())
    return;

  if (it->second.m_isLoaded)
  {
// TODO no longer needed
#ifdef traffic_dead_code
    ASSERT_GREATER_OR_EQUAL(m_currentCacheSizeBytes, it->second.m_dataSize, ());
    m_currentCacheSizeBytes -= it->second.m_dataSize;
#endif

    m_drapeEngine.SafeCall(&df::DrapeEngine::ClearTrafficCache, mwmId);

    GetPlatform().RunTask(Platform::Thread::Gui, [this, mwmId]()
    {
      m_observer.OnTrafficInfoRemoved(mwmId);
    });
  }
  m_mwmCache.erase(it);
  m_trafficETags.erase(mwmId);
  m_activeDrapeMwms.erase(mwmId);
  m_activeRoutingMwms.erase(mwmId);
  m_lastDrapeMwmsByRect.clear();
  m_lastRoutingMwmsByRect.clear();
}
#endif

bool TrafficManager::IsEnabled() const
{
  return m_state != TrafficState::Disabled;
}

bool TrafficManager::IsInvalidState() const
{
  return m_state == TrafficState::NetworkError;
}

void TrafficManager::UpdateState()
{
  if (!IsEnabled() || IsInvalidState())
    return;

  auto const currentTime = steady_clock::now();
  auto maxPassedTime = steady_clock::duration::zero();

  bool waiting = false;
  bool networkError = false;
  bool expiredApp = false;
  bool expiredData = false;
  bool noData = false;

  for (MwmSet::MwmId const & mwmId : m_activeDrapeMwms)
  {
    auto it = m_mwmCache.find(mwmId);
    ASSERT(it != m_mwmCache.end(), ());

    if (it->second.m_isWaitingForResponse)
    {
      waiting = true;
    }
    else
    {
      expiredApp |= it->second.m_lastAvailability == traffic::TrafficInfo::Availability::ExpiredApp;
      expiredData |= it->second.m_lastAvailability == traffic::TrafficInfo::Availability::ExpiredData;
      noData |= it->second.m_lastAvailability == traffic::TrafficInfo::Availability::NoData;

      if (it->second.m_isLoaded)
      {
        auto const timeSinceLastResponse = currentTime - it->second.m_lastResponseTime;
        if (timeSinceLastResponse > maxPassedTime)
          maxPassedTime = timeSinceLastResponse;
      }
      else if (it->second.m_retriesCount >= kMaxRetriesCount)
      {
        networkError = true;
      }
    }
  }

  if (networkError || maxPassedTime >= kNetworkErrorTimeout)
    ChangeState(TrafficState::NetworkError);
  else if (waiting)
    ChangeState(TrafficState::WaitingData);
  else if (expiredApp)
    ChangeState(TrafficState::ExpiredApp);
  else if (expiredData)
    ChangeState(TrafficState::ExpiredData);
  else if (noData)
    ChangeState(TrafficState::NoData);
  else if (maxPassedTime >= kOutdatedDataTimeout)
    ChangeState(TrafficState::Outdated);
  else
    ChangeState(TrafficState::Enabled);
}

void TrafficManager::ChangeState(TrafficState newState)
{
  if (m_state == newState)
    return;

  m_state = newState;

  GetPlatform().RunTask(Platform::Thread::Gui, [this, newState]()
  {
    if (m_onStateChangedFn != nullptr)
      m_onStateChangedFn(newState);
  });
}

void TrafficManager::OnEnterForeground()
{
  Resume();
}

void TrafficManager::OnEnterBackground()
{
  Pause();
}

void TrafficManager::Pause()
{
  m_isPaused = true;
}

void TrafficManager::Resume()
{
  if (!m_isPaused)
    return;

  m_isPaused = false;
  Invalidate();
}

void TrafficManager::SetSimplifiedColorScheme(bool simplified)
{
  m_hasSimplifiedColorScheme = simplified;
  m_drapeEngine.SafeCall(&df::DrapeEngine::SetSimplifiedTrafficColors, simplified);
}

void TrafficManager::SetTestMode()
{
  if (!m_canSetMode)
  {
    LOG(LWARNING, ("Mode cannot be set once the traffic manager has been enabled"));
    return;
  }
  m_mode = Mode::Test;
}

std::string DebugPrint(TrafficManager::TrafficState state)
{
  switch (state)
  {
  case TrafficManager::TrafficState::Disabled:
    return "Disabled";
  case TrafficManager::TrafficState::Enabled:
    return "Enabled";
  case TrafficManager::TrafficState::WaitingData:
    return "WaitingData";
  case TrafficManager::TrafficState::Outdated:
    return "Outdated";
  case TrafficManager::TrafficState::NoData:
    return "NoData";
  case TrafficManager::TrafficState::NetworkError:
    return "NetworkError";
  case TrafficManager::TrafficState::ExpiredData:
    return "ExpiredData";
  case TrafficManager::TrafficState::ExpiredApp:
    return "ExpiredApp";
  default:
      ASSERT(false, ("Unknown state"));
  }
  return "Unknown";
}
