/*
 * COPYRIGHT 2015 SEAGATE LLC
 *
 * THIS DRAWING/DOCUMENT, ITS SPECIFICATIONS, AND THE DATA CONTAINED
 * HEREIN, ARE THE EXCLUSIVE PROPERTY OF SEAGATE TECHNOLOGY
 * LIMITED, ISSUED IN STRICT CONFIDENCE AND SHALL NOT, WITHOUT
 * THE PRIOR WRITTEN PERMISSION OF SEAGATE TECHNOLOGY LIMITED,
 * BE REPRODUCED, COPIED, OR DISCLOSED TO A THIRD PARTY, OR
 * USED FOR ANY PURPOSE WHATSOEVER, OR STORED IN A RETRIEVAL SYSTEM
 * EXCEPT AS ALLOWED BY THE TERMS OF SEAGATE LICENSES AND AGREEMENTS.
 *
 * YOU SHOULD HAVE RECEIVED A COPY OF SEAGATE'S LICENSE ALONG WITH
 * THIS RELEASE. IF NOT PLEASE CONTACT A SEAGATE REPRESENTATIVE
 * http://www.seagate.com/contact
 *
 * Original author:  Kaustubh Deorukhkar   <kaustubh.deorukhkar@seagate.com>
 * Original creation date: 1-Oct-2015
 */

#include <stdlib.h>
#include <json/json.h>
#include "base64.h"

#include "s3_object_metadata.h"
#include "s3_datetime.h"

S3ObjectMetadata::S3ObjectMetadata(std::shared_ptr<S3RequestObject> req, bool ismultipart, std::string uploadid) : request(req) {
  account_name = request->get_account_name();
  account_id = request->get_account_id();
  user_name = request->get_user_name();
  user_id = request->get_user_id();
  bucket_name = request->get_bucket_name();
  object_name = request->get_object_name();
  state = S3ObjectMetadataState::empty;
  is_multipart = ismultipart;
  upload_id = uploadid;
  oid = M0_CLOVIS_ID_APP;

  s3_clovis_api = std::make_shared<ConcreteClovisAPI>();

  object_key_uri = bucket_name + "\\" + object_name;

  // Set the defaults
  S3DateTime current_time;
  current_time.init_current_time();
  system_defined_attribute["Date"] = current_time.get_gmtformat_string();
  system_defined_attribute["Content-Length"] = "";
  system_defined_attribute["Last-Modified"] = current_time.get_gmtformat_string();  // TODO
  system_defined_attribute["Content-MD5"] = "";
  system_defined_attribute["Owner-User"] = "";
  system_defined_attribute["Owner-User-id"] = "";
  system_defined_attribute["Owner-Account"] = "";
  system_defined_attribute["Owner-Account-id"] = "";

  system_defined_attribute["x-amz-server-side-encryption"] = "None"; // Valid values aws:kms, AES256
  system_defined_attribute["x-amz-version-id"] = "";  // Generate if versioning enabled
  system_defined_attribute["x-amz-storage-class"] = "STANDARD";  // Valid Values: STANDARD | STANDARD_IA | REDUCED_REDUNDANCY
  system_defined_attribute["x-amz-website-redirect-location"] = "None";
  system_defined_attribute["x-amz-server-side-encryption-aws-kms-key-id"] = "";
  system_defined_attribute["x-amz-server-side-encryption-customer-algorithm"] = "";
  system_defined_attribute["x-amz-server-side-encryption-customer-key"] = "";
  system_defined_attribute["x-amz-server-side-encryption-customer-key-MD5"] = "";
}

std::string S3ObjectMetadata::get_object_name() {
  return object_name;
}

std::string S3ObjectMetadata::get_user_id() {
  return user_id;
}

std::string S3ObjectMetadata::get_upload_id() {
  return upload_id;
}

std::string S3ObjectMetadata::get_user_name() {
  return user_name;
}

std::string S3ObjectMetadata::get_creation_date() {
  return system_defined_attribute["Date"];
}

std::string S3ObjectMetadata::get_last_modified() {
  return system_defined_attribute["Last-Modified"];
}

std::string S3ObjectMetadata::get_storage_class() {
  return system_defined_attribute["x-amz-storage-class"];
}

void S3ObjectMetadata::set_content_length(std::string length) {
  system_defined_attribute["Content-Length"] = length;
}

size_t S3ObjectMetadata::get_content_length() {
  return atol(system_defined_attribute["Content-Length"].c_str());
}

