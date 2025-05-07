#include "order.h"

namespace mx::dba::dbs {

std::shared_ptr<OrderStorage> OrderReader::CreateStorage() {
  return std::make_shared<OrderStorage>(CreateOrderStorage());
}

}  // namespace mx::dba::dbs
