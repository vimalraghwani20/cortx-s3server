
#include <json/json.h>

#include "s3_bucket_metadata.h"

S3BucketMetadata::S3BucketMetadata(std::shared_ptr<S3RequestObject> req) : request(req) {
  account_name = request->get_account_name();
  user_name = request->get_user_name();
  bucket_name = request->get_bucket_name();
  state = S3BucketMetadataState::empty;

  // Set the defaults
  system_defined_attribute["Date"] = "currentdate";  // TODO
  system_defined_attribute["LocationConstraint"] = "US";
  system_defined_attribute["Owner-User"] = "";
  system_defined_attribute["Owner-User-id"] = "";
  system_defined_attribute["Owner-Account"] = "";
  system_defined_attribute["Owner-Account-id"] = "";
}

void S3BucketMetadata::add_system_attribute(std::string key, std::string val) {
  system_defined_attribute[key] = val;
}

void S3BucketMetadata::add_user_defined_attribute(std::string key, std::string val) {
  user_defined_attribute[key] = val;
}

// AWS recommends that all bucket names comply with DNS naming convention
// See Bucket naming restrictions in above link.
void S3BucketMetadata::validate_bucket_name() {
  // TODO
}

void S3BucketMetadata::validate() {
  // TODO
}

void S3BucketMetadata::load(std::function<void(void)> on_success, std::function<void(void)> on_failed) {
  printf("Called S3BucketMetadata::load\n");

  this->handler_on_success = on_success;
  this->handler_on_failed  = on_failed;

  load_account_bucket();
}

void S3BucketMetadata::load_account_bucket() {
  printf("Called S3BucketMetadata::load_account_bucket\n");
  // Mark absent as we initiate fetch, in case it fails to load due to missing.
  state = S3BucketMetadataState::absent;

  clovis_kv_reader = std::make_shared<S3ClovisKVSReader>(request);
  clovis_kv_reader->get_keyval(account_name, bucket_name, std::bind( &S3BucketMetadata::load_account_bucket_successful, this), std::bind( &S3BucketMetadata::load_account_bucket_failed, this));
}

void S3BucketMetadata::load_account_bucket_successful() {
  printf("Called S3BucketMetadata::load_account_bucket_successful\n");
  this->from_json(clovis_kv_reader->get_value());
  state = S3BucketMetadataState::present;
  this->handler_on_success();
}

void S3BucketMetadata::load_account_bucket_failed() {
  // TODO - do anything more for failure?
  printf("Called S3BucketMetadata::load_account_bucket_successful\n");
  state = S3BucketMetadataState::failed;
  this->handler_on_failed();
}

void S3BucketMetadata::load_user_bucket() {
  printf("Called S3BucketMetadata::load_user_bucket\n");

  // Mark absent as we initiate fetch, in case it fails to load due to missing.
  state = S3BucketMetadataState::absent;

  clovis_kv_reader = std::make_shared<S3ClovisKVSReader>(request);
  clovis_kv_reader->get_keyval(account_name, bucket_name, std::bind( &S3BucketMetadata::load_user_bucket_successful, this), std::bind( &S3BucketMetadata::load_user_bucket_failed, this));
}

void S3BucketMetadata::load_user_bucket_successful() {
  printf("Called S3BucketMetadata::load_user_bucket_successful\n");
  this->from_json(clovis_kv_reader->get_value());
  state = S3BucketMetadataState::present;
  this->handler_on_success();
}

void S3BucketMetadata::load_user_bucket_failed() {
  // TODO - do anything more for failure?
  printf("Called S3BucketMetadata::load_user_bucket_successful\n");
  this->handler_on_failed();
}

void S3BucketMetadata::save(std::function<void(void)> on_success, std::function<void(void)> on_failed) {
  printf("Called S3BucketMetadata::save\n");

  this->handler_on_success = on_success;
  this->handler_on_failed  = on_failed;

  // TODO create only if it does not exists.
  create_account_bucket_index();
}

void S3BucketMetadata::create_account_bucket_index() {
  printf("Called S3BucketMetadata::create_account_bucket_index\n");
  // Mark absent as we initiate write, in case it fails to write.
  state = S3BucketMetadataState::absent;

  clovis_kv_writer = std::make_shared<S3ClovisKVSWriter>(request);
  clovis_kv_writer->create_index(account_name, std::bind( &S3BucketMetadata::create_account_bucket_index_successful, this), std::bind( &S3BucketMetadata::create_account_bucket_index_failed, this));
}

void S3BucketMetadata::create_account_bucket_index_successful() {
  printf("Called S3BucketMetadata::create_account_bucket_index_successful\n");
  create_account_user_bucket_index();
}

void S3BucketMetadata::create_account_bucket_index_failed() {
  // TODO - do anything more for failure?
  printf("Called S3BucketMetadata::create_account_bucket_index_failed\n");
  state = S3BucketMetadataState::failed;
  this->handler_on_failed();
}

