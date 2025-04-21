#pragma once

//#include "traffxml/traff_foo.hpp"

#include "geometry/latlon.hpp"
#include "geometry/point2d.hpp"

#include <string>
#include <vector>

namespace traffxml
{
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
using IsoTime = std::tm;

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

struct Point
{
  // TODO role?
  ms::LatLon m_coordinates = ms::LatLon::Zero();
  // TODO optional float m_distance;
  std::optional<std::string> m_junctionName;
  std::optional<std::string> m_junctionRef;
};

struct TraffLocation
{
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
  EventClass m_Class = EventClass::Invalid;
  EventType m_Type = EventType::Invalid;
  std::optional<uint8_t> m_length;
  std::optional<uint8_t> m_probability;
  // TODO optional quantifier
  std::optional<uint8_t> m_speed;
  // TODO supplementary information
};

struct TraffMessage
{
  std::string m_id;
  IsoTime m_receiveTime = {};
  IsoTime m_updateTime = {};
  IsoTime m_expirationTime = {};
  std::optional<IsoTime> m_startTime = {};
  std::optional<IsoTime> m_endTime = {};
  bool m_cancellation = false;
  bool m_forecast = false;
  // TODO std::optional<Urgency> m_urgency;
  std::optional<TraffLocation> m_location;
  std::vector<TraffEvent> m_events;
  std::vector<std::string> m_replaces;
};

using TraffFeed = std::vector<TraffMessage>;

std::string DebugPrint(IsoTime time);
std::string DebugPrint(Directionality directionality);
std::string DebugPrint(Ramps ramps);
std::string DebugPrint(RoadClass roadClass);
std::string DebugPrint(EventClass eventClass);
std::string DebugPrint(EventType eventType);
std::string DebugPrint(Point point);
std::string DebugPrint(TraffLocation location);
std::string DebugPrint(TraffEvent event);
std::string DebugPrint(TraffMessage message);
std::string DebugPrint(TraffFeed feed);
}  // namespace traffxml
