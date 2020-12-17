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
  static std::unique_ptr<TokenStore> Create(Log *log, const std::string &path);

  int GetAccessToken(std::string *token);
  int Refresh();

 private:
  TokenStore(Log *log, const std::string &path);

  int Read();
  int Write();

  Log *const log_ = nullptr;
  const std::string path_;

  // Normally these values come from the config file, but they can be overriden.
  std::string override_client_id_;
  std::string override_client_secret_;
  std::string override_token_endpoint_;

  std::string access_;
  std::string refresh_;
  time_t expiry_ = 0;

  int refresh_attempts_ = 0;
};

}  // namespace sasl_xoauth2

#endif  // SASL_XOAUTH2_TOKEN_STORE_H
