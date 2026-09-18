#include "execute/redirections.h"
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void handle_redirections(char *args[]) {
  for (int i = 0; args[i] != NULL; i++) {
    int is_out = (strcmp(args[i], ">") == 0);
    int is_append = (strcmp(args[i], ">>") == 0);
    int is_in = (strcmp(args[i], "<") == 0);

    if (!is_out && !is_in && !is_append) {
      continue;
    }

    char *filename = args[i + 1];
    if (filename == NULL) {
      (void)fprintf(stderr,
                    "[bebish]: syntax error near unexpected token 'newline'\n");
      _exit(1);
    }

    int flags = is_in ? O_RDONLY
                      : (O_WRONLY | O_CREAT | (is_append ? O_APPEND : O_TRUNC));
    int target_fd = (is_out || is_append) ? STDOUT_FILENO : STDIN_FILENO;

    int fd = open(filename, flags, 0644);
    if (fd < 0) {
      perror("[bebish]: open failed");
      _exit(1);
    }

    (void)dup2(fd, target_fd);
    (void)close(fd);

    args[i] = NULL;
  }
}
