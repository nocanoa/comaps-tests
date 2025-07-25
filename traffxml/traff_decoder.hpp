#pragma once

#include "traffxml/traff_model.hpp"

#include "indexer/data_source.hpp"
#include "indexer/mwm_set.hpp"

// Only needed for OpenlrTraffDecoder, see below
#if 0
#include "openlr/openlr_decoder.hpp"
#include "openlr/openlr_model.hpp"
#endif

#include "routing/index_router.hpp"
#include "routing/regions_decl.hpp"
#include "routing/router.hpp"

#include "routing_common/num_mwm_id.hpp"

#include "storage/country_info_getter.hpp"

#include <optional>

namespace traffxml
{
/**
 * @brief Abstract base class for all TraFF decoder implementations.
 *
 * At this point, `TraffDecoder` is single-threaded and not guaranteed to be thread-safe. This means
 * that all `TraffDecoder` operations should be limited to one thread or use appropriate thread
 * synchronization mechanisms. In particular, calling `DecodeMessage()` concurrently from multiple
 * threads is not supported.
 */
class TraffDecoder
{
public:
  using CountryInfoGetterFn = std::function<storage::CountryInfoGetter const &()>;
  using CountryParentNameGetterFn = std::function<std::string(std::string const &)>;

  TraffDecoder(DataSource & dataSource, CountryInfoGetterFn countryInfoGetter,
               const CountryParentNameGetterFn & countryParentNameGetter,
               std::map<std::string, traffxml::TraffMessage> & messageCache);

  /**
   * @brief Decodes a single message to its segments and their speed groups.
   *
   * This method is not guaranteed to be thread-safe. All calls to this method should either be
   * strictly limited to one designated thread, or be synchronized using an appropriate mechanism.
   *
   * In addition to the above, this method may access the message cache which was passed to the
   * constructor. This is not thread-safe and needs to be synchronized, unless all other operations
   * on the message cache are guaranteed to happen on the same thread that called this method.
   *
   * @param message The message to decode.
   */
  void DecodeMessage(traffxml::TraffMessage & message);

protected:
  /**
   * @brief Decodes a TraFF location.
   *
   * @param message The message to decode.
   * @param decoded Receives the decoded segments. The speed group will be `Unknown`.
   */
  virtual void DecodeLocation(traffxml::TraffMessage & message, traffxml::MultiMwmColoring & decoded) = 0;

  /**
   * @brief Applies traffic impact to a decoded TraFF location.
   *
   * Applying impact sets the corresponding speed groups of the decoded segments. Existing speed groups will be overwritten.
   *
   * @param impact The traffic impact to apply.
   * @param decoded The decoded segments.
   */
  void ApplyTrafficImpact(traffxml::TrafficImpact & impact, traffxml::MultiMwmColoring & decoded);

  DataSource & m_dataSource;
  CountryInfoGetterFn m_countryInfoGetterFn;
  CountryParentNameGetterFn m_countryParentNameGetterFn;

  /**
   * @brief Cache of all currently active TraFF messages.
   *
   * Keys are message IDs, values are messages.
   */
  std::map<std::string, traffxml::TraffMessage> & m_messageCache;

private:
};

// Disabled for now, as the OpenLR-based decoder is slow, buggy and not well suited to the task.
#if 0
/**
 * @brief A `TraffDecoder` implementation which internally uses the version 3 OpenLR decoder.
 */
class OpenLrV3TraffDecoder : public TraffDecoder
{
public:
  OpenLrV3TraffDecoder(DataSource & dataSource, CountryInfoGetterFn countryInfoGetter,
                       const CountryParentNameGetterFn & countryParentNameGetter,
                       std::map<std::string, traffxml::TraffMessage> & messageCache);

protected:
  /**
   * @brief Decodes a TraFF location.
   *
   * @param message The message to decode.
   * @param decoded Receives the decoded segments. The speed group will be `Unknown`.
   */
  void DecodeLocation(traffxml::TraffMessage & message, traffxml::MultiMwmColoring & decoded) override;

private:
  /**
   * @brief Returns the OpenLR functional road class (FRC) matching a TraFF road class.
   *
   * @param roadClass The TraFF road class.
   * @return The FRC.
   */
  static openlr::FunctionalRoadClass GetRoadClassFrc(std::optional<RoadClass> & roadClass);

  /**
   * @brief Guess the distance between two points.
   *
   * If both `p1` and `p2` have the `distance` attribute set, the difference between these two is
   * evaluated. If it is within a certain tolerance margin of the direct distance between the two
   * points, this value is returned. Otherwise, the distance is calculated from direct distance,
   * multiplied with a tolerance factor to account for the fact that the road is not always a
   * straight line.
   *
   * The result can be used to provide some semi-valid DNP values.
   *
   * @param p1 The first point.
   * @param p2 The second point.
   * @return The approximate distance on the ground, in meters.
   */
  static uint32_t GuessDnp(Point & p1, Point & p2);

