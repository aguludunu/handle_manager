#include <iostream>
#include <string>
#include <vector>

#include "nds_sqlite/nds_sqlite3.h"

// 错误处理函数
void handle_error(int rc, const char* operation, sqlite3* db = nullptr) {
  if (rc != SQLITE_OK) {
    std::cerr << "错误: " << operation << " 失败: " << (db ? sqlite3_errmsg(db) : "未知错误") << std::endl;
    if (db) {
      sqlite3_close(db);
    }
    exit(1);
  }
}

// 执行SQL语句的辅助函数
void exec_sql(sqlite3* db, const std::string& sql, const std::string& operation) {
  char* err_msg = nullptr;
  int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &err_msg);

  if (rc != SQLITE_OK) {
    std::cerr << "SQL错误: " << operation << ": " << err_msg << std::endl;
    sqlite3_free(err_msg);
    sqlite3_close(db);
    exit(1);
  }
}

// 创建A数据库和表的函数
void create_database_a(const std::string& db_name) {
  sqlite3* db = nullptr;
  int rc = sqlite3_open(db_name.c_str(), &db);
  handle_error(rc, ("打开数据库 " + db_name).c_str(), db);

  std::cout << "成功创建数据库: " << db_name << std::endl;

  // 创建用户表
  std::string create_users =
      "CREATE TABLE IF NOT EXISTS Users ("
      "user_id INTEGER PRIMARY KEY,"
      "username TEXT NOT NULL,"
      "email TEXT,"
      "age INTEGER,"
      "registration_date INTEGER"
      ");";

  exec_sql(db, create_users, "创建Users表");
  std::cout << "成功创建表: Users" << std::endl;

  // 创建订单表
  std::string create_orders =
      "CREATE TABLE IF NOT EXISTS Orders ("
      "order_id INTEGER PRIMARY KEY,"
      "user_id INTEGER,"
      "product_name TEXT NOT NULL,"
      "quantity INTEGER,"
      "price REAL,"
      "order_date INTEGER,"
      "FOREIGN KEY (user_id) REFERENCES Users(user_id)"
      ");";

  exec_sql(db, create_orders, "创建Orders表");
  std::cout << "成功创建表: Orders" << std::endl;

  // 向用户表插入数据
  std::vector<std::string> users_inserts = {
      "INSERT INTO Users (username, email, age, registration_date) VALUES ('张三', 'zhangsan@example.com', "
      "28, 1620000000);",
      "INSERT INTO Users (username, email, age, registration_date) VALUES ('李四', 'lisi@example.com', 32, "
      "1620100000);",
      "INSERT INTO Users (username, email, age, registration_date) VALUES ('王五', 'wangwu@example.com', "
      "32, 1620200000);",
      "INSERT INTO Users (username, email, age, registration_date) VALUES ('赵六', 'zhaoliu@example.com', "
      "40, 1620300000);"};

  for (const auto& sql : users_inserts) {
    exec_sql(db, sql, "向Users表插入数据");
  }
  std::cout << "成功向Users表插入4条数据" << std::endl;

  // 向订单表插入数据
  std::vector<std::string> orders_inserts = {
      "INSERT INTO Orders (user_id, product_name, quantity, price, order_date) VALUES (1, '手机', 1, "
      "3999.99, 1620400000);",
      "INSERT INTO Orders (user_id, product_name, quantity, price, order_date) VALUES (1, '耳机', 2, "
      "299.50, 1620500000);",
      "INSERT INTO Orders (user_id, product_name, quantity, price, order_date) VALUES (2, '笔记本电脑', 1, "
      "6999.00, 1620600000);",
      "INSERT INTO Orders (user_id, product_name, quantity, price, order_date) VALUES (3, '平板电脑', 1, "
      "2599.00, 1620700000);",
      "INSERT INTO Orders (user_id, product_name, quantity, price, order_date) VALUES (4, '智能手表', 1, "
      "1299.00, 1620800000);"};

  for (const auto& sql : orders_inserts) {
    exec_sql(db, sql, "向Orders表插入数据");
  }
  std::cout << "成功向Orders表插入5条数据" << std::endl;

  // 创建数据类型表，包含各种类型的数据
  const std::string create_data_types =
      "CREATE TABLE IF NOT EXISTS DataTypes ("
      "id INTEGER PRIMARY KEY,"
      "int_not_null INTEGER NOT NULL,"
      "int_nullable INTEGER,"
      "float_not_null REAL NOT NULL,"
      "float_nullable REAL,"
      "text_not_null TEXT NOT NULL,"
      "text_nullable TEXT,"
      "blob_data BLOB"
      ");";

  exec_sql(db, create_data_types, "创建DataTypes表");
  std::cout << "成功创建表: DataTypes" << std::endl;

  // 在 int_not_null 列上创建索引
  const std::string create_int_index =
      "CREATE INDEX IF NOT EXISTS idx_data_types_int_not_null "
      "ON DataTypes(int_not_null);";
  exec_sql(db, create_int_index, "创建int_not_null索引");
  std::cout << "成功创建索引: idx_data_types_int_not_null" << std::endl;

  // 在 text_not_null 和 float_not_null 列的组合上创建复合索引
  const std::string create_composite_index =
      "CREATE INDEX IF NOT EXISTS idx_data_types_text_float "
      "ON DataTypes(text_not_null, float_not_null);";
  exec_sql(db, create_composite_index, "创建text_not_null和float_not_null的复合索引");
  std::cout << "成功创建索引: idx_data_types_text_float" << std::endl;

  // 向DataTypes表插入数据
  const std::vector<std::string> data_types_inserts = {
      "INSERT INTO DataTypes (int_not_null, int_nullable, float_not_null, float_nullable, text_not_null, "
      "text_nullable, blob_data) "
      "VALUES (100, 200, 3.14, 2.71828, '必填字符串', '可空字符串', X'48656C6C6F20576F726C64');",

      "INSERT INTO DataTypes (int_not_null, int_nullable, float_not_null, float_nullable, text_not_null, "
      "text_nullable, blob_data) "
      "VALUES (101, NULL, 6.28, NULL, '另一个必填字符串', NULL, X'42696E61727920446174612054657374');",

      "INSERT INTO DataTypes (int_not_null, int_nullable, float_not_null, float_nullable, text_not_null, "
      "text_nullable, blob_data) "
      "VALUES (102, 300, 1.618, 0.577, '第三个字符串', '非空可选字符串', NULL);"};

  for (const auto& sql : data_types_inserts) {
    exec_sql(db, sql, "向DataTypes表插入数据");
  }
  std::cout << "成功向DataTypes表插入3条数据" << std::endl;

  // 关闭数据库连接
  sqlite3_close(db);
  std::cout << "数据库 " << db_name << " 创建完成" << std::endl << std::endl;
}

