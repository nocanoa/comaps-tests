#include "traffxml/traff_source.hpp"

#include "traffxml/traff_model_xml.hpp"
#include "traffxml/traff_storage.hpp"

#include "geometry/rect2d.hpp"

#include <vector>

namespace traffxml {
TraffSource::TraffSource(TraffSourceManager & manager)
  : m_manager(manager)
{}

void TraffSource::SubscribeOrChangeSubscription(std::set<MwmSet::MwmId> & mwms)
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!IsSubscribed())
    Subscribe(mwms);
  else
    ChangeSubscription(mwms);
}

std::string TraffSource::GetMwmFilters(std::set<MwmSet::MwmId> & mwms)
{
  std::vector<m2::RectD> rects;
  for (auto mwmId : mwms)
    rects.push_back(mwmId.GetInfo()->m_bordersRect);
  return traffxml::FiltersToXml(rects);
}

void MockTraffSource::Create(TraffSourceManager & manager)
{
  std::unique_ptr<MockTraffSource> source = std::unique_ptr<MockTraffSource>(new MockTraffSource(manager));
  manager.RegisterSource(std::move(source));
}

MockTraffSource::MockTraffSource(TraffSourceManager & manager)
  : TraffSource(manager)
{}

void MockTraffSource::Subscribe(std::set<MwmSet::MwmId> & mwms)
{
  std::string filterList = GetMwmFilters(mwms);
  LOG(LINFO, ("Would subscribe to:\n", filterList));
  m_subscriptionId = "placeholder_subscription_id";
  m_nextRequestTime = std::chrono::steady_clock::now(); // would be in the future if we got a feed here
}

void MockTraffSource::ChangeSubscription(std::set<MwmSet::MwmId> & mwms)
{
  if (!IsSubscribed())
    return;
  std::string filterList = GetMwmFilters(mwms);
  LOG(LINFO, ("Would change subscription", m_subscriptionId, "to:\n", filterList));
  m_nextRequestTime = std::chrono::steady_clock::now(); // would be in the future if we got a feed here
}

void MockTraffSource::Unsubscribe()
{
  std::lock_guard<std::mutex> lock(m_mutex);
  if (!IsSubscribed())
    return;
  LOG(LINFO, ("Would unsubscribe from", m_subscriptionId));
  m_subscriptionId.clear();
}

bool MockTraffSource::IsPollNeeded()
{
  return m_nextRequestTime.load() <= std::chrono::steady_clock::now();
}

void MockTraffSource::Poll()
{
  //std::string fileName("test_data/traff/PL-A18-Krzyzowa-Lipiany.xml");
  std::string fileName("test_data/traff/PL-A18-Krzyzowa-Lipiany-bidir.xml");
  //std::string fileName("test_data/traff/LT-A1-Vezaiciai-Endriejavas.xml");
  traffxml::LocalStorage storage(fileName);
  pugi::xml_document document;
  auto const load_result = storage.Load(document);
  if (!load_result)
    return;

  m_lastRequestTime = std::chrono::steady_clock::now();
  std::setlocale(LC_ALL, "en_US.UTF-8");
  traffxml::TraffFeed feed;
  if (traffxml::ParseTraff(document, std::nullopt /* dataSource */, feed))
  {
    m_lastResponseTime = std::chrono::steady_clock::now();
    m_nextRequestTime = std::chrono::steady_clock::now() + m_updateInterval;
    m_lastAvailability = Availability::IsAvailable;
    m_manager.ReceiveFeed(feed);
  }
  else
  {
    LOG(LWARNING, ("An error occurred parsing the TraFF feed"));
    m_lastAvailability = Availability::Error;
    /*
     * TODO how should we deal with future requests?
     * Static files usually don’t change.
     */
    m_nextRequestTime = std::chrono::steady_clock::now() + m_updateInterval;
  }
}
}  // namespace traffxml
