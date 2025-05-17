#include "traffxml/traff_model.hpp"

#include "base/logging.hpp"

#include "geometry/mercator.hpp"

#include <regex>

using namespace std;

namespace traffxml
{
const std::map<EventType, traffic::SpeedGroup> kEventSpeedGroupMap{
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
const std::map<EventType, uint8_t> kEventMaxspeedMap{
  // TODO Activity*, Authority*, Carpool* (not in enum yet)
  // TODO Construction* (not in enum yet)
  // TODO Environment*, EquipmentStatus*, Hazard*, Incident* (not in enum yet)
  // TODO complete Restriction* (not in enum yet)
  // TODO Security*, Transport*, Weather* (not in enum yet)
};
#endif

const std::map<EventType, uint16_t> kEventDelayMap{
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
  std::regex iso8601Regex("([0-9]{4})-([0-9]{2})-([0-9]{2})T([0-9]{2}):([0-9]{2}):([0-9]{2}(.[0-9]*)?)(Z|(([+-][0-9]{2})(:?([0-9]{2}))?))?");

  std::smatch iso8601Matcher;
  if (std::regex_search(timeString, iso8601Matcher, iso8601Regex))
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
    // Call timegm once to normalize tm; return value can be discarded
    timegm(&tm);
    IsoTime result(tm);
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
  std::time_t t = std::time(nullptr);
  std::tm* tm = std::gmtime(&t);
  return IsoTime(*tm);
}

IsoTime::IsoTime(std::tm tm)
  : m_tm(tm)
{}

bool IsoTime::IsPast()
{
  std::time_t t_now = std::time(nullptr);
  std::time_t t_tm = timegm(&m_tm);
  return t_tm < t_now;
}

bool IsoTime::operator< (IsoTime & rhs)
{
  std::time_t t_lhs = std::mktime(&m_tm);
  std::time_t t_rhs = std::mktime(&rhs.m_tm);
  return t_lhs < t_rhs;
}

bool IsoTime::operator> (IsoTime & rhs)
{
  std::time_t t_lhs = std::mktime(&m_tm);
  std::time_t t_rhs = std::mktime(&rhs.m_tm);
  return t_lhs > t_rhs;
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

openlr::LocationReferencePoint Point::ToLrp()
{
  openlr::LocationReferencePoint result;
  result.m_latLon = ms::LatLon(this->m_coordinates.m_lat, this->m_coordinates.m_lon);
  return result;
}

bool operator==(TraffLocation const & lhs, TraffLocation const & rhs)
{
  return (lhs.m_from == rhs.m_from)
      && (lhs.m_at == rhs.m_at)
      && (lhs.m_via == rhs.m_via)
      && (lhs.m_notVia == rhs.m_notVia)
      && (lhs.m_to == rhs.m_to);
}

openlr::LinearLocationReference TraffLocation::ToLinearLocationReference(bool backwards)
{
  openlr::LinearLocationReference locationReference;
  locationReference.m_points.clear();
  std::vector<Point> points;
  if (m_from)
    points.push_back(m_from.value());
  if (m_at)
    points.push_back(m_at.value());
  else if (m_via)
    points.push_back(m_via.value());
  if (m_to)
    points.push_back(m_to.value());
  if (backwards)
    std::reverse(points.begin(), points.end());
  // m_notVia is ignored as OpenLR does not support this functionality.
  CHECK_GREATER(points.size(), 1, ("At least two reference points must be given"));
  for (auto point : points)
  {
    openlr::LocationReferencePoint lrp = point.ToLrp();
    lrp.m_functionalRoadClass = GetFrc();
    if (m_ramps.value_or(traffxml::Ramps::None) != traffxml::Ramps::None)
      lrp.m_formOfWay = openlr::FormOfWay::Sliproad;
    if (!locationReference.m_points.empty())
    {
      // TODO use `distance` from TraFF reference point, if available and consistent with direct distance
      locationReference.m_points.back().m_distanceToNextPoint
          = GuessDnp(locationReference.m_points.back(), lrp);
    }
    locationReference.m_points.push_back(lrp);
  }
  return locationReference;
}

// TODO make segment ID in OpenLR a string value, and store messageId
std::vector<openlr::LinearSegment> TraffLocation::ToOpenLrSegments(std::string & messageId)
{
  // Convert the location to a format understood by the OpenLR decoder.
  std::vector<openlr::LinearSegment> segments;
  int dirs = (m_directionality == Directionality::BothDirections) ? 2 : 1;
  for (int dir = 0; dir < dirs; dir++)
  {
    openlr::LinearSegment segment;
    /*
     * Segment IDs are used internally by the decoder but nowhere else.
     * Since we decode TraFF locations one at a time, there are at most two segments in a single
     * decoder instance (one segment per direction). Therefore, a segment ID derived from the
     * direction is unique within the decoder instance.
     */
    segment.m_segmentId = dir;
    segment.m_messageId = messageId;
    /*
     * Segments generated from coordinates can have any number of points. Each point, except for
     * the last point, must indicate the distance to the next point. Line properties (functional
     * road class (FRC), form of way, bearing) or path properties other than distance to next point
     * (lowest FRC to next point, againstDrivingDirection) are ignored.
     * Segment length is never evaluated.
     * TODO update OpenLR decoder to make all line and path properties optional.
     */
    segment.m_source = openlr::LinearSegmentSource::FromCoordinatesTag;
    segment.m_locationReference = this->ToLinearLocationReference(dir == 0 ? false : true);

    segments.push_back(segment);
  }
  return segments;
}

openlr::FunctionalRoadClass TraffLocation::GetFrc()
{
  if (!m_roadClass)
    return openlr::FunctionalRoadClass::NotAValue;
  switch (m_roadClass.value())
  {
    case RoadClass::Motorway: return openlr::FunctionalRoadClass::FRC0;
    case RoadClass::Trunk: return openlr::FunctionalRoadClass::FRC0;
    case RoadClass::Primary: return openlr::FunctionalRoadClass::FRC1;
    case RoadClass::Secondary: return openlr::FunctionalRoadClass::FRC2;
    case RoadClass::Tertiary: return openlr::FunctionalRoadClass::FRC3;
    /*
     * TODO Revisit FRC for Other.
     * Other corresponds to FRC4–7.
     * FRC4 matches secondary/tertiary (zero score) and anything below (full score).
     * FRC5–7 match anything below tertiary (full score); secondary/tertiary never match.
     * Primary and above never matches any of these FRCs.
     */
    case RoadClass::Other: return openlr::FunctionalRoadClass::FRC4;
  }
  UNREACHABLE();
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

    // TODO if event is in delay class and has an explicit duration quantifier, use that and skip the map lookup.
    if (auto it = kEventDelayMap.find(event.m_type); it != kEventDelayMap.end())
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

// TODO tweak formula based on FRC, FOW and direct distance (lower FRC roads may have more and sharper turns)
uint32_t GuessDnp(openlr::LocationReferencePoint & p1, openlr::LocationReferencePoint & p2)
{
  double doe = mercator::DistanceOnEarth(mercator::FromLatLon(p1.m_latLon),
                                         mercator::FromLatLon(p2.m_latLon));
  /*
   * Acceptance boundaries for candidate paths are currently:
   *
   * for `openlr::LinearSegmentSource::FromLocationReferenceTag`, 0.6 to ~1.67 (i.e. 1/0.6) times
   * the direct distance,
   *
   * for `openlr::LinearSegmentSource::FromCoordinatesTag`, 0.25 to 4 times the direct distance.
   *
   * A tolerance factor of 1/0.6 is the maximum for which direct distance would be accepted in all
   * cases, with an upper boundary of at least ~2.78 times the direct distance. However, this may
   * cause the actual distance to be overestimated and an incorrect route chosen as a result, as
   * path candidates are scored based on the match between DNP and their length.
   * Also, since we use `openlr::LinearSegmentSource::FromCoordinatesTag`, acceptance limits are
   * much wider than that.
   * In practice, the shortest route from one valley to the next in a mountain area is seldom more
   * than 3 times the direct distance, based on a brief examination. This would be even within the
   * limits of direct distance, hence we do not need a large correction factor for this scenario.
   *
   * Candidate values:
   * 1.66 (1/0.6) – upper boundary for direct distance to be just within the most stringent limits
   * 1.41 (2^0.5) – ratio between two sides of a square and its diagonal
   * 1.3 – close to the square root of 1.66 (halfway between 1 and 1.66)
   * 1.19 – close to the square root of 1.41
   * 1 – direct distance unmodified
   */
  return doe * 1.19f + 0.5f;
}

void MergeMultiMwmColoring(MultiMwmColoring & delta, MultiMwmColoring & target)
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
  os << std::put_time(&time.m_tm, "%Y-%m-%d %H:%M:%S %z");
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
  // TODO optional float m_distance; (not in struct yet)
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
  // TODO fuzziness (not yet implemented in struct)
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
  os << "ramps: " << (location.m_ramps ? DebugPrint(location.m_ramps.value()) : "nullopt");
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
  // TODO optional quantifier
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
