#include "traffxml/traff_decoder.hpp"

//#include "traffxml/traff_foo.hpp"

#include "geometry/distance_on_sphere.hpp"
#include "geometry/mercator.hpp"

#include "indexer/feature.hpp"

#include "openlr/decoded_path.hpp"
#include "openlr/openlr_decoder.hpp"
#include "openlr/openlr_model.hpp"

#include "routing/async_router.hpp"
#include "routing/checkpoints.hpp"
#include "routing/edge_estimator.hpp"
#include "routing/maxspeeds.hpp"
#include "routing/route.hpp"
#include "routing/router_delegate.hpp"

#include "routing_common/maxspeed_conversion.hpp"

#include "storage/routing_helpers.hpp"

#include "traffic/traffic_cache.hpp"

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

// Timeout for the router in seconds, used by RoutingTraffDecoder
// TODO set to a sensible value
auto constexpr kRouterTimeoutSec = 30;

/*
 * One meter per second. The TraffEstimator works on distance in meters, not travel time. For code
 * which works with speeds and assumes cost to be time-based, a speed of 1 m/s means such
 * calculations will effectively return distances in meters.
 */
auto constexpr kOneMpSInKmpH = 3.6;

/*
 * Penalty factor for using a fake segment to get to a nearby road.
 * Maximum penalty for roads is currently 16 (4 for ramps * 4 for road type), offroad penalty is
 * twice the maximum road penalty. We might need to increase that, since offroad penalty applies to
 * direct distance whereas road penalty applies to roads, which can be up to around 3 times the
 * direct distance (theoretically unlimited). That would imply multiplying maximum road penalty by
 * more than 3 (e.g. 4).
 */
auto constexpr kOffroadPenalty = 32;

/*
 * Penalty factor for non-matching attributes
 */
auto constexpr kAttributePenalty = 4;

/*
 * Penalty factor for partially matching attributes
 */
auto constexpr kReducedAttributePenalty = 2;

/*
 * Invalid feature ID.
 * Borrowed from indexer/feature_decl.hpp.
 */
uint32_t constexpr kInvalidFeatureId = std::numeric_limits<uint32_t>::max();

TraffDecoder::TraffDecoder(DataSource & dataSource, CountryInfoGetterFn countryInfoGetter,
                           const CountryParentNameGetterFn & countryParentNameGetter,
                           std::map<std::string, TraffMessage> & messageCache)
  : m_dataSource(dataSource)
  , m_countryInfoGetterFn(countryInfoGetter)
  , m_countryParentNameGetterFn(countryParentNameGetter)
  , m_messageCache(messageCache)
{}

