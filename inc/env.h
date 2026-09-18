#ifndef BEBISH_ENV_H
#define BEBISH_ENV_H

#include <stddef.h>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern char **environ;

const char *get_env_var(const char *name); 

#endif // BEBISH_ENV_H
