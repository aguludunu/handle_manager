#pragma once

#include <string>

#include "inner_defines.h"

namespace mx::dba::dbs {

static constexpr const char* kADBFileName = "A.db";
static constexpr const char* kBDBFileName = "B.db";

std::string GetDbPathFromKey(const HandleKey& key);

}  // namespace mx::dba::dbs
