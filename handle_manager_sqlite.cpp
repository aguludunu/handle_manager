#include "handle_manager_sqlite.h"

namespace mx::dba::dbs {

SqliteHandleManager& SqliteHandleManager::Instance() {
  static SqliteHandleManager instance;
  return instance;
}

void SqliteHandleManager::SetMaxHandles(size_t max_handles) {
  std::lock_guard lock(mutex_);
  max_handles_ = max_handles;
}

void SqliteHandleManager::Release(const HandleKey& key) {
  std::lock_guard lock(mutex_);

  auto it = handles_.find(key);
  if (it != handles_.end()) {
    it->second.last_used = std::chrono::steady_clock::now();
  }
}

void SqliteHandleManager::Close(const HandleKey& key) {
  std::lock_guard lock(mutex_);
  handles_.erase(key);
}

void SqliteHandleManager::CloseAll() {
  std::lock_guard lock(mutex_);
  handles_.clear();
}

void SqliteHandleManager::CleanupLeastRecentlyUsed() {
  if (handles_.empty()) {
    return;
  }

  // 查找最久未使用的 handle
  auto oldest_it = handles_.begin();
  for (auto it = handles_.begin(); it != handles_.end(); ++it) {
    if (it->second.last_used < oldest_it->second.last_used) {
      oldest_it = it;
    }
  }

  // 移除最久未使用的 handle
  if (oldest_it != handles_.end()) {
    handles_.erase(oldest_it);
  }
}

}  // namespace mx::dba::dbs
