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

#ifndef SASL_XOAUTH2_TOKEN_STORE_H
#define SASL_XOAUTH2_TOKEN_STORE_H

#include <time.h>

#include <memory>
#include <string>

namespace sasl_xoauth2 {

class Log;

class TokenStore {
 public:
  static std::unique_ptr<TokenStore> Create(Log *log, const std::string &path,
                                            bool enable_updates = true);

  int GetAccessToken(std::string *token);
  int Refresh();

  std::string user() const { return user_; }
  bool has_user() const { return !user_.empty(); }

 private:
  TokenStore(Log *log, const std::string &path, bool enable_updates);

  int Read();
  int Write();

  Log *const log_ = nullptr;
  const std::string path_;
  const bool enable_updates_;

  // Normally these values come from the config file, but they can be overriden.
  std::string override_client_id_;
  std::string override_client_secret_;
  std::string override_token_endpoint_;
  std::string override_proxy_;
  std::string override_ca_bundle_file_;
  std::string override_ca_certs_dir_;

  std::string access_;
  std::string refresh_;
  std::string user_;
  time_t expiry_ = 0;

  int refresh_attempts_ = 0;
};

}  // namespace sasl_xoauth2

#endif  // SASL_XOAUTH2_TOKEN_STORE_H
