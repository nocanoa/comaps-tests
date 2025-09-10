#include "traffxml/traff_storage.hpp"

#include "platform/platform.hpp"

#include "coding/internal/file_data.hpp"

#include "base/logging.hpp"

#include <string>

namespace
{
std::string GetFilePath(std::string const & fileName) { return GetPlatform().WritablePathForFile(fileName); }
}  // namespace

namespace traffxml
{
// StorageLocal ------------------------------------------------------------------------------------
bool LocalStorage::Save(pugi::xml_document const & doc)
{
  auto const filePath = GetFilePath(m_fileName);

  std::lock_guard<std::mutex> guard(m_mutex);

  return base::WriteToTempAndRenameToFile(filePath, [&doc](std::string const & fileName) {
    return doc.save_file(fileName.data(), "  " /* indent */);
  });
}

bool LocalStorage::Load(pugi::xml_document & doc)
{
  auto const filePath = GetFilePath(m_fileName);

  std::lock_guard<std::mutex> guard(m_mutex);

  auto const result = doc.load_file(filePath.c_str());
  /*
   * Note: status_file_not_found is ok for our use cases:
   *  - editor: if a user has never made any edits.
   *  - traffic: if no traffic information has ever been retrieved (first run)
   */
  if (result != pugi::status_ok && result != pugi::status_file_not_found)
  {
    LOG(LERROR, ("Can't load file from disk:", filePath));
    return false;
  }

  return true;
}

bool LocalStorage::Reset()
{
  std::lock_guard<std::mutex> guard(m_mutex);

  return base::DeleteFileX(GetFilePath(m_fileName));
}

// StorageMemory -----------------------------------------------------------------------------------
bool InMemoryStorage::Save(pugi::xml_document const & doc)
{
  m_doc.reset(doc);
  return true;
}

bool InMemoryStorage::Load(pugi::xml_document & doc)
{
  doc.reset(m_doc);
  return true;
}

bool InMemoryStorage::Reset()
{
  m_doc.reset();
  return true;
}
}  // namespace traffxml
