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

#include <assert.h>
#include <sasl/sasl.h>
#include <sasl/saslplug.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <json/json.h>
#include <string>
#include <vector>

#include "config.h"
#include "http.h"
#include "log.h"
#include "module.h"
#include "token_store.h"

const std::string kUserName = "abc@def.com";

constexpr char kTempFileTemplate[] = "/tmp/sasl_xoauth2_test_token.XXXXXX";
constexpr char kTokenTemplate[] =
    R"({"access_token": "%s", "refresh_token": "%s", "expiry": "%s"})";

constexpr char kServerPermanentError[] =
    R"({"status":"500","schemes":"Bearer","scope":"https://mail.google.com/"})";
constexpr char kServerTokenExpired[] =
    R"({"status":"401","schemes":"Bearer","scope":"https://mail.google.com/"})";

std::string s_password;
std::vector<std::string> s_cleanup_files;

FILE *OpenTempTokenFile() {
  char temp_template[sizeof(kTempFileTemplate)];
  strcpy(temp_template, kTempFileTemplate);
  int fd = mkstemp(temp_template);
  s_password = temp_template;
  s_cleanup_files.push_back(s_password);
  return fdopen(fd, "w");
}

void SetPasswordToValidToken() {
  FILE *f = OpenTempTokenFile();
  std::string expiry_str = std::to_string(time(nullptr) + 3600);
  fprintf(f, kTokenTemplate, "access", "refresh", expiry_str.c_str());
  fclose(f);
}

void SetPasswordToInvalidToken() {
  FILE *f = OpenTempTokenFile();
  std::string expiry_str = std::to_string(0);
  fprintf(f, "invalid");
  fclose(f);
}

void SetPasswordToExpiredToken() {
  FILE *f = OpenTempTokenFile();
  std::string expiry_str = std::to_string(0);
  fprintf(f, kTokenTemplate, "access", "refresh", expiry_str.c_str());
  fclose(f);
}

void SetPasswordToInvalidPath() { s_password = "/tmp/this/path/is/not/valid"; }

void Cleanup() {
  for (const auto &file : s_cleanup_files) {
    unlink(file.c_str());
  }
}

int DefaultHttpIntercept(const std::string &url, const std::string &data,
                         long *response_code, std::string *response,
                         std::string *error) {
  fprintf(stderr, "TEST: default http intercept for url=%s\n", url.c_str());
  return SASL_FAIL;
}

