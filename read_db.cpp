#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include "nds_sqlite/nds_sqlite3.h"

// 错误处理函数
void HandleError(int rc, const char* operation, sqlite3* db = nullptr) {
    if (rc != SQLITE_OK && rc != SQLITE_ROW && rc != SQLITE_DONE) {
        std::cerr << "错误: " << operation << " 失败: " 
                  << (db ? sqlite3_errmsg(db) : "未知错误") << std::endl;
        if (db) {
            sqlite3_close(db);
        }
        exit(1);
    }
}

// 打印表头
void PrintTableHeader(const std::vector<std::string>& columns) {
    // 计算每列的宽度
    const int kColumnWidth = 20;
    
    // 打印表头
    for (const auto& column : columns) {
        std::cout << std::left << std::setw(kColumnWidth) << column << " | ";
    }
    std::cout << std::endl;
    
    // 打印分隔线
    for (size_t i = 0; i < columns.size(); ++i) {
        std::cout << std::string(kColumnWidth, '-') << " | ";
    }
    std::cout << std::endl;
}

// 读取并打印A.db中的Users表
void ReadUsersTable(sqlite3* db) {
    std::cout << "\n===== A.db - Users表 =====\n" << std::endl;
    
    const char* kSql = "SELECT user_id, username, email, age, registration_date FROM Users;";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(db, kSql, -1, &stmt, nullptr);
    HandleError(rc, "准备查询Users表", db);
    
    // 打印表头
    std::vector<std::string> columns = {"用户ID", "用户名", "邮箱", "年龄", "注册日期"};
    PrintTableHeader(columns);
    
    // 打印数据
    const int kColumnWidth = 20;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int user_id = sqlite3_column_int(stmt, 0);
        const char* username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        int age = sqlite3_column_int(stmt, 3);
        int registration_date = sqlite3_column_int(stmt, 4);
        
        std::cout << std::left << std::setw(kColumnWidth) << user_id << " | "
                  << std::left << std::setw(kColumnWidth) << (username ? username : "NULL") << " | "
                  << std::left << std::setw(kColumnWidth) << (email ? email : "NULL") << " | "
                  << std::left << std::setw(kColumnWidth) << age << " | "
                  << std::left << std::setw(kColumnWidth) << registration_date << " | "
                  << std::endl;
    }
    
    HandleError(rc, "读取Users表数据", db);
    sqlite3_finalize(stmt);
}

// 读取并打印A.db中的Orders表
void ReadOrdersTable(sqlite3* db) {
    std::cout << "\n===== A.db - Orders表 =====\n" << std::endl;
    
    const char* kSql = "SELECT order_id, user_id, product_name, quantity, price, order_date FROM Orders;";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(db, kSql, -1, &stmt, nullptr);
    HandleError(rc, "准备查询Orders表", db);
    
    // 打印表头
    std::vector<std::string> columns = {"订单ID", "用户ID", "产品名称", "数量", "价格", "订单日期"};
    PrintTableHeader(columns);
    
    // 打印数据
    const int kColumnWidth = 20;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int order_id = sqlite3_column_int(stmt, 0);
        int user_id = sqlite3_column_int(stmt, 1);
        const char* product_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        int quantity = sqlite3_column_int(stmt, 3);
        double price = sqlite3_column_double(stmt, 4);
        int order_date = sqlite3_column_int(stmt, 5);
        
        std::cout << std::left << std::setw(kColumnWidth) << order_id << " | "
                  << std::left << std::setw(kColumnWidth) << user_id << " | "
                  << std::left << std::setw(kColumnWidth) << (product_name ? product_name : "NULL") << " | "
                  << std::left << std::setw(kColumnWidth) << quantity << " | "
                  << std::left << std::setw(kColumnWidth) << price << " | "
                  << std::left << std::setw(kColumnWidth) << order_date << " | "
                  << std::endl;
    }
    
    HandleError(rc, "读取Orders表数据", db);
    sqlite3_finalize(stmt);
}

