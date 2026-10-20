# Step 2.3: Variables

In this step, we add support for variable references, such as `x` or `total`.

---

## The VariableExprAST Node

When an expression references a variable name, the AST needs to capture that identifier.

Add the `VariableExprAST` class to `kaleidoscope.cpp`:

```cpp
/// VariableExprAST - Expression class for referencing a variable, like "a".
class VariableExprAST : public ExprAST {
  std::string Name;

public:
  explicit VariableExprAST(std::string Name) : Name(std::move(Name)) {}

  void print(std::ostream &OS) const override {
    OS << "(Var " << Name << ")";
  }
};
```

---

## Parsing Identifiers

Add `ParseIdentifierExpr()` and wire it into `ParsePrimary()`:

```cpp
/// identifierexpr ::= identifier
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;
  getNextToken(); // eat identifier
  return std::make_unique<VariableExprAST>(IdName);
}

static std::unique_ptr<ExprAST> ParsePrimary() {
  switch (CurTok) {
  case tok_identifier:
    return ParseIdentifierExpr();
  case tok_number:
    return ParseNumberExpr();
  case '(':
    return ParseParenExpr();
  default:
    return LogError("unknown token when expecting an expression");
  }
}
```

Notice what `ParseIdentifierExpr()` does: it captures the name from `IdentifierStr`, consumes the token with `getNextToken()`, and returns a `VariableExprAST`.

---

## Compiling and Running

Recompile:

```bash
clang++ -std=c++17 -Wall -Wextra kaleidoscope.cpp -o kaleidoscope
```

Run the binary:

```bash
./kaleidoscope
```

Type variable names and parenthesised variables:

```text
Kaleidoscope AST Inspector (Step 2.3: Variables)
Type expressions and press Enter (Ctrl+D to exit):

ready> x;
Parsed AST: (Var x)
ready> (my_variable);
Parsed AST: (Var my_variable)
```

In [Step 2.4](04-function-calls.md), we will extend `ParseIdentifierExpr()` to recognise function calls like `foo(x, 10)`.
