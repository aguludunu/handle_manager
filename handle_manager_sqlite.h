#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <tuple>
#include <typeindex>
#include <unordered_map>

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

struct TypedHandleKey {
  HandleKey key;
  std::type_index type_idx;

  friend bool operator==(const TypedHandleKey& lhs, const TypedHandleKey& rhs) {
    return lhs.key == rhs.key && lhs.type_idx == rhs.type_idx;
  }
};

struct TypedHandleKeyHash {
  std::size_t operator()(const TypedHandleKey& typed_key) const {
    HandleKeyHash key_hasher;
    return key_hasher(typed_key.key) ^ std::hash<std::type_index>()(typed_key.type_idx);
  }
};

// 类型擦除的接口
class HandleBase {
 public:
  virtual ~HandleBase() = default;
  [[nodiscard]] virtual const std::type_info& GetType() const = 0;
};

// 类型安全的Handle包装器
template <typename T>
class TypedHandle : public HandleBase {
 public:
  explicit TypedHandle(std::shared_ptr<T> handle) : handle_(std::move(handle)) {}
  ~TypedHandle() override = default;

  [[nodiscard]] const std::type_info& GetType() const override { return typeid(T); }
  std::shared_ptr<T> GetHandle() const { return handle_; }

 private:
  std::shared_ptr<T> handle_;
};

struct HandleInfo {
  std::shared_ptr<HandleBase> handle_wrapper;
  std::chrono::steady_clock::time_point last_used;
};

class SqliteHandleManager {
 public:
  static SqliteHandleManager& Instance();

  SqliteHandleManager(const SqliteHandleManager&) = delete;
  SqliteHandleManager& operator=(const SqliteHandleManager&) = delete;
  SqliteHandleManager(SqliteHandleManager&&) noexcept = delete;
  SqliteHandleManager& operator=(SqliteHandleManager&&) noexcept = delete;

  void SetMaxHandles(size_t max_handles);
  void Release(const HandleKey& key);
  void Close(const HandleKey& key);
  void CloseAll();

  template <typename StorageType>
  std::shared_ptr<StorageType> Acquire(const HandleKey& key,
                                       std::function<std::shared_ptr<StorageType>()> creator_func) {
    std::lock_guard lock(mutex_);

    // 创建类型安全的键
    TypedHandleKey typed_key{key, std::type_index(typeid(StorageType))};

    // 查找现有handle
    if (auto it = typed_handles_.find(typed_key); it != typed_handles_.end()) {
      // 更新最后使用时间
      it->second.last_used = std::chrono::steady_clock::now();

      // 获取强类型的handle
      auto* typed_handle = static_cast<TypedHandle<StorageType>*>(it->second.handle_wrapper.get());
      return typed_handle->GetHandle();
    }

    // 检查是否超过最大handle数量
    if (typed_handles_.size() >= max_handles_) {
      CleanupLeastRecentlyUsed();
    }

    // 创建新的handle
    auto new_handle = creator_func();
    if (!new_handle) {
      return nullptr;
    }

    // 存储新的handle（使用类型擦除）
    HandleInfo info;
    info.handle_wrapper = std::make_shared<TypedHandle<StorageType>>(new_handle);
    info.last_used = std::chrono::steady_clock::now();
    typed_handles_[typed_key] = std::move(info);

    return new_handle;
  }

 private:
  SqliteHandleManager() = default;
  ~SqliteHandleManager() = default;
  void CleanupLeastRecentlyUsed();

  std::unordered_map<TypedHandleKey, HandleInfo, TypedHandleKeyHash> typed_handles_;
  std::mutex mutex_;
  size_t max_handles_{10};
};

// RAII 方式使用 SQLite Handle 的包装类
template <typename StorageType>
class ScopedSqliteHandle {
 public:
  ScopedSqliteHandle(const HandleKey& key, std::function<std::shared_ptr<StorageType>()> creator_func)
      : key_{key}, handle_{SqliteHandleManager::Instance().Acquire<StorageType>(key, creator_func)} {}
  ~ScopedSqliteHandle() { SqliteHandleManager::Instance().Release(key_); }
  ScopedSqliteHandle(const ScopedSqliteHandle&) = delete;
  ScopedSqliteHandle& operator=(const ScopedSqliteHandle&) = delete;
  ScopedSqliteHandle(ScopedSqliteHandle&&) noexcept = default;
  ScopedSqliteHandle& operator=(ScopedSqliteHandle&&) noexcept = default;

  [[nodiscard]] bool IsValid() const { return handle_ != nullptr; }
  StorageType& Get() { return *handle_; }
  const StorageType& Get() const { return *handle_; }
  std::shared_ptr<StorageType> GetPtr() const { return handle_; }
  StorageType* operator->() { return handle_.get(); }
  const StorageType* operator->() const { return handle_.get(); }

 private:
  HandleKey key_;
  std::shared_ptr<StorageType> handle_;
};

// 数据库读取器基类，用于抽象共通逻辑
template <typename StorageType>
class DatabaseReader {
 public:
  explicit DatabaseReader(const HandleKey& key) : key_{key} {}
  virtual ~DatabaseReader() = default;

  // 执行查询的模板方法
  template <typename Func>
  auto ExecuteQuery(Func query_func) {
    ScopedSqliteHandle<StorageType> handle(key_, [this]() { return this->CreateStorage(); });

    if (!handle.IsValid()) {
      throw std::runtime_error("Failed to acquire database handle");
    }

    return query_func(handle.Get());
  }

 protected:
  virtual std::shared_ptr<StorageType> CreateStorage() = 0;

 private:
  HandleKey key_;
};

}  // namespace mx::dba::dbs
