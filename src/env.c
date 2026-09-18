#include "env.h"
#include <string.h>

const char *get_env_var(const char *name) {
  size_t len = strlen(name);
  for (char **env = environ; *env != NULL; ++env) {
    if (strncmp(*env, name, len) == 0 && (*env)[len] == '=') {
      return *env + len + 1;
    }
  }
  return NULL;
}
