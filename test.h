#ifndef __BS_TEST_HPP__
#define __BS_TEST_HPP__

#include <stdbool.h>

// Encapsulates information about a single test
struct Test {
  const char* suite_name;  // Test-suite name
  const char* test_name;   // Test name
  void (*test_function)(); // Test function to run
  bool should_fail;        // Indicates whether the test should fail
};

// Add a new test - this is called _before_ main by the TEST and TEST_FAIL macros.
void test_add(struct Test test);

// Add a new test which should succeed
#define TEST(SUITE, NAME)                                               \
  void test_fn_##SUITE##_##NAME();                                      \
  void __attribute__ ((constructor)) add_test_fn_##SUITE##_##NAME() {   \
    test_add((struct Test) {                                            \
        .suite_name = #SUITE,                                           \
        .test_name = #NAME,                                             \
        .test_function = test_fn_##SUITE##_##NAME,                      \
        .should_fail = false                                            \
      });                                                               \
  }                                                                     \
  void test_fn_##SUITE##_##NAME()                                       \
    
// Add a new test which should fail
#define TEST_FAIL(SUITE, NAME)                                          \
  void test_fn_##SUITE##_##NAME();                                      \
  void __attribute__ ((constructor)) add_test_fn_##SUITE##_##NAME() {   \
    test_add((struct Test) {                                            \
        .suite_name = #SUITE,                                           \
        .test_name = #NAME,                                             \
        .test_function = test_fn_##SUITE##_##NAME,                      \
        .should_fail = true                                             \
      });                                                               \
  }                                                                     \
  void test_fn_##SUITE##_##NAME()                                       \

#endif  // __BS_TEST_HPP__
