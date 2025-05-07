#pragma once

#include <memory>
#include <string>

#include "handle_manager_sqlite.h"
#include "sqlite_orm/sqlite_orm.h"

namespace mx::dba::dbs {

// 数据库文件名常量
static constexpr const char* kDatabaseFileName = "A.db";

// 用户结构体定义
struct User {
  int user_id{0};
  std::string username{};
  std::string email{};
  int age{0};
  long registration_date{0};
};

// 创建用户表结构的工厂函数
inline auto CreateUserTable() {
  using namespace sqlite_orm;
  return make_table("Users", make_column("user_id", &User::user_id, primary_key()),
                    make_column("username", &User::username), make_column("email", &User::email),
                    make_column("age", &User::age),
                    make_column("registration_date", &User::registration_date));
}

// 创建用户存储的工厂函数
inline auto CreateUserStorage() {
  using namespace sqlite_orm;
  return make_storage(kDatabaseFileName, CreateUserTable());
}

// 定义用户表的存储类型
using UserStorage = decltype(CreateUserStorage());

// 用户表的读取器
class UserReader : public DatabaseReader<UserStorage> {
 public:
  using DatabaseReader<UserStorage>::DatabaseReader;

 protected:
  std::shared_ptr<UserStorage> CreateStorage() override;
};

}  // namespace mx::dba::dbs
