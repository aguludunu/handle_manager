#pragma once

#include "dbs/service/a_service.h"
#include "inner_defines.h"

namespace mx::dba::dbs {

class AStorageImpl;

class AStorage : public IStorage {
 public:
  explicit AStorage(const std::string_view& db_path);
  ~AStorage() override = default;

  [[nodiscard]] std::string GetDatabasePath() const override { return db_path_; }

  // 下面是该 storage 的业务接口，每个 storage 的业务都不同，接口也不同。
  std::vector<User> GetAllUsers();
  std::vector<User> GetUsersByCondition(const std::string_view& username_pattern, int min_age);
  std::vector<Order> GetAllOrders();
  std::vector<Order> GetOrdersByCondition(int user_id, double min_price);
  std::vector<UserOrder> GetUserOrders();
  std::vector<DataTypePartial> GetAllDataTypePartials();
  std::vector<DataTypePartial> GetDataTypePartialsByCondition(const std::string& text_pattern);

 private:
  std::string db_path_{};
  std::unique_ptr<AStorageImpl> impl_{};
};

std::shared_ptr<AStorage> CreateAStorage(const std::string_view& db_path);

}  // namespace mx::dba::dbs