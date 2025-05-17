#pragma once

//#include "traffxml/traff_foo.hpp"

#include "geometry/latlon.hpp"
#include "geometry/point2d.hpp"

#include "indexer/mwm_set.hpp"

#include "openlr/openlr_model.hpp"

#include "traffic/speed_groups.hpp"
#include "traffic/traffic_info.hpp"

#include <map>
#include <string>
#include <vector>

namespace traffxml
{
constexpr uint8_t kMaxspeedNone = 255;

/**
 * @brief Date and time decoded from ISO 8601.
 *
 * `IsoTime` is an opaque type. It is only guaranteed to be capable of holding a timestamp
 * converted from ISO 8601 which refers to the same UTC time as its ISO 8601 representation.
 * Time zone information is not guaranteed to be preserved: `13:37+01:00` may be returned e.g. as
 * `12:37Z` or `06:37-06:00`.
 *
 * Code using `IsoTime` must not rely on it being identical to any other type, as this is not
 * guaranteed to be stable.
 */
/*
 * Where no time zone is indicated, the timestamp shall always be interpreted as UTC.
 * `IsoTime` currently maps to `std::tm`, but this is not guaranteed.
 */
class IsoTime
{
public:
  /**
   * @brief Parses time in ISO 8601 format from a string and stores it in an `IsoTime`.
   *
   * ISO 8601 timestamps have the format `yyyy-mm-ddThh:mm:ss[.sss]`, optionally followed by a UTC
   * offset. For example, `2019-11-01T11:45:42+01:00` refers to 11:45:42 in the UTC+1 timezone, which
   * is 10:45:42 UTC.
   *
   * A UTC offset of `Z` denotes UTC and is equivalent to `+00:00` or `-00:00`. UTC is also assumed
   * if no UTC offset is specified.
   *
   * The UTC offset can be specified as `hh:mm`, `hhmm` or `hh`.
   *
   * Seconds can be specified as integer or float, but will be rounded to the nearest integer. For
   * example, 42.645 seconds will be rounded to 43 seconds.
   *
   * @param timeString Time in ISO8601 format
   * @return An `IsoTime` instance corresponding to `timeString`, or `std::nullopt` if `timeString` is not a valid ISO8601 time string.
   */
  static std::optional<IsoTime> ParseIsoTime(std::string timeString);

  /**
   * @brief Returns an `IsoTime` corresponding to current wall clock time.
   *
   * @return An `IsoTime` corresponding to current wall clock time.
   */
  static IsoTime Now();

  /**
   * @brief Whether the instance refers to a point of time in the past.
   *
   * Comparison is against system time.
   *
   * @return true if in the past, false of not.
   */
  bool IsPast();

  bool operator< (IsoTime & rhs);
  bool operator> (IsoTime & rhs);
private:
  friend std::string DebugPrint(IsoTime time);

  IsoTime(std::tm tm);

