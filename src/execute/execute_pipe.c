#include "execute/execute_pipe.h"
#include "execute/execute.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

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

int execute_pipe(char **cmds[], int cmd_count) {
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
