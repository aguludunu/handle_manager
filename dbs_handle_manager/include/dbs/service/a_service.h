#pragma once

#include <optional>
#include <string>
#include <vector>

#include "dbs/dbs.h"

namespace mx::dba::dbs {

// 需要什么数据，就定义什么数据结构
struct User {
  int user_id{0};
  std::string username{};
  std::string email{};
  int age{0};
  long registration_date{0};
};

struct Order {
  int order_id{0};
  int user_id{0};
  std::string product_name{};
  int quantity{0};
  double price{0.0};
  long order_date{0};
};

struct UserOrder {
  User user;
  Order order;
};
struct DataTypePartial {
  std::optional<int> int_nullable;
  double float_not_null;
  std::optional<double> float_nullable;
  std::string text_not_null;
  std::optional<std::string> text_nullable;
  BLOB blob_data;
};

// 需要什么业务，就定义什么方法
std::vector<User> GetAllUsers(const HandleKey& key);
std::vector<DataTypePartial> GetPartialDataTypes(const HandleKey& key);
std::vector<DataTypePartial> GetPartialDataTypesByCondition(const HandleKey& key,
                                                            const std::string& condition);

}  // namespace mx::dba::dbs
