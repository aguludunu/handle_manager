#pragma once

#include <any>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

namespace mx::dba::dbs {

struct HandleKey {
  int param1{0};
  int param2{0};
  int param3{0};
  int param4{0};

  friend bool operator==(const HandleKey& lhs, const HandleKey& rhs) {
    return lhs.param1 == rhs.param1 && lhs.param2 == rhs.param2 && lhs.param3 == rhs.param3 &&
           lhs.param4 == rhs.param4;
  }
};

struct HandleKeyHash {
  std::size_t operator()(const HandleKey& key) const {
    return std::hash<int>()(key.param1) ^ (std::hash<int>()(key.param2) << 1) ^
           (std::hash<int>()(key.param3) << 2) ^ (std::hash<int>()(key.param4) << 3);
  }
};

struct HandleInfo {
  std::any handle;
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

    if (auto it = handles_.find(key); it != handles_.end()) {
      // 更新最后使用时间
      it->second.last_used = std::chrono::steady_clock::now();

      // 尝试将存储的 handle 转换为请求的类型
      try {
        return std::any_cast<std::shared_ptr<StorageType>>(it->second.handle);
      } catch (const std::bad_any_cast&) {
        // 类型不匹配，返回 nullptr
        return nullptr;
      }
    }

    // 检查是否超过最大 handle 数量
    if (handles_.size() >= max_handles_) {
      CleanupLeastRecentlyUsed();
    }

    // 创建新的 handle
    auto new_handle = creator_func();
    if (!new_handle) {
      return nullptr;
    }

    // 存储新的 handle
    HandleInfo info;
    info.handle = new_handle;
    info.last_used = std::chrono::steady_clock::now();
    handles_[key] = std::move(info);

    return new_handle;
  }

 private:
  SqliteHandleManager() = default;
  ~SqliteHandleManager() = default;
  void CleanupLeastRecentlyUsed();

  std::unordered_map<HandleKey, HandleInfo, HandleKeyHash> handles_;
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
  // 创建存储对象的虚函数，子类必须实现
  virtual std::shared_ptr<StorageType> CreateStorage() = 0;

  // 获取 key 的 getter 方法
  [[nodiscard]] const HandleKey& GetKey() const { return key_; }

 private:
  HandleKey key_;
};

}  // namespace mx::dba::dbs