void TraffDecoder::ApplyTrafficImpact(traffxml::TrafficImpact & impact, traffxml::MultiMwmColoring & decoded)
{
  traffic::SpeedGroup fromDelay = traffic::SpeedGroup::Unknown;

  /*
   * Convert delay into speed group (skip if the location is blocked).
   * TODO: mapping delay to speed group is not optimal, as a long delay on a short location can
   * increase travel time by several orders of magnitude.
   * Assume a stretch of road with a speed limit of 60 km/h, i.e. 1 km/min, and a delay of 30 min.
   * The lowest speed group, G0, translates to 8% of the posted limit. Since travel time ratio is
   * the inverse of speed ratio, in order to represent 30 seconds of delay, the location would have
   * to be long enough to take ~8% of that time to traverse under normal conditions, i.e. 2.4 min.,
   * or 2.4 km at 60 km/h. For shorter locations, the speed group will underestimate the delay.
   * Higher speeds and longer delays would increase the required length. This is somewhat offset by
   * the fact that the router adds a somewhat arbitrary extra penalty to segments for which traffic
   * problems are reported.
   */
  if ((impact.m_delayMins > 0) && (impact.m_speedGroup != traffic::SpeedGroup::TempBlock))
  {
    double normalDurationS = 0;
    for (auto dit = decoded.begin(); dit != decoded.end(); dit++)
    {
      /*
       * We are initializing two structures for map access here: an MWM handle to get maxspeeds,
       * and a FeaturesLoaderGuard to retrieve points. This may not be very elegant, but works.
       * Since both involve somewhat complex structures, a rewrite might not be simple, although
       * improvements are certainly welcome.
       */
      auto const handle = m_dataSource.GetMwmHandleById(dit->first);
      auto const speeds = routing::LoadMaxspeeds(handle);

      FeaturesLoaderGuard g(m_dataSource, dit->first);
      uint32_t lastFid = kInvalidFeatureId;
      std::vector<m2::PointD> points;
      routing::MaxspeedType speedKmPH = routing::kInvalidSpeed;
      for (auto cit = dit->second.begin(); cit != dit->second.end(); cit++)
      {
        // read points only once per feature ID, not once per segment
        if (lastFid != cit->first.GetFid())
        {
          auto f = g.GetOriginalFeatureByIndex(cit->first.GetFid());
          f->ResetGeometry();
          assign_range(points, f->GetPoints(FeatureType::BEST_GEOMETRY));
          lastFid = cit->first.GetFid();
          auto const speed = speeds->GetMaxspeed(cit->first.GetFid());
          speedKmPH = speed.GetSpeedKmPH(cit->first.GetDir() == traffic::TrafficInfo::RoadSegmentId::kForwardDirection);
        }
        auto const idx = cit->first.GetIdx();
        // TODO sum of all lengths may differ from route length by up to ~6%, no idea why
        auto const length = mercator::DistanceOnEarth(points[idx], points[idx + 1]);
        if (speedKmPH != routing::kInvalidSpeed)
          normalDurationS += length * kOneMpSInKmpH / speedKmPH;
      }
    }
    auto const delayedDurationS = normalDurationS + impact.m_delayMins * 60;
    fromDelay = traffic::GetSpeedGroupByPercentage(normalDurationS * 100.0f / delayedDurationS);

    LOG(LINFO, ("Normal duration:", normalDurationS, "delayed duration:", delayedDurationS, "speed group:", DebugPrint(fromDelay)));
  }

  for (auto dit = decoded.begin(); dit != decoded.end(); dit++)
  {
    std::unique_ptr<routing::Maxspeeds> speeds = nullptr;
    if ((impact.m_speedGroup != traffic::SpeedGroup::TempBlock) && (impact.m_maxspeed != traffxml::kMaxspeedNone))
    {
      // load maxspeeds once per MWM and only if needed
      auto const handle = m_dataSource.GetMwmHandleById(dit->first);
      speeds = routing::LoadMaxspeeds(handle);
    }
    for (auto cit = dit->second.begin(); cit != dit->second.end(); cit++)
    {
      /*
       * Consolidate TrafficImpact into a single SpeedGroup per segment.
       * Exception: if TrafficImpact already has SpeedGrup::TempBlock, no need to evaluate
       * the rest.
       */
      traffic::SpeedGroup sg = impact.m_speedGroup;

      if ((sg != traffic::SpeedGroup::TempBlock) && (fromDelay != traffic::SpeedGroup::Unknown))
        // process delay
        if ((sg == traffic::SpeedGroup::Unknown) || (fromDelay < sg))
          sg = fromDelay;

      if ((sg != traffic::SpeedGroup::TempBlock) && (impact.m_maxspeed != traffxml::kMaxspeedNone))
      {
        // process maxspeed
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
      }
      cit->second = sg;
    }
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

OpenLrV3TraffDecoder::OpenLrV3TraffDecoder(DataSource & dataSource, CountryInfoGetterFn countryInfoGetter,
                                           const CountryParentNameGetterFn & countryParentNameGetter,
                                           std::map<std::string, TraffMessage> & messageCache)
  : TraffDecoder(dataSource, countryInfoGetter, countryParentNameGetter, messageCache)
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
    if (location.m_ramps != traffxml::Ramps::None)
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
    for (size_t i = 0; i < segment.m_locationReference.m_points.size(); i++)
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

double RoutingTraffDecoder::TraffEstimator::GetUTurnPenalty(Purpose /* purpose */) const
{
  // Adds 2 minutes penalty for U-turn. The value is quite arbitrary
  // and needs to be properly selected after a number of real-world
  // experiments.
  return 2 * 60;  // seconds
}

double RoutingTraffDecoder::TraffEstimator::GetFerryLandingPenalty(Purpose purpose) const
{
  switch (purpose)
  {
  case Purpose::Weight: return 20 * 60;   // seconds
  case Purpose::ETA: return 20 * 60;      // seconds
  }
  UNREACHABLE();
}

double RoutingTraffDecoder::TraffEstimator::CalcSegmentWeight(routing::Segment const & segment, routing::RoadGeometry const & road, Purpose purpose) const
{
  double result = road.GetDistance(segment.GetSegmentIdx());

  if (!m_decoder.m_message || !m_decoder.m_message.value().m_location.value().m_roadClass)
    return result;

  std::optional<routing::HighwayType> highwayType = road.GetHighwayType();

  if (highwayType)
  {
    if (IsRamp(highwayType.value()) != (m_decoder.m_message.value().m_location.value().m_ramps != Ramps::None))
      // if one is a ramp and the other is not, treat it as a mismatch
      result *= kAttributePenalty;
    if (m_decoder.m_message.value().m_location.value().m_roadClass)
      // if the message specifies a road class, penalize mismatches
      result *= GetRoadClassPenalty(m_decoder.m_message.value().m_location.value().m_roadClass.value(),
                                    GetRoadClass(highwayType.value()));
  }
  else // road has no highway class
  {
    // we can’t determine if it is a ramp, penalize for mismatch
    result *= kAttributePenalty;
    if (m_decoder.m_message.value().m_location.value().m_roadClass)
      // we can’t determine if the road matches the required road class, treat it as mismatch
      result *= kAttributePenalty;
  }

  return result;
}

RoutingTraffDecoder::DecoderRouter::DecoderRouter(CountryParentNameGetterFn const & countryParentNameGetterFn,
                                                  routing::TCountryFileFn const & countryFileFn,
                                                  routing::CountryRectFn const & countryRectFn,
                                                  std::shared_ptr<routing::NumMwmIds> numMwmIds,
                                                  std::unique_ptr<m4::Tree<routing::NumMwmId>> numMwmTree,
                                                  DataSource & dataSource, RoutingTraffDecoder & decoder)
  : routing::IndexRouter(routing::VehicleType::Car /* VehicleType vehicleType */,
                         false /* bool loadAltitudes */,
                         countryParentNameGetterFn,
                         countryFileFn,
                         countryRectFn,
                         numMwmIds,
                         std::move(numMwmTree),
                         //std::nullopt /* std::optional<traffic::TrafficCache> const & trafficCache */,
                         std::make_shared<TraffEstimator>(&dataSource, numMwmIds, kOneMpSInKmpH /* maxWeighSpeedKMpH */,
                                                          routing::SpeedKMpH(kOneMpSInKmpH / kOffroadPenalty /* weight */,
                                                                             routing::kNotUsed /* eta */) /* offroadSpeedKMpH */,
                                                          decoder),
                         dataSource)
  //, m_directionsEngine(CreateDirectionsEngine(m_vehicleType, m_numMwmIds, m_dataSource)) // TODO we don’t need directions, can we disable that?
{}

RoutingTraffDecoder::RoutingTraffDecoder(DataSource & dataSource, CountryInfoGetterFn countryInfoGetter,
                                         const CountryParentNameGetterFn & countryParentNameGetter,
                                         std::map<std::string, TraffMessage> & messageCache)
  : TraffDecoder(dataSource, countryInfoGetter, countryParentNameGetter, messageCache)
{
  InitRouter();
}

bool RoutingTraffDecoder::InitRouter()
{
  if (m_router)
    return true;

  // code mostly from RoutingManager::SetRouterImpl(RouterType)
  /*
   * RoutingManager::SetRouterImpl(RouterType) calls m_delegate.RegisterCountryFilesOnRoute(numMwmIds).
   * m_delegate is the framework, and the routine cycles through the countries in storage.
   * As we don’t have access to storage, we get our country files from the data source.
   */
  std::vector<std::shared_ptr<MwmInfo>> mwmsInfo;
  m_dataSource.GetMwmsInfo(mwmsInfo);
  /* TODO this should include all countries (whether we have the MWM or not), except World and WorldCoasts.
   * Excluding World and WorldCoasts is important, else the router will return bogus routes.
   * Storage uses a string comparison for filtering, we do the same here.
  storage.ForEachCountry([&](storage::Country const & country)
  {
    numMwmIds->RegisterFile(country.GetFile());
  });
   */
  for (auto mwmInfo : mwmsInfo)
    if (!mwmInfo->GetCountryName().starts_with(WORLD_FILE_NAME))
      m_numMwmIds->RegisterFile(mwmInfo->GetLocalFile().GetCountryFile());

  if (m_numMwmIds->IsEmpty())
    return false;

  auto const countryFileGetter = [this](m2::PointD const & p) -> std::string {
    // TODO (@gorshenin): fix CountryInfoGetter to return CountryFile
    // instances instead of plain strings.
    return m_countryInfoGetterFn().GetRegionCountryId(p);
  };

  auto const getMwmRectByName = [this](std::string const & countryId) -> m2::RectD {
    return m_countryInfoGetterFn().GetLimitRectForLeaf(countryId);
  };

  m_router =
      make_unique<RoutingTraffDecoder::DecoderRouter>(m_countryParentNameGetterFn,
                                                      countryFileGetter, getMwmRectByName, m_numMwmIds,
                                                      routing::MakeNumMwmTree(*m_numMwmIds, m_countryInfoGetterFn()),
                                                      m_dataSource, *this);

  return true;
}

// Copied from AsyncRouter
// static
void RoutingTraffDecoder::LogCode(routing::RouterResultCode code, double const elapsedSec)
{
  switch (code)
  {
    case routing::RouterResultCode::StartPointNotFound:
      LOG(LWARNING, ("Can't find start or end node"));
      break;
    case routing::RouterResultCode::EndPointNotFound:
      LOG(LWARNING, ("Can't find end point node"));
      break;
    case routing::RouterResultCode::PointsInDifferentMWM:
      LOG(LWARNING, ("Points are in different MWMs"));
      break;
    case routing::RouterResultCode::RouteNotFound:
      LOG(LWARNING, ("Route not found"));
      break;
    case routing::RouterResultCode::RouteFileNotExist:
      LOG(LWARNING, ("There is no routing file"));
      break;
    case routing::RouterResultCode::NeedMoreMaps:
      LOG(LINFO,
          ("Routing can find a better way with additional maps, elapsed seconds:", elapsedSec));
      break;
    case routing::RouterResultCode::Cancelled:
      LOG(LINFO, ("Route calculation cancelled, elapsed seconds:", elapsedSec));
      break;
    case routing::RouterResultCode::NoError:
      LOG(LINFO, ("Route found, elapsed seconds:", elapsedSec));
      break;
    case routing::RouterResultCode::NoCurrentPosition:
      LOG(LINFO, ("No current position"));
      break;
    case routing::RouterResultCode::InconsistentMWMandRoute:
      LOG(LINFO, ("Inconsistent mwm and route"));
      break;
    case routing::RouterResultCode::InternalError:
      LOG(LINFO, ("Internal error"));
      break;
    case routing::RouterResultCode::FileTooOld:
      LOG(LINFO, ("File too old"));
      break;
    case routing::RouterResultCode::IntermediatePointNotFound:
      LOG(LWARNING, ("Can't find intermediate point node"));
      break;
    case routing::RouterResultCode::TransitRouteNotFoundNoNetwork:
      LOG(LWARNING, ("No transit route is found because there's no transit network in the mwm of "
                     "the route point"));
      break;
    case routing::RouterResultCode::TransitRouteNotFoundTooLongPedestrian:
      LOG(LWARNING, ("No transit route is found because pedestrian way is too long"));
      break;
    case routing::RouterResultCode::RouteNotFoundRedressRouteError:
      LOG(LWARNING, ("Route not found because of a redress route error"));
      break;
  case routing::RouterResultCode::HasWarnings:
      LOG(LINFO, ("Route has warnings, elapsed seconds:", elapsedSec));
      break;
  }
}

void RoutingTraffDecoder::AddDecodedSegment(traffxml::MultiMwmColoring & decoded, routing::Segment & segment)
{
  auto const countryFile = m_numMwmIds->GetFile(segment.GetMwmId());
  MwmSet::MwmId mwmId = m_dataSource.GetMwmIdByCountryFile(countryFile);

  auto const fid = segment.GetFeatureId();
  auto const sid = segment.GetSegmentIdx();
  uint8_t direction = segment.IsForward() ?
        traffic::TrafficInfo::RoadSegmentId::kForwardDirection :
        traffic::TrafficInfo::RoadSegmentId::kReverseDirection;

  decoded[mwmId][traffic::TrafficInfo::RoadSegmentId(fid, sid, direction)] = traffic::SpeedGroup::Unknown;
}

void RoutingTraffDecoder::DecodeLocationDirection(traffxml::TraffMessage & message,
                                                  traffxml::MultiMwmColoring & decoded, bool backwards)
{
  bool adjustToPrevRoute = false; // calculate a fresh route, no adjustments to previous one
  uint64_t routeId = 0; // used in callbacks to identify the route, we might not need it at all

  std::vector<m2::PointD> points;
  if (message.m_location.value().m_from)
    points.push_back(mercator::FromLatLon(message.m_location.value().m_from.value().m_coordinates));
  if (message.m_location.value().m_at)
    points.push_back(mercator::FromLatLon(message.m_location.value().m_at.value().m_coordinates));
  else if (message.m_location.value().m_via)
    points.push_back(mercator::FromLatLon(message.m_location.value().m_via.value().m_coordinates));
  if (message.m_location.value().m_to)
    points.push_back(mercator::FromLatLon(message.m_location.value().m_to.value().m_coordinates));
  if (backwards)
    std::reverse(points.begin(), points.end());
  // m_notVia is ignored as OpenLR does not support this functionality.
  CHECK_GREATER(points.size(), 1, ("At least two reference points must be given"));

  /*
   * startDirection is the direction of travel at start. Can be m2::PointD::Zero() to ignore
   * direction, or PositionAccumulator::GetDirection(), which basically returns the difference
   * between the last position and an earlier one (offset between two points from which the
   * direction of travel can be inferred).
   *
   * For our purposes, points[1] - points[0] would be as close as we could get to a start direction.
   * This would be accurate on very straight roads, less accurate on not-so-straight ones. However,
   * even on a near-straight road, the standard router (with the default EdgeEstimator) seemed quite
   * unimpressed by the direction and insisted on starting off on the carriageway closest to the
   * start point and sending us on a huge detour, instead of taking the direct route on the opposite
   * carriageway.
   */
  m2::PointD startDirection = m2::PointD::Zero();

  routing::Checkpoints checkpoints(std::move(points));

  /*
   * This code is mostly lifted from AsyncRouter::CalculateRoute(), both with and without arguments.
   */

  /*
   * AsyncRouter::CalculateRoute() has a `DelegateProxy`, which is private. We just need the return
   * value of GetDelegate(), which is a `routing::RouterDelegate`, so use that instead. We don’t
   * need any of the callbacks, therefore we don’t set them.
   */
  routing::RouterDelegate delegate;
  delegate.SetTimeout(kRouterTimeoutSec);

  if (!m_router && !InitRouter())
    return;

  /*
   * TODO is that for following a track? If so, can we use that with just 2–3 reference points?
   * – Doesn’t look like it, m_guides only seems to get used in test functions
   */
  //router->SetGuides(std::move(m_guides));
  //m_guides.clear();

  auto route = std::make_shared<routing::Route>(m_router->GetName(), routeId);
  routing::RouterResultCode code;

  base::Timer timer;
  double elapsedSec = 0.0;

  try
  {
    LOG(LINFO, ("Calculating the route of direct length", checkpoints.GetSummaryLengthBetweenPointsMeters(),
                "m. checkpoints:", checkpoints, "startDirection:", startDirection, "router name:", m_router->GetName()));

    // Run basic request.
    code = m_router->CalculateRoute(checkpoints, startDirection, adjustToPrevRoute,
                                  delegate, *route);
    m_router->SetGuides({});
    elapsedSec = timer.ElapsedSeconds(); // routing time
    LogCode(code, elapsedSec);
    LOG(LINFO, ("ETA:", route->GetTotalTimeSec(), "sec."));
  }
  catch (RootException const & e)
  {
    code = routing::RouterResultCode::InternalError;
    LOG(LERROR, ("Exception happened while calculating route:", e.Msg()));
    return;
  }

  if (code == routing::RouterResultCode::NoError)
  {
    std::vector<routing::RouteSegment> rsegments(route->GetRouteSegments());

    // erase leading and trailing fake segments
    while(!rsegments.empty() && rsegments.front().GetSegment().GetMwmId() == routing::kFakeNumMwmId)
      rsegments.erase(rsegments.begin());
    while(!rsegments.empty() && rsegments.back().GetSegment().GetMwmId() == routing::kFakeNumMwmId)
      rsegments.erase(rsegments.end());

    if (!backwards && message.m_location.value().m_at && !message.m_location.value().m_to)
      // from–at in forward direction, add last segment
      AddDecodedSegment(decoded, rsegments.back().GetSegment());
    else if (!backwards && message.m_location.value().m_at && !message.m_location.value().m_from)
      // at–to in forward direction, add last segment
      AddDecodedSegment(decoded, rsegments.front().GetSegment());
    else if (backwards && message.m_location.value().m_at && !message.m_location.value().m_to)
      // from–at in backward direction, add first segment
      AddDecodedSegment(decoded, rsegments.front().GetSegment());
    else if (backwards && message.m_location.value().m_at && !message.m_location.value().m_from)
      // at–to in backward direction, add first segment
      AddDecodedSegment(decoded, rsegments.back().GetSegment());
    else if (message.m_location.value().m_at)
    {
      // from–at–to, find closest segment
      ms::LatLon at = message.m_location.value().m_at.value().m_coordinates;
      routing::RouteSegment & closestRSegment = rsegments.front();
      double closestDist = ms::DistanceOnEarth(at, mercator::ToLatLon(closestRSegment.GetJunction().GetPoint()));

      for (auto rsegment : rsegments)
      {
        // If we have more than two checkpoints, fake segments can occur in the middle, skip them.
        if (rsegment.GetSegment().GetMwmId() == routing::kFakeNumMwmId)
          continue;

        double dist = ms::DistanceOnEarth(at, mercator::ToLatLon(rsegment.GetJunction().GetPoint()));
        if (dist < closestDist)
        {
          closestRSegment = rsegment;
          closestDist = dist;
        }
      }
      AddDecodedSegment(decoded, closestRSegment.GetSegment());
    }
    else
      // from–[via]–to, add all real segments
      for (auto rsegment : rsegments)
      {
        routing::Segment & segment = rsegment.GetSegment();

        // If we have more than two checkpoints, fake segments can occur in the middle, skip them.
        if (segment.GetMwmId() == routing::kFakeNumMwmId)
          continue;

        AddDecodedSegment(decoded, segment);
      }
  }
}

void RoutingTraffDecoder::DecodeLocation(traffxml::TraffMessage & message, traffxml::MultiMwmColoring & decoded)
{
  ASSERT(message.m_location, ("Message has no location"));
  decoded.clear();

  m_message = message;

  int dirs = (message.m_location.value().m_directionality == Directionality::BothDirections) ? 2 : 1;
  for (int dir = 0; dir < dirs; dir++)
    DecodeLocationDirection(message, decoded, dir == 0 ? false : true /* backwards */);

  m_message = std::nullopt;
}

traffxml::RoadClass GetRoadClass(routing::HighwayType highwayType)
{
  switch(highwayType)
  {
  /*
   * Parallel carriageways may be tagged as link, hence consider them equivalent to the underlying
   * highway type.
   */
  case routing::HighwayType::HighwayMotorway:
  case routing::HighwayType::HighwayMotorwayLink:
    return traffxml::RoadClass::Motorway;
  case routing::HighwayType::HighwayTrunk:
  case routing::HighwayType::HighwayTrunkLink:
    return traffxml::RoadClass::Trunk;
  case routing::HighwayType::HighwayPrimary:
  case routing::HighwayType::HighwayPrimaryLink:
    return traffxml::RoadClass::Primary;
  case routing::HighwayType::HighwaySecondary:
  case routing::HighwayType::HighwaySecondaryLink:
    return traffxml::RoadClass::Secondary;
  case routing::HighwayType::HighwayTertiary:
  case routing::HighwayType::HighwayTertiaryLink:
    return traffxml::RoadClass::Tertiary;
  default:
    return traffxml::RoadClass::Other;
  }
}

/**
 * @brief Returns the penalty factor for road class match/mismatch.
 *
 * If `lhs` and `rhs` are identical, the penalty factor is 1 (no penalty). If they are adjacent road
 * classes (e.g. trunk and primary), the penalty factor is `kReducedAttributePenalty`, in all other
 * cases it is `kAttributePenalty`.
 *
 * @param lhs
 * @param rhs
 * @return The penalty factor, see above.
 */
double GetRoadClassPenalty(traffxml::RoadClass lhs, traffxml::RoadClass rhs)
{
  if (lhs == rhs)
    return 1;
  switch (lhs)
  {
  case traffxml::RoadClass::Motorway:
    if (rhs == traffxml::RoadClass::Trunk)
      return kReducedAttributePenalty;
    else
      return kAttributePenalty;
  case traffxml::RoadClass::Trunk:
    if (rhs == traffxml::RoadClass::Motorway || rhs == traffxml::RoadClass::Primary)
      return kReducedAttributePenalty;
    else
      return kAttributePenalty;
  case traffxml::RoadClass::Primary:
    if (rhs == traffxml::RoadClass::Trunk || rhs == traffxml::RoadClass::Secondary)
      return kReducedAttributePenalty;
    else
      return kAttributePenalty;
  case traffxml::RoadClass::Secondary:
    if (rhs == traffxml::RoadClass::Primary || rhs == traffxml::RoadClass::Tertiary)
      return kReducedAttributePenalty;
    else
      return kAttributePenalty;
  case traffxml::RoadClass::Tertiary:
    if (rhs == traffxml::RoadClass::Secondary || rhs == traffxml::RoadClass::Other)
      return kReducedAttributePenalty;
    else
      return kAttributePenalty;
  case traffxml::RoadClass::Other:
    if (rhs == traffxml::RoadClass::Tertiary)
      return kReducedAttributePenalty;
    else
      return kAttributePenalty;
  default:
    UNREACHABLE();
  }
}

bool IsRamp(routing::HighwayType highwayType)
{
  switch(highwayType)
  {
  case routing::HighwayType::HighwayMotorwayLink:
  case routing::HighwayType::HighwayTrunkLink:
  case routing::HighwayType::HighwayPrimaryLink:
  case routing::HighwayType::HighwaySecondaryLink:
  case routing::HighwayType::HighwayTertiaryLink:
    return true;
  default:
    return false;
  }
}
}  // namespace traffxml
