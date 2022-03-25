#ifndef __BS_AST_H__
#define __BS_AST_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "string.h"

enum BinaryOp {
  BO_Equal,
  BO_NotEqual,
  BO_LessEqual,
  BO_LessThan,
  BO_GreaterEqual,
  BO_GreaterThan,
  BO_ShiftLeft,
  BO_ShiftRight,
  BO_Add,
  BO_Subtract,
  BO_Multiply,
  BO_Divide,
  BO_Modulo,
  BO_BitOr,
  BO_BitAnd,
  BO_BitXor,
  BO_LogicalAnd,
  BO_LogicalOr,
};

const char* binary_op_to_str(enum BinaryOp op);

enum UnaryOp {
  UO_Minus,
  UO_BitNot,
  UO_LogicalNot,
};

const char* unary_op_to_str(enum UnaryOp op);

// Type of AST node.
enum AstType {
  AST_Program,
  AST_Block,
  AST_Struct,
  AST_Function,
  AST_If,
  AST_While,
  AST_For,
  AST_Let,
  AST_Require,
  AST_Yield,
  AST_Break,
  AST_Continue,
  AST_Return,
  AST_Member,
  AST_Index,
  AST_Assignment,
  AST_Binary,
  AST_Unary,
  AST_Call,
  AST_Self,
  AST_Varargs,
  AST_Array,
  AST_Set,
  AST_Dictionary,
  AST_String,
  AST_Identifier,
  AST_Float,
  AST_Integer,
  AST_True,
  AST_False,
  AST_Ellipsis,
  AST_Nil,
};

// Struct-based inheritance. There can be different types of AST nodes. Each
// node has this Base at the start for common properties.
struct Ast {
  enum AstType type; // Type of AST node
  size_t offset;     // Offset into the source code for the start of this node
};

// Growable array of AST pointers
struct AstVec {
  struct Ast** data; // Pointer to data
  size_t length;     // Current number of filled slots
  size_t capacity;   // Maximum allocated capacity
};

// Initialize an empty vector of AST nodes.
void ast_vec_init(struct AstVec* vec);

// Free memory for an AST vector
void ast_vec_fini(struct AstVec* vec);

// Push an AST node to the end of the vector
void ast_vec_push(struct AstVec* vec, struct Ast* ast);

// AST for the whole program
struct AstProgram {
  struct Ast ast;
  struct AstVec statements; // List of program statements
};

// Block statement
struct AstBlock {
  struct Ast ast;
  struct AstVec statements; // List of statements inside the block
  bool last_had_semicolon;  // Whether the last statement in the block had a semicolon
};

// Struct declaration
struct AstStruct {
  struct Ast ast;
  bool has_parent;       // Whether this struct has a parent struct (inheritance)
  struct Str opt_parent; // If `has_parent` is true, this is the parent
  struct Ast* body;      // Body of the struct declaration (block of member functions)
};

// Function declaration
struct AstFunction {
  struct Ast ast;
  struct AstVec parameters; // Function parameters (identifier or ellipsis)
  struct Ast* body;         // Block statement for the function body
};

// If statement
struct AstIf {
  struct Ast ast;
  struct Ast* condition; // if condition
  struct Ast* body;      // true block
  struct Ast* else_part; // Optional else block (can be NULL)
};

// While loop
struct AstWhile {
  struct Ast ast;
  struct Ast* condition; // Loop condition
  struct Ast* body;      // Loop body (block statement)
};

// For loop
struct AstFor {
  struct Ast ast;
  struct Str identifier; // The iterator
  struct Ast* generator; // Generator/iterator function
  struct Ast* body;      // Loop body
};

// "let" declarations
struct AstLet {
  struct Ast ast;
  bool public;         // Whether this is a public declaration
  struct Str variable; // Variable being assigned to
  struct Ast* rhs;     // Optional RHS (if NULL, variable is set to `nil`)
};

// "require" calls
struct AstRequire {
  struct Ast ast;
  struct Str module; // Module being "require"d
};

// Yield statement
struct AstYield {
  struct Ast ast;
  struct Ast* value; // Optional yield value (if NULL, yields `nil`)
};

// Break statement
struct AstBreak {
  struct Ast ast;
};

// Continue statement
struct AstContinue {
  struct Ast ast;
};

// Return statements
struct AstReturn {
  struct Ast ast;
  struct Ast* value; // Optional return value. If NULL, returns `nil`
};

// Struct member access
struct AstMember {
  struct Ast ast;
  struct Ast* lhs;   // Entity whose member we're accessing
  struct Str member; // Member name
};

// Indexing operation (array, dictionary, etc.)
struct AstIndex {
  struct Ast ast;
  struct Ast* lhs;   // Enity we're indexing
  struct Ast* index; // Index expression
};

// Assignment operation
struct AstAssignment {
  struct Ast ast;
  struct Ast* lhs;
  struct Ast* rhs;
};

// Binary operation
struct AstBinary {
  struct Ast ast;
  enum BinaryOp operation;
  struct Ast* lhs;
  struct Ast* rhs;
};

