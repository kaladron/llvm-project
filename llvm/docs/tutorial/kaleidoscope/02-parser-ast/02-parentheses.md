# Step 2.2: Parentheses and Grouping

In this step, we add support for parenthesised expressions, such as `(42)`.

---

## Why Parentheses Do Not Need an AST Node

Parentheses alter the grouping of tokens while parsing, but once the inner expression is assembled, the parentheses themselves carry no runtime semantic meaning.

Because of this, our parser simply eats the opening `'('`, recursively parses the subexpression inside, and verifies that a closing `')'` is present.  No new AST node is created.

---

## The Code Changes

Open `kaleidoscope.cpp` and add `ParseParenExpr()`:

```cpp
static std::unique_ptr<ExprAST> ParseExpression();

/// parenexpr ::= '(' expression ')'
static std::unique_ptr<ExprAST> ParseParenExpr() {
  getNextToken(); // eat '('
  auto V = ParseExpression();
  if (!V)
    return nullptr;

  if (CurTok != ')')
    return LogError("expected ')'");
  getNextToken(); // eat ')'
  return V;
}
```

Next, introduce a new helper function called `ParsePrimary()`.  In compiler terminology, a *primary expression* is an indivisible leaf unit of an expression - a number, a parenthesised subexpression, or a variable:

```cpp
/// primary
///   ::= numberexpr
///   ::= parenexpr
static std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  case tok_number:
    return ParseNumberExpr();
  case '(':
    return ParseParenExpr();
  default:
    return LogError("unknown token when expecting an expression");
  }
}

/// expression ::= primary
static std::unique_ptr<ExprAST> ParseExpression() {
  return ParsePrimary();
}
```

---

## Compiling and Running

Recompile:

```bash
clang++ -Wall -Wextra kaleidoscope.cpp -o kaleidoscope
```

Run the binary:

```bash
./kaleidoscope
```

Type nested parentheses or test missing closing brackets:

```text
Kaleidoscope AST Inspector (Step 2.2: Parentheses)
Type expressions and press Enter (Ctrl+D to exit):

ready> (42);
Parsed AST: (Number 42)
ready> (((123.45)));
Parsed AST: (Number 123.45)
ready> (42;
Error: expected ')'
```

The recursive-descent parser correctly unwinds nested grouping and catches missing parentheses.

In [Step 2.3](03-variables.md), we will add support for named variable references like `x` and `foo`.
