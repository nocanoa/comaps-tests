#include "routing/road_penalty.hpp"

#include "base/assert.hpp"

#include <sstream>

namespace routing
{
namespace penalty_impl
{
// Array of strings must be in the same order as RoadPenalty::Type enum
std::string const kNames[] = {"None", "SmallCalming",         "MediumCalming",      "LargeCalming",
                              "Gate", "UncontrolledJunction", "ControlledJunction", "Count"};
}  // namespace penalty_impl

std::optional<RoadPenalty::Penalty> RoadPenalty::GetPenalty(RoadPoint const & point) const
{
  auto const it = m_pointToPenalty.find(point);
  if (it != m_pointToPenalty.end())
    return it->second;
  return {};
}

bool RoadPenalty::operator==(RoadPenalty const & rhs) const
{
  return m_pointToPenalty == rhs.m_pointToPenalty;
}

std::string ToString(RoadPenalty::Type type)
{
  if (type <= RoadPenalty::Type::Count)
    return penalty_impl::kNames[static_cast<size_t>(type)];
  ASSERT(false, ("Bad RoadPenalty::Type", static_cast<size_t>(type)));
  return "Bad RoadPenalty::Type";
}

void FromString(std::string_view s, RoadPenalty::Type & result)
{
  for (size_t i = 0; i <= static_cast<size_t>(RoadPenalty::Type::Count); ++i)
  {
    if (s == penalty_impl::kNames[i])
    {
      result = static_cast<RoadPenalty::Type>(i);
      return;
    }
  }
  ASSERT(false, ("Could not read RoadPenalty::Type from string", s));
}

std::string DebugPrint(RoadPenalty::Type type)
{
  return ToString(type);
}

std::string DebugPrint(RoadPenalty::Penalty const & penalty)
{
  std::ostringstream oss;
  oss << "Penalty(" << DebugPrint(penalty.m_type) << ", " << penalty.m_timeSeconds << "s)";
  return oss.str();
}

std::string DebugPrint(RoadPenalty const & roadPenalty)
{
  size_t constexpr kMaxKV = 10;
  std::ostringstream oss;
  oss << "RoadPenalty { PointToPenalty: [";
  size_t i = 0;
  for (auto const & kv : roadPenalty.GetPointToPenalty())
  {
    if (i > 0)
      oss << ", ";
    oss << "(" << DebugPrint(kv.first) << ", " << DebugPrint(kv.second) << ")";
    ++i;
    if (i == kMaxKV)
      break;
  }
  if (roadPenalty.GetPointToPenalty().size() > kMaxKV)
    oss << ", ...";
  oss << "] }";
  return oss.str();
}
}  // namespace routing
