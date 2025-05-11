#include "dbs/service/a_service.h"

#include "dbs/dbs.h"
#include "storage/a_storage.h"
#include "storage_container.h"

namespace mx::dba::dbs {

namespace {

DataTypeDeserialization DeserializeBlob(const BLOB& blob_data) {
  DataTypeDeserialization deserialization_data;
  // 此处应该调用 zserio 的反序列化
  deserialization_data.i = blob_data.size();
  deserialization_data.s = "zhut";
  deserialization_data.d = 250;
  return deserialization_data;
}

std::vector<DataType> DataTypeConvert(std::vector<DataTypeInDB>& ori_datas) {
  std::vector<DataType> result;
  result.reserve(ori_datas.size());

  for (auto& ori_data : ori_datas) {
    DataType converted_data;
    if (ori_data.int_nullable.has_value()) converted_data.base.int_nullable = *ori_data.int_nullable;
    if (ori_data.float_nullable.has_value()) converted_data.base.float_nullable = *ori_data.float_nullable;
    if (ori_data.text_nullable.has_value())
      converted_data.base.text_nullable = std::move(*ori_data.text_nullable);

    converted_data.base.float_not_null = ori_data.float_not_null;
    converted_data.base.text_not_null = std::move(ori_data.text_not_null);
    converted_data.data = DeserializeBlob(ori_data.blob_data);

    result.emplace_back(std::move(converted_data));
  }
  return result;
}

}  // namespace

std::vector<User> GetAllUsers(const HandleKey& key) {
  GET_STORAGE_OR_RETURN_EMPTY(key, AStorage, std::vector<User>);
  return storage->GetAllUsers();
}

std::vector<UserPartial> GetPartialUsers(const HandleKey& key) {
  GET_STORAGE_OR_RETURN_EMPTY(key, AStorage, std::vector<UserPartial>);
  return storage->GetPartialUsers();
}

std::vector<UserPartial> GetPartialUsersByAge(const HandleKey& key, int age) {
  GET_STORAGE_OR_RETURN_EMPTY(key, AStorage, std::vector<UserPartial>);
  return storage->GetPartialUsersByAge(age);
}

std::vector<UserOrder> GetUserOrdersByUserId(const HandleKey& key, int user_id) {
  GET_STORAGE_OR_RETURN_EMPTY(key, AStorage, std::vector<UserOrder>);
  return storage->GetUserOrdersByUserId(user_id);
}

std::vector<DataType> GetPartialDataTypes(const HandleKey& key) {
  GET_STORAGE_OR_RETURN_EMPTY(key, AStorage, std::vector<DataType>);
  auto ori_data = storage->GetAllDataTypePartials();
  return DataTypeConvert(ori_data);
}

}  // namespace mx::dba::dbs
