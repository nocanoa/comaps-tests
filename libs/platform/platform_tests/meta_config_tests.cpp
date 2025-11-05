#include "testing/testing.hpp"

#include "platform/servers_list.hpp"

#include "cppjansson/cppjansson.hpp"

using namespace downloader;

UNIT_TEST(MetaConfig_JSONParser_OldFormat)
{
  std::string oldFormatJson = R"(["http://url1", "http://url2", "http://url3"])";
  auto result = ParseMetaConfig(oldFormatJson);
  TEST(result.has_value(), ());
  TEST_EQUAL(result->m_serversList.size(), 3, ());
  TEST_EQUAL(result->m_serversList[0], "http://url1", ());
  TEST_EQUAL(result->m_serversList[1], "http://url2", ());
  TEST_EQUAL(result->m_serversList[2], "http://url3", ());
  TEST(result->m_settings.empty(), ());
}

UNIT_TEST(MetaConfig_JSONParser_InvalidJSON)
{
  std::string invalidJson = R"({"servers": ["http://url1", "http://url2")";
  auto result = ParseMetaConfig(invalidJson);
  TEST(!result.has_value(), ());
}

UNIT_TEST(MetaConfig_JSONParser_EmptyServersList)
{
  std::string emptyServersJson = R"({"servers": []})";
  auto result = ParseMetaConfig(emptyServersJson);
  TEST(!result.has_value(), ());
}

UNIT_TEST(MetaConfig_JSONParser_NewFormatWithoutProducts)
{
  std::string newFormatJson = R"({
    "servers": ["http://url1", "http://url2"],
    "settings": {
      "DonateUrl": "value1",
      "key2": "value2"
    }
  })";
  auto result = ParseMetaConfig(newFormatJson);
  TEST(result.has_value(), ());
  TEST_EQUAL(result->m_serversList.size(), 2, ());
  TEST_EQUAL(result->m_serversList[0], "http://url1", ());
  TEST_EQUAL(result->m_serversList[1], "http://url2", ());
  TEST_EQUAL(result->m_settings.size(), 1, ());
  TEST_EQUAL(result->m_settings["DonateUrl"], "value1", ());
}

UNIT_TEST(MetaConfig_JSONParser_MissingServersKey)
{
  std::string missingServersJson = R"({
    "settings": {
      "key1": "value1"
    }
  })";
  auto result = ParseMetaConfig(missingServersJson);
  TEST(!result.has_value(), ("JSON shouldn't be parsed without 'servers' key"));
}
