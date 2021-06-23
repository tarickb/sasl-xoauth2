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

#ifndef SASL_XOAUTH2_CLIENT_H
#define SASL_XOAUTH2_CLIENT_H

#include <memory>
#include <string>

#include <sasl/sasl.h>
#include <sasl/saslplug.h>

namespace sasl_xoauth2 {

class Log;
class TokenStore;

class Client {
 public:
  Client();
  ~Client();

  int DoStep(sasl_client_params_t *params, const char *from_server,
             const unsigned int from_server_len, sasl_interact_t **prompt_need,
             const char **to_server, unsigned int *to_server_len,
             sasl_out_params_t *out_params);

 private:
  enum class State {
    kInitial,
    kTokenSent,
  };

  int InitialStep(sasl_client_params_t *params, sasl_interact_t **prompt_need,
                  const char **to_server, unsigned int *to_server_len,
                  sasl_out_params_t *out_params);

  int TokenSentStep(sasl_client_params_t *params, sasl_interact_t **prompt_need,
                    const char *from_server, const unsigned int from_server_len,
                    const char **to_server, unsigned int *to_server_len,
                    sasl_out_params_t *out_params);

  int SendToken(const char **to_server, unsigned int *to_server_len);

  State state_ = State::kInitial;
  std::string user_;
  std::string tokenfile_;
  std::string response_;

  // Order of destruction matters -- token_ holds a pointer to log_.
  std::unique_ptr<Log> log_;
  std::unique_ptr<TokenStore> token_;
};

}  // namespace sasl_xoauth2

#endif  // SASL_XOAUTH2_CLIENT_H
