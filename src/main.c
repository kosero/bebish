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

static void builtin_cd(char **args) {
  const char *path = args[1];
  if (path == NULL) {
    path = get_env_var("HOME");
  }

  if (path == NULL) {
    (void)fprintf(stderr, "[bebish]: cd: HOME not set\n");
    return;
  }

  if (chdir(path) != 0) {
    perror("[bebish]: cd failed");
  }
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

static void execute_pipe(char *left_args[], char *right_args[]) {
  int pipefd[2];
  if (pipe(pipefd) < 0) {
    perror("[bebish]: pipe failed");
    return;
  }

  if (fork() == 0) {
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);
    execute_child(left_args);
  }

  if (fork() == 0) {
    dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);
    execute_child(right_args);
  }

  close(pipefd[0]);
  close(pipefd[1]);
  wait(NULL);
  wait(NULL);
}

static void execute_args(char *args[]) {
  int is_background = 0;
  for (int i = 0; args[i] != NULL; i++) {
    if (strcmp(args[i], "|") == 0) {
      args[i] = NULL;
      execute_pipe(args, &args[i + 1]);
      return;
    }
    if (strcmp(args[i], "&") == 0) {
      args[i] = NULL;
      is_background = 1;
    }
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("[bebish]: fork failed");
    return;
  }

  if (pid == 0) {

    execute_child(args);
  } else if (!is_background) {
    wait(NULL);
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
    } else if (strcmp(args[0], "cd") == 0) {
      builtin_cd(args);
    } else {
      execute_args(args);
    }
  }

  free(line);
  return 0;
}
