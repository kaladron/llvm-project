# Step 3.3: Precedence Climbing

In [Step 3.1](01-binary-ast.md) and [Step 3.2](02-precedence-table.md), we saw why a flat loop mis-parses `1 + 2 * 3`: as soon as it reads `1`, `+`, and `2`, it rushes to build `(+ 1 2)` without peeking at what comes *after* `2`.

**Operator Precedence Climbing** fixes that with one elegant rule: before folding `(LHS BinOp RHS)` together, peek at the *next* operator (`NextPrec`).  If `NextPrec` binds more tightly than `BinOp` (`TokPrec < NextPrec`), let the tighter operator steal `RHS` as *its* left-hand side first!

---

## Tracing Precedence Climbing on `1 + 2 * 3 + 4`

Think of any expression as a leading primary atom followed by zero or more `(binop, primary)` pairs:

```text
expression ::= primary binoprhs
binoprhs   ::= (binop primary)*
```

Watch `ParseBinOpRHS(ExprPrec, LHS)` trace `1 + 2 * 3 + 4`:

```text
ParseExpression()
  LHS = (Number 1)
  |
  +--> ParseBinOpRHS(ExprPrec = 0, LHS = 1)
         1. Sees '+' (TokPrec = 20 >= 0).  Eats '+', parses RHS = (Number 2).
         2. Peeks ahead: sees '*' (NextPrec = 40).
            20 < 40!  '*' binds tighter than '+', so climb!
            |
            +--> ParseBinOpRHS(ExprPrec = 21, LHS = 2)
                   3. Sees '*' (TokPrec = 40 >= 21).  Eats '*', parses RHS = (Number 3).
                   4. Peeks ahead: sees '+' (NextPrec = 20).
                      40 < 20 is FALSE.  Fold LHS = (* 2 3).
                   5. Next loop iteration: sees '+' (TokPrec = 20 < ExprPrec 21).
                      Returns (* 2 3) to outer call!
            |
         6. Outer call receives RHS = (* 2 3), folds LHS = (+ 1 (* 2 3)).
         7. Next loop iteration: sees '+' (TokPrec = 20 >= 0).
            Eats '+', parses RHS = (Number 4), peeks ';' (NextPrec = -1),
            folds LHS = (+ (+ 1 (* 2 3)) 4), and returns on ';'!
```

Notice why step 2 passes `TokPrec + 1` (`21`) instead of `TokPrec` (`20`):
* Any operator strictly tighter than `+` (such as `*` at `40`) satisfies `40 >= 21` and gets absorbed into `RHS`.
* Another `+` or `-` at the *same* precedence (`20`) fails `20 >= 21`, forcing the recursive call to return so equal-precedence operators (`10 - 4 - 2`) associate **left-to-right** as `(- (- 10 4) 2)` instead of `(- 10 (- 4 2))`!

---

## Incremental Code Changes

Open `kaleidoscope.cpp` and replace our temporary flat loop from [Step 3.2](02-precedence-table.md) with `ParseBinOpRHS()` and the final five-line `ParseExpression()`.

### 1. Add `ParseBinOpRHS()` and Update `ParseExpression()`

Directly below `ParsePrimary()`, replace the Step 3.2 `ParseExpression()` function with `ParseBinOpRHS()` and the updated `ParseExpression()`:

```cpp
/// binoprhs ::= (binop primary)*
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
  while (true) {
    int TokPrec = GetTokPrecedence();

    if (TokPrec < ExprPrec)
      return LHS;

    int BinOp = CurTok;
    getNextToken(); // eat binary operator

    auto RHS = ParsePrimary();
    if (!RHS)
      return nullptr;

    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }

    LHS = std::make_unique<BinaryExprAST>(static_cast<char>(BinOp),
                                          std::move(LHS), std::move(RHS));
  }
}

static std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParsePrimary();
  if (!LHS)
    return nullptr;

  return ParseBinOpRHS(0, std::move(LHS));
}
```

Check what happens when an expression has *no* binary operators at all, such as `42;`:
1.  `ParseExpression()` parses `LHS = (Number 42)`, leaving `CurTok` on `';'`.
2.  `ParseBinOpRHS(0, LHS)` calls `GetTokPrecedence()`, which returns `-1` for `';'`.
3.  `-1 < 0` (`TokPrec < ExprPrec`) is immediately true, so `ParseBinOpRHS` returns `(Number 42)` untouched!

### 2. Update the `main()` Banner

Update the first `printf` line in `main()` to `Step 3.3: Precedence Climbing`:

```cpp
  printf("Kaleidoscope AST Inspector (Step 3.3: Precedence Climbing)\n");
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
#include <map>
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

static std::map<char, int> BinopPrecedence;

static int GetTokPrecedence() {
  if (!isascii(CurTok))
    return -1;

  int TokPrec = BinopPrecedence[static_cast<char>(CurTok)];
  if (TokPrec <= 0)
    return -1;
  return TokPrec;
}

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

/// binoprhs ::= (binop primary)*
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
  while (true) {
    int TokPrec = GetTokPrecedence();

    if (TokPrec < ExprPrec)
      return LHS;

    int BinOp = CurTok;
    getNextToken(); // eat binary operator

    auto RHS = ParsePrimary();
    if (!RHS)
      return nullptr;

    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }

    LHS = std::make_unique<BinaryExprAST>(static_cast<char>(BinOp),
                                          std::move(LHS), std::move(RHS));
  }
}

static std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParsePrimary();
  if (!LHS)
    return nullptr;

  return ParseBinOpRHS(0, std::move(LHS));
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
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;
  BinopPrecedence['/'] = 40;

  printf("Kaleidoscope AST Inspector (Step 3.3: Precedence Climbing)\n");
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

## Compiling and Running

Recompile:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra kaleidoscope.cpp -o kaleidoscope
```

Run `./kaleidoscope` and test mixed precedence, parenthesised overrides, and left-associative subtraction:

```bash
./kaleidoscope
```

```text
Kaleidoscope AST Inspector (Step 3.3: Precedence Climbing)
Type math expressions followed by ';' and press Enter (Ctrl+D to exit):

ready> 1 + 2 * 3;
Parsed AST: (+ (Number 1) (* (Number 2) (Number 3)))
ready> (1 + 2) * 3;
Parsed AST: (* (+ (Number 1) (Number 2)) (Number 3))
ready> 10 - 4 - 2;
Parsed AST: (- (- (Number 10) (Number 4)) (Number 2))
ready>
```

Look at all three trees:
* `1 + 2 * 3;` is now `(+ (Number 1) (* (Number 2) (Number 3)))`: the bug from [Step 3.1](01-binary-ast.md) is gone!
* `(1 + 2) * 3;` routes `(1 + 2)` through `ParseParenExpr()` from [Step 2.2](../02-parser-ast/02-parentheses.md), overriding `*` cleanly to build `(* (+ (Number 1) (Number 2)) (Number 3))`.
* `10 - 4 - 2;` groups left-to-right as `(- (- (Number 10) (Number 4)) (Number 2))` (`(10 - 4) - 2 = 4`, not `10 - (4 - 2) = 8`).

In [Step 3.4](04-immediate-eval.md), we will evaluate these trees directly in C++ so your REPL computes real numbers!

---

[Previous: Step 3.2 - The Precedence Table](02-precedence-table.md) | [Section 3 Overview](index.md) | [Next: Step 3.4 - Immediate Evaluation](04-immediate-eval.md)