// 创建B数据库和表的函数
void create_database_b(const std::string& db_name) {
  sqlite3* db = nullptr;
  int rc = sqlite3_open(db_name.c_str(), &db);
  handle_error(rc, ("打开数据库 " + db_name).c_str(), db);

  std::cout << "成功创建数据库: " << db_name << std::endl;

  // 创建城市表
  std::string create_cities =
      "CREATE TABLE IF NOT EXISTS Cities ("
      "city_id INTEGER PRIMARY KEY,"
      "city_name TEXT NOT NULL,"
      "country TEXT NOT NULL,"
      "population INTEGER,"
      "area REAL"
      ");";

  exec_sql(db, create_cities, "创建Cities表");
  std::cout << "成功创建表: Cities" << std::endl;

  // 创建天气表
  std::string create_weather =
      "CREATE TABLE IF NOT EXISTS Weather ("
      "weather_id INTEGER PRIMARY KEY,"
      "city_id INTEGER,"
      "date INTEGER,"
      "temperature REAL,"
      "humidity REAL,"
      "weather_condition TEXT,"
      "FOREIGN KEY (city_id) REFERENCES Cities(city_id)"
      ");";

  exec_sql(db, create_weather, "创建Weather表");
  std::cout << "成功创建表: Weather" << std::endl;

  // 向城市表插入数据
  std::vector<std::string> cities_inserts = {
      "INSERT INTO Cities (city_name, country, population, area) VALUES ('北京', '中国', 21540000, "
      "16410.54);",
      "INSERT INTO Cities (city_name, country, population, area) VALUES ('上海', '中国', 24280000, "
      "6340.50);",
      "INSERT INTO Cities (city_name, country, population, area) VALUES ('广州', '中国', 15300000, "
      "7434.40);",
      "INSERT INTO Cities (city_name, country, population, area) VALUES ('深圳', '中国', 13440000, "
      "1997.47);"};

  for (const auto& sql : cities_inserts) {
    exec_sql(db, sql, "向Cities表插入数据");
  }
  std::cout << "成功向Cities表插入4条数据" << std::endl;

  // 向天气表插入数据
  std::vector<std::string> weather_inserts = {
      "INSERT INTO Weather (city_id, date, temperature, humidity, weather_condition) VALUES (1, "
      "1620000000, 25.5, 60.2, '晴');",
      "INSERT INTO Weather (city_id, date, temperature, humidity, weather_condition) VALUES (1, "
      "1620086400, 27.0, 55.8, '多云');",
      "INSERT INTO Weather (city_id, date, temperature, humidity, weather_condition) VALUES (2, "
      "1620000000, 26.8, 70.5, '阴');",
      "INSERT INTO Weather (city_id, date, temperature, humidity, weather_condition) VALUES (2, "
      "1620086400, 28.2, 65.3, '晴');",
      "INSERT INTO Weather (city_id, date, temperature, humidity, weather_condition) VALUES (3, "
      "1620000000, 30.5, 75.0, '多云');",
      "INSERT INTO Weather (city_id, date, temperature, humidity, weather_condition) VALUES (4, "
      "1620000000, 29.8, 72.6, '晴');"};

  for (const auto& sql : weather_inserts) {
    exec_sql(db, sql, "向Weather表插入数据");
  }
  std::cout << "成功向Weather表插入6条数据" << std::endl;

  // 关闭数据库连接
  sqlite3_close(db);
  std::cout << "数据库 " << db_name << " 创建完成" << std::endl << std::endl;
}

int main() {
  // 创建A.db数据库
  create_database_a("A.db");

  // 创建B.db数据库
  create_database_b("B.db");

  std::cout << "所有数据库创建完成！" << std::endl;
  return 0;
}
