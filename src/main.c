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

typedef enum {
  TOKEN_WORD,
  TOKEN_PIPE,
  TOKEN_AND,
  TOKEN_OR,
} TokenType;

typedef struct {
  char *value;
  TokenType type;
} Token;

static char *parse_token(char **line_ptr) {
  char *p = *line_ptr;

  while (*p == ' ' || *p == '\t' || *p == '\n') {
    p++;
  }

  if (*p == '\0') {
    *line_ptr = p;
    return NULL;
  }

  char quote = 0;
  if (*p == '"' || *p == '\'') {
    quote = *p;
    p++;
  }

  char *token = p;
  while (*p != '\0') {
    if (quote && *p == quote) {
      *p = '\0';
      p++;
      break;
    }
    if (!quote && (*p == ' ' || *p == '\t' || *p == '\n')) {
      *p = '\0';
      p++;
      break;
    }
    p++;
  }

  *line_ptr = p;
  return token;
}

static int parse_line(char *args[], char *line) {
  size_t i = 0;
  char *cursor = line;
  while (i < 63) {
    char *token = parse_token(&cursor);
    if (token == NULL) {
      break;
    }
    args[i++] = token;
  }
  args[i] = NULL;
  return (args[0] == NULL) ? -1 : 0;
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
  }
}

enum { MAX_CMD_LEN = 256, MAX_ARGS = 64 };

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

static void execute_child(char *args[]) {
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

static void setup_child_pipes(int prev_fd, int pipefd[2], int is_last) {
  if (prev_fd != -1) {
    if (dup2(prev_fd, STDIN_FILENO) == -1) {
      perror("[bebish]: dup2 STDIN failed");
      _exit(EXIT_FAILURE);
    }
    close(prev_fd);
  }

  if (!is_last) {
    if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
      perror("[bebish]: dup2 STDOUT failed");
      _exit(EXIT_FAILURE);
    }
    close(pipefd[1]);
    close(pipefd[0]);
  }
}

static pid_t spawn_pipe_stage(char *cmd_args[], int prev_fd, int pipefd[2],
                              int is_last) {
  pid_t pid = fork();
  if (pid < 0) {
    perror("[bebish]: fork failed");
    return -1;
  }

  if (pid == 0) {
    setup_child_pipes(prev_fd, pipefd, is_last);
    execute_child(cmd_args);
    _exit(EXIT_FAILURE);
  }

  return pid;
}

static int wait_pipe_stages(pid_t pids[], int cmd_count) {
  int last_status = 0;

  for (int i = 0; i < cmd_count; i++) {
    int status = 0;
    if (waitpid(pids[i], &status, 0) == -1) {
      perror("[bebish]: waitpid failed");
      continue;
    }
    if (i == cmd_count - 1 && WIFEXITED(status)) {
      last_status = WEXITSTATUS(status);
    }
  }

  return last_status;
}

static int execute_pipe(char **cmds[], int cmd_count) {
  pid_t pids[64];
  int prev_fd = -1;

  for (int i = 0; i < cmd_count; i++) {
    int pipefd[2] = {-1, -1};
    int is_last = (i == cmd_count - 1);

    if (!is_last && pipe(pipefd) == -1) {
      perror("[bebish]: pipe failed");
      return -1;
    }

    pids[i] = spawn_pipe_stage(cmds[i], prev_fd, pipefd, is_last);
    if (pids[i] < 0) {
      return -1;
    }

    if (prev_fd != -1) {
      close(prev_fd);
    }
    if (!is_last) {
      close(pipefd[1]);
      prev_fd = pipefd[0];
    }
  }

  return wait_pipe_stages(pids, cmd_count);
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

static void print_prompt(void) {
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
