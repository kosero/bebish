#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int read_line(char **line, size_t *len) {
  ssize_t nread = getline(line, len, stdin);
  if (nread == -1) {
    return 1;
  }
  return 0;
}

static int parse_line(char *args[], char *line) {
  char i = 0;

  char *saveptr = NULL;
  char *token = strtok_r(line, " \n", &saveptr);
  while (token != NULL) {
    args[i] = token;
    i++;
    token = strtok_r(NULL, " \n", &saveptr);
  }

  args[i] = NULL;
  if (args[0] == NULL) {
    return -1;
  }

  return 0;
}

static void execute_args(char *args[]) {
  pid_t pid = fork();
  if (pid < 0) {
    perror("[bebish]: fork failed");
  } else if (pid == 0) {
    execvp(args[0], args);
    perror("[bebish]: execvp failed");
    _exit(1);
  } else {
    wait(NULL);
  }
}

int main(void) {
  char *line = NULL;
  size_t len = 0;

  while (1) {
    int f_err = fflush(stdout);
    assert(f_err == 0);
    printf("> ");

    int l_err = read_line(&line, &len);
    assert(l_err == 0);

    char *args[64] = {0};
    int status = parse_line(args, line);
    if (status == -1) {
      continue;
    }

    execute_args(args);
  }

  free(line);
  return 0;
}
