#include "execute/execute.h"
#include "execute/builtins.h"
#include "execute/execute_pipe.h"
#include "execute/redirections.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int sanitize_command_name(const char *src, char *dst, size_t dst_size) {
  if (src == NULL || dst == NULL || dst_size == 0) {
    return -1;
  }

  size_t i = 0;
  for (; src[i] != '\0'; i++) {
    if (i >= dst_size - 1) {
      return -1;
    }

    unsigned char c = (unsigned char)src[i];
    if (c <= 32 || c >= 127) {
      return -1;
    }

    dst[i] = (char)c;
  }

  dst[i] = '\0';

  if (strstr(dst, "..") != NULL) {
    return -1;
  }

  return 0;
}

void execute_child(char *args[]) {
  handle_redirections(args);

  if (args[0] == NULL) {
    _exit(0);
  }

  char clean_storage[MAX_ARGS][MAX_CMD_LEN];
  char *clean_args[MAX_ARGS];

  size_t count = 0;
  while (args[count] != NULL && count < (MAX_ARGS - 1)) {
    if (sanitize_command_name(args[count], clean_storage[count],
                              sizeof(clean_storage[count])) != 0) {
      (void)fprintf(stderr, "[bebish]: invalid or unsafe argument\n");
      _exit(1);
    }
    clean_args[count] = clean_storage[count];
    count++;
  }

  clean_args[count] = NULL;

  if (strstr(clean_args[0], "..") != NULL) {
    (void)fprintf(stderr, "[bebish]: invalid path traversal\n");
    _exit(1);
  }

  // NOLINTNEXTLINE(clang-analyzer-optin.taint.GenericTaint)
  execvp(clean_args[0], clean_args);
  perror("[bebish]: execvp failed");
  _exit(1);
}

static int execute_args(char *args[]) {
  if (args == NULL || args[0] == NULL) {
    return 0;
  }

  if (strcmp(args[0], "cd") == 0) {
    return builtin_cd(args);
  }
  if (strcmp(args[0], "exit") == 0) {
    _exit(0);
  }

  char **cmds[64] = {0};
  int cmd_count = 1;
  cmds[0] = args;

  int is_background = 0;

  for (int i = 0; args[i] != NULL; i++) {
    if (strcmp(args[i], "|") == 0) {
      args[i] = NULL;
      cmds[cmd_count] = &args[i + 1];
      cmd_count++;
    } else if (strcmp(args[i], "&") == 0) {
      args[i] = NULL;
      is_background = 1;
    }
  }

  if (cmd_count > 1) {
    return execute_pipe(cmds, cmd_count);
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("[bebish]: fork failed");
    return -1;
  }

  if (pid == 0) {
    execute_child(args);
  } else if (!is_background) {
    int status = 0;
    if (waitpid(pid, &status, 0) > 0) {
      if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
      }
    }
    return -1;
  }

  return 0;
}

static int find_next_operator(char *args[], int start) {
  int i = start;
  while (args[i] != NULL && strcmp(args[i], "&&") != 0 &&
         strcmp(args[i], "||") != 0) {
    i++;
  }
  return i;
}

static int skip_to_next_operator(char *args[], int start,
                                 const char *target_op) {
  int i = start;
  while (args[i] != NULL && strcmp(args[i], target_op) != 0) {
    i++;
  }
  if (args[i] != NULL) {
    i++;
  }
  return i;
}

void execute_line(char *args[]) {
  int i = 0;
  while (args[i] != NULL) {
    int op_index = find_next_operator(args, i);

    char *next_op = args[op_index];
    args[op_index] = NULL;

    int status = execute_args(&args[i]);

    if (next_op == NULL) {
      break;
    }

    if (strcmp(next_op, "&&") == 0) {
      if (status == 0) {
        i = op_index + 1;
      } else {
        i = skip_to_next_operator(args, op_index + 1, "||");
      }
    } else if (strcmp(next_op, "||") == 0) {
      if (status != 0) {
        i = op_index + 1;
      } else {
        i = skip_to_next_operator(args, op_index + 1, "&&");
      }
    }
  }
}
