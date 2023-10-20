#include "msgstream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int child_main(int write_fd);
int fork_child();
int join_child();

// 1. write the byte 42 / success
// 2. write "hello" with buf size 8 / success
// 3. write message of size 0x12345 with ordered bytes [0-n) / success

int main() {
  int read_fd = fork_child();
  if (read_fd == -1)
    return 1;

  int8_t b42 = 0;
  msgstream_size nread = msgstream_read(read_fd, &b42, sizeof(b42), stderr);
  if (nread == -1)
    return 1;

  if (b42 != 42) {
    fprintf(stderr, "expected to read 42 but read %d\n", b42);
    return 1;
  }

  char hello[9] = {};
  nread = msgstream_read(read_fd, hello, 8, stderr);
  if (nread == -1)
    return 1;

  if (strncmp(hello, "hello", nread) != 0) {
    fprintf(stderr, "expected to read 'hello' but read %s\n", hello);
    return 1;
  }

  uint8_t huge[0x12345] = {};
  nread = msgstream_read(read_fd, huge, sizeof(huge), stderr);
  if (nread == -1)
    return 1;

  for (size_t i = 0; i < 0x12345; ++i) {
    if (huge[i] != (i % 256)) {
      fprintf(stderr, "Error reading huge msg at index %lu\n", i);
      return 1;
    }
  }

  if (join_child() == -1) {
    fprintf(stderr, "child exited with nonzero status\n");
    return 1;
  }

  return 0;
}

int child_main(int write_fd) {
  int8_t b42 = 42;
  if (msgstream_write(write_fd, &b42, sizeof(b42), 1, stderr) == -1)
    return 1;

  char hello[8] = "hello";
  if (msgstream_write(write_fd, hello, sizeof(hello), strlen(hello), stderr) ==
      -1)
    return 1;

  uint8_t huge[0x12345];
  for (size_t i = 0; i < sizeof(huge); ++i)
    huge[i] = i % 256;

  if (msgstream_write(write_fd, huge, sizeof(huge), sizeof(huge), stderr) == -1)
    return 1;

  return 0;
}

int fork_child() {
  int pipes[2];
  if (pipe(pipes) == -1) {
    perror("pipe");
    return -1;
  }

  pid_t pid = fork();
  if (pid == -1) {
    perror("fork");
    return -1;
  } else if (pid == 0) {
    close(pipes[0]);
    int status = child_main(pipes[1]);
    exit(status);
  }

  close(pipes[1]);
  return pipes[0];
}

int join_child() {
  int stat_loc = 0;
  if (wait(&stat_loc) == -1) {
    perror("wait");
    return -1;
  }

  if (!(WIFEXITED(stat_loc) && WEXITSTATUS(stat_loc) == 0))
    return -1;

  return 0;
}
