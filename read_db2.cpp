#include <chrono>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <tuple>
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

static constexpr const char* kADBFileName = "A.db";
static constexpr const char* kBDBFileName = "B.db";

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

// 存储对象的键类型，由 HandleKey 和类型索引组成
using StorageKey = std::pair<HandleKey, std::type_index>;

// StorageKey 的哈希函数
struct StorageKeyHash {
  std::size_t operator()(const StorageKey& key) const {
    HandleKeyHash handle_hasher;
    return handle_hasher(key.first) ^ std::hash<std::type_index>()(key.second);
  }
};

// StorageKey 的比较函数
struct StorageKeyEqual {
  bool operator()(const StorageKey& lhs, const StorageKey& rhs) const {
    return lhs.first == rhs.first && lhs.second == rhs.second;
  }
};

// 类型索引持有者，用于预计算类型索引
template <typename T>
struct TypeIndexHolder {
  static const std::type_index value;
};

template <typename T>
const std::type_index TypeIndexHolder<T>::value = std::type_index(typeid(T));

class IStorage {
 public:
  virtual ~IStorage() = default;

  [[nodiscard]] virtual std::string GetDatabasePath() const = 0;
};

// 根据HandleKey获取数据库路径
static std::string GetDbPathFromKey(const HandleKey& key) {
  // 根据HandleKey的param1字段判断数据库类型
  switch (key.param1) {
    case 1:  // A数据库
      return kADBFileName;
    case 2:  // B数据库
      return kBDBFileName;
    default:
      return "";
  }
}

// 存储对象的LRU包装类，包含最后使用时间信息
struct StorageLRU {
  HandleKey key{};                             // 存储对象的键
  std::type_index type_idx{typeid(void)};      // 存储对象的类型
  std::shared_ptr<IStorage> storage{nullptr};  // 实际的存储对象
  std::chrono::steady_clock::time_point last_used_time{std::chrono::steady_clock::now()};  // 最后使用时间

  StorageLRU() = default;

  StorageLRU(const HandleKey& k, const std::type_index& t, std::shared_ptr<IStorage> s)
      : key(k), type_idx(t), storage(std::move(s)) {}

  StorageLRU(StorageLRU&&) noexcept = default;
  StorageLRU& operator=(StorageLRU&&) noexcept = default;
  StorageLRU(const StorageLRU&) = delete;
  StorageLRU& operator=(const StorageLRU&) = delete;
};

// 存储容器类，管理所有存储对象
class StorageContainer {
 public:
  using CreatorFunc = std::function<std::shared_ptr<IStorage>(const std::string_view&)>;

  // 默认最大存储对象数量
  static constexpr size_t kDefaultMaxStorageCount{100};

  static StorageContainer& Instance() {
    static StorageContainer instance;
    return instance;
  }

  StorageContainer(const StorageContainer&) = delete;
  StorageContainer& operator=(const StorageContainer&) = delete;
  StorageContainer(StorageContainer&&) noexcept = delete;
  StorageContainer& operator=(StorageContainer&&) noexcept = delete;

  void SetMaxStorageCount(size_t count) {
    std::lock_guard lock(mutex_);
    max_storage_count_ = count;
  }

  size_t GetStorageCount() const {
    std::lock_guard lock(mutex_);
    return storage_list_.size();
  }

  // 注册存储创建器
  template <typename T>
  void RegisterStorageCreator(const HandleKey& key,
                              std::function<std::shared_ptr<T>(const std::string_view&)> creator) {
    static_assert(std::is_base_of_v<IStorage, T>, "T must inherit from IStorage");
    std::lock_guard lock(mutex_);

    auto wrapped_creator =
        [creator_func = std::move(creator)](const std::string_view& db_path) -> std::shared_ptr<IStorage> {
      return creator_func(db_path);
    };

    // 使用预计算的类型索引
    const auto& type_idx = TypeIndexHolder<T>::value;
    StorageKey creator_key{key, type_idx};
    creators_[creator_key] = std::move(wrapped_creator);
  }

  // 获取存储对象，将其从容器中移出
  template <typename T>
  std::shared_ptr<T> GetStorage(const HandleKey& key) {
    static_assert(std::is_base_of_v<IStorage, T>, "T must inherit from IStorage");
    std::lock_guard lock(mutex_);

    // 使用预计算的类型索引
    const auto& type_idx = TypeIndexHolder<T>::value;
    StorageKey storage_key{key, type_idx};

    // 先尝试从容器中获取存储对象
    if (auto storage = ExtractStorage<T>(storage_key); storage) {
      return storage;
    }

    // 如果不存在，则创建新的存储对象
    return CreateStorage<T>(key, type_idx);
  }

