#include <iostream>

#include "handle_manager_sqlite.h"
#include "order.h"
#include "user.h"

using mx::dba::dbs::HandleKey;
using mx::dba::dbs::Order;
using mx::dba::dbs::OrderReader;
using mx::dba::dbs::SqliteHandleManager;
using mx::dba::dbs::User;
using mx::dba::dbs::UserReader;

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
