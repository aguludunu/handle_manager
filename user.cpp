#include "user.h"

namespace mx::dba::dbs {

std::shared_ptr<UserStorage> UserReader::CreateStorage() {
  return std::make_shared<UserStorage>(CreateUserStorage());
}

}  // namespace mx::dba::dbs