  // 归还存储对象，将其放回容器中
  template <typename T>
  void GiveBack(const HandleKey& key, std::shared_ptr<T> storage) {
    static_assert(std::is_base_of_v<IStorage, T>, "T must inherit from IStorage");
    if (!storage) {
      return;
    }

    std::lock_guard lock(mutex_);

    // 使用预计算的类型索引
    const auto& type_idx = TypeIndexHolder<T>::value;

    // 确保容量足够
    EnsureCapacity();

    // 将存储对象放入容器
    InsertStorage(key, type_idx, std::move(storage));
  }

  // 关闭指定HandleKey和类型的存储对象
  template <typename T>
  void CloseStorage(const HandleKey& key) {
    static_assert(std::is_base_of_v<IStorage, T>, "T must inherit from IStorage");
    std::lock_guard lock(mutex_);

    // 使用预计算的类型索引
    const auto& type_idx = TypeIndexHolder<T>::value;
    StorageKey storage_key{key, type_idx};

    // 移除指定的存储对象
    RemoveStorage(storage_key);
  }

  // 清空所有存储对象和创建器
  void Clear() {
    std::lock_guard lock(mutex_);
    storage_list_.clear();
    storages_.clear();
    creators_.clear();
  }

 private:
  StorageContainer() = default;
  ~StorageContainer() = default;

  // 从容器中移除最久未使用的存储对象
  void RemoveOldestStorage() {
    if (storage_list_.empty()) {
      return;
    }

    // 链表尾部是最久未使用的存储对象
    auto oldest_it = std::prev(storage_list_.end());
    StorageKey oldest_key{oldest_it->key, oldest_it->type_idx};
    storages_.erase(oldest_key);
    storage_list_.pop_back();
  }

  // 检查并维持存储对象数量在最大限制之内
  void EnsureCapacity() {
    if (storage_list_.size() >= max_storage_count_ && !storage_list_.empty()) {
      RemoveOldestStorage();
    }
  }

  // 从容器中移除指定的存储对象
  void RemoveStorage(const StorageKey& key) {
    auto it = storages_.find(key);
    if (it != storages_.end()) {
      storage_list_.erase(it->second);
      storages_.erase(it);
    }
  }

  // 从容器中获取存储对象，并将其移出
  template <typename T>
  std::shared_ptr<T> ExtractStorage(const StorageKey& key) {
    auto it = storages_.find(key);
    if (it == storages_.end()) {
      return nullptr;
    }

    auto list_it = it->second;
    auto storage = std::static_pointer_cast<T>(std::move(list_it->storage));

    // 从链表和查找表中移除
    storage_list_.erase(list_it);
    storages_.erase(it);

    return storage;
  }

  // 使用创建器创建新的存储对象
  template <typename T>
  std::shared_ptr<T> CreateStorage(const HandleKey& key, const std::type_index& type_idx) {
    StorageKey storage_key{key, type_idx};

    // 查找创建器
    auto creator_it = creators_.find(storage_key);
    if (creator_it == creators_.end()) {
      return nullptr;
    }

    // 通过 HandleKey 获取数据库路径
    std::string db_path = GetDbPathFromKey(key);
    if (db_path.empty()) {
      return nullptr;
    }

    // 确保容量足够
    EnsureCapacity();

    // 使用数据库路径创建存储实例
    auto storage = creator_it->second(db_path);
    if (!storage) {
      return nullptr;
    }

    return std::static_pointer_cast<T>(storage);
  }

  // 将存储对象放入容器
  template <typename T>
  void InsertStorage(const HandleKey& key, const std::type_index& type_idx, std::shared_ptr<T> storage) {
    StorageKey storage_key{key, type_idx};

    // 将 storage 放入链表头部（最近使用）
    storage_list_.emplace_front(key, type_idx, std::move(storage));

    // 将迭代器放入查找表
    storages_[storage_key] = storage_list_.begin();
  }

  // LRU 链表，存放所有的 StorageLRU 对象
  std::list<StorageLRU> storage_list_{};

  // 存储对象查找表，键为 HandleKey 和类型索引的组合，值为 storage_list_ 中的迭代器
  std::unordered_map<StorageKey, std::list<StorageLRU>::iterator, StorageKeyHash, StorageKeyEqual>
      storages_{};

  // 创建器容器
  std::unordered_map<StorageKey, CreatorFunc, StorageKeyHash, StorageKeyEqual> creators_{};

  // 最大存储对象数量
  size_t max_storage_count_{kDefaultMaxStorageCount};

  // 互斥锁，保证线程安全
  mutable std::mutex mutex_{};
};

// *********************************************************************************************************
// *********************************************************************************************************
// *********************************************************************************************************

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

std::shared_ptr<AStorage> CreateAStorage(const std::string_view& db_path) {
  return std::make_shared<AStorage>(db_path);
}

