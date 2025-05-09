#include "storage/a_storage.h"

#include "sqlite_orm/sqlite_orm.h"

using sqlite_orm::c;
using sqlite_orm::columns;
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
  auto users_table = make_table("Users", make_column("user_id", &User::user_id, primary_key()),
                                make_column("username", &User::username),
                                make_column("email", &User::email), make_column("age", &User::age),
                                make_column("registration_date", &User::registration_date));

  auto orders_table =
      make_table("Orders", make_column("order_id", &Order::order_id, primary_key()),
                 make_column("user_id", &Order::user_id), make_column("product_name", &Order::product_name),
                 make_column("quantity", &Order::quantity), make_column("price", &Order::price),
                 make_column("order_date", &Order::order_date));

  return make_storage(db_path, users_table, orders_table);
}

class AStorageImpl {
 public:
  using StorageType = decltype(CreateStorage(std::string{}));
  explicit AStorageImpl(std::string db_path)
      : db_path_(std::move(db_path)), storage_(CreateStorage(db_path_)) {}

  std::vector<User> GetAllUsers() { return storage_.get_all<User>(); }
  std::vector<User> GetUsersByCondition(const std::string_view& username_pattern, int min_age) {
    return storage_.get_all<User>(
        where(like(&User::username, std::string(username_pattern)) && c(&User::age) > min_age));
  }
  std::vector<Order> GetAllOrders() { return storage_.get_all<Order>(); }
  std::vector<Order> GetOrdersByCondition(int user_id, double min_price) {
    return storage_.get_all<Order>(where(c(&Order::user_id) == user_id && c(&Order::price) > min_price));
  }

  std::vector<UserOrder> GetUserOrders() {
    auto statement = storage_.prepare(
        select(columns(&User::user_id, &User::username, &User::email, &User::age, &User::registration_date,
                       &Order::order_id, &Order::user_id, &Order::product_name, &Order::quantity,
                       &Order::price, &Order::order_date),
               inner_join<Order>(on(c(&User::user_id) == &Order::user_id))));
    printf("[%s:%d]sql = %s\n", __FILE__, __LINE__, statement.sql().c_str());

    auto rows = storage_.execute(statement);
    std::vector<UserOrder> result;
    result.reserve(rows.size());

    for (const auto& row : rows) {
      const auto& [user_id, username, email, age, registration_date, order_id, order_user_id, product_name,
                   quantity, price, order_date] = row;
      User user{user_id, username, email, age, registration_date};
      Order order{order_id, order_user_id, product_name, quantity, price, order_date};
      result.push_back({user, order});
    }

    return result;
  }

 private:
  std::string db_path_;
  StorageType storage_;
};

AStorage::AStorage(const std::string_view& db_path)
    : db_path_(std::string(db_path)), impl_(std::make_unique<AStorageImpl>(std::string(db_path))) {}

AStorage::~AStorage() = default;

std::vector<User> AStorage::GetAllUsers() { return impl_->GetAllUsers(); }

std::vector<User> AStorage::GetUsersByCondition(const std::string_view& username_pattern, int min_age) {
  return impl_->GetUsersByCondition(username_pattern, min_age);
}

std::vector<Order> AStorage::GetAllOrders() { return impl_->GetAllOrders(); }

std::vector<Order> AStorage::GetOrdersByCondition(int user_id, double min_price) {
  return impl_->GetOrdersByCondition(user_id, min_price);
}

std::vector<UserOrder> AStorage::GetUserOrders() { return impl_->GetUserOrders(); }

std::shared_ptr<AStorage> CreateAStorage(const std::string_view& db_path) {
  return std::make_shared<AStorage>(db_path);
}

}  // namespace mx::dba::dbs