std::string S3ObjectMetadata::get_content_length_str() {
  return system_defined_attribute["Content-Length"];
}

void S3ObjectMetadata::set_md5(std::string md5) {
  system_defined_attribute["Content-MD5"] = md5;
}

std::string S3ObjectMetadata::get_md5() {
  return system_defined_attribute["Content-MD5"];
}

void S3ObjectMetadata::add_system_attribute(std::string key, std::string val) {
  system_defined_attribute[key] = val;
}

void S3ObjectMetadata::add_user_defined_attribute(std::string key, std::string val) {
  user_defined_attribute[key] = val;
}

void S3ObjectMetadata::validate() {
  // TODO
}

void S3ObjectMetadata::load(std::function<void(void)> on_success, std::function<void(void)> on_failed) {
  printf("Called S3ObjectMetadata::load\n");

  this->handler_on_success = on_success;
  this->handler_on_failed  = on_failed;

  clovis_kv_reader = std::make_shared<S3ClovisKVSReader>(request);
  if(is_multipart) {
    clovis_kv_reader->get_keyval(get_multipart_index_name(), object_name, std::bind( &S3ObjectMetadata::load_successful, this), std::bind( &S3ObjectMetadata::load_failed, this));
  } else {
    clovis_kv_reader->get_keyval(get_bucket_index_name(), object_name, std::bind( &S3ObjectMetadata::load_successful, this), std::bind( &S3ObjectMetadata::load_failed, this));
  }
}

void S3ObjectMetadata::load_successful() {
  printf("Called S3ObjectMetadata::load_successful\n");
  this->from_json(clovis_kv_reader->get_value());
  state = S3ObjectMetadataState::present;
  this->handler_on_success();
}

void S3ObjectMetadata::load_failed() {
  // TODO - do anything more for failure?
  printf("Called S3ObjectMetadata::load_failed\n");
  if (clovis_kv_reader->get_state() == S3ClovisKVSReaderOpState::missing) {
    state = S3ObjectMetadataState::missing;  // Missing
  } else {
    state = S3ObjectMetadataState::failed;
  }
  this->handler_on_failed();
}

void S3ObjectMetadata::save(std::function<void(void)> on_success, std::function<void(void)> on_failed) {
  printf("Called S3ObjectMetadata::save\n");

  this->handler_on_success = on_success;
  this->handler_on_failed  = on_failed;

  create_bucket_index();
}

void S3ObjectMetadata::create_bucket_index() {
  std::string index_name;
  printf("Called S3ObjectMetadata::create_bucket_index\n");
  // Mark missing as we initiate write, in case it fails to write.
  state = S3ObjectMetadataState::missing;

  clovis_kv_writer = std::make_shared<S3ClovisKVSWriter>(request, s3_clovis_api);
  if(is_multipart) {
    index_name = get_multipart_index_name();
  } else {
    index_name = get_bucket_index_name();
  }
  clovis_kv_writer->create_index(index_name, std::bind( &S3ObjectMetadata::create_bucket_index_successful, this), std::bind( &S3ObjectMetadata::create_bucket_index_failed, this));
}

void S3ObjectMetadata::create_bucket_index_successful() {
  printf("Called S3ObjectMetadata::create_bucket_index_successful\n");
  save_metadata();
}

void S3ObjectMetadata::create_bucket_index_failed() {
  printf("Called S3ObjectMetadata::create_bucket_index_failed\n");
  if (clovis_kv_writer->get_state() == S3ClovisKVSWriterOpState::exists) {
    // We need to create index only once.
    save_metadata();
  } else {
    state = S3ObjectMetadataState::failed;  // todo Check error
    this->handler_on_failed();
  }
}

void S3ObjectMetadata::save_metadata() {
  std::string index_name;
  std::string key;
  // Set up system attributes
  system_defined_attribute["Owner-User"] = user_name;
  system_defined_attribute["Owner-User-id"] = user_id;
  system_defined_attribute["Owner-Account"] = account_name;
  system_defined_attribute["Owner-Account-id"] = account_id;
  if( is_multipart ) {
    system_defined_attribute["Upload-ID"] = upload_id;
    index_name = get_multipart_index_name();
  } else {
    index_name = get_bucket_index_name();
  }

  clovis_kv_writer = std::make_shared<S3ClovisKVSWriter>(request, s3_clovis_api);
  clovis_kv_writer->put_keyval(index_name, object_name, this->to_json(), std::bind( &S3ObjectMetadata::save_metadata_successful, this), std::bind( &S3ObjectMetadata::save_metadata_failed, this));
}

