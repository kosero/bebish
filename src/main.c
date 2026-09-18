#include "execute/execute.h"
#include "parser.h"
#include "prompt.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(void) {
  char *line = NULL;
  size_t len = 0;

  while (1) {
    assert(fflush(stdout) == 0);

    print_prompt();

    int l_err = read_line(&line, &len);
    if (l_err != 0) {
      break;
    }

    char *args[64] = {0};
    int status = parse_line(args, line);
    if (status == -1) {
      continue;
    }

    execute_line(args);
  }

  free(line);
  return 0;
}
