# Step 2.4: Function Calls and Active Learning Challenge

In [Step 2.3](03-variables.md), every identifier became a `VariableExprAST`.  Now we add function call expressions like `sqrt(16)`, `foo(x, 10)`, and zero-argument calls like `run()`.

---

## Distinguishing Variables from Calls with Lookahead

Both a variable reference (`x`) and a function call (`foo(x, 10)`) begin with `tok_identifier`.  How does a recursive-descent parser tell them apart?  By consuming the identifier and inspecting `CurTok` (our one-token lookahead from [Step 2.1](01-first-ast-node.md)):
* If `CurTok != '('`, the identifier stands alone as a `VariableExprAST`.
* If `CurTok == '('`, it is a function call, so we parse a comma-separated list of argument expressions and build a `CallExprAST`:

```text
                +--------------------+
                | CallExprAST: "foo" |
                +--------------------+
                   /              \
                  v                v
    +---------------------+  +--------------------+
    | VariableExprAST "x" |  | NumberExprAST (10) |
    +---------------------+  +--------------------+
```

---

## Incremental Code Changes

Open `kaleidoscope.cpp` and make three targeted updates on top of [Step 2.3](03-variables.md).

### 1. Include `<vector>`

At the top of `kaleidoscope.cpp`, add `#include <vector>` right below `#include <utility>` so `CallExprAST` can hold a dynamic list of argument subtrees:

```cpp
#include <utility>
#include <vector>
```

### 2. Define `CallExprAST` (With a Quick `print()` Stretch!)

Directly below `VariableExprAST`, add `CallExprAST`.  It owns the `Callee` function name and a `std::vector<std::unique_ptr<ExprAST>> Args`:

```cpp
class CallExprAST : public ExprAST {
  std::string Callee;
  std::vector<std::unique_ptr<ExprAST>> Args;

public:
  CallExprAST(std::string Callee, std::vector<std::unique_ptr<ExprAST>> Args)
      : Callee(std::move(Callee)), Args(std::move(Args)) {}

  void print(std::ostream &OS) const override {
    OS << "(Call " << Callee;
    for (const auto &Arg : Args) {
      // Your stretch: print a space followed by Arg's S-expression!
    }
    OS << ")";
  }
};
```

**Your Stretch**: Fill in the two lines inside the `for` loop (`OS << " ";` and `Arg->print(OS);`) so `foo(x, 10)` prints `(Call foo (Var x) (Number 10))` and a zero-argument call `run()` prints `(Call run)`.

### 3. Upgrade `ParseIdentifierExpr()` and Update `main()`

Replace `ParseIdentifierExpr()` with the lookahead-aware version that handles both variables and comma-separated argument lists:

```cpp
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;
  getNextToken(); // eat identifier

  // Simple variable reference.
  if (CurTok != '(')
    return std::make_unique<VariableExprAST>(std::move(IdName));

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
  return std::make_unique<CallExprAST>(std::move(IdName), std::move(Args));
}
```

Trace how cleanly recursion handles nested calls like `foo(bar(x), 10)`:
* If `CurTok == ')'` right after `'('`, we skip the loop altogether and build a zero-argument `CallExprAST` (`run()`).
* Otherwise, each argument is parsed with `ParseExpression()`, which can itself be a number, parenthesised expression, variable, or another nested `CallExprAST`!
* After each argument, `CurTok` must be either `')'` (end of call) or `','` (more arguments follow).

Finally, update the banner in `main()` to `Step 2.4: Function Calls`:

```cpp
  printf("Kaleidoscope AST Inspector (Step 2.4: Function Calls)\n");
```

---

## Verifying Your Code

Compare your `kaleidoscope.cpp` against the complete reference file:

<details>
<summary>Click here to compare against the reference kaleidoscope.cpp</summary>

