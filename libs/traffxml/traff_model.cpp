#include "traffxml/traff_model.hpp"

#include "base/logging.hpp"

#include <iomanip>
#include <unordered_map>

#include <boost/regex.hpp>

using namespace std;

namespace traffxml
{
const std::unordered_map<EventType, traffic::SpeedGroup> kEventSpeedGroupMap{
  // TODO Activity*, Authority*, Carpool* (not in enum yet)
  {EventType::CongestionHeavyTraffic, traffic::SpeedGroup::G4},
  {EventType::CongestionLongQueue, traffic::SpeedGroup::G0},
  {EventType::CongestionNone, traffic::SpeedGroup::G5},
  {EventType::CongestionNormalTraffic, traffic::SpeedGroup::G5},
  {EventType::CongestionQueue, traffic::SpeedGroup::G2},
  {EventType::CongestionQueueLikely, traffic::SpeedGroup::G3},
  {EventType::CongestionSlowTraffic, traffic::SpeedGroup::G3},
  {EventType::CongestionStationaryTraffic, traffic::SpeedGroup::G1},
  {EventType::CongestionStationaryTrafficLikely, traffic::SpeedGroup::G2},
  {EventType::CongestionTrafficBuildingUp, traffic::SpeedGroup::G4},
  {EventType::CongestionTrafficCongestion, traffic::SpeedGroup::G3}, // TODO or G2? Unquantified, below normal
  {EventType::CongestionTrafficFlowingFreely, traffic::SpeedGroup::G5},
  {EventType::CongestionTrafficHeavierThanNormal, traffic::SpeedGroup::G4},
  {EventType::CongestionTrafficLighterThanNormal, traffic::SpeedGroup::G5},
  {EventType::CongestionTrafficMuchHeavierThanNormal, traffic::SpeedGroup::G3},
  {EventType::CongestionTrafficProblem, traffic::SpeedGroup::G3},  // TODO or G2? Unquantified, below normal
  // TODO Construction* (not in enum yet)
  /*
   * Some delay types have a duration which depends on the route. This is better expressed as a
   * speed group, although the mapping may be somewhat arbitrary and may need to be corrected.
   */
  {EventType::DelayDelay, traffic::SpeedGroup::G2},
  {EventType::DelayDelayPossible, traffic::SpeedGroup::G3},
  {EventType::DelayLongDelay, traffic::SpeedGroup::G1},
  {EventType::DelayVeryLongDelay, traffic::SpeedGroup::G0},
  // TODO Environment*, EquipmentStatus*, Hazard*, Incident* (not in enum yet)
  // TODO complete Restriction* (not in enum yet)
  {EventType::RestrictionBlocked, traffic::SpeedGroup::TempBlock},
  {EventType::RestrictionBlockedAhead, traffic::SpeedGroup::TempBlock},
  //{EventType::RestrictionCarriagewayBlocked, traffic::SpeedGroup::TempBlock}, // TODO FIXME other carriageways may still be open
  //{EventType::RestrictionCarriagewayClosed, traffic::SpeedGroup::TempBlock},  // TODO FIXME other carriageways may still be open
  {EventType::RestrictionClosed, traffic::SpeedGroup::TempBlock},
  {EventType::RestrictionClosedAhead, traffic::SpeedGroup::TempBlock},
  {EventType::RestrictionEntryBlocked, traffic::SpeedGroup::TempBlock},
  {EventType::RestrictionExitBlocked, traffic::SpeedGroup::TempBlock},
  {EventType::RestrictionRampBlocked, traffic::SpeedGroup::TempBlock},
  {EventType::RestrictionRampClosed, traffic::SpeedGroup::TempBlock},
  {EventType::RestrictionSpeedLimit, traffic::SpeedGroup::G4},
  // TODO Security*, Transport*, Weather* (not in enum yet)
};

// none of the currently define events imply an explicit maxspeed
#if 0
const std::unordered_map<EventType, uint8_t> kEventMaxspeedMap{
  // TODO Activity*, Authority*, Carpool* (not in enum yet)
  // TODO Construction* (not in enum yet)
  // TODO Environment*, EquipmentStatus*, Hazard*, Incident* (not in enum yet)
  // TODO complete Restriction* (not in enum yet)
  // TODO Security*, Transport*, Weather* (not in enum yet)
};
#endif

const std::unordered_map<EventType, uint16_t> kEventDelayMap{
  // TODO Activity*, Authority*, Carpool* (not in enum yet)
  // TODO Construction* (not in enum yet)
  //{EventType::DelayDelay, },             // mapped to speed group
  //{EventType::DelayDelayPossible, },     // mapped to speed group
  //{EventType::DelayLongDelay, },         // mapped to speed group
  {EventType::DelaySeveralHours, 150},     // assumption: 2.5 hours
  {EventType::DelayUncertainDuration, 60}, // assumption: 1 hour
  //{EventType::DelayVeryLongDelay, },     // mapped to speed group
  // TODO Environment*, EquipmentStatus*, Hazard*, Incident* (not in enum yet)
  // TODO complete Restriction* (not in enum yet)
  // TODO Security*, Transport*, Weather* (not in enum yet)
};

std::optional<IsoTime> IsoTime::ParseIsoTime(std::string timeString)
{
  /*
   * TODO this is ugly because we need to work around some compiler deficiencies.
   *
   * Ideally, we would be using `std::chrono::time_point<std::chrono::utc_clock>` and parse the
   * string using `std::chrono::from_stream`, using `%FT%T%z` for the format string.
   * This works in GCC 14+ and is pleasantly liberal about the time zone format (all of +01, +0100
   * and +01:00 are parsed correctly). Alas, Ubuntu 24.04 (currently the default dev platform) comes
   * with GCC 13.2, which lacks this support. Clang, the only supported compiler for Android (and,
   * presumably, iOS), as of mid-2025, doesn’t support it at all.
   *
   * The workaround is therefore to use `std::chrono::time_point<std::chrono::system_clock>`, which
   * exposes the same API as its `utc_clock` counterpart, making transition at a later point easy.
   * In addition, however, it can be constructed from `std::time_t`, which we can generate from
   * `std::tm`. Unlike the other C legacy functions, gmtime is thread-safe.
   * Still not the prettiest way (as it relies on legacy C functions which are not
   * thread-safe), but the best we can get until we have proper compiler support for `from_stream`.
   *
   * Should we have support for `std::chrono:clock_cast` but not `std::chrono::from_stream`, we
   * could build a `std::chrono::sys_seconds` from the constutuent values and use
   * `std::chrono::clock_cast` to convert it to a `std::chrono::time_point`, based on whatever clock
   * is supported. This works on Linux (using `utc_clock`) as of mid-2025, but not on the primary
   * target platforms (Android and iOS) and has therefore been left out for uniformity (and
   * reproducibility of bugs).
   */
  /*
   * Regex for ISO 8601 time, with some tolerance for time zone offset. If matched, the matcher
   * will contain the following items:
   *
   *  0: 2019-11-01T11:55:42+01:00  (entire expression)
   *  1: 2019                       (year)
   *  2: 11                         (month)
   *  3: 01                         (day)
   *  4: 11                         (hour, local)
   *  5: 55                         (minute, local)
   *  6: 42.445                     (second, local, float)
   *  7: .445                       (fractional seconds)
   *  8: +01:00                     (complete UTC offset, or Z; blank if not specified)
   *  9: +01:00                     (complete UTC offset, blank for Z or of not specified)
   * 10: +01                        (UTC offset, hours with sign; blank for Z or if not specified)
   * 11: :00                        (UTC offset, minutes, prefixed with separator)
   * 12: 00                         (UTC offset, minutes, unsigned; blank for Z or if not specified)
   */
  static boost::regex iso8601Regex("([0-9]{4})-([0-9]{2})-([0-9]{2})T([0-9]{2}):([0-9]{2}):([0-9]{2}(\\.[0-9]*)?)(Z|(([+-][0-9]{2})(:?([0-9]{2}))?))?");

  boost::smatch iso8601Matcher;
  if (boost::regex_search(timeString, iso8601Matcher, iso8601Regex))
  {
    int offset_h = iso8601Matcher[10].matched ? std::stoi(iso8601Matcher[10]) : 0;
    int offset_m = iso8601Matcher[12].matched ? std::stoi(iso8601Matcher[12]) : 0;
    if (offset_h < 0)
      offset_m *= -1;

    std::tm tm = {};
    tm.tm_year = std::stoi(iso8601Matcher[1]) - 1900;
    tm.tm_mon = std::stoi(iso8601Matcher[2]) - 1;
    tm.tm_mday = std::stoi(iso8601Matcher[3]);
    tm.tm_hour = std::stoi(iso8601Matcher[4]) - offset_h;
    tm.tm_min = std::stoi(iso8601Matcher[5]) - offset_m;
    tm.tm_sec = std::stof(iso8601Matcher[6]) + 0.5f;

    std::time_t tt = timegm(&tm);

    std::chrono::time_point<std::chrono::system_clock> tp = std::chrono::system_clock::from_time_t(tt);

    IsoTime result(tp);
    return result;
  }
  else
  {
    LOG(LINFO, ("Not a valid ISO 8601 timestamp:", timeString));
    return std::nullopt;
  }
}

IsoTime IsoTime::Now()
{
  return IsoTime(std::chrono::system_clock::now());
}

IsoTime::IsoTime(std::chrono::time_point<std::chrono::system_clock> tp)
  : m_tp(tp)
{}

bool IsoTime::IsPast()
{
  return m_tp < std::chrono::system_clock::now();
}\

void IsoTime::Shift(IsoTime nowRef)
{
  auto const offset = std::chrono::system_clock::now() - nowRef.m_tp;
  m_tp += offset;
}

std::string IsoTime::ToString() const
{
  auto const tp_seconds = time_point_cast<std::chrono::seconds>(m_tp);
  auto const time_t = std::chrono::system_clock::to_time_t(tp_seconds);
  std::tm tm = *std::gmtime(&time_t);
  
  std::ostringstream ss;
  ss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
  ss << "Z";
  return ss.str();
}

bool IsoTime::operator< (IsoTime & rhs)
{
  return m_tp < rhs.m_tp;
}

bool IsoTime::operator> (IsoTime & rhs)
{
  return m_tp > rhs.m_tp;
}

bool operator==(TrafficImpact const & lhs, TrafficImpact const & rhs)
{
  if ((lhs.m_speedGroup == traffic::SpeedGroup::TempBlock)
      && (rhs.m_speedGroup == traffic::SpeedGroup::TempBlock))
    return true;
  return (lhs.m_speedGroup == rhs.m_speedGroup)
      && (lhs.m_maxspeed == rhs.m_maxspeed)
      && (lhs.m_delayMins == rhs.m_delayMins);
}

bool operator==(Point const & lhs, Point const & rhs)
{
  return lhs.m_coordinates == rhs.m_coordinates;
}

bool operator==(TraffLocation const & lhs, TraffLocation const & rhs)
{
  return (lhs.m_from == rhs.m_from)
      && (lhs.m_at == rhs.m_at)
      && (lhs.m_via == rhs.m_via)
      && (lhs.m_notVia == rhs.m_notVia)
      && (lhs.m_to == rhs.m_to);
}

IsoTime TraffMessage::GetEffectiveExpirationTime()
{
  IsoTime result = m_expirationTime;
  if (m_startTime && m_startTime.value() > result)
    result = m_startTime.value();
  if (m_endTime && m_endTime.value() > result)
    result = m_endTime.value();
  return result;
}

bool TraffMessage::IsExpired(IsoTime now)
{
  return GetEffectiveExpirationTime() < now;
}

std::optional<TrafficImpact> TraffMessage::GetTrafficImpact()
{
  // no events, no impact
  if (m_events.empty())
    return std::nullopt;

  // examine events
  std::vector<TrafficImpact> impacts;
  for (auto event : m_events)
  {
    TrafficImpact impact;

    if (auto it = kEventSpeedGroupMap.find(event.m_type); it != kEventSpeedGroupMap.end())
      impact.m_speedGroup = it->second;

    if (event.m_speed)
      impact.m_maxspeed = event.m_speed.value();
    // TODO if no explicit speed given, look up in kEventMaxspeedMap (once we have entries)

    if (event.m_class == EventClass::Delay
        && event.m_type != EventType::DelayClearance
        && event.m_type != EventType::DelayForecastWithdrawn
        && event.m_type != EventType::DelaySeveralHours
        && event.m_type != EventType::DelayUncertainDuration
        && event.m_qDurationMins)
      impact.m_delayMins = event.m_qDurationMins.value();
    else if (auto it = kEventDelayMap.find(event.m_type); it != kEventDelayMap.end())
      impact.m_delayMins = it->second;

    // TempBlock overrules everything else, return immediately
    if (impact.m_speedGroup == traffic::SpeedGroup::TempBlock)
      return impact;
    // if there is no actual impact, discard
    if ((impact.m_maxspeed < kMaxspeedNone)
        || (impact.m_delayMins > 0)
        || (impact.m_speedGroup != traffic::SpeedGroup::Unknown))
      impacts.push_back(impact);
  }

  if (impacts.empty())
    return std::nullopt;

  TrafficImpact result;
  for (auto impact : impacts)
  {
    ASSERT(impact.m_speedGroup != traffic::SpeedGroup::TempBlock, ("Got SpeedGroup::TempBlock, which should not happen at this stage"));
    if (result.m_speedGroup == traffic::SpeedGroup::Unknown)
      result.m_speedGroup = impact.m_speedGroup;
    // TempBlock cannot occur here, so we can do just a simple comparison
    else if ((impact.m_speedGroup != traffic::SpeedGroup::Unknown) && (impact.m_speedGroup < result.m_speedGroup))
      result.m_speedGroup = impact.m_speedGroup;

    if (impact.m_maxspeed < result.m_maxspeed)
      result.m_maxspeed = impact.m_maxspeed;

    if (impact.m_delayMins > result.m_delayMins)
      result.m_delayMins = impact.m_delayMins;
  }
  if ((result.m_maxspeed < kMaxspeedNone)
      || (result.m_delayMins > 0)
      || (result.m_speedGroup != traffic::SpeedGroup::Unknown))
    return result;
  else
    // should never happen, unless we have a bug somewhere
    return std::nullopt;
}

void TraffMessage::ShiftTimestamps()
{
  IsoTime nowRef = m_updateTime;
  m_receiveTime.Shift(nowRef);
  m_updateTime.Shift(nowRef);
  m_expirationTime.Shift(nowRef);
  if (m_startTime)
    m_startTime.value().Shift(nowRef);
  if (m_endTime)
    m_endTime.value().Shift(nowRef);
}

void MergeMultiMwmColoring(const MultiMwmColoring & delta, MultiMwmColoring & target)
{
  // for each mwm in delta
  for (auto [mwmId, coloring] : delta)
    // if target contains mwm
    if (auto target_it = target.find(mwmId); target_it != target.end())
      // for each segment in delta[mwm] (coloring)
      for (auto [rsid, sg] : coloring)
        // if target[mwm] contains segment
        if (auto c_it = target_it->second.find(rsid) ; c_it != target_it->second.end())
        {
          // if delta overrules target (target is Unknown, delta is TempBlock or delta is slower than target)
          if ((sg == traffic::SpeedGroup::TempBlock)
              || (c_it->second == traffic::SpeedGroup::Unknown) || (sg < c_it->second))
            target_it->second[rsid] = sg;
        }
        else
          // if target[mwm] does not contain segment, add speed group
          target_it->second[rsid] = sg;
    else
      // if target does not contain mwm, add coloring
      target[mwmId] = coloring;
}

/*
string DebugPrint(LinearSegmentSource source)
{
  switch (source)
  {
  case LinearSegmentSource::NotValid: return "NotValid";
  case LinearSegmentSource::FromLocationReferenceTag: return "FromLocationReferenceTag";
  case LinearSegmentSource::FromCoordinatesTag: return "FromCoordinatesTag";
  }
  UNREACHABLE();
}
 */
std::string DebugPrint(IsoTime time)
{
  std::ostringstream os;
  //os << std::put_time(&time.m_tm, "%Y-%m-%d %H:%M:%S %z");
  // %FT%T%z
  auto const time_t = std::chrono::system_clock::to_time_t(time.m_tp);
  std::tm tm = *std::gmtime(&time_t);
  os << std::put_time(&tm, "%Y-%m-%d %H:%M:%S UTC");
  return os.str();
}

std::string DebugPrint(Directionality directionality)
{
  switch (directionality)
  {
  case Directionality::OneDirection: return "OneDirection";
  case Directionality::BothDirections: return "BothDirections";
  }
  UNREACHABLE();
}

std::string DebugPrint(Fuzziness fuzziness)
{
  switch (fuzziness)
  {
  case Fuzziness::LowRes: return "LowRes";
  case Fuzziness::MediumRes: return "MediumRes";
  case Fuzziness::EndUnknown: return "EndUnknown";
  case Fuzziness::StartUnknown: return "StartUnknown";
  case Fuzziness::ExtentUnknown: return "ExtentUnknown";
  }
  UNREACHABLE();
}

std::string DebugPrint(Ramps ramps)
{
  switch (ramps)
  {
    case Ramps::All: return "All";
    case Ramps::Entry: return "Entry";
    case Ramps::Exit: return "Exit";
    case Ramps::None: return "None";
  }
  UNREACHABLE();
}

std::string DebugPrint(RoadClass roadClass)
{
  switch (roadClass)
  {
    case RoadClass::Motorway: return "Motorway";
    case RoadClass::Trunk: return "Trunk";
    case RoadClass::Primary: return "Primary";
    case RoadClass::Secondary: return "Secondary";
    case RoadClass::Tertiary: return "Tertiary";
    case RoadClass::Other: return "Other";
  }
  UNREACHABLE();
}

std::string DebugPrint(EventClass eventClass)
{
  switch (eventClass)
  {
    case EventClass::Invalid: return "Invalid";
    case EventClass::Activity: return "Activity";
    case EventClass::Authority: return "Authority";
    case EventClass::Carpool: return "Carpool";
    case EventClass::Congestion: return "Congestion";
    case EventClass::Construction: return "Construction";
    case EventClass::Delay: return "Delay";
    case EventClass::Environment: return "Environment";
    case EventClass::EquipmentStatus: return "EquipmentStatus";
    case EventClass::Hazard: return "Hazard";
    case EventClass::Incident: return "Incident";
    case EventClass::Restriction: return "Restriction";
    case EventClass::Security: return "Security";
    case EventClass::Transport: return "Transport";
    case EventClass::Weather: return "Weather";
  }
  UNREACHABLE();
}

std::string DebugPrint(EventType eventType)
{
  switch (eventType)
  {
    case EventType::Invalid: return "Invalid";
    // TODO Activity*, Authority*, Carpool* (not in enum yet)
    case EventType::CongestionCleared: return "CongestionCleared";
    case EventType::CongestionForecastWithdrawn: return "CongestionForecastWithdrawn";
    case EventType::CongestionHeavyTraffic: return "CongestionHeavyTraffic";
    case EventType::CongestionLongQueue: return "CongestionLongQueue";
    case EventType::CongestionNone: return "CongestionNone";
    case EventType::CongestionNormalTraffic: return "CongestionNormalTraffic";
    case EventType::CongestionQueue: return "CongestionQueue";
    case EventType::CongestionQueueLikely: return "CongestionQueueLikely";
    case EventType::CongestionSlowTraffic: return "CongestionSlowTraffic";
    case EventType::CongestionStationaryTraffic: return "CongestionStationaryTraffic";
    case EventType::CongestionStationaryTrafficLikely: return "CongestionStationaryTrafficLikely";
    case EventType::CongestionTrafficBuildingUp: return "CongestionTrafficBuildingUp";
    case EventType::CongestionTrafficCongestion: return "CongestionTrafficCongestion";
    case EventType::CongestionTrafficEasing: return "CongestionTrafficEasing";
    case EventType::CongestionTrafficFlowingFreely: return "CongestionTrafficFlowingFreely";
    case EventType::CongestionTrafficHeavierThanNormal: return "CongestionTrafficHeavierThanNormal";
    case EventType::CongestionTrafficLighterThanNormal: return "CongestionTrafficLighterThanNormal";
    case EventType::CongestionTrafficMuchHeavierThanNormal: return "CongestionTrafficMuchHeavierThanNormal";
    case EventType::CongestionTrafficProblem: return "CongestionTrafficProblem";
    // TODO Construction* (not in enum yet)
    case EventType::DelayClearance: return "DelayClearance";
    case EventType::DelayDelay: return "DelayDelay";
    case EventType::DelayDelayPossible: return "DelayDelayPossible";
    case EventType::DelayForecastWithdrawn: return "DelayForecastWithdrawn";
    case EventType::DelayLongDelay: return "DelayLongDelay";
    case EventType::DelaySeveralHours: return "DelaySeveralHours";
    case EventType::DelayUncertainDuration: return "DelayUncertainDuration";
    case EventType::DelayVeryLongDelay: return "DelayVeryLongDelay";
    // TODO Environment*, EquipmentStatus*, Hazard*, Incident* (not in enum yet)
    // TODO complete Restriction* (not in enum yet)
    case EventType::RestrictionBlocked: return "RestrictionBlocked";
    case EventType::RestrictionBlockedAhead: return "RestrictionBlockedAhead";
    case EventType::RestrictionCarriagewayBlocked: return "RestrictionCarriagewayBlocked";
    case EventType::RestrictionCarriagewayClosed: return "RestrictionCarriagewayClosed";
    case EventType::RestrictionClosed: return "RestrictionClosed";
    case EventType::RestrictionClosedAhead: return "RestrictionClosedAhead";
    case EventType::RestrictionEntryBlocked: return "RestrictionEntryBlocked";
    case EventType::RestrictionEntryReopened: return "RestrictionEntryReopened";
    case EventType::RestrictionExitBlocked: return "RestrictionExitBlocked";
    case EventType::RestrictionExitReopened: return "RestrictionExitReopened";
    case EventType::RestrictionOpen: return "RestrictionOpen";
    case EventType::RestrictionRampBlocked: return "RestrictionRampBlocked";
    case EventType::RestrictionRampClosed: return "RestrictionRampClosed";
    case EventType::RestrictionRampReopened: return "RestrictionRampReopened";
    case EventType::RestrictionReopened: return "RestrictionReopened";
    case EventType::RestrictionSpeedLimit: return "RestrictionSpeedLimit";
    case EventType::RestrictionSpeedLimitLifted: return "RestrictionSpeedLimitLifted";
    // TODO Security*, Transport*, Weather* (not in enum yet)
  }
  UNREACHABLE();
}

std::string DebugPrint(ResponseStatus status)
{
  switch (status)
  {
    case ResponseStatus::Ok: return "Ok";
    case ResponseStatus::InvalidOperation: return "InvalidOperation";
    case ResponseStatus::SubscriptionRejected: return "SubscriptionRejected";
    case ResponseStatus::NotCovered: return "NotCovered";
    case ResponseStatus::PartiallyCovered: return "PartiallyCovered";
    case ResponseStatus::SubscriptionUnknown: return "SubscriptionUnknown";
    case ResponseStatus::PushRejected: return "PushRejected";
    case ResponseStatus::InternalError: return "InternalError";
    case ResponseStatus::Invalid: return "Invalid";
  }
  UNREACHABLE();
}

std::string DebugPrint(TrafficImpact impact)
{
  std::ostringstream os;
  os << "TrafficImpact { ";
  os << "speedGroup: " << DebugPrint(impact.m_speedGroup) << ", ";
  os << "maxspeed: " << (impact.m_maxspeed == kMaxspeedNone ? "none" : std::to_string(impact.m_maxspeed)) << ", ";
  os << "delayMins: " << impact.m_delayMins;
  os << " }";
  return os.str();
}

std::string DebugPrint(Point point)
{
  std::ostringstream os;
  os << "Point { ";
  os << "coordinates: " << DebugPrint(point.m_coordinates) << ", ";
  os << "distance: " << (point.m_distance ? std::to_string(point.m_distance.value()) : "nullopt") << ", ";
  os << "junctionName: " << point.m_junctionName.value_or("nullopt") << ", ";
  os << "junctionRef: " << point.m_junctionRef.value_or("nullopt");
  os << " }";
  return os.str();
}

std::string DebugPrint(TraffLocation location)
{
  std::ostringstream os;
  os << "TraffLocation { ";
  os << "from: " << (location.m_from ? DebugPrint(location.m_from.value()) : "nullopt") << ", ";
  os << "at: " << (location.m_at ? DebugPrint(location.m_at.value()) : "nullopt") << ", ";
  os << "via: " << (location.m_via ? DebugPrint(location.m_via.value()) : "nullopt") << ", ";
  os << "to: " << (location.m_to ? DebugPrint(location.m_to.value()) : "nullopt") << ", ";
  os << "notVia: " << (location.m_notVia ? DebugPrint(location.m_notVia.value()) : "nullopt") << ", ";
  os << "fuzziness: " << (location.m_fuzziness ? DebugPrint(location.m_fuzziness.value()) : "nullopt") << ", ";
  os << "country: " << location.m_country.value_or("nullopt") << ", ";
  os << "territory: " << location.m_territory.value_or("nullopt") << ", ";
  os << "town: " << location.m_town.value_or("nullopt") << ", ";
  os << "roadClass: " << (location.m_roadClass ? DebugPrint(location.m_roadClass.value()) : "nullopt") << ", ";
  os << "roadRef: " << location.m_roadRef.value_or("nullopt") << ", ";
  os << "roadName: " << location.m_roadName.value_or("nullopt") << ", ";
  os << "origin: " << location.m_origin.value_or("nullopt") << ", ";
  os << "destination: " << location.m_destination.value_or("nullopt") << ", ";
  os << "direction: " << location.m_direction.value_or("nullopt") << ", ";
  os << "directionality: " << DebugPrint(location.m_directionality) << ", ";
  os << "ramps: " << DebugPrint(location.m_ramps);
  os << " }";
  return os.str();
}

std::string DebugPrint(TraffEvent event)
{
  std::ostringstream os;
  os << "TraffEvent { ";
  os << "class: " << DebugPrint(event.m_class) << ", ";
  os << "type: " << DebugPrint(event.m_type) << ", ";
  os << "length: " << (event.m_length ? std::to_string(event.m_length.value()) : "nullopt") << ", ";
  os << "probability: " << (event.m_probability ? std::to_string(event.m_probability.value()) : "nullopt") << ", ";
  os << "q_duration: "
     << (event.m_qDurationMins
         ? (std::to_string(event.m_qDurationMins.value() / 60) + ":" + 
             (event.m_qDurationMins.value() % 60 < 10 ? "0" : "") + 
             std::to_string(event.m_qDurationMins.value() % 60))
         : "nullopt")
     << ", ";
  // TODO other quantifiers
  os << "speed: " << (event.m_speed ? std::to_string(event.m_speed.value()) : "nullopt");
  // TODO supplementary information
  os << " }";
  return os.str();
}

std::string DebugPrint(TraffMessage message)
{
  std::string sep;
  std::ostringstream os;
  os << "TraffMessage { ";
  os << "id: " << message.m_id << ", ";

  os << "replaces: [";
  sep = " ";
  for (auto const & replacedId : message.m_replaces)
  {
    os << sep << replacedId;
    sep = ", ";
  }
  os << " ], ";

  os << "receiveTime: " << DebugPrint(message.m_receiveTime) << ", ";
  os << "updateTime: " << DebugPrint(message.m_updateTime) << ", ";
  os << "expirationTime: " << DebugPrint(message.m_expirationTime) << ", ";
  os << "startTime: " << (message.m_startTime ? DebugPrint(message.m_startTime.value()) : "nullopt") << ", ";
  os << "endTime: " << (message.m_endTime ? DebugPrint(message.m_endTime.value()) : "nullopt") << ", ";
  os << "cancellation: " << message.m_cancellation << ", ";
  os << "forecast: " << message.m_forecast << ", ";
  // TODO std::optional<Urgency> m_urgency; (not in struct yet)
  os << "location: " << (message.m_location ? DebugPrint(message.m_location.value()) : "nullopt") << ", ";

  os << "events: [";
  sep = " ";
  for (auto const & event : message.m_events)
  {
    os << sep << DebugPrint(event);
    sep = ", ";
  }
  os << " ]";

  os << " }";
  return os.str();
}

std::string DebugPrint(TraffFeed feed)
{
  std::string sep;
  std::ostringstream os;
  os << "[ ";
  sep = "";
  for (auto const & message : feed)
  {
    os << sep << DebugPrint(message);
    sep = ", ";
  }
  os << " ]";
  return os.str();
}
}  // namespace traffxml
