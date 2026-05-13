#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  char *cmds = malloc(1024);
  assert(cmds != NULL);

  while (1) {
    int f_err = fflush(stdout);
    assert(f_err == 0);

    printf("> ");
    int s_res = scanf("%s", cmds);

    if (s_res == EOF) {
      free(cmds);
      return 0;
    }

    assert(s_res == 1);
  }

  free(cmds);
  return 0;
}