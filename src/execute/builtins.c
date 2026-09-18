#include "execute/builtins.h"
#include "env.h"
#include <stdio.h>
#include <unistd.h>

int builtin_cd(char **args) {
  const char *path = args[1];
  if (path == NULL) {
    path = get_env_var("HOME");
  }

  if (path == NULL) {
    (void)fprintf(stderr, "[bebish]: cd: HOME not set\n");
    return -1;
  }

  if (chdir(path) != 0) {
    perror("[bebish]: cd failed");
    return -1;
  }

  return 0;
}
