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

#ifndef SASL_XOAUTH2_MODULE_H
#define SASL_XOAUTH2_MODULE_H

#include <sasl/sasl.h>
#include <sasl/saslplug.h>

#ifdef __cplusplus
extern "C" {
#endif

int sasl_client_plug_init(const sasl_utils_t *utils, int max_version,
                          int *out_version, sasl_client_plug_t **plug_list,
                          int *plug_count);

#ifdef __cplusplus
}
#endif

#endif  // SASL_XOAUTH2_MODULE_H
