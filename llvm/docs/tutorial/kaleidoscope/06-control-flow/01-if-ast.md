# Step 6.1: Lexing and Parsing 'if / then / else'

In many traditional languages like C and C++, `if` is a statement.  Statements perform side effects and produce no value.  In functional languages, and in Kaleidoscope, `if` is an **expression**.  It computes a condition, evaluates one of two branches, and returns the result of that branch:

```kaleidoscope
if x < 3 then 1 else 2
```

Because `if` is an expression, it can appear anywhere a number or function call can appear, such as inside another expression:

```kaleidoscope
4 + (if x then 10 else 20) * 2
```

In this step, you will add `if`, `then`, and `else` to your lexer, create the AST node, and write the parser.

---

## 1. Updating the Lexer

Add three new tokens to your `Token` enum in `kaleidoscope.cpp`:

```cpp
enum Token {
  tok_eof = -1,
  tok_def = -2,
  tok_extern = -3,
  tok_identifier = -4,
  tok_number = -5,
  tok_if = -6,
  tok_then = -7,
  tok_else = -8,
};
```

Next, teach `gettok()` to recognise these keyword strings when scanning identifiers:

```cpp
    if (IdentifierStr == "def")
      return tok_def;
    if (IdentifierStr == "extern")
      return tok_extern;
    if (IdentifierStr == "if")
      return tok_if;
    if (IdentifierStr == "then")
      return tok_then;
    if (IdentifierStr == "else")
      return tok_else;
    return tok_identifier;
```

---

## 2. Defining the AST Node

An `if` expression needs three pieces of data: the condition expression, the "then" expression, and the "else" expression.  Add `IfExprAST` right after `CallExprAST`:

```cpp
class IfExprAST : public ExprAST {
  std::unique_ptr<ExprAST> Cond, Then, Else;

public:
  IfExprAST(std::unique_ptr<ExprAST> Cond, std::unique_ptr<ExprAST> Then,
            std::unique_ptr<ExprAST> Else)
      : Cond(std::move(Cond)), Then(std::move(Then)), Else(std::move(Else)) {}

  llvm::Value *codegen() override;
};
```

For now, provide a stub for `codegen()` so your code compiles:

```cpp
llvm::Value *IfExprAST::codegen() {
  fprintf(stderr, "Parsed an if/then/else AST node!\n");
  return llvm::ConstantFP::get(*TheContext, llvm::APFloat(1.0));
}
```

---

## 3. Parsing the Expression

Parsing `if / then / else` is simple because the grammar is strictly delimited.  Add `ParseIfExpr()`:

```cpp
static std::unique_ptr<ExprAST> ParseIfExpr() {
  getNextToken(); // eat the 'if'

  auto Cond = ParseExpression();
  if (!Cond)
    return nullptr;

  if (CurTok != tok_then)
    return LogError("expected 'then'");
  getNextToken(); // eat the 'then'

  auto Then = ParseExpression();
  if (!Then)
    return nullptr;

  if (CurTok != tok_else)
    return LogError("expected 'else'");
  getNextToken(); // eat the 'else'

  auto Else = ParseExpression();
  if (!Else)
    return nullptr;

  return std::make_unique<IfExprAST>(std::move(Cond), std::move(Then),
                                     std::move(Else));
}
```

Finally, hook `ParseIfExpr()` into `ParsePrimary()` so `if` expressions are recognised whenever a primary expression is expected:

```cpp
static std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  case tok_identifier:
    return ParseIdentifierExpr();
  case tok_number:
    return ParseNumberExpr();
  case '(':
    return ParseParenExpr();
  case tok_if:
    return ParseIfExpr();
  default:
    return LogError("unknown token when expecting an expression");
  }
}
```

---

## 4. Compile and Test

Compile your updated file:

```bash
clang++ -Iinclude -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -o kaleidoscope
```

Launch `./kaleidoscope` and enter a conditional expression:

```
ready> if 1 then 2 else 3;
Parsed an if/then/else AST node!
Evaluated to 1.000000
ready>
```

Your parser cleanly captures the conditional structure.

---

## Active Learning Challenge

<details>
<summary><b>Challenge: Handling Missing 'else'</b></summary>

What happens in your parser if you enter `if 1 then 2;` without an `else` branch?  Why must an expression-based language require both branches?

<br>

**Explanation**:
In statement-based languages like C, an `if` statement without an `else` simply does nothing if the condition is false.  However, in an expression-based language, every expression must produce a concrete value of a specific type.  If the condition were false and there were no `else` expression, what value would the expression evaluate to?  Requiring `else` guarantees that the expression always yields a well-defined value.

</details>

---

Next: [Step 6.2: Basic Blocks and Branching](02-basic-blocks-branching.md) - Learn how LLVM represents control flow using basic blocks.
