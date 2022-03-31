#include "test.h"

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    // In parent process, close the write end, and create a RunningTest and append to list.
    close(fds[1]);
    struct RunningTest running_test;
    running_test.suite_name = test->suite_name;
    running_test.test_name = test->test_name;
    running_test.should_fail = test->should_fail;
    running_test.pid = pid;
    running_test.stderr_fd = fds[0];
    RUNNING_TESTS.num_tests++;
    RUNNING_TESTS.tests = realloc(RUNNING_TESTS.tests,
                                  RUNNING_TESTS.num_tests * sizeof(struct RunningTest));
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

static bool test_name_is(const struct Test* test, const char* name) {
  size_t i = 0, j = 0;
  while (test->suite_name[i] && name[j]) {
    if (test->suite_name[i] != name[j]) {
      return false;
    }
    i++;
    j++;
  }
  if (name[j] != '.') {
    return false;
  }
  j++;
  i = 0;
  while (test->test_name[i] && name[j]) {
    if (test->test_name[i] != name[j]) {
      return false;
    }
    i++;
    j++;
  }
  if (test->test_name[i] || name[j]) {
    return false;
  } else {
    return true;
  }
}

#if defined(__linux)

// FIXME: This assumes debugger is GDB, and that it is installed. Allow for more debuggers, and
// handle GDB not being installed.
static void run_debugger(pid_t test_pid, const struct Test* test) {
    char pidstr[32], bpstr[256];
    ASSERT(snprintf(pidstr, sizeof(pidstr), "--pid=%d", test_pid) < sizeof(pidstr));
    ASSERT(snprintf(bpstr, sizeof(bpstr), "b test_fn_%s_%s", test->suite_name,
                    test->test_name) < sizeof(bpstr));
    fprintf(stderr, "Command line: gdb %s -ex '%s' -ex c\n", pidstr, bpstr);
    if (execlp("gdb", "gdb", pidstr, "-ex", bpstr, "-ex", "c", NULL) < 0) {
      ERR_FMT("Failed to run GDB: %s\n", strerror(errno));
    }
}

#elif defined(__APPLE__)

#include <fcntl.h>
#include <sys/syslimits.h>

// Writes LLDB commands to a tmpfile and returns the path to the file in pathbuf. Note that this
// assumes that pathbuf has sufficient space (it should be atleast PATH_MAX bytes).
static void write_lldb_initial_commands(const struct Test* test, char* pathbuf) {
  // Get a temporary file and write commands
  FILE* tmp = tmpfile();
  if (!tmp) {
    perror("Couldn't create temporary file");
    exit(1);
  }
  fprintf(tmp, "b test_fn_%s_%s\nc\n", test->suite_name, test->test_name);
  // Get path to temporary file
  if (fcntl(fileno(tmp), F_GETPATH, pathbuf) == -1) {
    perror("Couldn't get path to temporary file");
    fclose(tmp);
    exit(1);
  }
  // Close temporary file
  fclose(tmp);
}

// FIXME: This assumes debugger is LLDB, and that it is installed. Allow for more debuggers, and
// handle LLDB not being installed.
static void run_debugger(pid_t test_pid, const struct Test* test) {
  char pidstr[16], tmppath[PATH_MAX];
  //write_lldb_initial_commands(test, tmppath);
  fprintf(stderr, "Debugging test: test_fn_%s_%s\n", test->suite_name, test->test_name);
  ASSERT(snprintf(pidstr, sizeof(pidstr), "%d", test_pid) < sizeof(pidstr));
  fprintf(stderr, "Command line: lldb  --attach-pid %s\n", pidstr);
  if (execlp("lldb", "lldb", "--attach-pid", pidstr, /*"--source", tmppath,*/ NULL) < 0) {
    ERR_FMT("Failed to run LLDB: %s\n", strerror(errno));
  }
}

#endif  // OS-specific run_debugger()

static void debug_test(const struct Test* test) {
  // Spawn test process
  pid_t test_pid = fork();
  if (test_pid < 0) {
    perror("fork()");
    exit(1);
  }
  if (test_pid == 0) {
    // Stop this process till debugger attaches
    kill(getpid(), SIGSTOP);
    test->test_function();
    exit(0);
  }
  // Spawn debugger process
  pid_t debug_pid = fork();
  if (debug_pid < 0) {
    perror("Couldn't spawn debugger: fork()");
    // "Continue" the process and wait for it
    kill(test_pid, SIGCONT);
    waitpid(test_pid, NULL, 0);
    exit(1);
  }
  if (debug_pid == 0) {
    // Run debugger in child process
    run_debugger(test_pid, test);
    exit(0);
  }
  // Wait for the child processes
  waitpid(debug_pid, NULL, 0);
  waitpid(test_pid, NULL, 0);
}

static int debug_this_test(const char* name) {
  for (size_t i = 0; i < TESTS.num_tests; i++) {
    if (test_name_is(&TESTS.tests[i], name)) {
      debug_test(&TESTS.tests[i]);
      free(TESTS.tests);
      return 0;
    }
  }
  ERR_FMT("Could not find test: %s\n", name);
  free(TESTS.tests);
  return 1;
}

static void clean_up() {
  free(TESTS.tests);
  free(RUNNING_TESTS.tests);
}

int main(int argc, char *const *argv) {
  if (argc == 3) {
    if (!strcmp(argv[1], "-d")) {
      const char* test_name = argv[2];
      return debug_this_test(test_name);
    }
  }
  run_all_tests();
  clean_up();
  return 0;
}
