#include "path_manager.h"

#include <string>

namespace mx::dba::dbs {

std::string GetDbPathFromKey(const HandleKey& key) {
  // 根据HandleKey的param1字段判断数据库类型
  switch (key.param1) {
    case 1:  // A数据库
      return kADBFileName;
    case 2:  // B数据库
      return kBDBFileName;
    default:
      return "";
  }
}

}  // namespace mx::dba::dbs