  std::tm m_tm;
};

// TODO enum urgency

enum class Directionality
{
  OneDirection,
  BothDirections
};

// TODO enum fuzziness

enum class Ramps
{
  None,
  All,
  Entry,
  Exit
};

enum class RoadClass
{
  Motorway,
  Trunk,
  Primary,
  Secondary,
  Tertiary,
  Other
};

/*
 * When adding a new event class to this enum, be sure to do the following:
 *
 * * in `traff_model_xml.cpp`, add the corresponding mapping to `kEventClassMap`
 * * in `traff_model.cpp`, extend `DebugPrint(EventClass)` to correctly process the new event classes
 * * in this file, add event types for this class to `EventType`
 */
enum class EventClass
{
  Invalid,
  Activity,
  Authority,
  Carpool,
  Congestion,
  Construction,
  Delay,
  Environment,
  EquipmentStatus,
  Hazard,
  Incident,
  Restriction,
  Security,
  Transport,
  Weather
};

/*
 * When adding a new event type to this enum, be sure to do the following:
 *
 * * in `traff_model_xml.cpp`, add the corresponding mapping to `kEventTypeMap`
 * * in `traff_model.cpp`:
 *   * add speed group mappings in `kEventSpeedGroupMap`, if any
 *   * add maxspeed mappings in `kEventMaxspeedMap`, if any (uncomment if needed)
 *   * add delay mappings in `kEventDelayMap`, if any
 *   * extend `DebugPrint(TraffEvent)` to correctly process the new events
 */
enum class EventType
{
  Invalid,
  // TODO Activity*, Authority*, Carpool*
  CongestionCleared,
  CongestionForecastWithdrawn,
  CongestionHeavyTraffic,
  CongestionLongQueue,
  CongestionNone,
  CongestionNormalTraffic,
  CongestionQueue,
  CongestionQueueLikely,
  CongestionSlowTraffic,
  CongestionStationaryTraffic,
  CongestionStationaryTrafficLikely,
  CongestionTrafficBuildingUp,
  CongestionTrafficCongestion,
  CongestionTrafficEasing,
  CongestionTrafficFlowingFreely,
  CongestionTrafficHeavierThanNormal,
  CongestionTrafficLighterThanNormal,
  CongestionTrafficMuchHeavierThanNormal,
  CongestionTrafficProblem,
  // TODO Construction*
  DelayClearance,
  DelayDelay,
  DelayDelayPossible,
  DelayForecastWithdrawn,
  DelayLongDelay,
  DelaySeveralHours,
  DelayUncertainDuration,
  DelayVeryLongDelay,
  // TODO Environment*, EquipmentStatus*, Hazard*, Incident*
  // TODO complete Restriction*
  RestrictionBlocked,
  RestrictionBlockedAhead,
  RestrictionCarriagewayBlocked,
  RestrictionCarriagewayClosed,
  RestrictionClosed,
  RestrictionClosedAhead,
  RestrictionEntryBlocked,
  RestrictionEntryReopened,
  RestrictionExitBlocked,
  RestrictionExitReopened,
  RestrictionOpen,
  RestrictionRampBlocked,
  RestrictionRampClosed,
  RestrictionRampReopened,
  RestrictionReopened,
  RestrictionSpeedLimit,
  RestrictionSpeedLimitLifted,
  // TODO Security*, Transport*, Weather*
};

/**
 * @brief Represents the impact of one or more traffic events.
 *
 * Impact can be expressed in three ways:
 *
 * Traffic may flow at a certain percentage of the posted limit, often divided in bins. This is
 * used by some traffic services which report e.g. “slow traffic”, “stationary traffic” or
 * “queues”, and maps to speed groups in a straightforward way.
 *
 * Traffic may flow at, or be restricted to, a given speed. This is common with traffic flow
 * measurement data, or with temporary speed limits. Converting this to a speed group requires
 * knowledge of the regular speed limit.
 *
 * There may be a fixed delay, expressed as a duration in time. This may happen at checkpoints,
 * at sections where traffic flow is limited or where there is single alternate-lane traffic.
 * As the routing data model does not provide for explicit delays, they have to be converted into
 * speed groups. Again, this requires knowledge of the regular travel time along the route, as well
 * as its length.
 *
 * Closures can be expressed by setting `m_speedGroup` to `traffic::SpeedGroup::TempBlock`. If that
 * is the case, the other struct members are to be ignored.
 */
struct TrafficImpact
{
  /**
   * @brief Whether two `TrafficImpact` instances are equal.
   *
   * Instances are considered equal if both have a speed group of `TempBlock`, in which case other
   * members are not compared. Otherwise, they are equal if, and only if, all three members hold
   * identical values between both instances.
   */
  // Non-member friend as member operators do not work with std::optional
  friend bool operator==(TrafficImpact const & lhs, TrafficImpact const & rhs);
  friend bool operator!=(TrafficImpact const & lhs, TrafficImpact const & rhs) { return !(lhs == rhs); }

