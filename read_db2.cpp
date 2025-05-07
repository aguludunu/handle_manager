#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
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

struct HandleKey {
  int param1{0};
  int param2{0};
  int param3{0};
  int param4{0};

  friend bool operator==(const HandleKey& lhs, const HandleKey& rhs) {
    return std::tie(lhs.param1, lhs.param2, lhs.param3, lhs.param4) ==
           std::tie(rhs.param1, rhs.param2, rhs.param3, rhs.param4);
  }
};

struct HandleKeyHash {
  std::size_t operator()(const HandleKey& key) const {
    return std::hash<int>()(key.param1) ^ (std::hash<int>()(key.param2) << 1) ^
           (std::hash<int>()(key.param3) << 2) ^ (std::hash<int>()(key.param4) << 3);
  }
};

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

// 数据库文件名常量
static constexpr const char* kADBFileName = "A.db";
static constexpr const char* kBDBFileName = "B.db";

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

// 通用存储接口类，所有具体存储类型都应该从这个接口继承
class IStorage {
 public:
  virtual ~IStorage() = default;

  // 获取数据库文件路径
  [[nodiscard]] virtual std::string GetDatabasePath() const = 0;
};

// 创建 A 数据库存储的工厂函数
inline auto CreateADBStorage(const std::string_view& db_path) {
  return make_storage(std::string(db_path), CreateUserTable(), CreateOrderTable());
}

// A 数据库存储类型
using ADBStorage = decltype(CreateADBStorage(kADBFileName));

// A 数据库存储包装类，实现 IStorage 接口
class AStorage : public IStorage {
 public:
  explicit AStorage(const std::string_view& db_path)
      : db_path_(std::string(db_path)), storage_(CreateADBStorage(db_path)) {}

  [[nodiscard]] std::string GetDatabasePath() const override { return db_path_; }

  // 获取所有数据
  template <typename T>
  std::vector<T> GetAll() {
    return storage_.get_all<T>();
  }

  // 根据条件获取用户
  std::vector<User> GetUsersByCondition(const std::string_view& username_pattern, int min_age) {
    return storage_.get_all<User>(
        where(like(&User::username, username_pattern) && greater_than(&User::age, min_age)));
  }

  // 根据条件获取订单
  std::vector<Order> GetOrdersByCondition(int user_id, double min_price) {
    return storage_.get_all<Order>(
        where(is_equal(&Order::user_id, user_id) && greater_than(&Order::price, min_price)));
  }

