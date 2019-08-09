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
