#pragma once

#include <mutex>
#include <string>

#include "inner_defines.h"
#include "path_manager.h"

namespace mx::dba::dbs {

class StorageContainer {
 public:
  static StorageContainer& Instance();

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

  template <typename T>
  void RegisterStorageCreator(std::function<std::shared_ptr<T>(const std::string_view&)> creator) {
    static_assert(std::is_base_of_v<IStorage, T>, "T must inherit from IStorage");
    std::lock_guard lock(mutex_);

    // 将各种 T 转换为 std::shared_ptr<IStorage>，所以需要增加一个wrapper
    auto wrapped_creator =
        [creator_func = std::move(creator)](const std::string_view& db_path) -> std::shared_ptr<IStorage> {
      return creator_func(db_path);
    };

    creators_[TypeIndexHolder<T>::kValue] = std::move(wrapped_creator);
  }

  template <typename T>
  std::shared_ptr<T> GetStorage(const HandleKey& key) {
    static_assert(std::is_base_of_v<IStorage, T>, "T must inherit from IStorage");
    std::lock_guard lock(mutex_);

    const auto& type_idx = TypeIndexHolder<T>::kValue;
    StorageKey storage_key{key, type_idx};

    if (auto storage = TakeOutStorage<T>(storage_key); storage) {
      return storage;
    }
    return CreateStorage<T>(key, type_idx);
  }

  template <typename T>
  void GiveBack(const HandleKey& key, std::shared_ptr<T> storage) {
    static_assert(std::is_base_of_v<IStorage, T>, "T must inherit from IStorage");
    if (!storage) {
      return;
    }
    std::lock_guard lock(mutex_);

    EnsureCapacity();
    InsertStorage(key, TypeIndexHolder<T>::kValue, std::move(storage));
  }

  template <typename T>
  void CloseStorage(const HandleKey& key) {
    static_assert(std::is_base_of_v<IStorage, T>, "T must inherit from IStorage");
    std::lock_guard lock(mutex_);

    StorageKey storage_key{key, TypeIndexHolder<T>::kValue};
    RemoveStorage(storage_key);
  }

  void Clear() {
    std::lock_guard lock(mutex_);
    storage_list_.clear();
    storages_.clear();
    creators_.clear();
  }

  void RegisterAllStorages();

 private:
  StorageContainer() { RegisterAllStorages(); }
  ~StorageContainer() = default;

  void RemoveOldestStorage() {
    if (storage_list_.empty()) {
      return;
    }

    auto oldest_it = std::prev(storage_list_.end());
    StorageKey oldest_key{oldest_it->key, oldest_it->type_idx};

    auto it = storages_.find(oldest_key);
    if (it == storages_.end()) {
      printf("xxx!\n");
      return;
    }

    it->second.erase(oldest_it->id);
    if (it->second.empty()) {
      storages_.erase(it);
    }
    storage_list_.pop_back();
  }

  void EnsureCapacity() {
    if (storage_list_.size() >= max_storage_count_ && !storage_list_.empty()) {
      RemoveOldestStorage();
    }
  }

  void RemoveStorage(const StorageKey& key) {
    auto it = storages_.find(key);
    if (it != storages_.end()) {
      for (const auto& [id, list_it] : it->second) {
        printf("[%s:%d]Close storage %s\n", __FILE__, __LINE__,
               list_it->storage->GetDatabasePath().c_str());
        storage_list_.erase(list_it);
      }
      storages_.erase(it);
    }
  }

  template <typename T>
  std::shared_ptr<T> TakeOutStorage(const StorageKey& key) {
    auto it = storages_.find(key);
    if (it == storages_.end() || it->second.empty()) {
      return nullptr;
    }

    // 获取第一个存储对象（可以是任意一个，这里选择第一个）
    auto inner_it = it->second.begin();
    auto list_it = inner_it->second;
    if (list_it == storage_list_.end()) {
      printf("xxx!\n");
      return nullptr;
    }
    auto storage = std::static_pointer_cast<T>(std::move(list_it->storage));

    storage_list_.erase(list_it);
    it->second.erase(inner_it);
    if (it->second.empty()) {
      storages_.erase(it);
    }

    return storage;
  }

  template <typename T>
  std::shared_ptr<T> CreateStorage(const HandleKey& key, const std::type_index& type_idx) {
    std::string db_path = GetDbPathFromKey(key);
    if (db_path.empty()) {
      return nullptr;
    }

    if (auto creator_it = creators_.find(type_idx); creator_it != creators_.end()) {
      EnsureCapacity();
      if (auto storage = creator_it->second(db_path); storage) {
        printf("[%s:%d]Create storage %s\n", __FILE__, __LINE__, db_path.c_str());
        return std::static_pointer_cast<T>(storage);
      }
    }
    return nullptr;
  }

  template <typename T>
  void InsertStorage(const HandleKey& key, const std::type_index& type_idx, std::shared_ptr<T> storage) {
    StorageKey storage_key{key, type_idx};
    StorageId id = next_storage_id_;
    next_storage_id_++;

    storage_list_.emplace_front(key, type_idx, std::move(storage), id);
    storages_[storage_key][id] = storage_list_.begin();
  }

  mutable std::mutex mutex_{};
  size_t max_storage_count_{kDefaultMaxStorageCount};
  StorageId next_storage_id_{0};

  std::list<StorageLRU> storage_list_{};
  std::unordered_map<StorageKey, Storages, StorageKeyHash, StorageKeyEqual> storages_{};
  std::unordered_map<std::type_index, CreatorFunc> creators_{};
};

}  // namespace mx::dba::dbs
