#pragma once

#include <string>
#include <vector>

#include "dbs/dbs.h"

namespace mx::dba::dbs {

// Users 表的完整结构
// 因为要将这个结构提供给用户，所以定义在这里。如果不需要给用户提供，则定义在 xxx_storage.h中
struct User {
  int user_id{0};
  std::string username{};
  std::string email{};
  int age{0};
  long registration_date{0};
};

// Users 表的部分结构
struct UserPartial {
  int user_id{0};
  std::string username{};
};

// Users 和 Orders 表的复合结构(部分数据)
struct UserOrder {
  int user_id{0};
  std::string username{};
  int order_id{0};
  std::string product_name{};
};

struct DataTypeBase {
  int int_nullable{1234};
  double float_not_null{0};
  double float_nullable{12.34};
  std::string text_not_null;
  std::string text_nullable{"1234"};
};

struct DataTypeDeserialization {
  size_t i{0};
  std::string s;
  double d{0.0};
};

struct DataType {
  DataTypeBase base;
  DataTypeDeserialization data;
};

// 获取一个表中的全部数据
std::vector<User> GetAllUsers(const HandleKey& key);

// 获取一个表中某些列的数据
std::vector<UserPartial> GetPartialUsers(const HandleKey& key);

// 根据条件，获取一个表中某些列的数据
std::vector<UserPartial> GetPartialUsersByAge(const HandleKey& key, int age);

// 根据条件，获取多个表中某些列的数据
std::vector<UserOrder> GetUserOrdersByUserId(const HandleKey& key, int user_id);

// 1. 该例子只映射了部分字段。如果业务上只需要这个表的部分字段，这样做可以节省一次结构体的转换
// 2. 该例子展示了如何应对数据可能为空的情况
// 3. 该例子展示了反序列化的工作
std::vector<DataType> GetPartialDataTypes(const HandleKey& key);

}  // namespace mx::dba::dbs
