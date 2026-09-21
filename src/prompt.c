#include "prompt.h"
#include "env.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void get_system_hostname(char *buffer, size_t len) {
  if (len == 0) {
    return;
  }
  if (gethostname(buffer, len) != 0) {
    int err = snprintf(buffer, len, "unknown");
    if (err < 0 || (size_t)err >= len) {
      perror("[bebish]: get hostname failed");
      _exit(1);
    }
  } else {
    buffer[len - 1] = '\0';
  }
}

void print_prompt(void) {
  char cwd[1024];
  char hostname[256];
  get_system_hostname(hostname, sizeof(hostname));

  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    const char *prefix = get_env_var("HOME");
    size_t prefix_len = (prefix != NULL) ? strlen(prefix) : 0;

    if (prefix != NULL && strncmp(cwd, prefix, prefix_len) == 0) {
      memmove(cwd, cwd + prefix_len, strlen(cwd + prefix_len) + 1);
      printf("%s@%s:~%s$ ", get_env_var("USER"), hostname, cwd);
    } else {
      printf("%s@%s:%s$ ", get_env_var("USER"), hostname, cwd);
    }
  } else {
    perror("[bebish]: getcwd failed");
  }
}
