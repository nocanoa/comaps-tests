#pragma once

#include "routing/road_point.hpp"
#include "routing/vehicle_mask.hpp"

#include <optional>
#include <string>
#include <vector>

#include "3party/skarupke/flat_hash_map.hpp"

namespace routing
{
// This class provides information about road penalties (time delays)
// that are separate from access restrictions. Examples include:
// - Speed bumps and traffic calming measures
// - Traffic signals
// - Gates that take time to open
// One instance of RoadPenalty holds information about one mwm.
//
// FILE FORMATS:
//
// 1. Intermediate file (from RoadPenaltyCollector::Save()):
//    [uint64_t] Number of ways with penalties
//    For each way:
//      [uint64_t] Way ID
//      [uint32_t] Number of nodes with penalties
//      For each node:
//        [uint32_t] Node index within way
//        [uint32_t] Coordinate X
//        [uint32_t] Coordinate Y
//        [VehicleMask] Vehicle mask (uint32_t)
//        [uint8_t] Penalty type
//
// 2. Final MWM file:
//    [uint32_t] Number of vehicle types (4)
//    For each vehicle type:
//      [uint32_t] Number of point penalties
//      For each penalty:
//        [uint32_t] Feature ID
//        [uint32_t] Point ID
//        [uint8_t] Penalty type (time derived from type + vehicle)
class RoadPenalty final
{
public:
  // Types of penalties that can be applied to roads/nodes
  enum class Type : uint8_t
  {
    None = 0,

    // Traffic calming devices with different severities
    SmallCalming,   // Traffic islands
    MediumCalming,  // Cushions, chicanes, tables, choker
    LargeCalming,   // Humps, bumps

    // Gates and barriers
    Gate,  // Emergency gates, barrier gates

    // Traffic control
    UncontrolledJunction,
    ControlledJunction,

    // The number of different penalty types
    Count
  };

  struct Penalty
  {
    Type m_type = Type::None;
    uint16_t m_timeSeconds = 0;  // Time penalty in seconds (0-65535)

    Penalty() = default;

    // Constructor that derives time from type and vehicle
    Penalty(Type type, VehicleType vehicleType) : m_type(type), m_timeSeconds(GetTimePenalty(type, vehicleType)) {}

    bool operator==(Penalty const & rhs) const { return m_type == rhs.m_type && m_timeSeconds == rhs.m_timeSeconds; }

    bool operator!=(Penalty const & rhs) const { return !(*this == rhs); }

    // Get default time penalty for a given type and vehicle
    static uint16_t GetTimePenalty(Type type, VehicleType vehicleType)
    {
      // Penalty times lookup table [Type][VehicleType]
      // Rows: penalty types (None, SmallCalming, MediumCalming, LargeCalming, Gate, Uncontrolled, Controlled)
      // Columns: vehicle types (Pedestrian, Bicycle, Car, Transit)
      static constexpr uint16_t kPenaltyTimes[7][4] = {
          // Ped, Bic, Car, Tran
          {0, 0, 0, 0},     // None
          {0, 0, 3, 0},     // SmallCalming
          {0, 0, 5, 0},     // MediumCalming
          {0, 0, 7, 0},     // LargeCalming
          {10, 10, 30, 0},  // Gate
          {0, 10, 5, 0},    // Uncontrolled Junction
          {10, 15, 10, 0},  // Controlled Junction
      };

      if (type >= Type::Count || vehicleType >= VehicleType::Count)
        return 0;

      return kPenaltyTimes[static_cast<size_t>(type)][static_cast<size_t>(vehicleType)];
    }
  };

  using PointToPenalty = ska::flat_hash_map<RoadPoint, Penalty, RoadPoint::Hash>;

  RoadPenalty() = default;

  PointToPenalty const & GetPointToPenalty() const { return m_pointToPenalty; }

  std::optional<Penalty> GetPenalty(RoadPoint const & point) const;

  void SetPointPenalties(PointToPenalty && penalties) { m_pointToPenalty = std::move(penalties); }

  bool operator==(RoadPenalty const & rhs) const;

private:
  PointToPenalty m_pointToPenalty;
};

std::string ToString(RoadPenalty::Type type);
void FromString(std::string_view s, RoadPenalty::Type & result);

std::string DebugPrint(RoadPenalty::Type type);
std::string DebugPrint(RoadPenalty::Penalty const & penalty);
std::string DebugPrint(RoadPenalty const & roadPenalty);
}  // namespace routing
