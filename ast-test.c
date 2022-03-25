#include "ast.h"

#include "test.h"

TEST(Ast, Vec) {
  struct AstVec vec;
  ast_vec_init(&vec);
  ast_vec_push(&vec, NULL);
  ast_vec_push(&vec, NULL);
  ast_vec_push(&vec, NULL);
  ASSERT(vec.length == 3);
  ASSERT(vec.data[0] == NULL);
  ASSERT(vec.data[1] == NULL);
  ASSERT(vec.data[2] == NULL);
  ast_vec_fini(&vec);
}

TEST(Ast, PairVec) {
  struct AstPairVec vec;
  ast_pair_vec_init(&vec);
  ast_pair_vec_push(&vec, NULL, NULL);
  ast_pair_vec_push(&vec, NULL, NULL);
  ast_pair_vec_push(&vec, NULL, NULL);
  ASSERT(vec.length == 3);
  ASSERT(vec.data[0].key == NULL);
  ASSERT(vec.data[0].value == NULL);
  ASSERT(vec.data[1].key == NULL);
  ASSERT(vec.data[1].value == NULL);
  ASSERT(vec.data[2].key == NULL);
  ASSERT(vec.data[2].value == NULL);
  ast_pair_vec_fini(&vec);
}

TEST(Ast, PrintFunction) {
  struct Str fib, n;
  struct AstVec params, fib_body, if_body, else_body, first_args, second_args;
  ast_vec_init(&params);
  ast_vec_init(&fib_body);
  ast_vec_init(&if_body);
  ast_vec_init(&else_body);
  ast_vec_init(&first_args);
  ast_vec_init(&second_args);

  str_init(&fib, "fib", SIZE_MAX);
  str_init(&n, "n", SIZE_MAX);

  ast_vec_push(&params, ast_identifier_create(0, n));
  ast_vec_push(&if_body, ast_return_create(0, ast_integer_create(0, 1)));

  ast_vec_push(&first_args,
               ast_binary_create(0, BO_Subtract,
                                 ast_identifier_create(0, n),
                                 ast_integer_create(0, 1)));
  ast_vec_push(&second_args,
               ast_binary_create(0, BO_Subtract,
                                 ast_identifier_create(0, n),
                                 ast_integer_create(0, 2)));
  ast_vec_push(&else_body,
               ast_return_create(0,
                                 ast_binary_create(0, BO_Add,
                                                   ast_call_create(0,
                                                                   ast_identifier_create(0, fib),
                                                                   first_args),
                                                   ast_call_create(0,
                                                                   ast_identifier_create(0, fib),
                                                                   second_args))));
  ast_vec_push(&fib_body,
               ast_if_create(0,
                             ast_binary_create(0, BO_LessEqual,
                                               ast_identifier_create(0, n),
                                               ast_integer_create(0, 1)),
                             ast_block_create(0, if_body, true),
                             ast_block_create(0, else_body, true)));
  struct Ast* ast = ast_let_create(0, true, fib,
                                   ast_function_create(0, params,
                                                       ast_block_create(0, fib_body, false)));

  struct String buffer;
  string_init(&buffer, "");
  struct Writer* writer = (struct Writer*) string_writer_create(&buffer);
  ASSERT(ast_print(ast, writer) > 0);
  string_writer_free((struct StringWriter*) writer);

  struct Str slice = { buffer.data, buffer.length };
  struct Str target;
  str_init(&target, "(let fib <public> (fn (params n) (block <ret> (if (<= n 1) (block <noret> "
           "(return 1)) (else (block <noret> (return (+ (call fib (- n 1)) (call fib (- n "
           "2))))))))))", SIZE_MAX);
  ASSERT_STR_EQ(slice, target);

  ast_free(ast);
}
