cmake_minimum_required(VERSION 3.9)
set(CMAKE_C_STANDARD 11)
project(bs VERSION 0.1.0 DESCRIPTION "Barua's Scripting language")

# Enable warnings
if (MSVC)
  add_compile_options(/W4 /WX)
else()
  add_compile_options(-Wall -Wextra -pedantic)
endif()

# Add a preprocessor definition with the length of the source directory. This
# is useful for stripping out the prefix in macros.
string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
add_definitions("-DBS_SOURCE_PATH_SIZE=${SOURCE_PATH_SIZE}")

add_library(bs
  ast.c
  bs.c
  lexer.c
  parser.c
  string.c
  writer.c)

add_executable(bsc bsc.c)
target_link_libraries(bsc PRIVATE bs)

add_executable(tests
  test.c
  ast-test.c
  lexer-test.c
  parser-test.c
  string-test.c)
target_link_libraries(tests PRIVATE bs)
