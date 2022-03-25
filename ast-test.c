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
