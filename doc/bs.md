# BaruaScript (bs)

## Language Examples

```
let bed = require("bed");
let git = require("git");

bed.add_key("SPC g", fn () { git.status(); })
```

```
let std = require("std");
let bed = require("bed");

for i in std.range(0, 10) {
  bed.message("i: {}", i);
}
```

```
let std = require("std");
let str = require("std.str");

pub fn my_concat(...) {
  let output = "";
  // Can access variable arguments with list varargs
  for i in std.range(0, varargs.len()) {
    // Can use + for string concatenation
    output = str.concat(output, varargs[i].to_string());
  }
}
```

```
struct Bed {
  fn __init__(self) {
    self.hooks = {}
  }

  fn add_hook(self, hook, function) {
    if not self.hooks.contains(hook) {
      self.hooks[hook] = [function];
    } else {
      self.hooks[hook].push(function);
    }
  }
}

let bed = Bed();

pub fn add_hook(hook, function) {
  bed.add_hook(hook, function);
}
```

## Grammar

```
statements : ( no_semicolon_statement | semicolon_statement ";" )* statement

statement : no_semicolon_statement | semicolon_statement

no_semicolon_statement :
    | function_declaration
    | struct_declaration
    | if_block
    | while_loop
    | for_in_loop

semicolon_statement :
    | assignment_statement
    | let_statement
    | return_statement
    | "break"
    | "continue"
    | expression

block_statement : "{" statements "}"

function_declaration : "pub"? "fn" identifier "(" parameters? ")" block_statement

struct_declaration :
    | "pub"? "struct" identifier ( ":" identifier )? "{" function_declaration* "}"

if_block : "if" expression block_statement else_block?

else_block :
    | "else" if_block
    | "else" block_statement

while_loop : "while" expression block_statement

for_loop : "for" identifier "in" expression block_statement

assignment_statement : lhs ( assign_op expression )?

assign_op : "=" | "+=" | "-=" | "*=" | "/=" | "%=" | "<<=" | ">>=" | "|=" | "&=" | "^="

let_statement : "pub"? "let" identifier ( "=" expression ) )?

require : "require" "(" string ")"

yield : "yield" "(" expression ")"

return_statement : "return" expression?

lhs :
    | identifier
    | primary "." identifier
    | primary "[" expression "]"

expression : disjunction

parameters :
    | "..."
    | identifier ( "," identifier )* ( "," "..." )?

disjunction : conjunction ( "or" conjunction )*

conjunction : inversion ( "and" inversion )*

inversion :
    | "not" inversion
    | comparison

comparison : bitwise_or (compare_op bitwise_or)*

compare_op : "==" | "!=" | "<=" | "<" | ">=" | ">"

bitwise_or :
    | bitwise_or "|" bitwise_xor
    | bitwise_xor

bitwise_xor :
    | bitwise_xor "^" bitwise_and
    | bitwise_and

bitwise_and :
    | bitwise_and "&" shift_expr
    | shift_expr

shift_expr :
    | shift_expr "<<" sum
    | shift_expr ">>" sum
    | sum

sum :
    | sum "+" term
    | sum "-" term
    | term

term :
    | term "*" factor
    | term "/" factor
    | term "%" factor
    | factor

factor :
    | "+" factor
    | "-" factor
    | "!" factor
    | primary

primary :
    | primary "." identifier
    | primary "(" expressions ")"
    | primary "[" expression "]"
    | atom

expressions : expression ("," expression)*

atom :
    | "nil"
    | "true"
    | "false"
    | integer
    | float
    | identifier
    | string
    | lambda
    | require
    | yield
    | if_block
    | "(" expression ")"
    | "[" expressions? "]"
    | "{" expressions? "}"
    | "{" kv_expressions? "}"

string : '"' ANYTHING '"'

lambda : "fn" "(" parameters? ")" block_statement

kv_expressions : kv_expression ( "," kv_expression )*

kv_expression : expression ":" expression

identifier : [a-za-z_] [a-za-z0-9_]*

float : [0-9]+ ( "." [0-9]+ )?

integer : [0-9]+
```
