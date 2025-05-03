#include "openlr/openlr_model.hpp"

#include "geometry/mercator.hpp"

#include "base/assert.hpp"

using namespace std;

namespace openlr
{
// LinearSegment -----------------------------------------------------------------------------------
vector<m2::PointD> LinearSegment::GetMercatorPoints() const
{
  vector<m2::PointD> points;
  points.reserve(m_locationReference.m_points.size());
  for (auto const & point : m_locationReference.m_points)
    points.push_back(mercator::FromLatLon(point.m_latLon));
  return points;
}

vector<LocationReferencePoint> const & LinearSegment::GetLRPs() const
{
  return m_locationReference.m_points;
}

vector<LocationReferencePoint> & LinearSegment::GetLRPs()
{
  return m_locationReference.m_points;
}

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

string DebugPrint(FunctionalRoadClass frc)
{
  switch (frc)
  {
  case FunctionalRoadClass::FRC0: return "FRC0";
  case FunctionalRoadClass::FRC1: return "FRC1";
  case FunctionalRoadClass::FRC2: return "FRC2";
  case FunctionalRoadClass::FRC3: return "FRC3";
  case FunctionalRoadClass::FRC4: return "FRC4";
  case FunctionalRoadClass::FRC5: return "FRC5";
  case FunctionalRoadClass::FRC6: return "FRC6";
  case FunctionalRoadClass::FRC7: return "FRC7";
  case FunctionalRoadClass::NotAValue: return "NotAValue";
  }
  UNREACHABLE();
}
}  // namespace openlr
