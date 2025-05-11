#include "dbs/service/a_service.h"

#include "dbs/dbs.h"
#include "gtest/gtest.h"

using mx::dba::dbs::BLOB;
using mx::dba::dbs::GetAllUsers;
using mx::dba::dbs::GetPartialDataTypes;
using mx::dba::dbs::GetPartialUsers;
using mx::dba::dbs::GetPartialUsersByAge;
using mx::dba::dbs::GetUserOrdersByUserId;
using mx::dba::dbs::HandleKey;

class AServiceTest : public testing::Test {
 protected:
  static void SetUpTestCase() {}
  static void TearDownTestCase() {}
};

TEST_F(AServiceTest, TestAllUsers) {
  HandleKey key{1, 0, 0, 0};
  auto result = GetAllUsers(key);
  ASSERT_EQ(result.size(), 4);
}

TEST_F(AServiceTest, TestPartialUsers) {
  HandleKey key{1, 0, 0, 0};
  auto result = GetPartialUsers(key);
  ASSERT_EQ(result.size(), 4);
}

TEST_F(AServiceTest, TestPartialUsersByCondition) {
  HandleKey key{1, 0, 0, 0};
  auto result = GetPartialUsersByAge(key, 32);
  ASSERT_EQ(result.size(), 2);
}

TEST_F(AServiceTest, TestUserOrder) {
  HandleKey key{1, 0, 0, 0};
  auto result2 = GetUserOrdersByUserId(key, 1);
  ASSERT_EQ(result2.size(), 2);
  for (const auto& order : result2) {
    ASSERT_EQ(order.user_id, 1);
  }
}

TEST_F(AServiceTest, TestDataType) {
  HandleKey key{1, 0, 0, 0};
  auto result = GetPartialDataTypes(key);
  ASSERT_EQ(result.size(), 3);

  auto& data = result[0];
  EXPECT_EQ(data.base.int_nullable, 200);
  EXPECT_DOUBLE_EQ(data.base.float_not_null, 3.14);
  EXPECT_DOUBLE_EQ(data.base.float_nullable, 2.71828);
  EXPECT_EQ(data.base.text_not_null, "必填字符串");
  EXPECT_EQ(data.base.text_nullable, "可空字符串");
  EXPECT_EQ(data.data.i, 11);
  EXPECT_EQ(data.data.s, "zhut");
  EXPECT_DOUBLE_EQ(data.data.d, 250);

  data = result[1];
  EXPECT_EQ(data.base.int_nullable, 1234);
  EXPECT_DOUBLE_EQ(data.base.float_nullable, 12.34);
  EXPECT_EQ(data.base.text_nullable, "1234");
}
