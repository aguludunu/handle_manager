#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <any>
#include <sqlite_orm/sqlite_orm.h>

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

// 为 User 表创建存储对象
auto create_user_storage() {
    using namespace sqlite_orm;
    return make_storage("A.db",
        make_table("Users",
            make_column("user_id", &User::user_id, primary_key()),
            make_column("username", &User::username),
            make_column("email", &User::email),
            make_column("age", &User::age),
            make_column("registration_date", &User::registration_date)
        )
    );
}

// 为 Order 表创建存储对象
auto create_order_storage() {
    using namespace sqlite_orm;
    return make_storage("A.db",
        make_table("Orders",
            make_column("order_id", &Order::order_id, primary_key()),
            make_column("user_id", &Order::user_id),
            make_column("product_name", &Order::product_name),
            make_column("quantity", &Order::quantity),
            make_column("price", &Order::price),
            make_column("order_date", &Order::order_date),
            foreign_key(&Order::user_id).references(&User::user_id)
        )
    );
}

// 使用类型别名简化代码
using UserStorage = decltype(create_user_storage());
using OrderStorage = decltype(create_order_storage());

// 定义一个通用的 Handle 类型，用于存储不同类型的 storage
class Handle {
public:
    template <typename T>
    explicit Handle(std::shared_ptr<T> storage) : storage_(storage) {}
    
    template <typename T>
    std::shared_ptr<T> Get() {
        return std::any_cast<std::shared_ptr<T>>(storage_);
    }
    
private:
    std::any storage_;
};

int main() {
    // 创建两个不同的存储对象
    auto userStorage = create_user_storage();
    auto orderStorage = create_order_storage();
    
    // 使用 shared_ptr 包装存储对象
    auto userStoragePtr = std::make_shared<UserStorage>(std::move(userStorage));
    auto orderStoragePtr = std::make_shared<OrderStorage>(std::move(orderStorage));
    
    // 使用 Handle 类存储不同类型的 storage
    std::vector<Handle> handles;
    handles.emplace_back(userStoragePtr);
    handles.emplace_back(orderStoragePtr);
    
    // 从 Handle 中取出 UserStorage 并使用
    std::cout << "读取 Users 表数据：" << std::endl;
    auto user_storage_from_handle = handles[0].Get<UserStorage>();
    auto users = user_storage_from_handle->get_all<User>();
    for (const auto& user : users) {
        std::cout << "ID: " << user.user_id 
                  << ", 用户名: " << user.username 
                  << ", 邮箱: " << user.email 
                  << ", 年龄: " << user.age << std::endl;
    }
    
    // 从 Handle 中取出 OrderStorage 并使用
    std::cout << "\n读取 Orders 表数据：" << std::endl;
    auto order_storage_from_handle = handles[1].Get<OrderStorage>();
    auto orders = order_storage_from_handle->get_all<Order>();
    for (const auto& order : orders) {
        std::cout << "订单ID: " << order.order_id 
                  << ", 用户ID: " << order.user_id 
                  << ", 产品: " << order.product_name 
                  << ", 数量: " << order.quantity 
                  << ", 价格: " << order.price << std::endl;
    }
    
    // 验证从 Handle 取出的指针是否与原始指针相同
    std::cout << "\n验证指针地址：" << std::endl;
    std::cout << "原始 userStoragePtr 地址: " << userStoragePtr.get() << std::endl;
    std::cout << "从 Handle 取出的 userStoragePtr 地址: " << user_storage_from_handle.get() << std::endl;
    std::cout << "原始 orderStoragePtr 地址: " << orderStoragePtr.get() << std::endl;
    std::cout << "从 Handle 取出的 orderStoragePtr 地址: " << order_storage_from_handle.get() << std::endl;
    
    // 验证 storage 类型是否相同
    std::cout << "\n验证 storage 类型是否相同：" << std::endl;
    std::cout << "UserStorage 和 OrderStorage 是" 
              << (std::is_same<UserStorage, OrderStorage>::value ? "相同" : "不同") 
              << "类型" << std::endl;
    
    return 0;
}
