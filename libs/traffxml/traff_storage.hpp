#pragma once

#include <mutex>

#include <pugixml.hpp>

/*
 * TODO combine this header and its CPP file with editor/editor_storage.hpp and its CPP counterpart,
 * give it an appropriate namespace and place it where both editor and traffic can use it.
 * `traff_storage` is essentially copied from `editor_storage` with just a few modifications:
 *  - namespace
 *  - instead of relying on a hardcoded file name, `LocalStorage` is initialized with a file name
 *  - log output, variable names and comments changed to be use case neutral (no API changes)
 *  - refactoring: no global `using namespace` (no API changes)
 * Apart from the first two points, this file can serve as a drop-in replacement for `editor_storage`.
 * Traffic uses only `LocalStorage`, the rest has been kept to ease migration to a unified storage
 * component.
 */

namespace traffxml
{
/**
 * @brief Storage interface for XML data.
 */
class StorageBase
{
public:
  virtual ~StorageBase() = default;

  virtual bool Save(pugi::xml_document const & doc) = 0;
  virtual bool Load(pugi::xml_document & doc) = 0;
  virtual bool Reset() = 0;
};

/**
 * @brief Class which saves/loads XML data to/from local file.
 * @note this class IS thread-safe.
 */
class LocalStorage : public StorageBase
{
public:
  /**
   * @brief Constructs a `LocalStorage` instance.
   * @param fileName The file name and path where the file in question will be persisted. It is
   * interpreted relative to the platform-specific path; absolute paths are not supported as some
   * platforms restrict applications’ access to files outside their designated path.
   */
  LocalStorage(std::string const & fileName)
    : m_fileName(fileName)
  {}

  // StorageBase overrides:
  bool Save(pugi::xml_document const & doc) override;
  bool Load(pugi::xml_document & doc) override;
  bool Reset() override;

private:
  std::string m_fileName;
  std::mutex m_mutex;
};

/**
 * @brief Class which saves/loads data to/from xml_document class instance.
 * @note this class is NOT thread-safe.
 */
class InMemoryStorage : public StorageBase
{
public:
  // StorageBase overrides:
  bool Save(pugi::xml_document const & doc) override;
  bool Load(pugi::xml_document & doc) override;
  bool Reset() override;

private:
  pugi::xml_document m_doc;
};
}  // namespace traffxml
