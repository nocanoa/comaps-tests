#include "traffxml/traff_model_xml.hpp"
#include "traffxml/traff_model.hpp"

#include "base/logging.hpp"

#include <cstring>
#include <optional>
#include <regex>
#include <type_traits>
#include <utility>

#include <pugixml.hpp>

using namespace std;

namespace traffxml
{
const std::map<std::string, Directionality> kDirectionalityMap{
  {"ONE_DIRECTION", Directionality::OneDirection},
  {"BOTH_DIRECTIONS", Directionality::BothDirections}
};

const std::map<std::string, Ramps> kRampsMap{
  {"ALL_RAMPS", Ramps::All},
  {"ENTRY_RAMP", Ramps::Entry},
  {"EXIT_RAMP", Ramps::Exit},
  {"NONE", Ramps::None}
};

const std::map<std::string, RoadClass> kRoadClassMap{
  {"MOTORWAY", RoadClass::Motorway},
  {"TRUNK", RoadClass::Trunk},
  {"PRIMARY", RoadClass::Primary},
  {"SECONDARY", RoadClass::Secondary},
  {"TERTIARY", RoadClass::Tertiary},
  {"OTHER", RoadClass::Other}
};

const std::map<std::string, EventClass> kEventClassMap{
  {"INVALID", EventClass::Invalid},
  {"ACTIVITY", EventClass::Activity},
  {"AUTHORITY", EventClass::Authority},
  {"CARPOOL", EventClass::Carpool},
  {"CONGESTION", EventClass::Congestion},
  {"CONSTRUCTION", EventClass::Construction},
  {"DELAY", EventClass::Delay},
  {"ENVIRONMENT", EventClass::Environment},
  {"EQUIPMENT_STATUS", EventClass::EquipmentStatus},
  {"HAZARD", EventClass::Hazard},
  {"INCIDENT", EventClass::Incident},
  {"RESTRICTION", EventClass::Restriction},
  {"SECURITY", EventClass::Security},
  {"TRANSPORT", EventClass::Transport},
  {"WEATHER", EventClass::Weather}
};

const std::map<std::string, EventType> kEventTypeMap{
  {"INVALID", EventType::Invalid},
  // TODO Activity*, Authority*, Carpool* (not in enum yet)
  {"CONGESTION_CLEARED", EventType::CongestionCleared},
  {"CONGESTION_FORECAST_WITHDRAWN", EventType::CongestionForecastWithdrawn},
  {"CONGESTION_HEAVY_TRAFFIC", EventType::CongestionHeavyTraffic},
  {"CONGESTION_LONG_QUEUE", EventType::CongestionLongQueue},
  {"CONGESTION_NONE", EventType::CongestionNone},
  {"CONGESTION_NORMAL_TRAFFIC", EventType::CongestionNormalTraffic},
  {"CONGESTION_QUEUE", EventType::CongestionQueue},
  {"CONGESTION_QUEUE_LIKELY", EventType::CongestionQueueLikely},
  {"CONGESTION_SLOW_TRAFFIC", EventType::CongestionSlowTraffic},
  {"CONGESTION_STATIONARY_TRAFFIC", EventType::CongestionStationaryTraffic},
  {"CONGESTION_STATIONARY_TRAFFIC_LIKELY", EventType::CongestionStationaryTrafficLikely},
  {"CONGESTION_TRAFFIC_BUILDING_UP", EventType::CongestionTrafficBuildingUp},
  {"CONGESTION_TRAFFIC_CONGESTION", EventType::CongestionTrafficCongestion},
  {"CONGESTION_TRAFFIC_EASING", EventType::CongestionTrafficEasing},
  {"CONGESTION_TRAFFIC_FLOWING_FREELY", EventType::CongestionTrafficFlowingFreely},
  {"CONGESTION_TRAFFIC_HEAVIER_THAN_NORMAL", EventType::CongestionTrafficHeavierThanNormal},
  {"CONGESTION_TRAFFIC_LIGHTER_THAN_NORMAL", EventType::CongestionTrafficLighterThanNormal},
  {"CONGESTION_TRAFFIC_MUCH_HEAVIER_THAN_NORMAL", EventType::CongestionTrafficMuchHeavierThanNormal},
  {"CONGESTION_TRAFFIC_PROBLEM", EventType::CongestionTrafficProblem},
  // TODO Construction* (not in enum yet)
  {"DELAY_CLEARANCE", EventType::DelayClearance},
  {"DELAY_DELAY", EventType::DelayDelay},
  {"DELAY_DELAY_POSSIBLE", EventType::DelayDelayPossible},
  {"DELAY_FORECAST_WITHDRAWN", EventType::DelayForecastWithdrawn},
  {"DELAY_LONG_DELAY", EventType::DelayLongDelay},
  {"DELAY_SEVERAL_HOURS", EventType::DelaySeveralHours},
  {"DELAY_UNCERTAIN_DURATION", EventType::DelayUncertainDuration},
  {"DELAY_VERY_LONG_DELAY", EventType::DelayVeryLongDelay},
  // TODO Environment*, EquipmentStatus*, Hazard*, Incident* (not in enum yet)
  // TODO complete Restriction* (not in enum yet)
  {"RESTRICTION_BLOCKED", EventType::RestrictionBlocked},
  {"RESTRICTION_BLOCKED_AHEAD", EventType::RestrictionBlockedAhead},
  {"RESTRICTION_CARRIAGEWAY_BLOCKED", EventType::RestrictionCarriagewayBlocked},
  {"RESTRICTION_CARRIAGEWAY_CLOSED", EventType::RestrictionCarriagewayClosed},
  {"RESTRICTION_CLOSED", EventType::RestrictionClosed},
  {"RESTRICTION_CLOSED_AHEAD", EventType::RestrictionClosedAhead},
  {"RESTRICTION_ENTRY_BLOCKED", EventType::RestrictionEntryBlocked},
  {"RESTRICTION_ENTRY_REOPENED", EventType::RestrictionEntryReopened},
  {"RESTRICTION_EXIT_BLOCKED", EventType::RestrictionExitBlocked},
  {"RESTRICTION_EXIT_REOPENED", EventType::RestrictionExitReopened},
  {"RESTRICTION_OPEN", EventType::RestrictionOpen},
  {"RESTRICTION_RAMP_BLOCKED", EventType::RestrictionRampBlocked},
  {"RESTRICTION_RAMP_CLOSED", EventType::RestrictionRampClosed},
  {"RESTRICTION_RAMP_REOPENED", EventType::RestrictionRampReopened},
  {"RESTRICTION_REOPENED", EventType::RestrictionReopened},
  {"RESTRICTION_SPEED_LIMIT", EventType::RestrictionSpeedLimit},
  {"RESTRICTION_SPEED_LIMIT_LIFTED", EventType::RestrictionSpeedLimitLifted},
  // TODO Security*, Transport*, Weather* (not in enum yet)
};

/**
 * @brief Retrieves an integer value from an attribute.
 *
 * @param attribute The XML attribute to retrieve.
 * @return `true` on success, `false` if the attribute is not set or does not contain an integer value.
 */
std::optional<uint8_t> OptionalIntegerFromXml(pugi::xml_attribute attribute)
{
  if (attribute.empty())
    return std::nullopt;
  try
  {
    uint8_t result = std::stoi(attribute.as_string());
    return result;
  }
  catch (std::invalid_argument const& ex)
  {
    return std::nullopt;
  }
}

/**
 * @brief Retrieves a float value from an attribute.
 *
 * @param attribute The XML attribute to retrieve.
 * @return `true` on success, `false` if the attribute is not set or does not contain a float value.
 */
std::optional<float> OptionalFloatFromXml(pugi::xml_attribute attribute)
{
  if (attribute.empty())
    return std::nullopt;
  try
  {
    float result = std::stof(attribute.as_string());
    return result;
  }
  catch (std::invalid_argument const& ex)
  {
    return std::nullopt;
  }
}

/**
 * @brief Retrieves a string from an attribute.
 *
 * @param attribute The XML attribute to retrieve.
 * @param string Receives the string retrieved.
 * @return `true` on success, `false` if the attribute is not set or set to an empty string.
 */
bool StringFromXml(pugi::xml_attribute attribute, std::string & string)
{
  if (attribute.empty())
    return false;
  string = attribute.as_string();
  return true;
}

/**
 * @brief Retrieves a string from an XML element.
 *
 * @param node The XML element to retrieve.
 * @param string Receives the string retrieved.
 * @return `true` on success, `false` if the node does not exist.
 */
bool StringFromXml(pugi::xml_node node, std::string & string)
{
  if (!node)
    return false;
  string = node.text().as_string();
  return true;
}

/**
 * @brief Retrieves a string from an attribute.
 *
 * @param attribute The XML attribute to retrieve.
 * @return The string, or `std::nullopt` if the attribute is not set or set to an empty string.
 */
std::optional<std::string> OptionalStringFromXml(pugi::xml_attribute attribute)
{
  std::string result;
  if (!StringFromXml(attribute, result))
    return std::nullopt;
  return result;
}

/**
 * @brief Parses time in ISO 8601 format from a time attribute and stores it in an `IsoTime`.
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
 * @param attribute The XML attribute from which to receive time.
 * @param tm Receives the parsed time.
 * @return `true` on success, `false` if the attribute is not set or does not contain a timestamp.
 */
bool TimeFromXml(pugi::xml_attribute attribute, IsoTime & tm)
{
  std::string timeString;
  if (!StringFromXml(attribute, timeString))
    return false;

  std::optional<IsoTime> result = IsoTime::ParseIsoTime(timeString);
  if (!result)
    return false;

  tm = result.value();
  return true;
}

/**
 * @brief Parses time in ISO 8601 format from a time attribute and stores it in an `IsoTime`.
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
 * @param attribute The XML attribute from which to receive time.
 * @return The parsed time, or `std::nullopt` if the attribute is not set or does not contain a timestamp.
 */
std::optional<IsoTime> OptionalTimeFromXml(pugi::xml_attribute attribute)
{
  IsoTime result = IsoTime::Now();
  if (!TimeFromXml(attribute, result))
    return std::nullopt;
  return result;
}

/**
 * @brief Retrieves a boolean value from an attribute.
 * @param attribute The XML attribute to retrieve.
 * @param defaultValue The default value to return.
 * @return The value of the attribute, or `defaultValue` if the attribute is not set.
 */
bool BoolFromXml(pugi::xml_attribute attribute, bool defaultValue)
{
  if (attribute.empty())
    return defaultValue;
  return attribute.as_bool();
}

/**
 * @brief Retrieves a boolean value from an attribute.
 * @param attribute The XML attribute to retrieve.
 * @return The value of the attribute, or `std::nullopt` if the attribute is not set.
 */
std::optional<bool> OptionalBoolFromXml(pugi::xml_attribute attribute)
{
  if (attribute.empty())
    return std::nullopt;
  return attribute.as_bool();
}

/**
 * @brief Retrieves an enum value from an attribute.
 *
 * Enum values are retrieved in two steps: first, a string is retrieved, which is then decoded to
 * an enum value. The enum type is determined by the type of `value` and the value type of `map`,
 * both of which must match. The mapping between strings and their corresponding enum values is
 * determined by the entries in `map`.
 *
 * @param attribute The XML attribute to retrieve.
 * @param Value Receives the enum value to retrieve.
 * @param map A map from strings to their respective enum values.
 * @return `true` on success, `false` if the attribute is not set or its value is not found in `map`.
 */
template <typename Value>
bool EnumFromXml(pugi::xml_attribute attribute, Value & value, std::map<std::string, Value> map)
{
  std::string string;
  if (StringFromXml(attribute, string))
  {
    auto it = map.find(string);
    if (it != map.end())
    {
      value = it->second;
      return true;
    }
    else
      LOG(LWARNING, ("Unknown value for", attribute.name(), ":", string, "(ignoring)"));
  }
  return false;
}

/**
 * @brief Retrieves an enum value from an attribute.
 *
 * Enum values are retrieved in two steps: first, a string is retrieved, which is then decoded to
 * an enum value. The enum type is determined by the value type of `map`. The mapping between
 * strings and their corresponding enum values is determined by the entries in `map`.
 *
 * @param attribute The XML attribute to retrieve.
 * @param map A map from strings to their respective enum values.
 * @return The enum value, or `std::nullopt` if the attribute is not set or its value is not found in `map`.
 */
template <typename Value>
std::optional<Value> OptionalEnumFromXml(pugi::xml_attribute attribute, std::map<std::string, Value> map)
{
  std::string string;
  if (StringFromXml(attribute, string))
  {
    auto it = map.find(string);
    if (it != map.end())
      return it->second;
    else
      LOG(LWARNING, ("Unknown value for", attribute.name(), ":", string, "(ignoring)"));
  }
  return std::nullopt;
}

/**
 * @brief Retrieves the IDs of replaced messages from an XML element.
 * @param node The XML element to retrieve (`merge`).
 * @param replacedIds Receives the replaced IDs.
 * @return `true` on success (including if the node contains no replaced IDs), `false` if the node does not exist or does not contain valid data.
 */
bool ReplacedMessageIdsFromXml(pugi::xml_node node, std::vector<std::string> & replacedIds)
{
  if (!node)
    return false;

  bool result = false;
  auto const replacedIdNodes = node.select_nodes("./replaces");

  if (replacedIdNodes.empty())
    return true;

  for (auto const & xpathNode : replacedIdNodes)
  {
    auto const replacedIdNode = xpathNode.node();
    std::string replacedId;
    if (StringFromXml(replacedIdNode.attribute("id"), replacedId))
    {
      replacedIds.push_back(replacedId);
      result = true;
    }
    else
      LOG(LWARNING, ("Could not parse merge element, skipping"));
  }
  return result;
}

/**
 * @brief Retrieves a latitude/longitude pair from an XML element.
 *
 * Coordinates must be given as latitude, followed by as space, then longitude. Latitude and
 * longitude are given as floating-point numbers, optionally with a sign (plus is assumed if no
 * sign is given). Coordinates are interpreted as degrees in WGS84 format.
 *
 * @param node The XML element to retrieve.
 * @param latLon Receives the latitude/longitude pair.
 * @return `true` on success, `false` if the node does not exist or does not contain valid coordinates.
 */
bool LatLonFromXml(pugi::xml_node node, ms::LatLon & latLon)
{
  if (!node)
    return false;
  std::string string;
  if (StringFromXml(node, string))
  {
    std::regex latLonRegex("([+-]?[0-9]*\\.?[0-9]*)\\s+([+-]?[0-9]*\\.?[0-9]*)");
    std::smatch latLonMatcher;
    if (std::regex_search(string, latLonMatcher, latLonRegex) && latLonMatcher[1].matched && latLonMatcher[2].matched)
    {
      try
      {
        latLon.m_lat = std::stod(latLonMatcher[1]);
        latLon.m_lon = std::stod(latLonMatcher[2]);
        return true;
      }
      catch (std::invalid_argument const& ex)
      {
        LOG(LWARNING, ("Not a valid coordinate pair:", string));
      }
    }
    else
      LOG(LWARNING, ("Not a valid coordinate pair:", string));
  }
  return false;
}

/**
 * @brief Retrieves a Traff `Point` from an XML element.
 * @param node The XML element to retrieve (any child of `location`).
 * @return The point, or `std::nullopt` if the node does not exist or does not contain valid point data,
 */
std::optional<Point> OptionalPointFromXml(pugi::xml_node node)
{
  if (!node)
    return std::nullopt;
  Point result;

  if (!LatLonFromXml(node, result.m_coordinates))
  {
    LOG(LWARNING, (node.name(), "has no coordinates, ignoring"));
    return std::nullopt;
  }

  // TODO optional float m_distance (not yet implemented in struct)

  result.m_junctionName = OptionalStringFromXml(node.attribute("junction_name"));
  result.m_junctionRef = OptionalStringFromXml(node.attribute("junction_ref"));
  result.m_distance = OptionalFloatFromXml(node.attribute("distance"));

  return result;
}

/**
 * @brief Retrieves a `TraffLocation` from an XML element.
 * @param node The XML element to retrieve (`location`).
 * @param location Receives the location.
 * @return `true` on success, `false` if the node does not exist or does not contain valid location data.
 */
bool LocationFromXml(pugi::xml_node node, TraffLocation & location)
{
  if (!node)
    return false;

  location.m_from = OptionalPointFromXml(node.child("from"));
  location.m_to = OptionalPointFromXml(node.child("to"));
  location.m_at = OptionalPointFromXml(node.child("at"));
  location.m_via = OptionalPointFromXml(node.child("via"));
  location.m_notVia = OptionalPointFromXml(node.child("not_via"));

  int numPoints = 0;
  for (std::optional<Point> point : {location.m_from, location.m_to, location.m_at})
    if (point)
      numPoints++;
  // single-point locations are not supported, locations without points are not valid
  if (numPoints < 2)
  {
    LOG(LWARNING, ("Only", numPoints, "points of from/to/at specified, ignoring location"));
    return false;
  }

  location.m_country = OptionalStringFromXml(node.attribute("country"));
  location.m_destination = OptionalStringFromXml(node.attribute("destination"));
  location.m_direction = OptionalStringFromXml(node.attribute("direction"));

  EnumFromXml(node.attribute("directionality"), location.m_directionality, kDirectionalityMap);

  // TODO fuzziness (not yet implemented in struct)

  location.m_origin = OptionalStringFromXml(node.attribute("origin"));
  location.m_ramps = OptionalEnumFromXml(node.attribute("ramps"), kRampsMap);
  location.m_roadClass = OptionalEnumFromXml(node.attribute("road_class"), kRoadClassMap);
  // disabled for now
  //location.m_roadIsUrban = OptionalBoolFromXml(node.attribute(("road_is_urban")));
  location.m_roadRef = OptionalStringFromXml(node.attribute("road_ref"));
  location.m_roadName = OptionalStringFromXml(node.attribute("road_name"));
  location.m_territory = OptionalStringFromXml(node.attribute("territory"));
  location.m_town = OptionalStringFromXml(node.attribute("town"));

  return true;
}

/**
 * @brief Retrieves a `TraffEvent` from an XML element.
 * @param node The XML element to retrieve (`event`).
 * @param event Receives the event.
 * @return `true` on success, `false` if the node does not exist or does not contain valid event data.
 */
bool EventFromXml(pugi::xml_node node, TraffEvent & event)
{
  std::string eventClass;
  if (!StringFromXml(node.attribute("class"), eventClass))
  {
    LOG(LWARNING, ("No event class specified, ignoring"));
    return false;
  }
  if (!EnumFromXml(node.attribute("class"), event.m_class, kEventClassMap))
    return false;

  std::string eventType;
  if (!StringFromXml(node.attribute("type"), eventType))
  {
    LOG(LWARNING, ("No event type specified, ignoring"));
    return false;
  }
  if (!eventType.starts_with(eventClass + "_"))
  {
    LOG(LWARNING, ("Event type", eventType, "does not match event class", eventClass, "(ignoring)"));
    return false;
  }
  if (!EnumFromXml(node.attribute("type"), event.m_type, kEventTypeMap))
    return false;

  event.m_length = OptionalIntegerFromXml(node.attribute("length"));
  event.m_probability = OptionalIntegerFromXml(node.attribute("probability"));

  // TODO optional quantifier (not yet implemented in struct)

  event.m_speed = OptionalIntegerFromXml(node.attribute("speed"));

  // TODO supplementary information (not yet implemented in struct)
  return true;
}

/**
 * @brief Retrieves the TraFF events associsted with a message from an XML element.
 * @param node The XML element to retrieve (`events`).
 * @param events Receives the events.
 * @return `true` on success, `false` if the node does not exist or does not contain valid event data (including if the node contains no events).
 */
bool EventsFromXml(pugi::xml_node node, std::vector<TraffEvent> & events)
{
  if (!node)
    return false;

  bool result = false;
  auto const eventNodes = node.select_nodes("./event");

  if (eventNodes.empty())
    return false;

  for (auto const & xpathNode : eventNodes)
  {
    auto const eventNode = xpathNode.node();
    TraffEvent event;
    if (EventFromXml(eventNode, event))
    {
      events.push_back(event);
      result = true;
    }
    else
      LOG(LWARNING, ("Could not parse event, skipping"));
  }
  return result;
}

/**
 * @brief Retrieves a TraFF message from an XML element.
 * @param node The XML element to retrieve (`message`).
 * @param message Receives the message.
 * @return `true` on success, `false` if the node does not exist or does not contain valid message data.
 */
bool MessageFromXml(pugi::xml_node node, TraffMessage & message)
{
  if (!StringFromXml(node.attribute("id"), message.m_id))
  {
    LOG(LWARNING, ("Message has no id"));
    return false;
  }

  if (!TimeFromXml(node.attribute("receive_time"), message.m_receiveTime))
  {
    LOG(LWARNING, ("Message", message.m_id, "has no receive_time"));
    return false;
  }

  if (!TimeFromXml(node.attribute("update_time"), message.m_updateTime))
  {
    LOG(LWARNING, ("Message", message.m_id, "has no update_time"));
    return false;
  }

  if (!TimeFromXml(node.attribute("expiration_time"), message.m_expirationTime))
  {
    LOG(LWARNING, ("Message", message.m_id, "has no expiration_time"));
    return false;
  }

  message.m_startTime = OptionalTimeFromXml(node.attribute("start_time"));
  message.m_endTime = OptionalTimeFromXml(node.attribute("end_time"));

  message.m_cancellation = BoolFromXml(node.attribute("cancellation"), false);
  message.m_forecast = BoolFromXml(node.attribute("forecast"), false);

  // TODO urgency (not yet implemented in struct)

  ReplacedMessageIdsFromXml(node.child("merge"), message.m_replaces);

  if (!message.m_cancellation)
  {
    message.m_location.emplace();
    if (!LocationFromXml(node.child("location"), message.m_location.value()))
    {
      message.m_location.reset();
      LOG(LWARNING, ("Message", message.m_id, "has no location but is not a cancellation message"));
      return false;
    }

    if (!EventsFromXml(node.child("events"), message.m_events))
      {
        LOG(LWARNING, ("Message", message.m_id, "has no events but is not a cancellation message"));
        return false;
      }
  }
  return true;
}

bool ParseTraff(pugi::xml_document const & document, TraffFeed & feed)
{
  bool result = false;

  // Select all messages elements that are direct children of the root.
  auto const messages = document.document_element().select_nodes("./message");

  if (messages.empty())
    return true;

  // TODO try block?
  for (auto const & xpathNode : messages)
  {
    auto const messageNode = xpathNode.node();
    TraffMessage message;
    if (MessageFromXml(messageNode, message))
    {
      feed.push_back(message);
      result = true;
    }
    else
      LOG(LWARNING, ("Could not parse message, skipping"));
  }
  return result;
}

std::string FiltersToXml(std::vector<m2::RectD> & bboxRects)
{
  std::ostringstream os;
  for (auto rect : bboxRects)
    os << std::format("<filter bbox=\"{} {} {} {}\"/>\n",
                      mercator::YToLat(rect.minY()),
                      mercator::XToLon(rect.minX()),
                      mercator::YToLat(rect.maxY()),
                      mercator::XToLon(rect.maxX()));
  return os.str();
}
}  // namespace openlr