  /**
   * @brief Converts a TraFF point to an OpenLR location reference point.
   *
   * Only coordinates are populated.
   *
   * @param point The point
   * @return An OpenLR LRP with the coordinates of the point.
   */
  static openlr::LocationReferencePoint PointToLrp(Point & point);

  /**
   * @brief Converts a TraFF location to an OpenLR linear location reference.
   *
   * @param location The location
   * @param backwards If true, gnerates a linear location reference for the backwards direction,
   * with the order of points reversed.
   * @return An OpenLR linear location reference which corresponds to the location.
   */
  static openlr::LinearLocationReference TraffLocationToLinearLocationReference(TraffLocation & location, bool backwards);

  /**
   * @brief Converts a TraFF location to a vector of OpenLR segments.
   *
   * Depending on the directionality, the resulting vector will hold one or two elements: one for
   * the forward direction, and for bidirectional locations, a second one for the backward
   * direction.
   *
   * @param location The location
   * @param messageId The message ID
   * @return A vector holding the resulting OpenLR segments.
   */
  static std::vector<openlr::LinearSegment> TraffLocationToOpenLrSegments(TraffLocation & location, std::string & messageId);

  /**
   * @brief The OpenLR decoder instance.
   *
   * Used to decode TraFF locations into road segments on the map.
   */
  openlr::OpenLRDecoder m_openLrDecoder;
};
#endif

/**
 * @brief A `TraffDecoder` implementation which internally uses the routing engine.
 */
class RoutingTraffDecoder : public TraffDecoder,
                            public MwmSet::Observer
{
public:
  class DecoderRouter : public routing::IndexRouter
  {
  public:
    /**
     * @brief Creates a new `DecoderRouter` instance.
     *
     * @param countryParentNameGetterFn Function which converts a country name into the name of its parent country)
     * @param countryFileFn Function which converts a pointer to its country name
     * @param countryRectFn Function which returns the rect for a country
     * @param numMwmIds
     * @param numMwmTree
     * @param trafficCache The traffic cache (used only if `vehicleType` is `VehicleType::Car`)
     * @param dataSource The MWM data source
     * @param decoder The `TraffDecoder` instance to which this router instance is coupled
     */
    DecoderRouter(CountryParentNameGetterFn const & countryParentNameGetterFn,
                  routing::TCountryFileFn const & countryFileFn,
                  routing::CountryRectFn const & countryRectFn,
                  std::shared_ptr<routing::NumMwmIds> numMwmIds,
                  std::unique_ptr<m4::Tree<routing::NumMwmId>> numMwmTree,
                  DataSource & dataSource, RoutingTraffDecoder & decoder);
  protected:
    /**
     * @brief Whether the set of fake endings generated for the check points is restricted.
     *
     * The return value is used internally when snapping checkpoints to edges. If this function
     * returns true, this instructs the `PointsOnEdgesSnapping` instance to consider only edges which
     * are not fenced off, i.e. can be reached from the respective checkpoint without crossing any
     * other edges. If it returns false, this restriction does not apply, and all nearby edges are
     * considered.
     *
     * Restricting the set of fake endings in this manner decreases the options considered for routing
     * and thus processing time, which is desirable for regular routing and has no side effects.
     * For TraFF location matching, simplification has undesirable side effects: if reference points
     * are located on one side of the road, the other carriageway may not be considered. This would
     * lead to situations like these:
     *
     * --<--<-+<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<+-<--<--
     * -->-->-+>==>==>==>==>==>==>-->-->-->-->-->-->-->-->-->-->-->==>==>==>==>==>==>==>==>+->-->--
     *                          *<                               <*
     *
     * (-- carriageway, + junction, < > direction, *< end point, <* start point, == route)
     *
     * To avoid this, the `DecoderRouter` implementation always returns false.
     */
    /**
     * @brief Returns the mode in which the router is operating.
     *
     * The `DecoderRouter` always returns `Mode::Decoding`.
     *
     * In navigation mode, the router may exit with `RouterResultCode::NeedMoreMaps` if it determines
     * that a better route can be calculated with additional maps. When snapping endpoints to edges,
     * it will consider only edges which are not “fenced off” by other edges, i.e. which can be
     * reached from the endpoint without crossing other edges. This decreases the number of fake
     * endings and thus speeds up routing, without any undesirable side effects for that use case.
     *
     * Asking the user to download extra maps is neither practical for a TraFF decoder which runs in
     * the background and may decode many locations, one by one, nor is it needed (if maps are
     * missing, we do not need to decode traffic reports for them).
     *
     * Eliminating fenced-off edges from the snapping candidates has an undesirable side effect for
     * TraFF location decoding on dual-carriageway roads: if the reference points are outside the
     * carriageways, only one direction gets considered for snapping, as the opposite direction is
     * fenced off by it. This may lead to situations like these:
     *
     * ~~~
     * --<--<-+<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<==<+-<--<--
     * -->-->-+>==>==>==>==>==>==>-->-->-->-->-->-->-->-->-->-->-->==>==>==>==>==>==>==>==>+->-->--
     *                          |<                               <|
     *
     * (-- carriageway, + junction, < > direction, |< end point, <| start point, == route)
     * ~~~
     *
     * Therefore, in decoding mode, the router will never exit with `RouterResultCode::NeedMoreMaps`
     * but tries to find a route with the existing maps, or exits without a route. When snapping
     * endpoints to edges, it considers all edges within the given radius, fenced off or not.
     */
    IndexRouter::Mode GetMode() { return IndexRouter::Mode::Decoding; }

  private:
  };

  class TraffEstimator final : public routing::EdgeEstimator
  {
  public:
    TraffEstimator(DataSource * dataSourcePtr, std::shared_ptr<routing::NumMwmIds> numMwmIds,
                   double maxWeightSpeedKMpH,
                   routing::SpeedKMpH const & offroadSpeedKMpH,
                   RoutingTraffDecoder & decoder)
      : EdgeEstimator(maxWeightSpeedKMpH, offroadSpeedKMpH, dataSourcePtr, numMwmIds)
      , m_decoder(decoder)
    {
    }

    // EdgeEstimator overrides:

    /**
     * @brief Estimates travel time between two points along a direct fake edge.
     *
     * Estimates time in seconds it takes to go from point `from` to point `to` along direct fake edge.
     *
     * @param from The start point.
     * @param to The destination point.
     * @param purpose The purpose for which the result is to be used.
     * @return Travel time in seconds.
     */
    double CalcOffroad(ms::LatLon const & from, ms::LatLon const & to, Purpose purpose) const override;
    double CalcSegmentWeight(routing::Segment const & segment, routing::RoadGeometry const & road, Purpose purpose) const override;
    double GetUTurnPenalty(Purpose /* purpose */) const override;
    double GetFerryLandingPenalty(Purpose purpose) const override;

  private:
    RoutingTraffDecoder & m_decoder;
  };

  RoutingTraffDecoder(DataSource & dataSource, CountryInfoGetterFn countryInfoGetter,
                      const CountryParentNameGetterFn & countryParentNameGetter,
                      std::map<std::string, traffxml::TraffMessage> & messageCache);

  /**
   * @brief Called when a map is registered for the first time and can be used.
   */
  void OnMapRegistered(platform::LocalCountryFile const & localFile) override;

  /**
   * @brief Called when a map is deregistered and can no longer be used.
   *
   * This implementation does nothing, as `NumMwmIds` does not support removal.
   */
  virtual void OnMapDeregistered(platform::LocalCountryFile const & /* localFile */) override {}

protected:
  /**
   * @brief Initializes the router.
   *
   * This is usually done in the constructor but fails if no maps are loaded (attempting to
   * construct a router without maps results in a crash, hence we check for maps and exit with an
   * error if we have none). It can be repeated any time.
   *
   * Attempting to initialize a router which has already been succesfully initialized is a no-op. It
   * will be reported as success.
   *
   * @return true if successful, false if not.
   */
  bool InitRouter();

  /**
   * @brief Adds a segment to the decoded segments.
   *
   * @param decoded The decoded segments.
   * @param segment The segment to add.
   */
  void AddDecodedSegment(traffxml::MultiMwmColoring & decoded, routing::Segment & segment);

  /**
   * @brief Decodes one direction of a TraFF location.
   *
   * @param message The message to decode.
   * @param decoded Receives the decoded segments. The speed group will be `Unknown`.
   * @param backwards If true, decode the backward direction, else the forward direction.
   */
  void DecodeLocationDirection(traffxml::TraffMessage & message,
                               traffxml::MultiMwmColoring & decoded, bool backwards);

  /**
   * @brief Decodes a TraFF location.
   *
   * @param message The message to decode.
   * @param decoded Receives the decoded segments. The speed group will be `Unknown`.
   */
  void DecodeLocation(traffxml::TraffMessage & message, traffxml::MultiMwmColoring & decoded) override;

private:
  static void LogCode(routing::RouterResultCode code, double const elapsedSec);

  /**
   * @brief Mutex for access to shared members.
   *
   * This is to prevent adding newly-registered maps while the router is in use.
   *
   * @todo As per the `MwmSet::Observer` documentation, implementations should be quick and lean,
   * as they may be called from any thread. Locking a mutex may be in conflict with this, as it may
   * mean locking up the caller while a location is being decoded.
   */
  std::mutex m_mutex;

  std::shared_ptr<routing::NumMwmIds> m_numMwmIds = std::make_shared<routing::NumMwmIds>();
  std::unique_ptr<routing::IRouter> m_router;
  std::optional<traffxml::TraffMessage> m_message = std::nullopt;
};

/**
 * @brief The default TraFF decoder implementation, recommended for production use.
 */
//using DefaultTraffDecoder = OpenLrV3TraffDecoder;
using DefaultTraffDecoder = RoutingTraffDecoder;

traffxml::RoadClass GetRoadClass(routing::HighwayType highwayType);
double GetRoadClassPenalty(traffxml::RoadClass lhs, traffxml::RoadClass rhs);
bool IsRamp(routing::HighwayType highwayType);
}  // namespace traffxml
