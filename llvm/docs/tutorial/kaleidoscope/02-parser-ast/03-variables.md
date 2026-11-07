# Step 2.3: Variables

In [Step 1.4](../01-lexer/04-identifiers-keywords.md), our lexer learned to scan identifiers like `x` and `my_variable` into `IdentifierStr` and return `tok_identifier`.  In this step, we represent variable references in our AST and wire `tok_identifier` into the parser.

---

## Representing Variables in the AST

When an expression mentions a variable name, the AST stores that identifier string so later compiler phases can look it up in a symbol table:

```text
+-------------------+   Lexer    +-------------------------------------+   Parser   +-----------------------+
| Source Text: "x"  | ---------> | tok_identifier (IdentifierStr = "x")| ---------> | VariableExprAST("x")  |
+-------------------+            +-------------------------------------+            +-----------------------+
```

---

## Stretch: Authoring `VariableExprAST` Yourself

First, include `<utility>` right after `<string>` at the top of `kaleidoscope.cpp` so we can move `std::string` values efficiently with `std::move`:

```cpp
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
```

Now recall `NumberExprAST` from [Step 2.1](01-first-ast-node.md):

```cpp
class NumberExprAST : public ExprAST {
  double Val;

public:
  explicit NumberExprAST(double Val) : Val(Val) {}

  void print(std::ostream &OS) const override {
    OS << "(Number " << Val << ")";
  }
};
```

`VariableExprAST` follows the exact same shape, storing a `std::string Name` instead of a `double Val`!

**Your Stretch**: Write `class VariableExprAST` directly below `NumberExprAST` in `kaleidoscope.cpp`:
* Inherit publicly from `ExprAST`.
* Store a private `std::string Name;` member.
* Write an `explicit VariableExprAST(std::string Name)` constructor that initialises `Name(std::move(Name))`.
* Override `void print(std::ostream &OS) const override` to stream `"(Var " << Name << ")"`.

Type it out yourself before peeking at the fold below!

<details>
<summary>Click here to check your VariableExprAST class</summary>

```cpp
class VariableExprAST : public ExprAST {
  std::string Name;

public:
  explicit VariableExprAST(std::string Name) : Name(std::move(Name)) {}

  void print(std::ostream &OS) const override { OS << "(Var " << Name << ")"; }
};
```
</details>

---

## Incremental Code Changes: Parsing Identifiers

With `VariableExprAST` in place, update the parser and `main()` banner.

### 1. Add `ParseIdentifierExpr()`

Directly below `ParseParenExpr()`, add `ParseIdentifierExpr()`:

```cpp
static std::unique_ptr<ExprAST> ParseIdentifierExpr() {
  std::string IdName = IdentifierStr;
  getNextToken(); // eat identifier
  return std::make_unique<VariableExprAST>(std::move(IdName));
}
```

Why copy `IdentifierStr` into a local `IdName` *before* calling `getNextToken()`?  Because `getNextToken()` immediately calls `gettok()` to read the *next* token, and if that next token also happens to be a word, `gettok()` would overwrite the global `IdentifierStr`!  Capturing `IdName` first keeps our variable's name safe, and `std::move(IdName)` transfers the string buffer into `VariableExprAST` without a second heap allocation.

### 2. Add `tok_identifier` to `ParsePrimary()` and Update `main()`

In `ParsePrimary()`, add `case tok_identifier:` at the top of the `switch`:

```cpp
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

And update the first `printf` line in `main()` to `Step 2.3: Variables`:

```cpp
  printf("Kaleidoscope AST Inspector (Step 2.3: Variables)\n");
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
  return std::make_unique<VariableExprAST>(std::move(IdName));
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
  printf("Kaleidoscope AST Inspector (Step 2.3: Variables)\n");
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

Run `./kaleidoscope` and test bare and parenthesised variable names:

```bash
./kaleidoscope
```

```text
Kaleidoscope AST Inspector (Step 2.3: Variables)
Type expressions followed by ';' and press Enter (Ctrl+D to exit):

ready> x;
Parsed AST: (Var x)
ready> (my_variable);
Parsed AST: (Var my_variable)
ready>
```

Notice how `(my_variable);` combines your parenthesis parser from [Step 2.2](02-parentheses.md) with your brand-new `VariableExprAST` without a single extra line of glue code!

In [Step 2.4](04-function-calls.md), we will extend `ParseIdentifierExpr()` to tell simple variables (`x`) apart from function calls (`foo(x, 10)`).

---

[Previous: Step 2.2 - Parentheses and Grouping](02-parentheses.md) | [Section 2 Overview](index.md) | [Next: Step 2.4 - Function Calls](04-function-calls.md)