#define TEST_ABORT(x)                                                     \
  do {                                                                    \
    bool __result = (x);                                                  \
    if (!__result) {                                                      \
      fprintf(stderr, "TEST ASSERTION FAILED at %s:%d: %s -- ABORTING\n", \
              __FILE__, __LINE__, #x);                                    \
      Cleanup();                                                          \
      exit(1);                                                            \
    }                                                                     \
  } while (0)

#define TEST_ASSERT(x)                                                  \
  do {                                                                  \
    bool __result = (x);                                                \
    if (!__result) {                                                    \
      fprintf(stderr, "TEST ASSERTION FAILED at %s:%d: %s\n", __FILE__, \
              __LINE__, #x);                                            \
      return false;                                                     \
    }                                                                   \
  } while (0)

#define TEST_ASSERT_OK(x)                                                 \
  do {                                                                    \
    int __result = (x);                                                   \
    if (__result != SASL_OK) {                                            \
      fprintf(stderr, "TEST ASSERTION FAILED at %s:%d: %s returned %d\n", \
              __FILE__, __LINE__, #x, __result);                          \
      return false;                                                       \
    }                                                                     \
  } while (0)

void FakeFree(void *ptr) { free(ptr); }

void *FakeMalloc(size_t size) { return malloc(size); }

int FakeGetAuthName(void *, int id, const char **result, unsigned int *len) {
  assert(id == SASL_CB_AUTHNAME);
  *result = kUserName.c_str();
  *len = kUserName.size();
  return 0;
}

int FakeGetPassword(sasl_conn_t *, void *, int id, sasl_secret_t **pass) {
  assert(id == SASL_CB_PASS);
  auto *p = static_cast<sasl_secret_t *>(
      malloc(sizeof(sasl_secret_t) + s_password.size() + 1));
  p->len = s_password.size();
  strcpy(reinterpret_cast<char *>(p->data), s_password.c_str());
  *pass = p;
  return SASL_OK;
}

int FakeGetCallbackAll(sasl_conn_t *, unsigned long id, sasl_callback_ft *ft,
                       void **context) {
  if (id == SASL_CB_AUTHNAME)
    *ft = reinterpret_cast<sasl_callback_ft>(&FakeGetAuthName);
  else if (id == SASL_CB_PASS)
    *ft = reinterpret_cast<sasl_callback_ft>(&FakeGetPassword);
  else
    return SASL_FAIL;
  return SASL_OK;
}

int FakeGetCallbackNone(sasl_conn_t *, unsigned long, sasl_callback_ft *,
                        void **) {
  return SASL_FAIL;
}

int FakeCanonUser(sasl_conn_t *, const char *, unsigned int, unsigned int,
                  sasl_out_params_t *) {
  return SASL_OK;
}

class PlugCleanup {
 public:
  PlugCleanup(sasl_utils_t *utils, sasl_client_plug_t plug, void *context)
      : utils_(utils), plug_(plug), context_(context) {}

  ~PlugCleanup() { plug_.mech_dispose(context_, utils_); }

 private:
  sasl_utils_t *utils_;
  sasl_client_plug_t plug_;
  void *context_;
};

void PrintTestName(const char *name) {
  fprintf(stderr, "\n");
  fprintf(stderr, "TEST: %s\n", name);
  fprintf(stderr, "%s\n", std::string(strlen(name) + 6, '=').c_str());
}

bool TestWithPrompts(sasl_client_plug_t plug) {
  PrintTestName(__func__);
  SetPasswordToValidToken();
  sasl_xoauth2::SetHttpInterceptForTesting(&DefaultHttpIntercept);

  sasl_utils_t utils = {};
  utils.free = &FakeFree;
  utils.getcallback = &FakeGetCallbackNone;
  utils.malloc = &FakeMalloc;

  void *context = nullptr;
  TEST_ASSERT_OK(plug.mech_new(nullptr, nullptr, &context));
  PlugCleanup _(&utils, plug, context);

  sasl_client_params_t params = {};
  params.utils = &utils;
  params.canon_user = &FakeCanonUser;

  auto *prompt_need =
      static_cast<sasl_interact_t *>(malloc(sizeof(sasl_interact_t) * 3));
  memset(prompt_need, 0, sizeof(sasl_interact_t) * 3);

  sasl_interact_t *cred_auth_name = prompt_need + 0;
  cred_auth_name->id = SASL_CB_AUTHNAME;
  cred_auth_name->result = kUserName.data();
  cred_auth_name->len = kUserName.size();

  sasl_interact_t *cred_pass = prompt_need + 1;
  cred_pass->id = SASL_CB_PASS;
  cred_pass->result = s_password.data();
  cred_pass->len = s_password.size();

  sasl_interact_t *end = prompt_need + 2;
  end->id = SASL_CB_LIST_END;

  const char *to_server = nullptr;
  unsigned int to_server_len = 0;
  sasl_out_params_t out_params = {};

  TEST_ASSERT_OK(plug.mech_step(context, &params, nullptr, 0, &prompt_need,
                                &to_server, &to_server_len, &out_params));
  fprintf(stderr, "to_server=[%s], len=%d\n", to_server, to_server_len);
  TEST_ASSERT(strstr(to_server, "Bearer") != nullptr);
  TEST_ASSERT(strstr(to_server, kUserName.c_str()) != nullptr);

  TEST_ASSERT_OK(plug.mech_step(context, &params, "", 0, nullptr, &to_server,
                                &to_server_len, &out_params));
  TEST_ASSERT(to_server_len == 0);

  return true;
}

bool TestWithoutPrompts(sasl_client_plug_t plug) {
  PrintTestName(__func__);
  SetPasswordToValidToken();
  sasl_xoauth2::SetHttpInterceptForTesting(&DefaultHttpIntercept);

  sasl_utils_t utils = {};
  utils.free = &FakeFree;
  utils.getcallback = &FakeGetCallbackNone;
  utils.malloc = &FakeMalloc;

  void *context = nullptr;
  TEST_ASSERT_OK(plug.mech_new(nullptr, nullptr, &context));
  PlugCleanup _(&utils, plug, context);

  sasl_client_params_t params = {};
  params.utils = &utils;
  params.canon_user = &FakeCanonUser;

  auto *prompt_need =
      static_cast<sasl_interact_t *>(malloc(sizeof(sasl_interact_t) * 1));
  memset(prompt_need, 0, sizeof(sasl_interact_t) * 1);

  sasl_interact_t *end = prompt_need + 0;
  end->id = SASL_CB_LIST_END;

  const char *to_server = nullptr;
  unsigned int to_server_len = 0;
  sasl_out_params_t out_params = {};

  int err = plug.mech_step(context, &params, nullptr, 0, &prompt_need,
                           &to_server, &to_server_len, &out_params);
  TEST_ASSERT(err == SASL_INTERACT);
  TEST_ASSERT(to_server_len == 0);

  sasl_interact_t *p = prompt_need;
  while (p && p->id != SASL_CB_LIST_END) {
    fprintf(stderr, "p: id=%lu, challenge=%s\n", p->id, p->challenge);
    if (p->id == SASL_CB_AUTHNAME) {
      p->result = kUserName.c_str();
      p->len = kUserName.size();
    } else if (p->id == SASL_CB_PASS) {
      p->result = s_password.c_str();
      p->len = s_password.size();
    } else {
      fprintf(stderr, "unexpected id=%lu\n", p->id);
      TEST_ASSERT(false);
    }
    p++;
  }

  TEST_ASSERT_OK(plug.mech_step(context, &params, nullptr, 0, &prompt_need,
                                &to_server, &to_server_len, &out_params));
  fprintf(stderr, "to_server=[%s], len=%d\n", to_server, to_server_len);
  TEST_ASSERT(strstr(to_server, "Bearer") != nullptr);
  TEST_ASSERT(strstr(to_server, kUserName.c_str()) != nullptr);

  TEST_ASSERT_OK(plug.mech_step(context, &params, "", 0, nullptr, &to_server,
                                &to_server_len, &out_params));
  TEST_ASSERT(to_server_len == 0);

  return true;
}

bool TestWithCallbacks(sasl_client_plug_t plug) {
  PrintTestName(__func__);
  SetPasswordToValidToken();
  sasl_xoauth2::SetHttpInterceptForTesting(&DefaultHttpIntercept);

  sasl_utils_t utils = {};
  utils.free = &FakeFree;
  utils.getcallback = &FakeGetCallbackAll;
  utils.malloc = &FakeMalloc;

  void *context = nullptr;
  TEST_ASSERT_OK(plug.mech_new(nullptr, nullptr, &context));
  PlugCleanup _(&utils, plug, context);

  sasl_client_params_t params = {};
  params.utils = &utils;
  params.canon_user = &FakeCanonUser;

  const char *to_server = nullptr;
  unsigned int to_server_len = 0;
  sasl_out_params_t out_params = {};

  TEST_ASSERT_OK(plug.mech_step(context, &params, nullptr, 0, nullptr,
                                &to_server, &to_server_len, &out_params));
  fprintf(stderr, "to_server=[%s], len=%d\n", to_server, to_server_len);
  TEST_ASSERT(strstr(to_server, "Bearer") != nullptr);
  TEST_ASSERT(strstr(to_server, kUserName.c_str()) != nullptr);

  TEST_ASSERT_OK(plug.mech_step(context, &params, "", 0, nullptr, &to_server,
                                &to_server_len, &out_params));
  TEST_ASSERT(to_server_len == 0);

  return true;
}

bool TestWithPermanentError(sasl_client_plug_t plug) {
  PrintTestName(__func__);
  SetPasswordToValidToken();
  sasl_xoauth2::SetHttpInterceptForTesting(&DefaultHttpIntercept);

  sasl_utils_t utils = {};
  utils.free = &FakeFree;
  utils.getcallback = &FakeGetCallbackAll;
  utils.malloc = &FakeMalloc;

  void *context = nullptr;
  TEST_ASSERT_OK(plug.mech_new(nullptr, nullptr, &context));
  PlugCleanup _(&utils, plug, context);

  sasl_client_params_t params = {};
  params.utils = &utils;
  params.canon_user = &FakeCanonUser;

  const char *to_server = nullptr;
  unsigned int to_server_len = 0;
  sasl_out_params_t out_params = {};

  TEST_ASSERT_OK(plug.mech_step(context, &params, nullptr, 0, nullptr,
                                &to_server, &to_server_len, &out_params));
  fprintf(stderr, "to_server=[%s], len=%d\n", to_server, to_server_len);
  TEST_ASSERT(strstr(to_server, "Bearer") != nullptr);
  TEST_ASSERT(strstr(to_server, kUserName.c_str()) != nullptr);

  int err = plug.mech_step(context, &params, kServerPermanentError,
                           sizeof(kServerPermanentError), nullptr, &to_server,
                           &to_server_len, &out_params);
  fprintf(stderr, "err=%d\n", err);
  fprintf(stderr, "to_server=[%s], len=%d\n", to_server, to_server_len);
  TEST_ASSERT(err != SASL_OK);
  TEST_ASSERT(to_server_len == 0);

  return true;
}

bool TestWithTokenExpiredError(sasl_client_plug_t plug) {
  PrintTestName(__func__);
  SetPasswordToValidToken();
  sasl_xoauth2::SetHttpInterceptForTesting(&DefaultHttpIntercept);

  sasl_utils_t utils = {};
  utils.free = &FakeFree;
  utils.getcallback = &FakeGetCallbackAll;
  utils.malloc = &FakeMalloc;

  void *context = nullptr;
  TEST_ASSERT_OK(plug.mech_new(nullptr, nullptr, &context));
  PlugCleanup _(&utils, plug, context);

  sasl_client_params_t params = {};
  params.utils = &utils;
  params.canon_user = &FakeCanonUser;

  const char *to_server = nullptr;
  unsigned int to_server_len = 0;
  sasl_out_params_t out_params = {};

  TEST_ASSERT_OK(plug.mech_step(context, &params, nullptr, 0, nullptr,
                                &to_server, &to_server_len, &out_params));
  fprintf(stderr, "to_server=[%s], len=%d\n", to_server, to_server_len);
  TEST_ASSERT(strstr(to_server, "Bearer") != nullptr);
  TEST_ASSERT(strstr(to_server, kUserName.c_str()) != nullptr);

  bool intercept_called = false;
  sasl_xoauth2::SetHttpInterceptForTesting(
      [&intercept_called](const std::string &url, const std::string &data,
                          long *response_code, std::string *response,
                          std::string *error) {
        *response = R"({"access_token": "access", "expires_in": 3600})";
        *response_code = 200;
        intercept_called = true;
        return SASL_OK;
      });

  int err = plug.mech_step(context, &params, kServerTokenExpired,
                           sizeof(kServerTokenExpired), nullptr, &to_server,
                           &to_server_len, &out_params);
  fprintf(stderr, "err=%d\n", err);
  fprintf(stderr, "to_server=[%s], len=%d\n", to_server, to_server_len);
  TEST_ASSERT(err == SASL_TRYAGAIN);
  TEST_ASSERT(to_server_len == 0);
  TEST_ASSERT(intercept_called);

  return true;
}

bool TestPreemptiveTokenRefresh(sasl_client_plug_t plug) {
  PrintTestName(__func__);
  SetPasswordToExpiredToken();
  sasl_xoauth2::SetHttpInterceptForTesting(&DefaultHttpIntercept);

  sasl_utils_t utils = {};
  utils.free = &FakeFree;
  utils.getcallback = &FakeGetCallbackAll;
  utils.malloc = &FakeMalloc;

  void *context = nullptr;
  TEST_ASSERT_OK(plug.mech_new(nullptr, nullptr, &context));
  PlugCleanup _(&utils, plug, context);

  sasl_client_params_t params = {};
  params.utils = &utils;
  params.canon_user = &FakeCanonUser;

  const char *to_server = nullptr;
  unsigned int to_server_len = 0;
  sasl_out_params_t out_params = {};

  bool intercept_called = false;
  sasl_xoauth2::SetHttpInterceptForTesting(
      [&intercept_called](const std::string &url, const std::string &data,
                          long *response_code, std::string *response,
                          std::string *error) {
        *response =
            R"({"access_token": "refreshed_access", "expires_in": 3600})";
        *response_code = 200;
        intercept_called = true;
        return SASL_OK;
      });

  TEST_ASSERT_OK(plug.mech_step(context, &params, nullptr, 0, nullptr,
                                &to_server, &to_server_len, &out_params));
  fprintf(stderr, "to_server=[%s], len=%d\n", to_server, to_server_len);
  TEST_ASSERT(strstr(to_server, "Bearer") != nullptr);
  TEST_ASSERT(strstr(to_server, "refreshed_access") != nullptr);
  TEST_ASSERT(strstr(to_server, kUserName.c_str()) != nullptr);
  TEST_ASSERT(intercept_called);

  TEST_ASSERT_OK(plug.mech_step(context, &params, "", 0, nullptr, &to_server,
                                &to_server_len, &out_params));
  TEST_ASSERT(to_server_len == 0);

  return true;
}

bool TestFailedPreemptiveTokenRefresh(sasl_client_plug_t plug) {
  PrintTestName(__func__);
  SetPasswordToExpiredToken();
  sasl_xoauth2::SetHttpInterceptForTesting(&DefaultHttpIntercept);

  sasl_utils_t utils = {};
  utils.free = &FakeFree;
  utils.getcallback = &FakeGetCallbackAll;
  utils.malloc = &FakeMalloc;

  void *context = nullptr;
  TEST_ASSERT_OK(plug.mech_new(nullptr, nullptr, &context));
  PlugCleanup _(&utils, plug, context);

  sasl_client_params_t params = {};
  params.utils = &utils;
  params.canon_user = &FakeCanonUser;

  const char *to_server = nullptr;
  unsigned int to_server_len = 0;
  sasl_out_params_t out_params = {};

  bool intercept_called = false;
  sasl_xoauth2::SetHttpInterceptForTesting(
      [&intercept_called](const std::string &url, const std::string &data,
                          long *response_code, std::string *response,
                          std::string *error) {
        *response = "";
        *response_code = 400;
        intercept_called = true;
        return SASL_OK;
      });

  int err = plug.mech_step(context, &params, nullptr, 0, nullptr, &to_server,
                           &to_server_len, &out_params);
  fprintf(stderr, "err=%d\n", err);
  TEST_ASSERT(err != SASL_OK);
  TEST_ASSERT(to_server_len == 0);
  TEST_ASSERT(intercept_called);

  return true;
}

int main(int argc, char **argv) {
  sasl_xoauth2::EnableLoggingForTesting();

  Json::Value config;
  config["client_id"] = "dummy client id";
  config["client_secret"] = "dummy client secret";
  TEST_ASSERT_OK(sasl_xoauth2::Config::InitForTesting(config));

  int version = 0;
  sasl_client_plug_t *plug_list = nullptr;
  int plug_count = 0;

  sasl_utils_t utils = {};
  utils.free = &FakeFree;
  utils.getcallback = &FakeGetCallbackNone;
  utils.malloc = &FakeMalloc;

  TEST_ABORT(sasl_client_plug_init(&utils, SASL_CLIENT_PLUG_VERSION, &version,
                                   &plug_list, &plug_count) == SASL_OK);
  fprintf(stderr,
          "INIT: sasl_client_plug_init returned version=%d, plug_count=%d\n",
          version, plug_count);
  TEST_ABORT(plug_count == 1);

  sasl_client_plug_t plug = *plug_list;
  fprintf(stderr, "INIT: module name: %s\n", plug.mech_name);

  TEST_ABORT(TestWithPrompts(plug));
  TEST_ABORT(TestWithoutPrompts(plug));
  TEST_ABORT(TestWithCallbacks(plug));
  TEST_ABORT(TestWithPermanentError(plug));
  TEST_ABORT(TestWithTokenExpiredError(plug));
  TEST_ABORT(TestPreemptiveTokenRefresh(plug));
  TEST_ABORT(TestFailedPreemptiveTokenRefresh(plug));

  Cleanup();
  fprintf(stderr, "\nALL TESTS PASS.\n");

  return 0;
}
