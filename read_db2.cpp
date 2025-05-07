#include <iostream>
#include <string>
#include <string_view>
#include <vector>

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
using sqlite_orm::where;

namespace mx::dba::dbs {

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

static constexpr const char* kADBFileName = "A.db";

inline auto CreateUserTable() {
  return make_table("Users", make_column("user_id", &User::user_id, primary_key()),
                    make_column("username", &User::username), make_column("email", &User::email),
                    make_column("age", &User::age),
                    make_column("registration_date", &User::registration_date));
}

inline auto CreateOrderTable() {
  return make_table("Orders", make_column("order_id", &Order::order_id, primary_key()),
                    make_column("user_id", &Order::user_id),
                    make_column("product_name", &Order::product_name),
                    make_column("quantity", &Order::quantity), make_column("price", &Order::price),
                    make_column("order_date", &Order::order_date));
}

inline auto CreateADBStorage(const std::string_view& db_path) {
  return make_storage(std::string(db_path), CreateUserTable(), CreateOrderTable());
}

using ADBStorage = decltype(CreateADBStorage(kADBFileName));

std::vector<User> GetAllUsers(ADBStorage& storage) { return storage.get_all<User>(); }

std::vector<User> GetUsersByCondition(ADBStorage& storage, const std::string_view& username_pattern,
                                      int min_age) {
  return storage.get_all<User>(
      where(like(&User::username, username_pattern) && greater_than(&User::age, min_age)));
}

std::vector<Order> GetAllOrders(ADBStorage& storage) { return storage.get_all<Order>(); }

std::vector<Order> GetOrdersByCondition(ADBStorage& storage, int user_id, double min_price) {
  return storage.get_all<Order>(
      where(is_equal(&Order::user_id, user_id) && greater_than(&Order::price, min_price)));
}

std::vector<UserOrder> GetUserOrders(ADBStorage& storage) {
  auto rows =
      storage.select(columns(&User::user_id, &User::username, &User::email, &User::age,
                             &User::registration_date, &Order::order_id, &Order::user_id,
                             &Order::product_name, &Order::quantity, &Order::price, &Order::order_date),
                     inner_join<Order>(on(c(&User::user_id) == &Order::user_id)));

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

}  // namespace mx::dba::dbs

// 打印用户信息的辅助函数
void PrintUser(const mx::dba::dbs::User& user) {
  std::cout << "用户ID: " << user.user_id << ", 用户名: " << user.username << ", 邮箱: " << user.email
            << ", 年龄: " << user.age << ", 注册时间: " << user.registration_date << std::endl;
}

// 打印订单信息的辅助函数
void PrintOrder(const mx::dba::dbs::Order& order) {
  std::cout << "订单ID: " << order.order_id << ", 用户ID: " << order.user_id
            << ", 产品名称: " << order.product_name << ", 数量: " << order.quantity
            << ", 价格: " << order.price << ", 订单时间: " << order.order_date << std::endl;
}

int main() {
  try {
    auto storage = mx::dba::dbs::CreateADBStorage(mx::dba::dbs::kADBFileName);

    // 1. 读取 user 表的全部数据
    std::cout << "===== 所有用户 =====" << std::endl;
    auto all_users = mx::dba::dbs::GetAllUsers(storage);
    for (const auto& user : all_users) {
      PrintUser(user);
    }
    std::cout << std::endl;

    // 2. 根据条件读取 user 表的部分数据
    std::cout << "===== 年龄大于 30 且用户名包含 '李' 的用户 =====" << std::endl;
    auto filtered_users = mx::dba::dbs::GetUsersByCondition(storage, "%李%", 30);
    for (const auto& user : filtered_users) {
      PrintUser(user);
    }
    std::cout << std::endl;

    // 3. 读取 order 表的全部数据
    std::cout << "===== 所有订单 =====" << std::endl;
    auto all_orders = mx::dba::dbs::GetAllOrders(storage);
    for (const auto& order : all_orders) {
      PrintOrder(order);
    }
    std::cout << std::endl;

    // 4. 根据条件读取 order 表的部分数据
    std::cout << "===== 用户ID为 1 且价格大于 1000 的订单 =====" << std::endl;
    auto filtered_orders = mx::dba::dbs::GetOrdersByCondition(storage, 1, 1000.0);
    for (const auto& order : filtered_orders) {
      PrintOrder(order);
    }
    std::cout << std::endl;

    // 5. 读取 user 表和 order 表的关联数据
    std::cout << "===== 用户和订单关联数据 =====" << std::endl;
    auto user_orders = mx::dba::dbs::GetUserOrders(storage);
    for (const auto& user_order : user_orders) {
      std::cout << "用户: " << user_order.user.username << " (ID: " << user_order.user.user_id << ")"
                << " 订购了: " << user_order.order.product_name << " 价格: " << user_order.order.price
                << std::endl;
    }

  } catch (const std::exception& e) {
    std::cerr << "错误: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
