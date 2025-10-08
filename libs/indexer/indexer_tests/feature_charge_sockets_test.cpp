#include "indexer/feature_charge_sockets.hpp"

#include "testing/testing.hpp"

namespace feature_charge_sockets_test
{

ChargeSocketsHelper FromKVs(OSMKeyValues const & kvs)
{
  ChargeSocketsHelper helper;
  for (auto const & kv : kvs)
    helper.AggregateChargeSocketKey(kv.first, kv.second);
  return helper;
}

UNIT_TEST(ChargeSockets_Processing)
{
  ///////////////////////////////////////////////////////////
  // Internal model consistency

  // valid socket lists
  TEST_EQUAL(ChargeSocketsHelper("type2||").GetSockets().size(), 1, ());
  TEST_EQUAL(ChargeSocketsHelper("type2|1|50;type2|2|43").GetSockets().size(), 2, ());
  TEST_EQUAL(ChargeSocketsHelper("type2|1|50;type2|2|43;type1||").GetSockets().size(), 3, ());
  TEST_EQUAL(ChargeSocketsHelper("type2||;type2_cable||;type2_combo||;type1||;nacs||;chademo||").GetSockets().size(), 6,
             ());
  TEST_EQUAL(ChargeSocketsHelper("unknown|2|150").GetSockets().size(), 1, ());
  TEST_EQUAL(ChargeSocketsHelper("||").GetSockets().size(), 1, ());

  // invalid/partially valid socket lists
  TEST_EQUAL(ChargeSocketsHelper("type2|").GetSockets().size(), 0, ());
  TEST_EQUAL(ChargeSocketsHelper("type2|;|;|||;|45||").GetSockets().size(), 0, ());
  TEST_EQUAL(ChargeSocketsHelper("type2|;type2_cable|1|50").GetSockets().size(), 1, ());

  TEST(ChargeSocketsHelper("type2||").ToString() != "", ("ChargeSocketsHelper::ToString returned an empty string!"));

  TEST_EQUAL(ChargeSocketsHelper("||").ToString(), "unknown||", ());
  TEST_EQUAL(ChargeSocketsHelper("this_type_does_not_exist||").ToString(), "unknown||", ());
  TEST_EQUAL(ChargeSocketsHelper("type2||").ToString(), "type2||", ());
  TEST_EQUAL(ChargeSocketsHelper("type2|0|0").ToString(), "type2||", ());
  TEST_EQUAL(ChargeSocketsHelper("type2||50").ToString(), "type2||50", ());
  TEST_EQUAL(ChargeSocketsHelper("type2||7.4").ToString(), "type2||7.4", ());
  TEST_EQUAL(ChargeSocketsHelper("type2|0|50").ToString(), "type2||50", ());
  TEST_EQUAL(ChargeSocketsHelper("type2|1|50").ToString(), "type2|1|50", ());
  TEST_EQUAL(ChargeSocketsHelper("unknown||").ToString(), "unknown||", ());
  TEST_EQUAL(ChargeSocketsHelper("unknown||150").ToString(), "unknown||150", ());
  TEST_EQUAL(ChargeSocketsHelper("type2|;type2_cable|1|50").ToString(), "type2_cable|1|50", ());

  // sorting
  TEST_EQUAL(ChargeSocketsHelper("type2||;type2_cable|1|50").ToString(), "type2_cable|1|50;type2||", ());
  TEST_EQUAL(ChargeSocketsHelper("type2||;type2_cable||;||;type2_combo||").ToString(),
             "type2_combo||;type2_cable||;type2||;unknown||", ());

  // multiple sockets of same type, but different power
  TEST_EQUAL(ChargeSocketsHelper("type2|1|50;type2|2|43").ToString(), "type2|1|50;type2|2|43", ());

  // TEST_EQUAL(ChargeSocketsHelper("type2|1|43;type2|2|50").ToString(), "type2|1|50;type2|2|43", ());

  ///////////////////////////////////////////////////////////
  // OSM key parsing

  OSMKeyValues kvs;

  kvs = {{"socket:type2", "2"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2|2|", ());

  kvs = {{"socket:type2", "yes"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2||", ());

  kvs = {{"socket:type2", "malformed"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "", ());

  kvs = {{"socket:type2:output", "22"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2||22", ());

  kvs = {{"socket:type2:malformed", "22"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "", ());

  kvs = {{"socket:type2", "malformed"}, {"socket:type2:output", "22 kW"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2||22", ());

  kvs = {{"socket:type2", "2"}, {"socket:type2:output", "22 kW"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2|2|22", ());

  kvs = {{"socket:type2", "2"}, {"socket:type2:output", "22kW"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2|2|22", ());

  kvs = {{"socket:type2", "2"}, {"socket:type2:output", "22KW"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2|2|22", ());

  kvs = {{"socket:type2", "2"}, {"socket:type2:output", "22"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2|2|22", ());

  kvs = {{"socket:chademo", "2"},      {"socket:chademo:output", "50"},
         {"socket:type2_combo", "10"}, {"socket:type2_combo:output", "200"},
         {"socket:type2_cable", "2"},  {"socket:type2_cable:output", "50"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2_combo|10|200;chademo|2|50;type2_cable|2|50", ());

  // unit conversion
  kvs = {{"socket:type2:output", "7400W"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2||7.4", ());

  kvs = {{"socket:type2_combo:output", "150 kw"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2_combo||150", ());

  kvs = {{"socket:type2_combo:output", "150     KW"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2_combo||150", ());

  kvs = {{"socket:type2_combo:output", " 150kw"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2_combo||150", ());

  kvs = {{"socket:type2_combo:output", "150kva"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2_combo||150", ());

  kvs = {{"socket:type2_combo:output", "0.150MW"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2_combo||150", ());

  kvs = {{"socket:type2_combo:output", ".150MW"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2_combo||150", ());

  // invalid
  kvs = {{"socket:type2", "-1"}};
  TEST_EQUAL(FromKVs(kvs).GetSockets().size(), 0, ());

  kvs = {{"socket:type2", "-2"}};
  TEST_EQUAL(FromKVs(kvs).GetSockets().size(), 0, ());

  kvs = {{"socket:type2", "-2"}, {"socket:type2:output", "-50"}};
  TEST_EQUAL(FromKVs(kvs).GetSockets().size(), 0, ());

  // count ignored (because invalid), but power valid
  kvs = {{"socket:type2", "-2"}, {"socket:type2:output", "50"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2||50", ());

  // normalization
  kvs = {{"socket:tesla_supercharger", "2"}, {"socket:tesla_supercharger:output", "150"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "nacs|2|150", ());

  kvs = {{"socket:ccs", "2"}, {"socket:ccs:output", "150"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2_combo|2|150", ());

  // multiple socket of same type
  kvs = {{"socket:type2", "2;3"}, {"socket:type2:output", "50;43"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2|2|50;type2|3|43", ());

  kvs = {{"socket:type2:output", "50;43"}, {"socket:type2", "2;3"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2|2|50;type2|3|43", ());

  kvs = {{"socket:type2", "2;3"}, {"socket:type2:output", "43;50"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2|3|50;type2|2|43", ());

  kvs = {{"socket:type2", "2;3;5"}, {"socket:type2:output", "43;50;7.4"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2|3|50;type2|2|43;type2|5|7.4", ());

  kvs = {{"socket:type2", "2;3"}, {"socket:type2:output", "50 kW; 43 kw"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2|2|50;type2|3|43", ());

  // unbalanced multiple sockets
  kvs = {{"socket:type2", "2"}, {"socket:type2:output", "50 kW; 43 kw"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2|2|50;type2||43", ());

  kvs = {{"socket:type2", "2;"}, {"socket:type2:output", "50 kW; 43 kw"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2|2|50;type2||43", ());

  kvs = {{"socket:type2", "2;3"}, {"socket:type2:output", "50 kW"}};
  TEST_EQUAL(FromKVs(kvs).ToString(), "type2|2|50;type2|3|", ());

  ///////////////////////////////////////////////////////////
  // OSM key generation

  kvs = {{"socket:type2", "2"}};
  TEST_EQUAL(ChargeSocketsHelper("type2|2|").GetOSMKeyValues(), kvs, ());
  TEST_EQUAL(ChargeSocketsHelper("type2|2|0").GetOSMKeyValues(), kvs, ());

  kvs = {{"socket:type2", "yes"}};
  TEST_EQUAL(ChargeSocketsHelper("type2||").GetOSMKeyValues(), kvs, ());
  TEST_EQUAL(ChargeSocketsHelper("type2|0|").GetOSMKeyValues(), kvs, ());

  kvs = {{"socket:type2", "yes"}, {"socket:type2:output", "150"}};
  TEST_EQUAL(ChargeSocketsHelper("type2||150").GetOSMKeyValues(), kvs, ());

  // the order should always be the same, as the sockets are sorted
  kvs = {{"socket:type2_combo", "10"},    {"socket:type2_combo:output", "200"}, {"socket:chademo", "2"},
         {"socket:chademo:output", "50"}, {"socket:type2_cable", "2"},          {"socket:type2_cable:output", "50"}};
  TEST_EQUAL(ChargeSocketsHelper("type2_combo|10|200;chademo|2|50;type2_cable|2|50").GetOSMKeyValues(), kvs, ());
  TEST_EQUAL(ChargeSocketsHelper("chademo|2|50;type2_cable|2|50;type2_combo|10|200").GetOSMKeyValues(), kvs, ());

  // multiple socket of same type
  // (should always be ordered from higher power to lower)
  kvs = {{"socket:type2", "2;3"}, {"socket:type2:output", "50;43"}};
  TEST_EQUAL(ChargeSocketsHelper("type2|2|50;type2|3|43").GetOSMKeyValues(), kvs, ());

  kvs = {{"socket:type2", "3;2"}, {"socket:type2:output", "50;43"}};
  TEST_EQUAL(ChargeSocketsHelper("type2|2|43;type2|3|50").GetOSMKeyValues(), kvs, ());

  kvs = {{"socket:type2", "2;3;5"}, {"socket:type2:output", "50;43;7.4"}};
  TEST_EQUAL(ChargeSocketsHelper("type2|3|43;type2|5|7.4;type2|2|50").GetOSMKeyValues(), kvs, ());

  kvs = {{"socket:type2_combo", "10;2"},
         {"socket:type2_combo:output", "250;100"},
         {"socket:type2", "2;3;5"},
         {"socket:type2:output", "50;43;7.4"}};
  TEST_EQUAL(
      ChargeSocketsHelper("type2|3|43;type2|5|7.4;type2_combo|10|250;type2|2|50;type2_combo|2|100").GetOSMKeyValues(),
      kvs, ());
}

}  // namespace feature_charge_sockets_test