HandleKey CreateADBKey() {
  HandleKey key;
  key.param1 = 1;  // 数据库类型：A
  key.param2 = 0;  // 保留位
  key.param3 = 0;  // 保留位
  key.param4 = 0;  // 保留位
  return key;
}

// *********************************************************************************************************
// *********************************************************************************************************
// *********************************************************************************************************

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

std::shared_ptr<BStorage> CreateBStorage(const std::string_view& db_path) {
  return std::make_shared<BStorage>(db_path);
}

HandleKey CreateBDBKey() {
  HandleKey key;
  key.param1 = 2;  // 数据库类型：B
  key.param2 = 0;  // 保留位
  key.param3 = 0;  // 保留位
  key.param4 = 0;  // 保留位
  return key;
}

}  // namespace mx::dba::dbs

// *********************************************************************************************************
// *********************************************************************************************************
// *********************************************************************************************************

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

using mx::dba::dbs::CreateADBKey;
using mx::dba::dbs::CreateAStorage;
using mx::dba::dbs::CreateBDBKey;
using mx::dba::dbs::CreateBStorage;

int main() {
  // 初始化存储容器
  auto& container = mx::dba::dbs::StorageContainer::Instance();

  // 注册创建函数
  container.RegisterStorageCreator<mx::dba::dbs::AStorage>(
      CreateADBKey(), [](const std::string_view& db_path) { return CreateAStorage(db_path); });
  container.RegisterStorageCreator<mx::dba::dbs::BStorage>(
      CreateBDBKey(), [](const std::string_view& db_path) { return CreateBStorage(db_path); });

  try {
    // 使用 A 数据库存储
    std::cout << "\n===== 使用 A 数据库 =====\n" << std::endl;
    auto a_storage = container.GetStorage<mx::dba::dbs::AStorage>(CreateADBKey());
    if (!a_storage) {
      std::cerr << "Failed to get A storage" << std::endl;
      return 1;
    }

    // 1. 读取 user 表的全部数据
    std::cout << "===== 所有用户 =====" << std::endl;
    auto all_users = a_storage->GetAll<mx::dba::dbs::User>();
    for (const auto& user : all_users) {
      PrintUser(user);
    }
    std::cout << std::endl;

    // 2. 根据条件读取 user 表的部分数据
    std::cout << "===== 年龄大于 30 且用户名包含 '李' 的用户 =====" << std::endl;
    auto filtered_users = a_storage->GetUsersByCondition("%李%", 30);
    for (const auto& user : filtered_users) {
      PrintUser(user);
    }
    std::cout << std::endl;

    // 3. 读取 order 表的全部数据
    std::cout << "===== 所有订单 =====" << std::endl;
    auto all_orders = a_storage->GetAll<mx::dba::dbs::Order>();
    for (const auto& order : all_orders) {
      PrintOrder(order);
    }
    std::cout << std::endl;

    // 4. 根据条件读取 order 表的部分数据
    std::cout << "===== 用户ID为 1 且价格大于 1000 的订单 =====" << std::endl;
    auto filtered_orders = a_storage->GetOrdersByCondition(1, 1000.0);
    for (const auto& order : filtered_orders) {
      PrintOrder(order);
    }
    std::cout << std::endl;

    // 5. 读取 user 表和 order 表的关联数据
    std::cout << "===== 用户和订单关联数据 =====" << std::endl;
    auto user_orders = a_storage->GetUserOrders();
    for (const auto& user_order : user_orders) {
      std::cout << "用户: " << user_order.user.username << " (ID: " << user_order.user.user_id << ")"
                << " 订购了: " << user_order.order.product_name << " 价格: " << user_order.order.price
                << std::endl;
    }
    std::cout << std::endl;

    // A 数据库存储使用完毕
    // 不需要归还，智能指针会自动管理

    // 使用 B 数据库存储
    std::cout << "\n===== 使用 B 数据库 =====\n" << std::endl;
    auto b_storage = container.GetStorage<mx::dba::dbs::BStorage>(CreateBDBKey());
    if (!b_storage) {
      std::cerr << "Failed to get B storage" << std::endl;
      return 1;
    }

    // 1. 读取 city 表的全部数据
    std::cout << "===== 所有城市 =====" << std::endl;
    auto all_cities = b_storage->GetAll<mx::dba::dbs::City>();
    for (const auto& city : all_cities) {
      PrintCity(city);
    }
    std::cout << std::endl;

    // 2. 根据条件读取 city 表的部分数据
    std::cout << "===== 中国的人口大于 2000万的城市 =====" << std::endl;
    auto filtered_cities = b_storage->GetCitiesByCondition("中国", 20000000);
    for (const auto& city : filtered_cities) {
      PrintCity(city);
    }
    std::cout << std::endl;

    // 3. 读取 weather 表的全部数据
    std::cout << "===== 所有天气记录 =====" << std::endl;
    auto all_weather = b_storage->GetAll<mx::dba::dbs::Weather>();
    for (const auto& weather : all_weather) {
      PrintWeather(weather);
    }
    std::cout << std::endl;

    // 4. 根据条件读取 weather 表的部分数据
    std::cout << "===== 城市ID为 1 且温度大于 26 度的天气记录 =====" << std::endl;
    auto filtered_weather = b_storage->GetWeatherByCondition(1, 26.0);
    for (const auto& weather : filtered_weather) {
      PrintWeather(weather);
    }
    std::cout << std::endl;

    // 5. 读取 city 表和 weather 表的关联数据
    std::cout << "===== 城市和天气关联数据 =====" << std::endl;
    auto city_weathers = b_storage->GetCityWeathers();
    for (const auto& city_weather : city_weathers) {
      std::cout << "城市: " << city_weather.city.city_name << " (ID: " << city_weather.city.city_id << ")"
                << " 天气状况: " << city_weather.weather.weather_condition
                << " 温度: " << city_weather.weather.temperature << std::endl;
    }
    std::cout << std::endl;

    std::cout << "\n===== 成功完成所有操作 =====\n" << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "错误: " << e.what() << std::endl;
    return 1;
  }

  // 测试 StorageContainer 类的各个接口
  std::cout << "\n===== 测试 StorageContainer 类的各个接口 =====\n" << std::endl;
  
  // 设置最大存储对象数量
  container.SetMaxStorageCount(5);
  std::cout << "设置最大存储对象数量为: 5" << std::endl;
  
  // 获取当前存储对象数量
  std::cout << "当前存储对象数量: " << container.GetStorageCount() << std::endl;
  
  // 获取 AStorage 对象
  std::cout << "\n获取 AStorage 对象..." << std::endl;
  auto a_storage_test = container.GetStorage<mx::dba::dbs::AStorage>(mx::dba::dbs::CreateADBKey());
  if (a_storage_test) {
    std::cout << "获取 AStorage 对象成功，数据库路径: " << a_storage_test->GetDatabasePath() << std::endl;
  } else {
    std::cout << "获取 AStorage 对象失败" << std::endl;
    return 1;
  }
  
  // 获取 BStorage 对象
  std::cout << "\n获取 BStorage 对象..." << std::endl;
  auto b_storage_test = container.GetStorage<mx::dba::dbs::BStorage>(mx::dba::dbs::CreateBDBKey());
  if (b_storage_test) {
    std::cout << "获取 BStorage 对象成功，数据库路径: " << b_storage_test->GetDatabasePath() << std::endl;
  } else {
    std::cout << "获取 BStorage 对象失败" << std::endl;
    return 1;
  }
  
  // 获取当前存储对象数量
  std::cout << "\n当前存储对象数量: " << container.GetStorageCount() << std::endl;
  
  // 归还 AStorage 对象
  std::cout << "\n归还 AStorage 对象..." << std::endl;
  container.GiveBack<mx::dba::dbs::AStorage>(mx::dba::dbs::CreateADBKey(), std::move(a_storage_test));
  std::cout << "归还 AStorage 对象成功" << std::endl;
  
  // 验证 a_storage_test 已被移动
  if (!a_storage_test) {
    std::cout << "验证: a_storage_test 已被移动，现在为空" << std::endl;
  }
  
  // 获取当前存储对象数量
  std::cout << "\n当前存储对象数量: " << container.GetStorageCount() << std::endl;
  
  // 归还 BStorage 对象
  std::cout << "\n归还 BStorage 对象..." << std::endl;
  container.GiveBack<mx::dba::dbs::BStorage>(mx::dba::dbs::CreateBDBKey(), std::move(b_storage_test));
  std::cout << "归还 BStorage 对象成功" << std::endl;
  
  // 获取当前存储对象数量
  std::cout << "\n当前存储对象数量: " << container.GetStorageCount() << std::endl;
  
  // 关闭指定的存储对象
  std::cout << "\n关闭指定的 AStorage 对象..." << std::endl;
  container.CloseStorage<mx::dba::dbs::AStorage>(mx::dba::dbs::CreateADBKey());
  std::cout << "关闭 AStorage 对象成功" << std::endl;
  
  // 获取当前存储对象数量
  std::cout << "当前存储对象数量: " << container.GetStorageCount() << std::endl;
  
  // 清空所有存储对象
  std::cout << "\n清空所有存储对象..." << std::endl;
  container.Clear();
  std::cout << "清空所有存储对象成功" << std::endl;
  
  // 获取当前存储对象数量
  std::cout << "当前存储对象数量: " << container.GetStorageCount() << std::endl;
  
  std::cout << "\n===== 测试完成 =====" << std::endl;
  
  return 0;
}