  /**
   * @brief The speed group for the affected segments, or `traffic::SpeedGroup::Unknown` if unknown.
   */
  traffic::SpeedGroup m_speedGroup = traffic::SpeedGroup::Unknown;

  /**
   * @brief The speed limit, or speed of flowing traffic; `kMaxspeedNone` if none or unknown.
   */
  uint8_t m_maxspeed = kMaxspeedNone;

  /**
   * @brief The delay in minutes; 0 if none or unknown.
   */
  uint16_t m_delayMins = 0;
};

struct Point
{
  /**
   * @brief Whether two points are equal.
   *
   * Two points are equal if, and only if, their coordinates are. Other attributes are not compared.
   */
  // Non-member friend as member operators do not work with std::optional
  friend bool operator==(Point const & lhs, Point const & rhs);
  friend bool operator!=(Point const & lhs, Point const & rhs) { return !(lhs == rhs); }

  /**
   * @brief Converts the point to an OpenLR location reference point.
   *
   * Only coordinates are populated.
   *
   * @return An OpenLR LRP with the coordinates of the point.
   */
  openlr::LocationReferencePoint ToLrp();

  // TODO role?
  ms::LatLon m_coordinates = ms::LatLon::Zero();
  // TODO optional float m_distance;
  std::optional<std::string> m_junctionName;
  std::optional<std::string> m_junctionRef;
};

struct TraffLocation
{
  /**
   * @brief Whether two locations are equal.
   *
   * Two locations are equal if, and only if, they contain the same points in the same roles.
   *
   * @todo Road class and ramps are not compared, though these values are used by the decoder. Not
   * comparing these values could lead to two seemingly equal locations resolving to a different
   * path. However, given that comparison only takes place between messages with identical IDs
   * (indicating both refer to the same event at the same location), such a situation is highly
   * unlikely to occur in practice.
   */
  // Non-member friend as member operators do not work with std::optional
  friend bool operator==(TraffLocation const & lhs, TraffLocation const & rhs);
  friend bool operator!=(TraffLocation const & lhs, TraffLocation const & rhs) { return !(lhs == rhs); }

  /**
   * @brief Converts the location to an OpenLR linear location reference.
   *
   * @param backwards If true, gnerates a linear location reference for the backwards direction,
   * with the order of points reversed.
   * @return An OpenLR linear location reference which corresponds to the location.
   */
  openlr::LinearLocationReference ToLinearLocationReference(bool backwards);

  /**
   * @brief Converts the location to a vector of OpenLR segments.
   *
   * Depending on the directionality, the resulting vector will hold one or two elements: one for
   * the forward direction, and for bidirectional locations, a second one for the backward
   * direction.
   *
   * @param messageId The message ID
   * @return A vector holding the resulting OpenLR segments.
   */
  std::vector<openlr::LinearSegment> ToOpenLrSegments(std::string & messageId);

  /**
   * @brief Returns the OpenLR functional road class (FRC) matching `m_roadClass`.
   *
   * @return The FRC.
   */
  openlr::FunctionalRoadClass GetFrc();

