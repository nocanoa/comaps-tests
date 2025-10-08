#pragma once

#include "routing/road_penalty.hpp"
#include "routing/road_point.hpp"

#include "coding/reader.hpp"
#include "coding/write_to_sink.hpp"

#include "base/assert.hpp"

#include <cstdint>
#include <vector>

namespace routing
{
class RoadPenaltySerializer final
{
public:
  using PointToPenalty = RoadPenalty::PointToPenalty;
  using Penalty = RoadPenalty::Penalty;

  RoadPenaltySerializer() = delete;

  template <typename Sink>
  static void Serialize(Sink & sink, RoadPenalty const & roadPenalty)
  {
    SerializePenalties(sink, roadPenalty.GetPointToPenalty());
  }

  template <typename Source>
  static void Deserialize(Source & src, RoadPenalty & roadPenalty, VehicleType vehicleType)
  {
    PointToPenalty pointToPenalty;
    DeserializePenalties(src, pointToPenalty, vehicleType);

    roadPenalty.SetPointPenalties(std::move(pointToPenalty));
  }

private:
  template <typename Sink>
  static void SerializePenalty(Sink & sink, Penalty const & penalty)
  {
    WriteToSink(sink, static_cast<uint8_t>(penalty.m_type));
    // Time is derived from type + vehicle when loading
  }

  template <typename Source>
  static void DeserializePenalty(Source & src, Penalty & penalty, VehicleType vehicleType)
  {
    penalty.m_type = static_cast<RoadPenalty::Type>(ReadPrimitiveFromSource<uint8_t>(src));
    penalty.m_timeSeconds = Penalty::GetTimePenalty(penalty.m_type, vehicleType);
  }

  template <typename Sink, typename PenaltyMap>
  static void SerializePenalties(Sink & sink, PenaltyMap const & penalties)
  {
    WriteToSink(sink, static_cast<uint32_t>(penalties.size()));
    for (auto const & [key, penalty] : penalties)
    {
      SerializeKey(sink, key);
      SerializePenalty(sink, penalty);
    }
  }

  template <typename Source, typename PenaltyMap>
  static void DeserializePenalties(Source & src, PenaltyMap & penalties, VehicleType vehicleType)
  {
    uint32_t const size = ReadPrimitiveFromSource<uint32_t>(src);
    penalties.reserve(size);

    for (uint32_t i = 0; i < size; ++i)
    {
      typename PenaltyMap::key_type key;
      DeserializeKey(src, key);

      Penalty penalty;
      DeserializePenalty(src, penalty, vehicleType);

      penalties[key] = penalty;
    }
  }

  template <typename Sink>
  static void SerializeKey(Sink & sink, uint32_t key)
  {
    WriteToSink(sink, key);
  }

  template <typename Source>
  static void DeserializeKey(Source & src, uint32_t & key)
  {
    key = ReadPrimitiveFromSource<uint32_t>(src);
  }

  template <typename Sink>
  static void SerializeKey(Sink & sink, RoadPoint const & key)
  {
    WriteToSink(sink, key.GetFeatureId());
    WriteToSink(sink, key.GetPointId());
  }

  template <typename Source>
  static void DeserializeKey(Source & src, RoadPoint & key)
  {
    uint32_t featureId = ReadPrimitiveFromSource<uint32_t>(src);
    uint32_t pointId = ReadPrimitiveFromSource<uint32_t>(src);
    key = RoadPoint(featureId, pointId);
  }
};
}  // namespace routing