// 读取并打印B.db中的Cities表
void ReadCitiesTable(sqlite3* db) {
    std::cout << "\n===== B.db - Cities表 =====\n" << std::endl;
    
    const char* kSql = "SELECT city_id, city_name, country, population, area FROM Cities;";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(db, kSql, -1, &stmt, nullptr);
    HandleError(rc, "准备查询Cities表", db);
    
    // 打印表头
    std::vector<std::string> columns = {"城市ID", "城市名称", "国家", "人口", "面积"};
    PrintTableHeader(columns);
    
    // 打印数据
    const int kColumnWidth = 20;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int city_id = sqlite3_column_int(stmt, 0);
        const char* city_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* country = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        int population = sqlite3_column_int(stmt, 3);
        double area = sqlite3_column_double(stmt, 4);
        
        std::cout << std::left << std::setw(kColumnWidth) << city_id << " | "
                  << std::left << std::setw(kColumnWidth) << (city_name ? city_name : "NULL") << " | "
                  << std::left << std::setw(kColumnWidth) << (country ? country : "NULL") << " | "
                  << std::left << std::setw(kColumnWidth) << population << " | "
                  << std::left << std::setw(kColumnWidth) << area << " | "
                  << std::endl;
    }
    
    HandleError(rc, "读取Cities表数据", db);
    sqlite3_finalize(stmt);
}

// 读取并打印B.db中的Weather表
void ReadWeatherTable(sqlite3* db) {
    std::cout << "\n===== B.db - Weather表 =====\n" << std::endl;
    
    const char* kSql = "SELECT weather_id, city_id, date, temperature, humidity, weather_condition FROM Weather;";
    sqlite3_stmt* stmt = nullptr;
    
    int rc = sqlite3_prepare_v2(db, kSql, -1, &stmt, nullptr);
    HandleError(rc, "准备查询Weather表", db);
    
    // 打印表头
    std::vector<std::string> columns = {"天气ID", "城市ID", "日期", "温度", "湿度", "天气状况"};
    PrintTableHeader(columns);
    
    // 打印数据
    const int kColumnWidth = 20;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        int weather_id = sqlite3_column_int(stmt, 0);
        int city_id = sqlite3_column_int(stmt, 1);
        int date = sqlite3_column_int(stmt, 2);
        double temperature = sqlite3_column_double(stmt, 3);
        double humidity = sqlite3_column_double(stmt, 4);
        const char* weather_condition = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
        
        std::cout << std::left << std::setw(kColumnWidth) << weather_id << " | "
                  << std::left << std::setw(kColumnWidth) << city_id << " | "
                  << std::left << std::setw(kColumnWidth) << date << " | "
                  << std::left << std::setw(kColumnWidth) << temperature << " | "
                  << std::left << std::setw(kColumnWidth) << humidity << " | "
                  << std::left << std::setw(kColumnWidth) << (weather_condition ? weather_condition : "NULL") << " | "
                  << std::endl;
    }
    
    HandleError(rc, "读取Weather表数据", db);
    sqlite3_finalize(stmt);
}

// 读取A.db数据库
void ReadDatabaseA(const std::string& db_path) {
    sqlite3* db = nullptr;
    int rc = sqlite3_open(db_path.c_str(), &db);
    HandleError(rc, ("打开数据库 " + db_path).c_str(), db);
    
    std::cout << "\n读取数据库: " << db_path << std::endl;
    
    // 读取Users表
    ReadUsersTable(db);
    
    // 读取Orders表
    ReadOrdersTable(db);
    
    // 关闭数据库连接
    sqlite3_close(db);
}

// 读取B.db数据库
void ReadDatabaseB(const std::string& db_path) {
    sqlite3* db = nullptr;
    int rc = sqlite3_open(db_path.c_str(), &db);
    HandleError(rc, ("打开数据库 " + db_path).c_str(), db);
    
    std::cout << "\n读取数据库: " << db_path << std::endl;
    
    // 读取Cities表
    ReadCitiesTable(db);
    
    // 读取Weather表
    ReadWeatherTable(db);
    
    // 关闭数据库连接
    sqlite3_close(db);
}

int main() {
    // 读取A.db数据库
    ReadDatabaseA("A.db");
    
    // 读取B.db数据库
    ReadDatabaseB("B.db");
    
    return 0;
}
