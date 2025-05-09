#include "dbs/service/a_service.h"

#include "dbs/dbs.h"
#include "gtest/gtest.h"

using mx::dba::dbs::GetAllUsers;
using mx::dba::dbs::HandleKey;

class AServiceTest : public testing::Test {
 protected:
  static void SetUpTestCase() {}
  static void TearDownTestCase() {}
};

TEST_F(AServiceTest, TestUser) {
  HandleKey key{1, 0, 0, 0};
  auto result = GetAllUsers(key);
  EXPECT_EQ(result.size(), 4);
}
