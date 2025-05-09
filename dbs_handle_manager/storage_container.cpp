#include "storage_container.h"

#include "storage/a_storage.h"

namespace mx::dba::dbs {

StorageContainer& StorageContainer::Instance() {
  static StorageContainer instance;
  return instance;
}

void StorageContainer::RegisterAllStorages() {
  RegisterStorageCreator<AStorage>([](const std::string_view& db_path) { return CreateAStorage(db_path); });
}

}  // namespace mx::dba::dbs
