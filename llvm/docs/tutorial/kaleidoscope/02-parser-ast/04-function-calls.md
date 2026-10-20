# Step 2.4: Function Calls and Active Learning Challenge

In this step, we add function call expressions like `sqrt(16)` or `add(a, b)`.

---

## Distinguishing Variables from Calls

Both variable references and function calls begin with an identifier token.  The parser distinguishes between the two by looking at the very next token:
- If the token following the identifier is `'('`, it is a function call.
- Otherwise, it is a simple variable reference.

---

## The CallExprAST Node

A function call needs to store two things: the name of the function being called (`Callee`), and a list of argument expressions (`Args`).

Add `CallExprAST` to `kaleidoscope.cpp`:

```cpp
/// CallExprAST - Expression class for function calls, like "foo(a, 2.0)".
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(std::string Callee, std::vector<std::unique_ptr<ExprAST>> Args)
      : Callee(std::move(Callee)), Args(std::move(Args)) {}

  void print(std::ostream &OS) const override {
    OS << "(Call " << Callee;
    for (const auto &Arg : Args) {
      OS << " ";
      Arg->print(OS);
    }
    OS << ")";
  }
};
```

Notice that `Args` stores expressions as `std::vector<std::unique_ptr<ExprAST>>`.  This allows each argument to be any valid expression, including nested function calls.

---

## Parsing Argument Lists

Update `ParseIdentifierExpr()` to handle function calls:

```cpp
/// identifierexpr
///   ::= identifier
///   ::= identifier '(' expression* ')'
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;
  getNextToken(); // eat identifier

  // Simple variable reference.
  if (CurTok != '(')
    return std::make_unique<VariableExprAST>(IdName);

  // Function call.
  getNextToken(); // eat '('
  std::vector<std::unique_ptr<ExprAST>> Args;
  if (CurTok != ')') {
    while (true) {
      if (auto Arg = ParseExpression())
        Args.push_back(std::move(Arg));
      else
        return nullptr;

      if (CurTok == ')')
        break;

      if (CurTok != ',')
        return LogError("expected ')' or ',' in argument list");
      getNextToken(); // eat ','
    }
  }

  getNextToken(); // eat ')'
  return std::make_unique<CallExprAST>(IdName, std::move(Args));
}
```

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

Type function calls with zero, one, or multiple arguments:

```text
Kaleidoscope AST Inspector (Step 2.4: Function Calls)
Type expressions and press Enter (Ctrl+D to exit):

ready> foo(x, 10);
Parsed AST: (Call foo (Var x) (Number 10))
ready> sqrt(16);
Parsed AST: (Call sqrt (Number 16))
ready> run();
Parsed AST: (Call run)
```

The parser successfully differentiates variables from calls, handles nested parentheses, and pretty-prints multi-argument function calls.

---

## Active Learning Challenge: Indented Tree Printing

S-expressions are concise, but nested trees are often easier to read as an indented outline.

### The Challenge

Update the `print()` method on `ExprAST` to accept an optional `int Indent = 0` parameter:
```cpp
virtual void print(std::ostream &OS, int Indent = 0) const = 0;
```

Update each AST class so that child nodes are printed on new lines indented by two spaces per level.  When you type `foo(x, (42));`, your binary should print:

```text
Call: foo
  Var: x
  Number: 42
```

Give this a try before expanding the solution fold below.

<details>
<summary>Click here to reveal the solution</summary>

Here are the required changes:

1.  Update `ExprAST`:
```cpp
class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual void print(std::ostream &OS, int Indent = 0) const = 0;
};
```

2.  Update `NumberExprAST`:
```cpp
  void print(std::ostream &OS, int Indent = 0) const override {
    OS << std::string(Indent, ' ') << "Number: " << Val << "\n";
  }
```

3.  Update `VariableExprAST`:
```cpp
  void print(std::ostream &OS, int Indent = 0) const override {
    OS << std::string(Indent, ' ') << "Var: " << Name << "\n";
  }
```

4.  Update `CallExprAST`:
```cpp
  void print(std::ostream &OS, int Indent = 0) const override {
    OS << std::string(Indent, ' ') << "Call: " << Callee << "\n";
    for (const auto &Arg : Args)
      Arg->print(OS, Indent + 2);
  }
```
</details>

---

## Next Steps

You now have a complete AST for primary expressions and an interactive tree inspector.

In **Section 3: Operator Precedence and The Calculator**, we will implement operator precedence climbing to parse expressions like `1 + 2 * 3` and evaluate mathematical results directly in C++.
