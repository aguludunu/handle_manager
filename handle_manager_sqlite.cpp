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
  
  // 遍历所有类型的handles，查找匹配key的条目
  for (auto it = typed_handles_.begin(); it != typed_handles_.end(); ) {
    if (it->first.key == key) {
      it->second.last_used = std::chrono::steady_clock::now();
      break;
    } else {
      ++it;
    }
  }
}

void SqliteHandleManager::Close(const HandleKey& key) {
  std::lock_guard lock(mutex_);
  
  // 遍历所有类型的handles，删除匹配key的条目
  for (auto it = typed_handles_.begin(); it != typed_handles_.end(); ) {
    if (it->first.key == key) {
      it = typed_handles_.erase(it);
    } else {
      ++it;
    }
  }
}

void SqliteHandleManager::CloseAll() {
  std::lock_guard lock(mutex_);
  typed_handles_.clear();
}

void SqliteHandleManager::CleanupLeastRecentlyUsed() {
  if (typed_handles_.empty()) {
    return;
  }

  // 查找最久未使用的handle
  auto oldest_it = typed_handles_.begin();
  for (auto it = typed_handles_.begin(); it != typed_handles_.end(); ++it) {
    if (it->second.last_used < oldest_it->second.last_used) {
      oldest_it = it;
    }
  }

  // 移除最久未使用的handle
  if (oldest_it != typed_handles_.end()) {
    typed_handles_.erase(oldest_it);
  }
}

}  // namespace mx::dba::dbs
