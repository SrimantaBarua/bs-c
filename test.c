#include "test.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "test-macros.h"

struct Tests {
  struct Test* tests; // List of tests
  size_t num_tests;   // Number of tests
};

static struct Tests TESTS = { NULL, 0 };

void test_add(struct Test test) {
  TESTS.num_tests++;
  TESTS.tests = realloc(TESTS.tests, TESTS.num_tests * sizeof(struct Test));
  TESTS.tests[TESTS.num_tests - 1] = test;
}

struct RunningTest {
  const char *suite_name; // Test suite name
  const char *test_name;  // Test name
  bool should_fail;       // Should this test fail?
  pid_t pid;              // PID of test process
  int stderr_fd;          // Handle to piped STDERR for the test process
};

struct RunningTests {
  struct RunningTest* tests; // List of tests
  size_t num_tests;          // Number of tests
};

static struct RunningTests RUNNING_TESTS = { NULL, 0 };

static void run_test(const struct Test* test) {
  int fds[2];
  // Create a pipe to read STDERR
  if (pipe(fds) < 0) {
    perror("pipe()");
    exit(1);
  }
  // Spawn test process
  pid_t pid = fork();
  if (pid < 0) {
    perror("fork()");
    exit(1);
  }
  if (pid == 0) {
    // In child test process, close read end of pipe, redirect STDERR to write end, and run test.
    close(fds[0]);
    dup2(fds[1], 2);
    test->test_function();
    exit(0);
  } else {
    // In parent process, create a RunningTest and append to list.
    struct RunningTest running_test;
    running_test.suite_name = test->suite_name;
    running_test.test_name = test->test_name;
    running_test.should_fail = test->should_fail;
    running_test.pid = pid;
    running_test.stderr_fd = fds[0];
    RUNNING_TESTS.num_tests++;
    RUNNING_TESTS.tests = realloc(RUNNING_TESTS.tests, RUNNING_TESTS.num_tests);
    RUNNING_TESTS.tests[RUNNING_TESTS.num_tests - 1] = running_test;
  }
}

static void read_all_and_dump_to_stderr(int fd) {
  char buf[1024];
  while (true) {
      ssize_t ret = read(fd, buf, sizeof(buf) - 1);
      if (ret < 0) {
        if (errno == EINTR) {
          continue;
        }
        exit(1);
      }
      if (ret == 0) {
        break;
      }
      buf[ret] = '\0';
      fwrite(buf, 1, ret, stderr);
      if (ret < sizeof(buf) - 1) {
        // We (probably?) didn't have enough data to read.
        break;
      }
  }
}

static void run_all_tests() {
  // Run all tests
  for (size_t i = 0; i < TESTS.num_tests; i++) {
    run_test(&TESTS.tests[i]);
  }
  // Wait for all tests
  size_t num_success = 0, num_fail = 0, num_tests = RUNNING_TESTS.num_tests;
  for (size_t i = 0; i < RUNNING_TESTS.num_tests; i++) {
    int status = 0;
    const struct RunningTest* test = &RUNNING_TESTS.tests[i];
    // Wait for the process to exit
    pid_t pid = waitpid(test->pid, &status, 0);
    ASSERT(pid > 0);
    // Check if failure or success
    if ((!test->should_fail && status == 0) || (test->should_fail && status != 0)) {
      fprintf(stderr, "\x1b[1;32mPASS\x1b[0m \x1b[1m%s.%s\x1b[0m\n", test->suite_name,
              test->test_name);
      num_success++;
    } else {
      fprintf(stderr, "\x1b[1;31mFAIL\x1b[0m \x1b[1m%s.%s\x1b[0m\n", test->suite_name,
              test->test_name);
      read_all_and_dump_to_stderr(test->stderr_fd);
      fputc('\n', stderr);
      num_fail++;
    }
    // Close the stderr file descriptor
    close(test->stderr_fd);
  }
}

static void clean_up() {
  free(TESTS.tests);
  free(RUNNING_TESTS.tests);
}

int main(int argc, char *const *argv) {
  run_all_tests();
  clean_up();
  return 0;
}
