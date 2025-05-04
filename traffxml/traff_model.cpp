#include "traffxml/traff_model.hpp"

using namespace std;

namespace traffxml
{
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
  os << std::put_time(&time, "%Y-%m-%d %H:%M:%S %z");
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