void S3BucketMetadata::create_account_user_bucket_index() {
  printf("Called S3BucketMetadata::create_account_user_bucket_index\n");
  // Mark absent as we initiate write, in case it fails to write.
  state = S3BucketMetadataState::absent;

  clovis_kv_writer = std::make_shared<S3ClovisKVSWriter>(request);
  clovis_kv_writer->create_index(account_name + "\\" + user_name, std::bind( &S3BucketMetadata::create_account_user_bucket_index_successful, this), std::bind( &S3BucketMetadata::create_account_user_bucket_index_failed, this));
}

void S3BucketMetadata::create_account_user_bucket_index_successful() {
  printf("Called S3BucketMetadata::create_account_user_bucket_index_successful\n");
  save_account_bucket();
}

void S3BucketMetadata::create_account_user_bucket_index_failed() {
  // TODO - do anything more for failure?
  printf("Called S3BucketMetadata::create_account_user_bucket_index_failed\n");
  state = S3BucketMetadataState::failed;
  this->handler_on_failed();
}

void S3BucketMetadata::save_account_bucket() {
  printf("Called S3BucketMetadata::save_account_bucket\n");
  // Mark absent as we initiate write, in case it fails to write.
  state = S3BucketMetadataState::absent;

  // Set up system attributes
  system_defined_attribute["Owner-User"] = user_name;
  system_defined_attribute["Owner-User-id"] = request->get_user_id();
  system_defined_attribute["Owner-Account"] = account_name;
  system_defined_attribute["Owner-Account-id"] = request->get_account_id();

  clovis_kv_writer = std::make_shared<S3ClovisKVSWriter>(request);
  clovis_kv_writer->put_keyval(account_name, bucket_name, this->to_json(), std::bind( &S3BucketMetadata::save_account_bucket_successful, this), std::bind( &S3BucketMetadata::save_account_bucket_failed, this));
}

void S3BucketMetadata::save_account_bucket_successful() {
  printf("Called S3BucketMetadata::save_account_bucket_successful\n");
  save_user_bucket();
}

void S3BucketMetadata::save_account_bucket_failed() {
  // TODO - do anything more for failure?
  printf("Called S3BucketMetadata::save_account_bucket_failed\n");
  state = S3BucketMetadataState::failed;
  this->handler_on_failed();
}

void S3BucketMetadata::save_user_bucket() {
  printf("Called S3BucketMetadata::save_user_bucket\n");
  // Mark absent as we initiate write, in case it fails to write.
  state = S3BucketMetadataState::absent;

  clovis_kv_writer = std::make_shared<S3ClovisKVSWriter>(request);
  clovis_kv_writer->put_keyval(account_name + "\\" + user_name, bucket_name, this->to_json(), std::bind( &S3BucketMetadata::save_user_bucket_successful, this), std::bind( &S3BucketMetadata::save_user_bucket_failed, this));
}

void S3BucketMetadata::save_user_bucket_successful() {
  printf("Called S3BucketMetadata::save_user_bucket_successful\n");
  this->from_json(clovis_kv_reader->get_value());
  state = S3BucketMetadataState::saved;
  this->handler_on_success();
}

void S3BucketMetadata::save_user_bucket_failed() {
  // TODO - do anything more for failure?
  printf("Called S3BucketMetadata::save_user_bucket_failed\n");
  state = S3BucketMetadataState::failed;
  this->handler_on_failed();
}

// Streaming to json
std::string S3BucketMetadata::to_json() {
  printf("Called S3BucketMetadata::to_json\n");
  Json::Value root;
  root["Bucket-Name"] = bucket_name;

  for (auto sit: system_defined_attribute) {
    root["System-Defined"][sit.first] = sit.second;
  }
  for (auto uit: user_defined_attribute) {
    root["User-Defined"][uit.first] = uit.second;
  }
  root["ACL"] = bucket_ACL.to_json();

  Json::FastWriter fastWriter;
  return fastWriter.write(root);;
}

void S3BucketMetadata::from_json(std::string content) {
  printf("Called S3BucketMetadata::from_json\n");
  Json::Value newroot;
  Json::Reader reader;
  bool parsingSuccessful = reader.parse(content.c_str(), newroot);
  if (!parsingSuccessful)
  {
    printf("Json Parsing failed.\n");
    return;
  }

  Json::Value::Members members = newroot["System-Defined"].getMemberNames();
  for(auto it : members) {
    system_defined_attribute[it.c_str()] = newroot["System-Defined"][it].asString();
    printf("System-Defined [%s] = %s\n", it.c_str(), newroot["System-Defined"][it].asString().c_str());
  }
  members = newroot["User-Defined"].getMemberNames();
  for(auto it : members) {
    user_defined_attribute[it.c_str()] = newroot["User-Defined"][it].asString();
    printf("User-Defined [%s] = %s\n", it.c_str(), newroot["User-Defined"][it].asString().c_str());
  }
  bucket_ACL.from_json(newroot["ACL"].asString());
}
