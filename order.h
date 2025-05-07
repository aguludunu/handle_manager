#pragma once

#include <memory>
#include <string>
#include <sqlite_orm/sqlite_orm.h>
#include "handle_manager_sqlite.h"
#include "user.h"  // 引入数据库文件名常量

namespace mx::dba::dbs {

// 订单结构体定义
struct Order {
  int order_id{0};
  int user_id{0};
  std::string product_name{};
  int quantity{0};
  double price{0.0};
  long order_date{0};
};

// 创建订单表结构的工厂函数
inline auto CreateOrderTable() {
  using namespace sqlite_orm;
  return make_table(
      "Orders", make_column("order_id", &Order::order_id, primary_key()),
      make_column("user_id", &Order::user_id),
      make_column("product_name", &Order::product_name),
      make_column("quantity", &Order::quantity),
      make_column("price", &Order::price),
      make_column("order_date", &Order::order_date));
}

// 创建订单存储的工厂函数
inline auto CreateOrderStorage() {
  using namespace sqlite_orm;
  return make_storage(kDatabaseFileName, CreateOrderTable());
}

// 定义订单表的存储类型
using OrderStorage = decltype(CreateOrderStorage());

// 订单表的读取器
class OrderReader : public DatabaseReader<OrderStorage> {
 public:
  using DatabaseReader<OrderStorage>::DatabaseReader;

 protected:
  std::shared_ptr<OrderStorage> CreateStorage() override;
};

}  // namespace mx::dba::dbs
