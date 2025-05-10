#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <list>
#include <memory>
#include <string_view>
#include <typeindex>
#include <unordered_map>

#include "dbs/dbs.h"

namespace mx::dba::dbs {

class IStorage;
struct HandleKey;
struct StorageLRU;

using StorageKey = std::pair<HandleKey, std::type_index>;
using StorageId = uint64_t;
using CreatorFunc = std::function<std::shared_ptr<IStorage>(const std::string_view&)>;
using Storages = std::unordered_map<StorageId, std::list<StorageLRU>::iterator>;

static constexpr size_t kDefaultMaxStorageCount{100};

struct StorageKeyHash {
  std::size_t operator()(const StorageKey& key) const {
    HandleKeyHash handle_hasher;
    return handle_hasher(key.first) ^ std::hash<std::type_index>()(key.second);
  }
};

struct StorageKeyEqual {
  bool operator()(const StorageKey& lhs, const StorageKey& rhs) const {
    return lhs.first == rhs.first && lhs.second == rhs.second;
  }
};

struct StorageLRU {
  StorageId id{0};
  HandleKey key;
  std::type_index type_idx{typeid(void)};
  std::shared_ptr<IStorage> storage{nullptr};
  std::chrono::steady_clock::time_point last_used_time{std::chrono::steady_clock::now()};

  StorageLRU() = default;
  StorageLRU(const HandleKey& k, const std::type_index& t, std::shared_ptr<IStorage> s, StorageId i)
      : id(i), key(k), type_idx(t), storage(std::move(s)) {}
  StorageLRU(StorageLRU&&) noexcept = default;
  StorageLRU& operator=(StorageLRU&&) noexcept = default;
  StorageLRU(const StorageLRU&) = delete;
  StorageLRU& operator=(const StorageLRU&) = delete;
};

// 不想每次都运算 std::type_index(typeid(T))，用 TypeIndexHolder 只计算一次
template <typename T>
struct TypeIndexHolder {
  static const std::type_index kValue;
};

template <typename T>
const std::type_index TypeIndexHolder<T>::kValue = std::type_index(typeid(T));

// 各种 Storage 的通用接口，好放在容器中统一管理
class IStorage {
 public:
  IStorage() = default;
  IStorage(const IStorage&) = delete;
  IStorage& operator=(const IStorage&) = delete;
  IStorage(IStorage&&) noexcept = delete;
  IStorage& operator=(IStorage&&) noexcept = delete;
  virtual ~IStorage() = default;

  [[nodiscard]] virtual std::string GetDatabasePath() const = 0;
};

}  // namespace mx::dba::dbs