  // 获取用户订单关联数据
  std::vector<UserOrder> GetUserOrders() {
    auto rows =
        storage_.select(columns(&User::user_id, &User::username, &User::email, &User::age,
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

  // 获取原始存储对象
  ADBStorage& GetRawStorage() { return storage_; }

 private:
  std::string db_path_;
  ADBStorage storage_;
};

// 城市结构体定义
struct City {
  int city_id{0};
  std::string city_name{};
  std::string country{};
  int population{0};
  double area{0.0};
};

// 天气结构体定义
struct Weather {
  int weather_id{0};
  int city_id{0};
  long date{0};
  double temperature{0.0};
  double humidity{0.0};
  std::string weather_condition{};
};

// 城市天气关联结构体
struct CityWeather {
  City city;
  Weather weather;
};

// 创建城市表结构的工厂函数
inline auto CreateCityTable() {
  return make_table("Cities", make_column("city_id", &City::city_id, primary_key()),
                    make_column("city_name", &City::city_name), make_column("country", &City::country),
                    make_column("population", &City::population), make_column("area", &City::area));
}

// 创建天气表结构的工厂函数
inline auto CreateWeatherTable() {
  return make_table("Weather", make_column("weather_id", &Weather::weather_id, primary_key()),
                    make_column("city_id", &Weather::city_id), make_column("date", &Weather::date),
                    make_column("temperature", &Weather::temperature),
                    make_column("humidity", &Weather::humidity),
                    make_column("weather_condition", &Weather::weather_condition));
}

// 创建 B 数据库存储的工厂函数
inline auto CreateBDBStorage(const std::string_view& db_path) {
  return make_storage(std::string(db_path), CreateCityTable(), CreateWeatherTable());
}

// B 数据库存储类型
using BDBStorage = decltype(CreateBDBStorage(kBDBFileName));

// B 数据库存储包装类，实现 IStorage 接口
class BStorage : public IStorage {
 public:
  explicit BStorage(const std::string_view& db_path)
      : db_path_(std::string(db_path)), storage_(CreateBDBStorage(db_path)) {}

  [[nodiscard]] std::string GetDatabasePath() const override { return db_path_; }

  // 获取所有数据
  template <typename T>
  std::vector<T> GetAll() {
    return storage_.get_all<T>();
  }

  // 根据条件获取城市
  std::vector<City> GetCitiesByCondition(const std::string_view& country, int min_population) {
    return storage_.get_all<City>(
        where(is_equal(&City::country, country) && greater_than(&City::population, min_population)));
  }

  // 根据条件获取天气记录
  std::vector<Weather> GetWeatherByCondition(int city_id, double min_temperature) {
    return storage_.get_all<Weather>(where(is_equal(&Weather::city_id, city_id) &&
                                           greater_than(&Weather::temperature, min_temperature)));
  }

  // 获取城市天气关联数据
  std::vector<CityWeather> GetCityWeathers() {
    auto rows =
        storage_.select(columns(&City::city_id, &City::city_name, &City::country, &City::population,
                                &City::area, &Weather::weather_id, &Weather::city_id, &Weather::date,
                                &Weather::temperature, &Weather::humidity, &Weather::weather_condition),
                        inner_join<Weather>(on(c(&City::city_id) == &Weather::city_id)));

    std::vector<CityWeather> result;
    result.reserve(rows.size());

    for (const auto& row : rows) {
      const auto& [city_id, city_name, country, population, area, weather_id, weather_city_id, date,
                   temperature, humidity, weather_condition] = row;
      City city{city_id, city_name, country, population, area};
      Weather weather{weather_id, weather_city_id, date, temperature, humidity, weather_condition};
      result.push_back({city, weather});
    }

    return result;
  }

  // 获取原始存储对象
  BDBStorage& GetRawStorage() { return storage_; }

 private:
  std::string db_path_;
  BDBStorage storage_;
};

// 存储容器类，管理不同类型的存储实例
class StorageContainer {
 public:
  // 定义类型安全的创建存储实例的函数类型
  template <typename T>
  using TypedCreatorFunc = std::function<std::shared_ptr<T>()>;

  static StorageContainer& Instance() {
    static StorageContainer instance;
    return instance;
  }

  StorageContainer(const StorageContainer&) = delete;
  StorageContainer& operator=(const StorageContainer&) = delete;
  StorageContainer(StorageContainer&&) noexcept = delete;
  StorageContainer& operator=(StorageContainer&&) noexcept = delete;

  // 注册创建存储实例的函数，类型安全版本
  template <typename T>
  void RegisterCreator(const mx::dba::dbs::HandleKey& key, TypedCreatorFunc<T> creator) {
    std::lock_guard lock(mutex_);
    // 将类型信息编码到键中
    auto typed_key = std::make_pair(key, std::type_index(typeid(T)));
    // 将类型特定的创建函数存储到类型操作映射中
    auto type_erased_creator = [creator_func = std::move(creator)]() -> std::shared_ptr<IStorage> {
      return creator_func();
    };
    typed_creators_[typed_key] = std::move(type_erased_creator);
  }

  // 获取存储实例，如果不存在则创建，类型安全版本
  template <typename T>
  std::shared_ptr<T> GetStorage(const mx::dba::dbs::HandleKey& key) {
    std::lock_guard lock(mutex_);

    // 创建带类型信息的键
    auto typed_key = std::make_pair(key, std::type_index(typeid(T)));

    // 先检查缓存中是否已存在
    if (auto it = typed_storages_.find(typed_key); it != typed_storages_.end()) {
      // 这里的转换是安全的，因为我们在添加时已经确保了类型匹配
      return std::static_pointer_cast<T>(it->second);
    }

    // 如果不存在，检查是否有创建函数
    auto creator_it = typed_creators_.find(typed_key);
    if (creator_it == typed_creators_.end()) {
      return nullptr;  // 没有找到创建函数
    }

    // 使用创建函数创建存储实例
    auto storage = creator_it->second();
    if (!storage) {
      return nullptr;  // 创建失败
    }

    // 存储到缓存中
    typed_storages_[typed_key] = storage;

    // 同时将数据库路径保存到数据库文件级别的缓存中
    // 这样可以确保同一个数据库文件只有一个连接
    db_connections_[key] = storage->GetDatabasePath();

    // 这里的转换是安全的，因为我们在注册时已经确保了类型匹配
    return std::static_pointer_cast<T>(storage);
  }

  // 归还存储实例，实际上什么也不做，因为存储实例是共享的
  template <typename T>
  void ReleaseStorage(const std::shared_ptr<T>& /* storage */) const {
    // 什么也不做，因为存储实例是共享的
    // 智能指针会自动管理引用计数
  }

  // 移除存储实例
  template <typename T>
  void RemoveStorage(const mx::dba::dbs::HandleKey& key) {
    std::lock_guard lock(mutex_);
    auto typed_key = std::make_pair(key, std::type_index(typeid(T)));
    typed_storages_.erase(typed_key);

    // 检查是否还有其他存储实例使用这个数据库文件
    bool has_other_storages = false;
    for (const auto& [storage_key, storage_val] : typed_storages_) {
      if (storage_key.first == key) {
        has_other_storages = true;
        break;
      }
    }

    // 如果没有其他存储实例使用这个数据库文件，则删除连接记录
    if (!has_other_storages) {
      db_connections_.erase(key);
    }
  }

  // 清空所有存储实例
  void Clear() {
    std::lock_guard lock(mutex_);
    typed_storages_.clear();
    typed_creators_.clear();
    db_connections_.clear();
  }

 private:
  StorageContainer() = default;
  ~StorageContainer() = default;

  // 定义带类型信息的键类型
  using TypedKey = std::pair<mx::dba::dbs::HandleKey, std::type_index>;

  // 定义带类型信息的键的哈希函数
  struct TypedKeyHash {
    std::size_t operator()(const TypedKey& key) const {
      mx::dba::dbs::HandleKeyHash handle_hasher;
      return handle_hasher(key.first) ^ std::hash<std::type_index>()(key.second);
    }
  };

  // 定义带类型信息的键的相等函数
  struct TypedKeyEqual {
    bool operator()(const TypedKey& lhs, const TypedKey& rhs) const {
      return lhs.first == rhs.first && lhs.second == rhs.second;
    }
  };

  // 存储实例缓存，按类型存储
  std::unordered_map<TypedKey, std::shared_ptr<IStorage>, TypedKeyHash, TypedKeyEqual> typed_storages_{};

  // 创建函数映射，按类型存储
  std::unordered_map<TypedKey, std::function<std::shared_ptr<IStorage>()>, TypedKeyHash, TypedKeyEqual>
      typed_creators_{};

  // 数据库文件连接记录，用于跟踪每个数据库文件的连接
  std::unordered_map<mx::dba::dbs::HandleKey, std::string, mx::dba::dbs::HandleKeyHash> db_connections_{};

  // 互斥锁，保护对容器的并发访问
  std::mutex mutex_;
};

// 以下是为了向后兼容保留的旧接口函数
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

// 打印城市信息的辅助函数
void PrintCity(const mx::dba::dbs::City& city) {
  std::cout << "城市ID: " << city.city_id << ", 城市名: " << city.city_name << ", 国家: " << city.country
            << ", 人口: " << city.population << ", 面积: " << city.area << std::endl;
}

// 打印天气信息的辅助函数
void PrintWeather(const mx::dba::dbs::Weather& weather) {
  std::cout << "天气ID: " << weather.weather_id << ", 城市ID: " << weather.city_id
            << ", 日期: " << weather.date << ", 温度: " << weather.temperature
            << ", 湿度: " << weather.humidity << ", 天气状况: " << weather.weather_condition << std::endl;
}

// 创建 A 数据库存储的函数
std::shared_ptr<mx::dba::dbs::AStorage> CreateAStorage() {
  return std::make_shared<mx::dba::dbs::AStorage>(mx::dba::dbs::kADBFileName);
}

// 创建 B 数据库存储的函数
std::shared_ptr<mx::dba::dbs::BStorage> CreateBStorage() {
  return std::make_shared<mx::dba::dbs::BStorage>(mx::dba::dbs::kBDBFileName);
}

// 创建用于 A 数据库的 HandleKey
mx::dba::dbs::HandleKey CreateADBKey() {
  // 创建一个唯一标识 A 数据库的 HandleKey
  mx::dba::dbs::HandleKey key;
  key.param1 = 1;  // 数据库类型：A
  key.param2 = 0;  // 保留位
  key.param3 = 0;  // 保留位
  key.param4 = 0;  // 保留位
  return key;
}

// 创建用于 B 数据库的 HandleKey
mx::dba::dbs::HandleKey CreateBDBKey() {
  // 创建一个唯一标识 B 数据库的 HandleKey
  mx::dba::dbs::HandleKey key;
  key.param1 = 2;  // 数据库类型：B
  key.param2 = 0;  // 保留位
  key.param3 = 0;  // 保留位
  key.param4 = 0;  // 保留位
  return key;
}

int main() {
  try {
    // 初始化存储容器
    auto& container = mx::dba::dbs::StorageContainer::Instance();

    // 注册创建函数
    container.RegisterCreator<mx::dba::dbs::AStorage>(CreateADBKey(), CreateAStorage);
    container.RegisterCreator<mx::dba::dbs::BStorage>(CreateBDBKey(), CreateBStorage);

    // 使用 A 数据库存储
    std::cout << "\n===== 使用 A 数据库 =====\n" << std::endl;
    auto a_db = container.GetStorage<mx::dba::dbs::AStorage>(CreateADBKey());
    if (a_db) {
      // 1. 读取 user 表的全部数据
      std::cout << "===== 所有用户 =====" << std::endl;
      auto all_users = a_db->GetAll<mx::dba::dbs::User>();
      for (const auto& user : all_users) {
        PrintUser(user);
      }
      std::cout << std::endl;

      // 2. 根据条件读取 user 表的部分数据
      std::cout << "===== 年龄大于 30 且用户名包含 '李' 的用户 =====" << std::endl;
      auto filtered_users = a_db->GetUsersByCondition("%李%", 30);
      for (const auto& user : filtered_users) {
        PrintUser(user);
      }
      std::cout << std::endl;

      // 3. 读取 order 表的全部数据
      std::cout << "===== 所有订单 =====" << std::endl;
      auto all_orders = a_db->GetAll<mx::dba::dbs::Order>();
      for (const auto& order : all_orders) {
        PrintOrder(order);
      }
      std::cout << std::endl;

      // 4. 根据条件读取 order 表的部分数据
      std::cout << "===== 用户ID为 1 且价格大于 1000 的订单 =====" << std::endl;
      auto filtered_orders = a_db->GetOrdersByCondition(1, 1000.0);
      for (const auto& order : filtered_orders) {
        PrintOrder(order);
      }
      std::cout << std::endl;

      // 5. 读取 user 表和 order 表的关联数据
      std::cout << "===== 用户和订单关联数据 =====" << std::endl;
      auto user_orders = a_db->GetUserOrders();
      for (const auto& user_order : user_orders) {
        std::cout << "用户: " << user_order.user.username << " (ID: " << user_order.user.user_id << ")"
                  << " 订购了: " << user_order.order.product_name << " 价格: " << user_order.order.price
                  << std::endl;
      }
      std::cout << std::endl;
    }

    // 归还 A 数据库存储
    container.ReleaseStorage(a_db);

    // 使用 B 数据库存储
    std::cout << "\n===== 使用 B 数据库 =====\n" << std::endl;
    auto b_db = container.GetStorage<mx::dba::dbs::BStorage>(CreateBDBKey());
    if (b_db) {
      // 1. 读取 city 表的全部数据
      std::cout << "===== 所有城市 =====" << std::endl;
      auto all_cities = b_db->GetAll<mx::dba::dbs::City>();
      for (const auto& city : all_cities) {
        PrintCity(city);
      }
      std::cout << std::endl;

      // 2. 根据条件读取 city 表的部分数据
      std::cout << "===== 中国的人口大于 2000万的城市 =====" << std::endl;
      auto filtered_cities = b_db->GetCitiesByCondition("中国", 20000000);
      for (const auto& city : filtered_cities) {
        PrintCity(city);
      }
      std::cout << std::endl;

      // 3. 读取 weather 表的全部数据
      std::cout << "===== 所有天气记录 =====" << std::endl;
      auto all_weather = b_db->GetAll<mx::dba::dbs::Weather>();
      for (const auto& weather : all_weather) {
        PrintWeather(weather);
      }
      std::cout << std::endl;

      // 4. 根据条件读取 weather 表的部分数据
      std::cout << "===== 城市ID为 1 且温度大于 26 度的天气记录 =====" << std::endl;
      auto filtered_weather = b_db->GetWeatherByCondition(1, 26.0);
      for (const auto& weather : filtered_weather) {
        PrintWeather(weather);
      }
      std::cout << std::endl;

      // 5. 读取 city 表和 weather 表的关联数据
      std::cout << "===== 城市和天气关联数据 =====" << std::endl;
      auto city_weathers = b_db->GetCityWeathers();
      for (const auto& city_weather : city_weathers) {
        std::cout << "城市: " << city_weather.city.city_name << " (ID: " << city_weather.city.city_id << ")"
                  << " 天气状况: " << city_weather.weather.weather_condition
                  << " 温度: " << city_weather.weather.temperature << std::endl;
      }
    }

    // 归还 B 数据库存储
    container.ReleaseStorage(b_db);

  } catch (const std::exception& e) {
    std::cerr << "错误: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