```cpp
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

//===----------------------------------------------------------------------===//
// Lexer
//===----------------------------------------------------------------------===//

enum Token {
  tok_eof = -1,

  // Commands / Keywords
  tok_def = -100,
  tok_extern = -101,

  // Primary
  tok_identifier = -200,
  tok_number = -201,
};

static std::string IdentifierStr; // Filled in if tok_identifier
static double NumVal;             // Filled in if tok_number

static int gettok() {
  static int LastChar = ' ';

  while (isspace(LastChar))
    LastChar = getchar();

  // Recognise identifiers and keywords: [a-zA-Z_][a-zA-Z0-9_]*
  if (isalpha(LastChar) || LastChar == '_') {
    IdentifierStr = LastChar;
    while (isalnum((LastChar = getchar())) || LastChar == '_')
      IdentifierStr += LastChar;

    if (IdentifierStr == "def")
      return tok_def;
    if (IdentifierStr == "extern")
      return tok_extern;
    return tok_identifier;
  }

  // Recognise numeric literals: [0-9.]+
  if (isdigit(LastChar) || LastChar == '.') {
    std::string NumStr;
    do {
      NumStr += LastChar;
      LastChar = getchar();
    } while (isdigit(LastChar) || LastChar == '.');

    NumVal = strtod(NumStr.c_str(), nullptr);
    return tok_number;
  }

  // Recognise comments: '#' through end of line.
  if (LastChar == '#') {
    do
      LastChar = getchar();
    while (LastChar != EOF && LastChar != '\n' && LastChar != '\r');

    if (LastChar != EOF)
      return gettok();
  }

  if (LastChar == EOF)
    return tok_eof;

  int ThisChar = LastChar;
  LastChar = getchar();
  return ThisChar;
}

//===----------------------------------------------------------------------===//
// Abstract Syntax Tree (AST)
//===----------------------------------------------------------------------===//

class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual void print(std::ostream &OS) const = 0;
};

class NumberExprAST : public ExprAST {
  double Val;

public:
  explicit NumberExprAST(double Val) : Val(Val) {}

  void print(std::ostream &OS) const override {
    OS << "(Number " << Val << ")";
  }
};

class VariableExprAST : public ExprAST {
  std::string Name;

public:
  explicit VariableExprAST(std::string Name) : Name(std::move(Name)) {}

  void print(std::ostream &OS) const override { OS << "(Var " << Name << ")"; }
};

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

//===----------------------------------------------------------------------===//
// Parser
//===----------------------------------------------------------------------===//

static int CurTok;
static int getNextToken() { return CurTok = gettok(); }

static std::unique_ptr<ExprAST> LogError(const char *Str) {
  fprintf(stderr, "Error: %s\n", Str);
  return nullptr;
}

static std::unique_ptr<ExprAST> ParseExpression();

static std::unique_ptr<ExprAST> ParseNumberExpr() {
  auto Result = std::make_unique<NumberExprAST>(NumVal);
  getNextToken(); // consume the number
  return Result;
}

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

static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;
  getNextToken(); // eat identifier

  // Simple variable reference.
  if (CurTok != '(')
    return std::make_unique<VariableExprAST>(std::move(IdName));

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
  return std::make_unique<CallExprAST>(std::move(IdName), std::move(Args));
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

static std::unique_ptr<ExprAST> ParseExpression() { return ParsePrimary(); }

//===----------------------------------------------------------------------===//
// Interactive Driver Loop
//===----------------------------------------------------------------------===//

static void Prompt() { fprintf(stderr, "ready> "); }

static void MainLoop() {
  while (true) {
    switch (CurTok) {
    case tok_eof:
      return;
    case ';':
      Prompt();
      getNextToken();
      break;
    default:
      if (auto AST = ParseExpression()) {
        std::cout << "Parsed AST: ";
        AST->print(std::cout);
        std::cout << "\n";
      } else {
        // Skip tokens for error recovery until the next semicolon.
        while (CurTok != ';' && CurTok != tok_eof)
          getNextToken();
      }
      break;
    }
  }
}

int main() {
  printf("Kaleidoscope AST Inspector (Step 2.4: Function Calls)\n");
  printf(
      "Type expressions followed by ';' and press Enter (Ctrl+D to exit):\n\n");

  Prompt();
  getNextToken();
  MainLoop();
  return 0;
}
```
</details>

---

## Compiling and Running

Recompile:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra kaleidoscope.cpp -o kaleidoscope
```

Run `./kaleidoscope` and test multi-argument, single-argument, and zero-argument calls:

```bash
./kaleidoscope
```

```text
Kaleidoscope AST Inspector (Step 2.4: Function Calls)
Type expressions followed by ';' and press Enter (Ctrl+D to exit):

ready> foo(x, 10);
Parsed AST: (Call foo (Var x) (Number 10))
ready> sqrt(16);
Parsed AST: (Call sqrt (Number 16))
ready> run();
Parsed AST: (Call run)
ready>
```

---

## Active Learning Challenge: Indented Tree Printing

Compact S-expressions fit nicely on a single line, while indented multi-line dumps make deep trees easy to scan visually.  Try writing an indented printer as a quick experiment!

### The Challenge

Temporarily add an optional `int Indent = 0` parameter to `ExprAST::print()`:

```cpp
virtual void print(std::ostream &OS, int Indent = 0) const = 0;
```

Update `NumberExprAST`, `VariableExprAST`, and `CallExprAST` so each child node prints on its own line indented by `Indent + 2` spaces.  When you type `foo(x, (42));`, your experiment can print:

```text
Call: foo
  Var: x
  Number: 42
```

Try coding this yourself before checking the fold below.

<details>
<summary>Click here to reveal the challenge solution</summary>

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

:::{important}
**Keep the Compact S-Expression `print()` Before Starting Section 3!**
Once you have tested your indented tree printer, **switch `print()` back to the compact single-line S-expression format** shown in the Step 2.4 reference fold above (`void print(std::ostream &OS) const`).  Section 3 adds `BinaryExprAST::print()` using that compact `(+ (Number 1) (Number 2))` format so you can spot operator-precedence bugs at a single glance!
:::

---

## Next Steps

Congratulations!  You now have a complete AST hierarchy and recursive-descent parser for all primary expressions in Kaleidoscope.

In [Section 3: Operator Precedence and Evaluation](../03-operators-eval/index.md), we will add binary infix operators (`+`, `-`, `*`, `/`, `<`), watch a naive left-to-right parser mis-group `1 + 2 * 3`, fix it with operator-precedence climbing, and turn Kaleidoscope into an interactive calculator!

---

[Previous: Step 2.3 - Variables](03-variables.md) | [Section 2 Overview](index.md) | [Next: Section 3 - Operator Precedence and Evaluation](../03-operators-eval/index.md)
