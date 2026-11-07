# Step 3.1: The Binary AST Node

Welcome to Section 3!  In this step, you will create `BinaryExprAST` to represent binary infix expressions like `1 + 2` or `x * 2.0`, and wire up a deliberately naive left-to-right loop so you can see *why* operator precedence matters.

---

## Representing Binary Expressions

Every binary expression has three parts:
1.  An operator character (`Op`), such as `'+'` or `'*'`.
2.  A left-hand side expression (`LHS`).
3.  A right-hand side expression (`RHS`).

Because `LHS` and `RHS` are both `std::unique_ptr<ExprAST>`, either child can itself be another `BinaryExprAST`.  That recursive structure lets our AST represent expression trees of arbitrary depth.  Compare the two ways a parser might group `1 + 2 * 3`:

```text
  Naive Left-to-Right (WRONG: 9)         Arithmetic Precedence (RIGHT: 7)

            (*)                                        (+)
           /   \                                      /   \
         (+)   (3)                                  (1)   (*)
        /   \                                            /   \
      (1)   (2)                                        (2)   (3)
```

Let us build `BinaryExprAST` first, then run the naive loop so we can catch the left-hand tree red-handed in our terminal!

---

## Stretch: Authoring `BinaryExprAST` Yourself

You have already built `NumberExprAST`, `VariableExprAST` ([Step 2.3](../02-parser-ast/03-variables.md)), and `CallExprAST` ([Step 2.4](../02-parser-ast/04-function-calls.md)).  Use that exact same pattern to write `BinaryExprAST` directly below `VariableExprAST` (and above `CallExprAST`) in `kaleidoscope.cpp`:
* Inherit publicly from `ExprAST`.
* Store private members `char Op;` and `std::unique_ptr<ExprAST> LHS, RHS;`.
* Write a constructor `BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS, std::unique_ptr<ExprAST> RHS)` that moves `LHS` and `RHS` into place.
* Override `print(std::ostream &OS) const override` to stream `"(" << Op << " "`, call `LHS->print(OS)`, stream `" "`, call `RHS->print(OS)`, and close with `")"`.

Try writing `BinaryExprAST` in your editor before opening the fold below!

<details>
<summary>Click here to check your BinaryExprAST class</summary>

```cpp
class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

  void print(std::ostream &OS) const override {
    OS << "(" << Op << " ";
    LHS->print(OS);
    OS << " ";
    RHS->print(OS);
    OS << ")";
  }
};
```
</details>

---

## Incremental Code Changes: A Naive Left-to-Right Loop

To test `BinaryExprAST` right away, replace the one-line `ParseExpression()` from [Step 2.1](../02-parser-ast/01-first-ast-node.md) with a flat `while` loop that folds `+`, `-`, `*`, and `/` strictly from left to right:

```cpp
// Temporary flat left-to-right scan (ignoring operator precedence for now!).
static std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParsePrimary();
  if (!LHS)
    return nullptr;

  while (CurTok == '+' || CurTok == '-' || CurTok == '*' || CurTok == '/') {
    char Op = static_cast<char>(CurTok);
    getNextToken(); // eat operator
    auto RHS = ParsePrimary();
    if (!RHS)
      return nullptr;
    LHS = std::make_unique<BinaryExprAST>(Op, std::move(LHS), std::move(RHS));
  }
  return LHS;
}
```

Also update the two `printf` lines in `main()` so the banner reflects Step 3.1:

```cpp
  printf("Kaleidoscope AST Inspector (Step 3.1: Binary AST Nodes)\n");
  printf("Type math expressions followed by ';' and press Enter (Ctrl+D to "
         "exit):\n\n");
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

class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

  void print(std::ostream &OS) const override {
    OS << "(" << Op << " ";
    LHS->print(OS);
    OS << " ";
    RHS->print(OS);
    OS << ")";
  }
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

// Temporary flat left-to-right scan (ignoring operator precedence for now!).
static std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParsePrimary();
  if (!LHS)
    return nullptr;

  while (CurTok == '+' || CurTok == '-' || CurTok == '*' || CurTok == '/') {
    char Op = static_cast<char>(CurTok);
    getNextToken(); // eat operator
    auto RHS = ParsePrimary();
    if (!RHS)
      return nullptr;
    LHS = std::make_unique<BinaryExprAST>(Op, std::move(LHS), std::move(RHS));
  }
  return LHS;
}

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
  printf("Kaleidoscope AST Inspector (Step 3.1: Binary AST Nodes)\n");
  printf("Type math expressions followed by ';' and press Enter (Ctrl+D to "
         "exit):\n\n");

  Prompt();
  getNextToken();
  MainLoop();
  return 0;
}
```
</details>

---

## Compiling and Watching the Precedence Bug!

Recompile:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra kaleidoscope.cpp -o kaleidoscope
```

Run `./kaleidoscope` and type `1 + 2;` followed by `1 + 2 * 3;`:

```bash
./kaleidoscope
```

```text
Kaleidoscope AST Inspector (Step 3.1: Binary AST Nodes)
Type math expressions followed by ';' and press Enter (Ctrl+D to exit):

ready> 1 + 2;
Parsed AST: (+ (Number 1) (Number 2))
ready> 1 + 2 * 3;
Parsed AST: (* (+ (Number 1) (Number 2)) (Number 3))
ready>
```

Look closely at that second line:
* We typed `1 + 2 * 3;`, which in mathematics means `1 + (2 * 3) = 7`.
* Our flat `while` loop eagerly wrapped `1 + 2` into `(+ (Number 1) (Number 2))` before it ever looked at `*`, producing `(* (+ (Number 1) (Number 2)) (Number 3))` (`(1 + 2) * 3 = 9`)!

To know whether `2` belongs to `+` on its left or `*` on its right, the parser needs to compare the **binding strength** (precedence) of both operators.  In [Step 3.2](02-precedence-table.md), we will build a data-driven operator precedence table, and in [Step 3.3](03-precedence-climbing.md), we will fix this tree for good!

---

[Previous: Step 2.4 - Function Calls](../02-parser-ast/04-function-calls.md) | [Section 3 Overview](index.md) | [Next: Step 3.2 - The Precedence Table](02-precedence-table.md)
