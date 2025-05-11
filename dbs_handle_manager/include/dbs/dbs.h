#pragma once

#include <functional>
#include <tuple>
#include <vector>

// 获取存储实例的宏，如果获取失败则返回空结果
#define GET_STORAGE_OR_RETURN_EMPTY(key, storage_type, result_type)            \
  auto storage = StorageContainer::Instance().GetStorage<storage_type>((key)); \
  if (!storage) {                                                              \
    printf("xxx!\n");                                                          \
    return (result_type){};                                                    \
  }

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

using BLOB = std::vector<char>;

}  // namespace mx::dba::dbs