void S3ObjectMetadata::save_metadata_successful() {
  printf("Called S3ObjectMetadata::save_metadata_successful\n");
  state = S3ObjectMetadataState::saved;
  this->handler_on_success();
}

void S3ObjectMetadata::save_metadata_failed() {
  // TODO - do anything more for failure?
  printf("Called S3ObjectMetadata::save_metadata_failed\n");
  state = S3ObjectMetadataState::failed;
  this->handler_on_failed();
}

void S3ObjectMetadata::remove(std::function<void(void)> on_success, std::function<void(void)> on_failed) {
  std::string index_name;
  printf("Called S3ObjectMetadata::remove\n");

  this->handler_on_success = on_success;
  this->handler_on_failed  = on_failed;

  if( is_multipart ) {
    index_name = get_multipart_index_name();
  } else {
    index_name = get_bucket_index_name();
  }


  clovis_kv_writer = std::make_shared<S3ClovisKVSWriter>(request, s3_clovis_api);
  clovis_kv_writer->delete_keyval(index_name, object_name, std::bind( &S3ObjectMetadata::remove_successful, this), std::bind( &S3ObjectMetadata::remove_failed, this));
}

void S3ObjectMetadata::remove_successful() {
  printf("Called S3ObjectMetadata::remove_successful\n");
  state = S3ObjectMetadataState::deleted;
  this->handler_on_success();
}

void S3ObjectMetadata::remove_failed() {
  printf("Called S3ObjectMetadata::remove_failed\n");
  state = S3ObjectMetadataState::failed;
  this->handler_on_failed();
}

// Streaming to json
std::string S3ObjectMetadata::to_json() {
  printf("Called S3ObjectMetadata::to_json\n");
  Json::Value root;
  root["Bucket-Name"] = bucket_name;
  root["Object-Name"] = object_name;
  root["Object-URI"] = object_key_uri;
  if(is_multipart) {
    root["Upload-ID"] = upload_id;
  }

  root["mero_oid_u_hi"] = base64_encode((unsigned char const*)&oid.u_hi, sizeof(oid.u_hi));
  root["mero_oid_u_lo"] = base64_encode((unsigned char const*)&oid.u_lo, sizeof(oid.u_lo));
  // root["mero_oid"] = base64_encode((unsigned char const*)&oid, sizeof(struct m0_uint128));

  for (auto sit: system_defined_attribute) {
    root["System-Defined"][sit.first] = sit.second;
  }
  for (auto uit: user_defined_attribute) {
    root["User-Defined"][uit.first] = uit.second;
  }
  root["ACL"] = object_ACL.to_json();

  Json::FastWriter fastWriter;
  return fastWriter.write(root);;
}

void S3ObjectMetadata::from_json(std::string content) {
  printf("Called S3ObjectMetadata::from_json\n");
  Json::Value newroot;
  Json::Reader reader;
  bool parsingSuccessful = reader.parse(content.c_str(), newroot);
  if (!parsingSuccessful)
  {
    printf("Json Parsing failed.\n");
    return;
  }

  bucket_name = newroot["Bucket-Name"].asString();
  object_name = newroot["Object-Name"].asString();
  object_key_uri = newroot["Object-URI"].asString();
  upload_id = newroot["Upload-ID"].asString();
  std::string oid_u_hi_str = newroot["mero_oid_u_hi"].asString();
  std::string oid_u_lo_str = newroot["mero_oid_u_lo"].asString();

  std::string dec_oid_u_hi_str = base64_decode(oid_u_hi_str);
  std::string dec_oid_u_lo_str = base64_decode(oid_u_lo_str);

  // std::string decoded_oid_str = base64_decode(oid_str);
  memcpy((void*)&oid.u_hi, dec_oid_u_hi_str.c_str(), dec_oid_u_hi_str.length());
  memcpy((void*)&oid.u_lo, dec_oid_u_lo_str.c_str(), dec_oid_u_lo_str.length());

  Json::Value::Members members = newroot["System-Defined"].getMemberNames();
  for(auto it : members) {
    system_defined_attribute[it.c_str()] = newroot["System-Defined"][it].asString().c_str();
  }
  members = newroot["User-Defined"].getMemberNames();
  for(auto it : members) {
    user_defined_attribute[it.c_str()] = newroot["User-Defined"][it].asString().c_str();
  }
  object_ACL.from_json(newroot["ACL"].asString());
}
