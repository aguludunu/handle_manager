#pragma once

#include <string>
#include <vector>

#include "dbs/dbs.h"

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

std::vector<User> GetAllUsers(const HandleKey& key);

}  // namespace mx::dba::dbs
