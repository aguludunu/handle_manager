#include "dbs/service/a_service.h"

#include "storage/a_storage.h"
#include "storage_container.h"

namespace mx::dba::dbs {

std::vector<User> GetAllUsers(const HandleKey& key) {
  auto storage = StorageContainer::Instance().GetStorage<AStorage>(key);
  if (!storage) {
    printf("xxx!\n");
    return {};
  }

  return storage->GetAllUsers();
}

}  // namespace mx::dba::dbs
