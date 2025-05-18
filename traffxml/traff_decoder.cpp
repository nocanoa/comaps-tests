#include "traffxml/traff_decoder.hpp"

//#include "traffxml/traff_foo.hpp"

#include "openlr/decoded_path.hpp"
#include "openlr/openlr_decoder.hpp"
#include "openlr/openlr_model.hpp"

#include "routing/maxspeeds.hpp"

#include "routing_common/maxspeed_conversion.hpp"

namespace traffxml
{
// Number of worker threads for the OpenLR decoder
/*
 * TODO how to determine the best number of worker threads?
 * One per direction? Does not seem to help with bidirectional locations (two reference points).
 * One per segment (from–via/from–at, via–to/at–to)? Not yet tested.
 * Otherwise there is little to be gained, as we decode messages one at a time.
 */
auto constexpr kNumDecoderThreads = 1;

TraffDecoder::TraffDecoder(DataSource & dataSource,
                           const CountryParentNameGetterFn & countryParentNameGetter,
                           std::map<std::string, TraffMessage> & messageCache)
  : m_dataSource(dataSource)
  , m_countryParentNameGetterFn(countryParentNameGetter)
  , m_messageCache(messageCache)
{}

void TraffDecoder::ApplyTrafficImpact(traffxml::TrafficImpact & impact, traffxml::MultiMwmColoring & decoded)
{
  for (auto dit = decoded.begin(); dit != decoded.end(); dit++)
    for (auto cit = dit->second.begin(); cit != dit->second.end(); cit++)
    {
      /*
       * Consolidate TrafficImpact into a single SpeedGroup per segment.
       * Exception: if TrafficImpact already has SpeedGrup::TempBlock, no need to evaluate
       * the rest.
       */
      traffic::SpeedGroup sg = impact.m_speedGroup;
      /*
       * TODO also process m_delayMins if greater than zero.
       * This would require a separate pass over all edges, calculating length,
       * total (normal) travel time (length / maxspeed), then a speed group based on
       * (normal_travel_time / delayed_travel_time) – which is the same as the ratio between
       * reduced and normal speed. That would give us a third potential speed group.
       */
      if ((sg != traffic::SpeedGroup::TempBlock) && (impact.m_maxspeed != traffxml::kMaxspeedNone))
      {
        auto const handle = m_dataSource.GetMwmHandleById(dit->first);
        auto const speeds = routing::LoadMaxspeeds(handle);
        if (speeds)
        {
          traffic::SpeedGroup fromMaxspeed = traffic::SpeedGroup::Unknown;
          auto const speed = speeds->GetMaxspeed(cit->first.GetFid());
          auto const speedKmPH = speed.GetSpeedKmPH(cit->first.GetDir() == traffic::TrafficInfo::RoadSegmentId::kForwardDirection);
          if (speedKmPH != routing::kInvalidSpeed)
          {
            fromMaxspeed = traffic::GetSpeedGroupByPercentage(impact.m_maxspeed * 100.0f / speedKmPH);
            if ((sg == traffic::SpeedGroup::Unknown) || (fromMaxspeed < sg))
              sg = fromMaxspeed;
          }
        }
        /*
         * TODO fully process TrafficImpact (unless m_speedGroup is TempBlock, which overrules everything else)
         * If no maxspeed or delay is set, just give out speed groups.
         * Else, examine segments, length, normal travel time, travel time considering impact, and
         * determine the closest matching speed group.
         */
      }
      // TODO process all TrafficImpact fields and determine the speed group based on that
      cit->second = sg;
    }
}

void TraffDecoder::DecodeMessage(traffxml::TraffMessage & message)
{
  if (!message.m_location)
    return;
  // Decode events into consolidated traffic impact
  std::optional<traffxml::TrafficImpact> impact = message.GetTrafficImpact();

  LOG(LINFO, ("    Impact: ", impact));

  // Skip further processing if there is no impact
  if (!impact)
    return;

  traffxml::MultiMwmColoring decoded;

  auto it = m_messageCache.find(message.m_id);
  if ((it != m_messageCache.end())
      && !it->second.m_decoded.empty()
      && (it->second.m_location == message.m_location))
  {
    // cache already has a message with reusable location

    LOG(LINFO, ("    Location for message", message.m_id, "can be reused from cache"));

    std::optional<traffxml::TrafficImpact> cachedImpact = it->second.GetTrafficImpact();
    if (cachedImpact.has_value() && cachedImpact.value() == impact.value())
    {
      LOG(LINFO, ("    Impact for message", message.m_id, "unchanged, reusing cached coloring"));

      // same impact, m_decoded can be reused altogether
      message.m_decoded = it->second.m_decoded;
      return;
    }
    else
      decoded = it->second.m_decoded;
  }
  else
    DecodeLocation(message, decoded);

  if (impact)
  {
    ApplyTrafficImpact(impact.value(), decoded);
    std::swap(message.m_decoded, decoded);
  }
}

OpenLrV3TraffDecoder::OpenLrV3TraffDecoder(DataSource & dataSource,
                                           const CountryParentNameGetterFn & countryParentNameGetter,
                                           std::map<std::string, TraffMessage> & messageCache)
  : TraffDecoder(dataSource, countryParentNameGetter, messageCache)
  , m_openLrDecoder(dataSource, countryParentNameGetter)
{}

openlr::FunctionalRoadClass OpenLrV3TraffDecoder::GetRoadClassFrc(std::optional<RoadClass> & roadClass)
{
  if (!roadClass)
    return openlr::FunctionalRoadClass::NotAValue;
  switch (roadClass.value())
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

// TODO tweak formula based on FRC, FOW and direct distance (lower FRC roads may have more and sharper turns)
uint32_t OpenLrV3TraffDecoder::GuessDnp(Point & p1, Point & p2)
{
  // direct distance
  double doe = mercator::DistanceOnEarth(mercator::FromLatLon(p1.m_coordinates),
                                         mercator::FromLatLon(p2.m_coordinates));

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
  double dist = doe * 1.19f;

  // if we have kilometric points, calculate nominal distance as the difference between them
  if (p1.m_distance && p2.m_distance)
  {
    LOG(LINFO, ("Both points have distance, calculating nominal difference"));
    float nominalDist = (p1.m_distance.value() - p2.m_distance.value()) * 1000.0;
    if (nominalDist < 0)
      nominalDist *= -1;
    /*
     * Plausibility check for nominal distance, as kilometric points along the route may not be
     * continuous: discard if shorter than direct distance (geometrically impossible) or if longer
     * than 4 times direct distance (somewhat arbitrary, based on the OpenLR acceptance limit for
     * `openlr::LinearSegmentSource::FromCoordinatesTag`, as well as real-world observations of
     * distances between two adjacent mountain valleys, which are up to roughly 3 times the direct
     * distance). If nominal distance is outside these boundaries, discard it and use `dist` (direct
     * distance with a tolerance factor).
     */
    if ((nominalDist >= doe) && (nominalDist <= doe * 4))
      dist = nominalDist;
    else
      LOG(LINFO, ("Nominal distance:", nominalDist, "direct distance:", doe, "– discarding"));
  }
  return dist + 0.5f;
}

openlr::LocationReferencePoint OpenLrV3TraffDecoder::PointToLrp(Point & point)
{
  openlr::LocationReferencePoint result;
  result.m_latLon = ms::LatLon(point.m_coordinates.m_lat, point.m_coordinates.m_lon);
  return result;
}

openlr::LinearLocationReference OpenLrV3TraffDecoder::TraffLocationToLinearLocationReference(TraffLocation & location, bool backwards)
{
  openlr::LinearLocationReference locationReference;
  locationReference.m_points.clear();
  std::vector<Point> points;
  if (location.m_from)
    points.push_back(location.m_from.value());
  if (location.m_at)
    points.push_back(location.m_at.value());
  else if (location.m_via)
    points.push_back(location.m_via.value());
  if (location.m_to)
    points.push_back(location.m_to.value());
  if (backwards)
    std::reverse(points.begin(), points.end());
  // m_notVia is ignored as OpenLR does not support this functionality.
  CHECK_GREATER(points.size(), 1, ("At least two reference points must be given"));
  for (size_t i = 0; i < points.size(); i++)
  {
    openlr::LocationReferencePoint lrp = PointToLrp(points[i]);
    lrp.m_functionalRoadClass = GetRoadClassFrc(location.m_roadClass);
    if (location.m_ramps.value_or(traffxml::Ramps::None) != traffxml::Ramps::None)
      lrp.m_formOfWay = openlr::FormOfWay::Sliproad;
    if (i < points.size() - 1)
    {
      lrp.m_distanceToNextPoint
          = GuessDnp(points[i], points[i + 1]);
      /*
       * Somewhat hackish. LFRCNP is evaluated by the same function as FRC and the candidate is
       * used or discarded based on whether a score was returned or not (the score itself is not
       * used for LFRCNP). However, this means we can use FRC as LFRCNP.
       */
      lrp.m_lfrcnp = GetRoadClassFrc(location.m_roadClass);
    }
    locationReference.m_points.push_back(lrp);
  }
  return locationReference;
}

// TODO make segment ID in OpenLR a string value, and store messageId
std::vector<openlr::LinearSegment> OpenLrV3TraffDecoder::TraffLocationToOpenLrSegments(TraffLocation & location, std::string & messageId)
{
  // Convert the location to a format understood by the OpenLR decoder.
  std::vector<openlr::LinearSegment> segments;
  int dirs = (location.m_directionality == Directionality::BothDirections) ? 2 : 1;
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
    segment.m_locationReference = TraffLocationToLinearLocationReference(location, dir == 0 ? false : true);

    segments.push_back(segment);
  }
  return segments;
}

/*
 * TODO the OpenLR decoder is designed to handle multiple segments (i.e. locations).
 * Decoding message by message kind of defeats the purpose.
 * But after decoding the location, we need to examine the map features we got in order to
 * determine the speed groups, thus we may need to decode one by one (TBD).
 * If we batch-decode segments, we need to fix the [partner] segment IDs in the segment and path
 * structures to accept a TraFF message ID (string) rather than an integer, or derive
 * [partner] segment IDs from TraFF message IDs.
 */
void OpenLrV3TraffDecoder::DecodeLocation(traffxml::TraffMessage & message, traffxml::MultiMwmColoring & decoded)
{
  ASSERT(message.m_location, ("Message has no location"));
  decoded.clear();

  // Convert the location to a format understood by the OpenLR decoder.
  std::vector<openlr::LinearSegment> segments
      = TraffLocationToOpenLrSegments(message.m_location.value(), message.m_id);

  for (auto segment : segments)
  {
    LOG(LINFO, ("    Segment:", segment.m_segmentId));
    for (int i = 0; i < segment.m_locationReference.m_points.size(); i++)
    {
      LOG(LINFO, ("     ", i, ":", segment.m_locationReference.m_points[i].m_latLon));
      if (i < segment.m_locationReference.m_points.size() - 1)
      {
        LOG(LINFO, ("        FRC:", segment.m_locationReference.m_points[i].m_functionalRoadClass));
        LOG(LINFO, ("        DNP:", segment.m_locationReference.m_points[i].m_distanceToNextPoint));
      }
    }
  }

  // Decode the location into a path on the map.
  // One path per segment
  std::vector<openlr::DecodedPath> paths(segments.size());
  m_openLrDecoder.DecodeV3(segments, kNumDecoderThreads, paths);

  for (size_t i = 0; i < paths.size(); i++)
    for (size_t j = 0; j < paths[i].m_path.size(); j++)
    {
      auto fid = paths[i].m_path[j].GetFeatureId().m_index;
      auto segment = paths[i].m_path[j].GetSegId();
      uint8_t direction = paths[i].m_path[j].IsForward() ?
                          traffic::TrafficInfo::RoadSegmentId::kForwardDirection :
                          traffic::TrafficInfo::RoadSegmentId::kReverseDirection;
      decoded[paths[i].m_path[j].GetFeatureId().m_mwmId][traffic::TrafficInfo::RoadSegmentId(fid, segment, direction)] = traffic::SpeedGroup::Unknown;
    }
}
}  // namespace traffxml
