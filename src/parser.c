#include "parser.h"
#include <stdio.h>

int read_line(char **line, size_t *len) {
  ssize_t nread = getline(line, len, stdin);
  if (nread == -1) {
    return 1;
  }
  return 0;
}

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

int parse_line(char *args[], char *line) {
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