// Unary operation
struct AstUnary {
  struct Ast ast;
  enum UnaryOp operation;
  struct Ast* rhs;
};

// Function call
struct AstCall {
  struct Ast ast;
  struct Ast* function;    // Entity we're calling
  struct AstVec arguments; // Function arguments for the call
};

// "self" reference
struct AstSelf {
  struct Ast ast;
};

// "varargs" variable to refer to variable arguments
struct AstVarargs {
  struct Ast ast;
};

// Array literal
struct AstArray {
  struct Ast ast;
  struct AstVec elements;
};

// Set literal
struct AstSet {
  struct Ast ast;
  struct AstVec elements;
};

// Key-value pair of AST nodes
struct AstPair {
  struct Ast* key;
  struct Ast* value;
};

// Growable array of key-value AST pairs
struct AstPairVec {
  struct AstPair* data; // Pointer to data
  size_t length;        // Current number of filled slots
  size_t capacity;      // Maximum allocated capacity
};

// Initialize an empty vector of pairs of AST nodes.
void ast_pair_vec_init(struct AstPairVec* vec);

// Free memory for an AST pair vector
void ast_pair_vec_fini(struct AstPairVec* vec);

// Push a pair of key-value nodes to the end of the vector
void ast_pair_vec_push(struct AstPairVec* vec, struct Ast* key, struct Ast* value);

// Dictionary literal
struct AstDictionary {
  struct Ast ast;
  struct AstPairVec pairs; // Key-value pairs
};

// String literal
struct AstString {
  struct Ast ast;
  struct Str string;
};

// Identifier
struct AstIdentifier {
  struct Ast ast;
  struct Str identifier;
};

// Floating point number
struct AstFloat {
  struct Ast ast;
  double f;
};

// Integer number
struct AstInteger {
  struct Ast ast;
  int64_t i;
};

// "true" keyword
struct AstTrue {
  struct Ast ast;
};

// "false" keyword
struct AstFalse {
  struct Ast ast;
};

// "..."
struct AstEllipsis {
  struct Ast ast;
};

// "nil" keyword
struct AstNil {
  struct Ast ast;
};

// Functions to create (allocate) and return different ASTs
struct Ast* ast_program_create(size_t offset, struct AstVec statements);
struct Ast* ast_block_create(size_t offset, struct AstVec statements, bool last_had_semicolon);
struct Ast* ast_struct_create(size_t offset, bool has_parent, struct Str opt_parent, struct Ast* body);
struct Ast* ast_function_create(size_t offset, struct AstVec parameters, struct Ast* body);
struct Ast* ast_if_create(size_t offset, struct Ast* condition, struct Ast* body, struct Ast* else_part);
struct Ast* ast_while_create(size_t offset, struct Ast* condition, struct Ast* body);
struct Ast* ast_for_create(size_t offset, struct Str identifier, struct Ast* generator, struct Ast* body);
struct Ast* ast_let_create(size_t offset, bool public, struct Str variable, struct Ast* rhs);
struct Ast* ast_require_create(size_t offset, struct Str module);
struct Ast* ast_yield_create(size_t offset, struct Ast* value);
struct Ast* ast_break_create(size_t offset);
struct Ast* ast_continue_create(size_t offset);
struct Ast* ast_return_create(size_t offset, struct Ast* value);
struct Ast* ast_member_create(size_t offset, struct Ast* lhs, struct Str member);
struct Ast* ast_index_create(size_t offset, struct Ast* lhs, struct Ast* index);
struct Ast* ast_assignment_create(size_t offset, struct Ast* lhs, struct Ast* rhs);
struct Ast* ast_binary_create(size_t offset, enum BinaryOp operation, struct Ast* lhs, struct Ast* rhs);
struct Ast* ast_unary_create(size_t offset, enum UnaryOp operation, struct Ast* rhs);
struct Ast* ast_call_create(size_t offset, struct Ast* function, struct AstVec arguments);
struct Ast* ast_self_create(size_t offset);
struct Ast* ast_varargs_create(size_t offset);
struct Ast* ast_array_create(size_t offset, struct AstVec elements);
struct Ast* ast_set_create(size_t offset, struct AstVec elements);
struct Ast* ast_dictionary_create(size_t offset, struct AstPairVec kvpairs);
struct Ast* ast_string_create(size_t offset, struct Str str);
struct Ast* ast_identifier_create(size_t offset, struct Str str);
struct Ast* ast_float_create(size_t offset, double f);
struct Ast* ast_integer_create(size_t offset, int64_t i);
struct Ast* ast_true_create(size_t offset);
struct Ast* ast_false_create(size_t offset);
struct Ast* ast_ellipsis_create(size_t offset);
struct Ast* ast_nil_create(size_t offset);

// Print AST recursively
void ast_print(const struct Ast* ast, size_t indent);

// Free root AST node. This internally figures out which type of node it is, and
// recursively frees all child nodes.
void ast_free(struct Ast* ast);

#endif  // __BS_AST_H__
