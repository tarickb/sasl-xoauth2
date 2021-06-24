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

#include "module.h"

#include <sasl/sasl.h>
#include <sasl/saslplug.h>

#include "client.h"
#include "config.h"

namespace {

int mech_new(void *, sasl_client_params_t *params, void **context) {
  sasl_xoauth2::Client *client = new sasl_xoauth2::Client();
  if (!client) {
    params->utils->seterror(params->utils->conn, 0,
                            "Failed to create Client instance.");
    return SASL_NOMEM;
  }
  *context = client;
  return SASL_OK;
}

int mech_step(void *context, sasl_client_params_t *params,
              const char *from_server, unsigned int from_server_len,
              sasl_interact_t **prompt_need, const char **to_server,
              unsigned int *to_server_len, sasl_out_params_t *out_params) {
  if (!context) return SASL_BADPARAM;
  return static_cast<sasl_xoauth2::Client *>(context)->DoStep(
      params, from_server, from_server_len, prompt_need, to_server,
      to_server_len, out_params);
}

void mech_dispose(void *context, const sasl_utils_t *utils) {
  if (!context) return;
  delete static_cast<sasl_xoauth2::Client *>(context);
}

sasl_client_plug_t s_plugin = {
    /* mech_name = */ "XOAUTH2",
    /* max_ssf = */ 60,
    /* security_flags = */ SASL_SEC_NOANONYMOUS | SASL_SEC_PASS_CREDENTIALS,
    /* features = */ SASL_FEAT_WANT_CLIENT_FIRST | SASL_FEAT_ALLOWS_PROXY,
    /* required_prompts = */ nullptr,
    /* glob_context = */ nullptr,
    /* mech_new = */ &mech_new,
    /* mech_step = */ &mech_step,
    /* mech_dispose = */ &mech_dispose,
    /* mech_free = */ nullptr,
    /* idle = */ nullptr,
    /* spare_fptr1 = */ nullptr,
    /* spare_fptr2 = */ nullptr};

sasl_client_plug_t s_plugins[] = {s_plugin};

}  // namespace

extern "C" int sasl_client_plug_init(const sasl_utils_t *utils, int max_version,
                                     int *out_version,
                                     sasl_client_plug_t **plug_list,
                                     int *plug_count) {
  if (max_version < SASL_CLIENT_PLUG_VERSION) {
    utils->seterror(utils->conn, 0, "sasl-xoauth2: need version %d, got %d",
                    SASL_CLIENT_PLUG_VERSION, max_version);
    return SASL_BADVERS;
  }

  // Do this here because subsequent calls are chroot-ed (for Postfix, at
  // least).
  int err = sasl_xoauth2::Config::Init();
  if (err != SASL_OK) return err;

  *out_version = SASL_CLIENT_PLUG_VERSION;
  *plug_list = s_plugins;
  *plug_count = sizeof(s_plugins) / sizeof(s_plugins[0]);
  return SASL_OK;
}
