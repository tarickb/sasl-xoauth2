/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SASL_XOAUTH2_CONFIG_H
#define SASL_XOAUTH2_CONFIG_H

#include <json/json.h>

#include <string>

namespace sasl_xoauth2 {

class Config {
 public:
  static void EnableLoggingToStderr();

  static int Init(std::string path = "");
  static int InitForTesting(const Json::Value &root);

  static Config *Get();

  std::string client_id() const { return client_id_; }
  std::string client_secret() const { return client_secret_; }
  std::string scope() const { return scope_; }
  bool always_log_to_syslog() const { return always_log_to_syslog_; }
  bool log_to_syslog_on_failure() const { return log_to_syslog_on_failure_; }
  bool log_full_trace_on_failure() const { return log_full_trace_on_failure_; }
  bool manage_token_externally() const { return manage_token_externally_; }
  bool use_client_credentials() const { return use_client_credentials_; }
  std::string token_endpoint() const { return token_endpoint_; }
  std::string proxy() const { return proxy_; }
  std::string ca_bundle_file() const { return ca_bundle_file_; }
  std::string ca_certs_dir() const { return ca_certs_dir_; }
  int refresh_window() const { return refresh_window_; }

 private:
  Config() = default;

  int Init(const Json::Value &root);

  std::string client_id_;
  std::string client_secret_;
  std::string scope_;
  bool always_log_to_syslog_ = false;
  bool log_to_syslog_on_failure_ = true;
  bool log_full_trace_on_failure_ = false;
  bool manage_token_externally_ = false;
  bool use_client_credentials_ = false;
  std::string token_endpoint_ = "https://accounts.google.com/o/oauth2/token";
  std::string proxy_ = "";
  std::string ca_bundle_file_ = "";
  std::string ca_certs_dir_ = "";
  int refresh_window_ = 10;  // seconds
};

}  // namespace sasl_xoauth2

#endif  // SASL_XOAUTH2_CONFIG_H
