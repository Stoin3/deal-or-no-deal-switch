#include "glmini.h"
#include "platform.h"
#define GL_DEF(ret, name, args) PFN_##name gm_##name = nullptr;
GL_FUNCS(GL_DEF)
#undef GL_DEF
bool glminiLoad(GLGetProcFn fn) {
  bool ok = true;
#define GL_LOAD(ret, name, args) gm_##name = (PFN_##name)fn(#name); if (!gm_##name) { plat_log("missing GL function %s", #name); ok = false; }
  GL_FUNCS(GL_LOAD)
#undef GL_LOAD
  return ok;
}