  std::optional<std::string> m_country;
  std::optional<std::string> m_destination;
  std::optional<std::string> m_direction;
  Directionality m_directionality = Directionality::BothDirections;
  // TODO std::optional<Fuzziness> m_fuzziness;
  std::optional<std::string> m_origin;
  std::optional<Ramps> m_ramps;
  std::optional<RoadClass> m_roadClass;
  // disabled for now, optional<bool> behaves weird and we don't really need it
  //std::optional<bool> m_roadIsUrban;
  std::optional<std::string> m_roadRef;
  std::optional<std::string> m_roadName;
  std::optional<std::string> m_territory;
  std::optional<std::string> m_town;
  std::optional<Point> m_from;
  std::optional<Point> m_to;
  std::optional<Point> m_at;
  std::optional<Point> m_via;
  std::optional<Point> m_notVia;
};

struct TraffEvent
{
  EventClass m_class = EventClass::Invalid;
  EventType m_type = EventType::Invalid;
  std::optional<uint8_t> m_length;
  std::optional<uint8_t> m_probability;
  // TODO optional quantifier
  std::optional<uint8_t> m_speed;
  // TODO supplementary information
};

/**
 * @brief Global mapping from feature segments to speed groups, across all MWMs.
 */
using MultiMwmColoring = std::map<MwmSet::MwmId, std::map<traffic::TrafficInfo::RoadSegmentId, traffic::SpeedGroup>>;

struct TraffMessage
{
  /**
   * @brief Retrieves the traffic impact of all events.
   *
   * If the message has multiple events, the traffic impact is determined separately for each
   * event and then aggregated. Aggregation takes the most restrictive value in each category
   * (speed group, maxspeed, delay).
   *
   * If the aggregated traffic impact includes `SpeedGroup::TempBlock`, its other members are to
   * be considered invalid.
   *
   * @return The aggregated traffic impact, or `std::nullopt` if the message has no events with traffic impact.
   */
  std::optional<TrafficImpact> GetTrafficImpact();

  std::string m_id;
  IsoTime m_receiveTime = IsoTime::Now();
  IsoTime m_updateTime = IsoTime::Now();
  IsoTime m_expirationTime = IsoTime::Now();
  std::optional<IsoTime> m_startTime = {};
  std::optional<IsoTime> m_endTime = {};
  bool m_cancellation = false;
  bool m_forecast = false;
  // TODO std::optional<Urgency> m_urgency;
  std::optional<TraffLocation> m_location;
  std::vector<TraffEvent> m_events;
  std::vector<std::string> m_replaces;
  MultiMwmColoring m_decoded;
};

using TraffFeed = std::vector<TraffMessage>;

// TODO Capabilities

/*
 * Filter: currently not implemented.
 * We only use bbox, for which we have a suitable data type.
 * min_road_class is not needed as we do not filter by road class.
 */

/*
 * TraffSubscription: currently not implemented.
 * We just store the ID as a string.
 * Filters are only by bbox, not by min_road_class. The list is auto-generated from the list of
 * active MWMs and changes exactly when the active MWM set changes, eliminating the need to store
 * the full filter list.
 */

/**
 * @brief Guess the distance to the next point.
 *
 * This is calculated as direct distance, multiplied with a tolerance factor to account for the
 * fact that the road is not always a straight line.
 *
 * The result can be used to provide some semi-valid DNP values.
 *
 * @param p1 The first point.
 * @param p2 The second point.
 * @return The approximate distance on the ground, in meters.
 */
uint32_t GuessDnp(openlr::LocationReferencePoint & p1, openlr::LocationReferencePoint & p2);

/**
 * @brief Merges the contents of one `MultiMwmColoring` into another.
 *
 * After this function returns, `target` will hold the union of the entries it had prior to the
 * function call and the entries from `delta`.
 *
 * In case of conflict, the more restrictive speed group wins. That is, `TempBlock` overrides
 * everything else, `Unknown` never overrides anything else, and among `G0` to `G5`, the lowest
 * group wins.
 *
 * @param delta Contains the entries to be added.
 * @param target Receives the added entries.
 */
void MergeMultiMwmColoring(MultiMwmColoring &delta, MultiMwmColoring & target);

std::string DebugPrint(IsoTime time);
std::string DebugPrint(Directionality directionality);
std::string DebugPrint(Ramps ramps);
std::string DebugPrint(RoadClass roadClass);
std::string DebugPrint(EventClass eventClass);
std::string DebugPrint(EventType eventType);
std::string DebugPrint(TrafficImpact impact);
std::string DebugPrint(Point point);
std::string DebugPrint(TraffLocation location);
std::string DebugPrint(TraffEvent event);
std::string DebugPrint(TraffMessage message);
std::string DebugPrint(TraffFeed feed);
}  // namespace traffxml
