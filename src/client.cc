// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "client.h"

#include <json/json.h>
#include <string.h>

#include <sstream>

#include "log.h"
#include "token_store.h"

namespace sasl_xoauth2 {

namespace {

void ReadPrompt(Log *log, sasl_interact_t **prompts, const unsigned int id,
                std::string *value) {
  if (!prompts || !*prompts) return;
  for (const auto *p = *prompts; p->id != SASL_CB_LIST_END; ++p) {
    if (p->id == id) {
      value->assign(static_cast<const char *>(p->result), p->len);
      log->Write("ReadPrompt: found id %d with value [%s]", id, value->c_str());
      return;
    }
  }
  log->Write("ReadPrompt: unable to find id %d", id);
}

int TriggerAuthNameCallback(Log *log, const sasl_utils_t *utils,
                            std::string *value) {
  sasl_getsimple_t *get_simple_cb = nullptr;
  void *context;
  int err = utils->getcallback(
      utils->conn, SASL_CB_AUTHNAME,
      reinterpret_cast<sasl_callback_ft *>(&get_simple_cb), &context);
  if (err != SASL_OK) {
    log->Write("TriggerAuthNameCallback: getcallback err=%d", err);
    return err;
  }
  if (!get_simple_cb) {
    log->Write("TriggerAuthNameCallback: null callback");
    return SASL_INTERACT;
  }

  const char *response = nullptr;
  unsigned int response_len = 0;
  err = get_simple_cb(context, SASL_CB_AUTHNAME, &response, &response_len);
  if (err != SASL_OK) {
    log->Write("TriggerAuthNameCallback: callback err=%d", err);
    return err;
  }

  value->assign(response, response_len);
  return SASL_OK;
}

int TriggerPasswordCallback(Log *log, const sasl_utils_t *utils,
                            std::string *value) {
  sasl_getsecret_t *get_secret_cb = nullptr;
  void *context;
  int err = utils->getcallback(
      utils->conn, SASL_CB_PASS,
      reinterpret_cast<sasl_callback_ft *>(&get_secret_cb), &context);
  if (err != SASL_OK) {
    log->Write("TriggerPasswordCallback: getcallback err=%d", err);
    return err;
  }
  if (!get_secret_cb) {
    log->Write("TriggerPasswordCallback: null callback");
    return SASL_BADPROT;
  }

  sasl_secret_t *password = nullptr;
  err = get_secret_cb(utils->conn, context, SASL_CB_PASS, &password);
  if (err != SASL_OK) {
    log->Write("TriggerPasswordCallback: callback err=%d", err);
    return err;
  }
  if (!password) {
    log->Write("TriggerPasswordCallback: null password");
    return SASL_BADPROT;
  }

  value->assign(reinterpret_cast<const char *>(password->data), password->len);
  return SASL_OK;
}

int RequestPrompts(sasl_client_params_t *params, sasl_interact_t **prompts,
                   const bool need_auth_name, const bool need_password) {
  if (!prompts) return SASL_BADPARAM;
  if (!need_auth_name && !need_password) return SASL_BADPARAM;

  // +1 for trailing SASL_CB_LIST_END.
  const int num_prompts = need_auth_name + need_password + 1;

  auto *req_prompts = static_cast<sasl_interact_t *>(
      params->utils->malloc(sizeof(sasl_interact_t) * num_prompts));
  if (!req_prompts) return SASL_NOMEM;
  memset(req_prompts, 0, sizeof(*req_prompts) * num_prompts);
  sasl_interact_t *p = req_prompts;

  if (need_auth_name) {
    p->id = SASL_CB_AUTHNAME;
    p->challenge = "Authentication Name";
    p->prompt = "Authentication Name";
    p++;
  }

  if (need_password) {
    p->id = SASL_CB_PASS;
    p->challenge = "Password";
    p->prompt = "Password";
    p++;
  }

  p->id = SASL_CB_LIST_END;

  *prompts = req_prompts;
  return SASL_INTERACT;
}

}  // namespace

Client::Client() {
  log_.reset(new Log());
  log_->Write("Client: created");
}

Client::~Client() { log_->Write("Client: destroyed"); }

int Client::DoStep(sasl_client_params_t *params, const char *from_server,
                   const unsigned int from_server_len,
                   sasl_interact_t **prompt_need, const char **to_server,
                   unsigned int *to_server_len, sasl_out_params_t *out_params) {
  log_->Write("Client::DoStep: called with state %d", static_cast<int>(state_));

  int err = SASL_BADPROT;

  switch (state_) {
    case State::kInitial:
      err = InitialStep(params, prompt_need, to_server, to_server_len,
                        out_params);
      break;

    case State::kTokenSent:
      err = TokenSentStep(params, prompt_need, from_server, from_server_len,
                          to_server, to_server_len, out_params);
      break;

    default:
      log_->Write("Client::DoStep: invalid state");
  }

  if (err != SASL_OK && err != SASL_INTERACT) log_->SetFlushOnDestroy();
  log_->Write("Client::DoStep: new state %d and err %d",
              static_cast<int>(state_), err);
  return err;
}

int Client::InitialStep(sasl_client_params_t *params,
                        sasl_interact_t **prompt_need, const char **to_server,
                        unsigned int *to_server_len,
                        sasl_out_params_t *out_params) {
  *to_server = nullptr;
  *to_server_len = 0;

  std::string auth_name;
  ReadPrompt(log_.get(), prompt_need, SASL_CB_AUTHNAME, &auth_name);
  if (auth_name.empty()) {
    int err = TriggerAuthNameCallback(log_.get(), params->utils, &auth_name);
    log_->Write("Client::InitialStep: TriggerAuthNameCallback err=%d", err);
  }

  std::string password;
  ReadPrompt(log_.get(), prompt_need, SASL_CB_PASS, &password);
  if (password.empty()) {
    int err = TriggerPasswordCallback(log_.get(), params->utils, &password);
    log_->Write("Client::InitialStep: TriggerPasswordCallback err=%d", err);
  }

  if (prompt_need && *prompt_need) {
    params->utils->free(*prompt_need);
    *prompt_need = nullptr;
  }

  if (prompt_need && (auth_name.empty() || password.empty())) {
    return RequestPrompts(params, prompt_need, auth_name.empty(),
                          password.empty());
  }

  int err = params->canon_user(params->utils->conn, auth_name.data(),
                               auth_name.size(),
                               SASL_CU_AUTHID | SASL_CU_AUTHZID, out_params);
  if (err != SASL_OK) return err;

  user_ = auth_name;
  token_ = TokenStore::Create(log_.get(), password);
  if (!token_) return SASL_FAIL;

  err = SendToken(to_server, to_server_len);
  if (err != SASL_OK) return err;

  state_ = State::kTokenSent;
  return SASL_OK;
}

int Client::TokenSentStep(sasl_client_params_t *params,
                          sasl_interact_t **prompt_need,
                          const char *from_server,
                          const unsigned int from_server_len,
                          const char **to_server, unsigned int *to_server_len,
                          sasl_out_params_t *out_params) {
  *to_server = nullptr;
  *to_server_len = 0;

  log_->Write("Client::TokenSentStep: from server: %s", from_server);

  if (from_server_len == 0) return SASL_OK;

  std::string from_server_str(from_server, from_server_len);
  std::stringstream stream(from_server_str);
  std::string status;

  try {
    Json::Value root;
    stream >> root;
    if (root.isMember("status")) status = root["status"].asString();
  } catch (const std::exception &e) {
    log_->Write("Client::TokenSentStep: caught exception: %s", e.what());
    return SASL_BADPROT;
  }

  if (status == "400" || status == "401") {
    int err = token_->Refresh();
    if (err != SASL_OK) return err;
    return SASL_TRYAGAIN;
  }

  if (status.empty()) {
    log_->Write("Client::TokenSentStep: blank status, assuming we're okay");
    return SASL_OK;
  }

  log_->Write("Client::TokenSentStep: status: %s", status.c_str());
  return SASL_BADPROT;
}

int Client::SendToken(const char **to_server, unsigned int *to_server_len) {
  std::string token;
  int err = token_->GetAccessToken(&token);
  if (err != SASL_OK) return err;

  response_ = "user=" + user_ + "\1auth=Bearer " + token + "\1\1";
  log_->Write("Client::SendToken: response: %s", response_.c_str());

  *to_server = response_.data();
  *to_server_len = response_.size();

  return SASL_OK;
}

}  // namespace sasl_xoauth2
