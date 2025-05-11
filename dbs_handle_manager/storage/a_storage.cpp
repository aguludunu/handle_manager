#include "storage/a_storage.h"

#include "dbs/service/a_service.h"
#include "sqlite_orm/sqlite_orm.h"

using sqlite_orm::c;
using sqlite_orm::columns;
using sqlite_orm::get_all;
using sqlite_orm::greater_than;
using sqlite_orm::inner_join;
using sqlite_orm::is_equal;
using sqlite_orm::like;
using sqlite_orm::make_column;
using sqlite_orm::make_storage;
using sqlite_orm::make_table;
using sqlite_orm::on;
using sqlite_orm::primary_key;
using sqlite_orm::select;
using sqlite_orm::where;

namespace mx::dba::dbs {

auto CreateStorage(const std::string& db_path) {
  // CREATE TABLE Users (user_id INTEGER PRIMARY KEY,username TEXT NOT NULL,email TEXT,age
  // INTEGER,registration_date INTEGER)
  auto users_table = make_table("Users", make_column("user_id", &User::user_id, primary_key()),
                                make_column("username", &User::username),
                                make_column("email", &User::email), make_column("age", &User::age),
                                make_column("registration_date", &User::registration_date));
  // CREATE TABLE Orders (order_id INTEGER PRIMARY KEY,user_id INTEGER,product_name TEXT NOT NULL,quantity
  // INTEGER,price REAL,order_date INTEGER,FOREIGN KEY (user_id) REFERENCES Users(user_id))
  auto orders_table =
      make_table("Orders", make_column("order_id", &Order::order_id, primary_key()),
                 make_column("user_id", &Order::user_id), make_column("product_name", &Order::product_name),
                 make_column("quantity", &Order::quantity), make_column("price", &Order::price),
                 make_column("order_date", &Order::order_date));
  // CREATE TABLE DataTypes (id INTEGER PRIMARY KEY,int_not_null INTEGER NOT NULL,int_nullable
  // INTEGER,float_not_null REAL NOT NULL,float_nullable REAL,text_not_null TEXT NOT NULL,text_nullable
  // TEXT,blob_data BLOB)
  // 注意: id 和 int_not_null 字段没有映射，这样可以节省一次数据转换
  auto data_types_table = make_table("DataTypes", make_column("int_nullable", &DataTypeInDB::int_nullable),
                                     make_column("float_not_null", &DataTypeInDB::float_not_null),
                                     make_column("float_nullable", &DataTypeInDB::float_nullable),
                                     make_column("text_not_null", &DataTypeInDB::text_not_null),
                                     make_column("text_nullable", &DataTypeInDB::text_nullable),
                                     make_column("blob_data", &DataTypeInDB::blob_data));

  return make_storage(db_path, users_table, orders_table, data_types_table);
}

class AStorageImpl {
 public:
  using StorageType = decltype(CreateStorage(std::string{}));
  explicit AStorageImpl(std::string db_path)
      : db_path_(std::move(db_path)), storage_(CreateStorage(db_path_)) {}

  std::vector<User> GetAllUsers() {
    auto statement = storage_.prepare(get_all<User>());
    printf("[%s:%d]sql = %s\n", __FILE__, __LINE__, statement.sql().c_str());

    return storage_.execute(statement);
  }

  std::vector<UserPartial> GetPartialUsers() {
    auto statement = storage_.prepare(select(columns(&User::user_id, &User::username)));
    printf("[%s:%d]sql = %s\n", __FILE__, __LINE__, statement.sql().c_str());

    auto rows = storage_.execute(statement);
    std::vector<UserPartial> result;
    result.reserve(rows.size());

    for (const auto& row : rows) {
      const auto& [userid, username] = row;
      result.push_back({userid, username});
    }

    return result;
  }

  std::vector<UserPartial> GetPartialUsersByAge(int age) {
    auto statement =
        storage_.prepare(select(columns(&User::user_id, &User::username), where(c(&User::age) = age)));
    printf("[%s:%d]sql = %s\n", __FILE__, __LINE__, statement.sql().c_str());

    auto rows = storage_.execute(statement);
    std::vector<UserPartial> result;
    result.reserve(rows.size());

    for (const auto& row : rows) {
      const auto& [userid, username] = row;
      result.push_back({userid, username});
    }

    return result;
  }

  std::vector<UserOrder> GetUserOrdersByUserId(int user_id) {
    auto statement = storage_.prepare(select(
        columns(&User::user_id, &User::username, &Order::order_id, &Order::product_name),
        inner_join<Order>(on(c(&User::user_id) == &Order::user_id)), where(c(&User::user_id) == user_id)));
    printf("[%s:%d]sql = %s\n", __FILE__, __LINE__, statement.sql().c_str());

    auto rows = storage_.execute(statement);
    std::vector<UserOrder> result;
    result.reserve(rows.size());

    for (const auto& row : rows) {
      const auto& [userid, username, order_id, product_name] = row;
      result.push_back({userid, username, order_id, product_name});
    }

    return result;
  }

  std::vector<DataTypeInDB> GetAllDataTypePartials() {
    auto statement = storage_.prepare(get_all<DataTypeInDB>());
    printf("[%s:%d]sql = %s\n", __FILE__, __LINE__, statement.sql().c_str());

    return storage_.execute(statement);
  }

 private:
  std::string db_path_;
  StorageType storage_;
};

// *********************************************************************************************************
// *********************************************************************************************************
// *********************************************************************************************************

AStorage::AStorage(const std::string_view& db_path)
    : db_path_(std::string(db_path)), impl_(std::make_unique<AStorageImpl>(std::string(db_path))) {}

std::vector<User> AStorage::GetAllUsers() { return impl_->GetAllUsers(); }

std::vector<UserPartial> AStorage::GetPartialUsers() { return impl_->GetPartialUsers(); }

std::vector<UserPartial> AStorage::GetPartialUsersByAge(int age) {
  return impl_->GetPartialUsersByAge(age);
}

std::vector<UserOrder> AStorage::GetUserOrdersByUserId(int user_id) {
  return impl_->GetUserOrdersByUserId(user_id);
}

std::vector<DataTypeInDB> AStorage::GetAllDataTypePartials() { return impl_->GetAllDataTypePartials(); }

// *********************************************************************************************************
// *********************************************************************************************************
// *********************************************************************************************************

std::shared_ptr<AStorage> CreateAStorage(const std::string_view& db_path) {
  return std::make_shared<AStorage>(db_path);
}

}  // namespace mx::dba::dbs
