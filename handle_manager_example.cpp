#include <sqlite_orm/sqlite_orm.h>

#include <iostream>
#include <memory>
#include <string>

#include "handle_manager_sqlite.h"

using mx::dba::dbs::DatabaseReader;
using mx::dba::dbs::HandleKey;
using mx::dba::dbs::SqliteHandleManager;

// 定义 A.db 中的表结构
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

// 定义 User 表的存储类型
using UserStorage = decltype(sqlite_orm::make_storage(
    "A.db", sqlite_orm::make_table(
                "Users", sqlite_orm::make_column("user_id", &User::user_id, sqlite_orm::primary_key()),
                sqlite_orm::make_column("username", &User::username),
                sqlite_orm::make_column("email", &User::email), sqlite_orm::make_column("age", &User::age),
                sqlite_orm::make_column("registration_date", &User::registration_date))));

// 用户表的读取器
class UserReader : public DatabaseReader<UserStorage> {
 public:
  using DatabaseReader<UserStorage>::DatabaseReader;

 protected:
  std::shared_ptr<UserStorage> CreateStorage() override {
    using namespace sqlite_orm;
    auto storage = std::make_shared<UserStorage>(
        make_storage("A.db", make_table("Users", make_column("user_id", &User::user_id, primary_key()),
                                        make_column("username", &User::username),
                                        make_column("email", &User::email), make_column("age", &User::age),
                                        make_column("registration_date", &User::registration_date))));
    return storage;
  }
};

// 定义 Order 表的存储类型
using OrderStorage = decltype(sqlite_orm::make_storage(
    "A.db", sqlite_orm::make_table(
                "Orders", sqlite_orm::make_column("order_id", &Order::order_id, sqlite_orm::primary_key()),
                sqlite_orm::make_column("user_id", &Order::user_id),
                sqlite_orm::make_column("product_name", &Order::product_name),
                sqlite_orm::make_column("quantity", &Order::quantity),
                sqlite_orm::make_column("price", &Order::price),
                sqlite_orm::make_column("order_date", &Order::order_date))));

// 订单表的读取器
class OrderReader : public DatabaseReader<OrderStorage> {
 public:
  using DatabaseReader<OrderStorage>::DatabaseReader;

 protected:
  std::shared_ptr<OrderStorage> CreateStorage() override {
    using namespace sqlite_orm;
    auto storage = std::make_shared<OrderStorage>(make_storage(
        "A.db", make_table("Orders", make_column("order_id", &Order::order_id, primary_key()),
                           make_column("user_id", &Order::user_id),
                           make_column("product_name", &Order::product_name),
                           make_column("quantity", &Order::quantity), make_column("price", &Order::price),
                           make_column("order_date", &Order::order_date))));
    return storage;
  }
};

int main() {
  // 设置最大 handle 数量
  SqliteHandleManager::Instance().SetMaxHandles(5);

  // 创建 HandleKey
  HandleKey user_key{1, 2, 3, 4};
  HandleKey order_key{5, 6, 7, 8};

  // 创建用户表读取器
  UserReader user_reader(user_key);

  // 读取用户表数据
  std::cout << "读取 Users 表数据：" << std::endl;
  auto users = user_reader.ExecuteQuery([](auto& storage) { return storage.template get_all<User>(); });

  for (const auto& user : users) {
    std::cout << "ID: " << user.user_id << ", 用户名: " << user.username << ", 邮箱: " << user.email
              << ", 年龄: " << user.age << std::endl;
  }

  // 创建订单表读取器
  OrderReader order_reader(order_key);

  // 读取订单表数据
  std::cout << "\n读取 Orders 表数据：" << std::endl;
  auto orders = order_reader.ExecuteQuery([](auto& storage) { return storage.template get_all<Order>(); });

  for (const auto& order : orders) {
    std::cout << "订单ID: " << order.order_id << ", 用户ID: " << order.user_id
              << ", 产品: " << order.product_name << ", 数量: " << order.quantity
              << ", 价格: " << order.price << std::endl;
  }

  // 再次使用相同的 HandleKey 读取用户表
  std::cout << "\n再次使用相同的 HandleKey 读取 Users 表：" << std::endl;
  auto users2 = user_reader.ExecuteQuery([](auto& storage) { return storage.template get_all<User>(); });

  std::cout << "读取到 " << users2.size() << " 条用户记录" << std::endl;

  // 测试 LRU 清理机制
  std::cout << "\n测试 LRU 清理机制：" << std::endl;

  // 创建多个不同的 HandleKey，超过最大 handle 数量
  for (int i = 0; i < 10; ++i) {
    HandleKey key{i, i, i, i};
    UserReader temp_reader(key);
    auto count = temp_reader.ExecuteQuery([](auto& storage) { return storage.template count<User>(); });
    std::cout << "HandleKey{" << i << "," << i << "," << i << "," << i << "} 读取到 " << count
              << " 条用户记录" << std::endl;
  }

  // 再次使用第一个 HandleKey，检查是否被清理
  std::cout << "\n再次使用第一个 HandleKey：" << std::endl;
  try {
    auto users3 = user_reader.ExecuteQuery([](auto& storage) { return storage.template get_all<User>(); });
    std::cout << "成功读取到 " << users3.size() << " 条用户记录" << std::endl;
  } catch (const std::exception& e) {
    std::cout << "读取失败：" << e.what() << std::endl;
  }

  return 0;
}
