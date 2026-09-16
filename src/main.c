#include <assert.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern char **environ;

static const char *get_env_var(const char *name) {
  size_t len = strlen(name);
  for (char **env = environ; *env != NULL; ++env) {
    if (strncmp(*env, name, len) == 0 && (*env)[len] == '=') {
      return *env + len + 1;
    }
  }
  return NULL;
}

static int builtin_cd(char **args) {
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

static int read_line(char **line, size_t *len) {
  ssize_t nread = getline(line, len, stdin);
  if (nread == -1) {
    return 1;
  }
  return 0;
}

static int parse_line(char *args[], char *line) {
  size_t i = 0;

  char *saveptr = NULL;
  char *token = strtok_r(line, " \n", &saveptr);
  while (token != NULL && i < 63) {
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

static void handle_redirections(char *args[]) {
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
    break;
  }
}

static void execute_child(char *args[]) {
  handle_redirections(args);

  if (args[0] == NULL) {
    _exit(0);
  }

  execvp(args[0], args);
  perror("[bebish]: execvp failed");
  _exit(1);
}

static int execute_pipe(char *left_args[], char *right_args[]) {
  int pipefd[2];
  if (pipe(pipefd) < 0) {
    perror("[bebish]: pipe failed");
    return -1;
  }

  pid_t pid1 = fork();
  if (pid1 == 0) {
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);
    execute_child(left_args);
  }

  pid_t pid2 = fork();
  if (pid2 == 0) {
    dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);
    execute_child(right_args);
  }

  close(pipefd[0]);
  close(pipefd[1]);

  waitpid(pid1, NULL, 0);

  int status = 0;
  if (waitpid(pid2, &status, 0) > 0) {
    if (WIFEXITED(status)) {
      return WEXITSTATUS(status);
    }
  }

  return 0;
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

  int is_background = 0;
  for (int i = 0; args[i] != NULL; i++) {
    if (strcmp(args[i], "|") == 0) {
      args[i] = NULL;
      return execute_pipe(args, &args[i + 1]);
    }
    if (strcmp(args[i], "&") == 0) {
      args[i] = NULL;
      is_background = 1;
    }
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

static void execute_line(char *args[]) {
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

static void get_system_hostname(char *buffer, size_t len) {
  if (gethostname(buffer, len)) {
    (void)snprintf(buffer, len, "unknown");
  }
}

static void print_prompt() {
  char cwd[1024];
  char hostname[256];
  get_system_hostname(hostname, sizeof(hostname));

  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    const char *prefix = get_env_var("HOME");
    size_t prefix_len = (prefix != NULL) ? strlen(prefix) : 0;

    if (prefix != NULL && strncmp(cwd, prefix, prefix_len) == 0) {
      memmove(cwd, cwd + prefix_len, strlen(cwd + prefix_len) + 1);
      printf("%s@%s ~%s$ ", get_env_var("USER"), hostname, cwd);
    } else {
      printf("%s@%s ~%s$ ", get_env_var("USER"), hostname, cwd);
    }
  } else {
    perror("[bebish]: getcwd failed");
  }
}

int main(void) {
  char *line = NULL;
  size_t len = 0;

  while (1) {
    int f_err = fflush(stdout);
    assert(f_err == 0);

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

    if (strcmp(args[0], "exit") == 0) {
      _exit(0);
    } else {
      execute_line(args);
    }
  }

  free(line);
  return 0;
}
