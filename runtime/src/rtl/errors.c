#include "xbrt_int.h"

/* The Error class: what describes a runtime error, in the order of an Error
 * object's items. */
static const xb_class_var error_vars[] = {
  {"ARGS", XB_VAR_ARRAY},           {"CANDEFAULT", XB_VAR_LOGICAL}, {"CANRETRY", XB_VAR_LOGICAL},
  {"CANSUBSTITUTE", XB_VAR_LOGICAL}, {"CARGO", XB_VAR_ANY},         {"DESCRIPTION", XB_VAR_STRING},
  {"FILENAME", XB_VAR_STRING},      {"GENCODE", XB_VAR_INT32},      {"OPERATION", XB_VAR_STRING},
  {"OSCODE", XB_VAR_INT32},         {"SEVERITY", XB_VAR_UINT16},    {"SUBCODE", XB_VAR_INT32},
  {"SUBSYSTEM", XB_VAR_STRING},     {"TRIES", XB_VAR_UINT16},
};

static const xb_class error_class = {"ERROR", sizeof error_vars / sizeof error_vars[0], error_vars};

void xbf_ERRORNEW(xb_ctx *ctx, xb_item *ret, uint32_t argc, xb_item *argv) {
  XB_UNUSED(ctx);
  XB_UNUSED(argc);
  XB_UNUSED(argv);
  xb_set_object(ret, xb_object_new(&error_class));
}
