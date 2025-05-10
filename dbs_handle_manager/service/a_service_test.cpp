#include "dbs/service/a_service.h"

#include "dbs/dbs.h"
#include "gtest/gtest.h"

using mx::dba::dbs::BLOB;
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
  ASSERT_EQ(result.size(), 4);
}

TEST_F(AServiceTest, TestDataType) {
  HandleKey key{1, 0, 0, 0};
  auto result = GetPartialDataTypes(key);
  ASSERT_EQ(result.size(), 3);

  auto& data = result[0];
  EXPECT_TRUE(data.int_nullable.has_value());
  EXPECT_EQ(*data.int_nullable, 200);
  EXPECT_DOUBLE_EQ(data.float_not_null, 3.14);
  EXPECT_TRUE(data.float_nullable.has_value());
  EXPECT_DOUBLE_EQ(*data.float_nullable, 2.71828);
  EXPECT_EQ(data.text_not_null, "必填字符串");
  EXPECT_TRUE(data.text_nullable.has_value());
  EXPECT_EQ(*data.text_nullable, "可空字符串");
  const std::string expected_str = "Hello World";
  BLOB expected_blob(expected_str.begin(), expected_str.end());
  EXPECT_EQ(data.blob_data, expected_blob);

  data = result[1];
  EXPECT_FALSE(data.int_nullable.has_value());
  EXPECT_FALSE(data.float_nullable.has_value());
  EXPECT_FALSE(data.text_nullable.has_value());
}
