#include <assert.h>
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

static void print_prompt() {
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    printf("%s $ ", cwd);
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
