#pragma once

#include <optional>

#include "dbs/service/a_service.h"
#include "inner_defines.h"

namespace mx::dba::dbs {

class AStorageImpl;

// Orders 表的完整结构
struct Order {
  int order_id{0};
  int user_id{0};
  std::string product_name{};
  int quantity{0};
  double price{0.0};
  long order_date{0};
};

// DataTypes 表的部分结构，因为根据业务需要只获取这部分数据即可
struct DataTypeInDB {
  std::optional<int> int_nullable;
  double float_not_null;
  std::optional<double> float_nullable;
  std::string text_not_null;
  std::optional<std::string> text_nullable;
  BLOB blob_data;
};

class AStorage : public IStorage {
 public:
  explicit AStorage(const std::string_view& db_path);
  ~AStorage() override = default;

  [[nodiscard]] std::string GetDatabasePath() const override { return db_path_; }

  // 下面是该 storage 的业务接口，每个 storage 的业务都不同，接口也不同。
  std::vector<User> GetAllUsers();
  std::vector<UserPartial> GetPartialUsers();
  std::vector<UserPartial> GetPartialUsersByAge(int age);
  std::vector<UserOrder> GetUserOrdersByUserId(int user_id);
  std::vector<DataTypeInDB> GetAllDataTypePartials();

 private:
  std::string db_path_{};
  std::unique_ptr<AStorageImpl> impl_{};
};

std::shared_ptr<AStorage> CreateAStorage(const std::string_view& db_path);

}  // namespace mx::dba::dbs