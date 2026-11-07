# Step 3.2: The Precedence Table

In [Step 3.1](01-binary-ast.md), we hardcoded `CurTok == '+' || CurTok == '-' || ...` directly inside `ParseExpression()`, and every operator bound with the exact same left-to-right priority.

In classic recursive-descent compilers, people sometimes write a separate function per precedence tier (`ParseComparison()` calls `ParseAdditive()`, which calls `ParseMultiplicative()`, which calls `ParsePrimary()`).  That works for fixed grammars, but Kaleidoscope has an exciting surprise waiting in Section 8: **user-defined binary operators** whose precedence is chosen at runtime!

Instead of hardcoding grammar tiers, we store operator binding strengths in a `std::map<char, int>` lookup table.

---

## Choosing Precedence Numbers

In our table, larger positive integers mean **tighter binding**:

| Operator | Precedence | Associativity | Meaning |
| :--- | :--- | :--- | :--- |
| `<` | `10` | Left-to-right | Less-than comparison |
| `+`, `-` | `20` | Left-to-right | Addition and subtraction |
| `*`, `/` | `40` | Left-to-right | Multiplication and division |

Why leave gaps (`10`, `20`, `40`) instead of numbering them `1`, `2`, `3`?  For the exact same reason we left gaps in `enum Token` back in [Step 1.3](../01-lexer/03-numeric-literals.md)!  Leaving numeric room between tiers lets us (or a Kaleidoscope user in Section 8!) slot new operators like `=` (precedence `2`) or `^` (precedence `50`) in between existing operators without touching anything else.

---

## Incremental Code Changes

Open `kaleidoscope.cpp` and apply four small updates on top of [Step 3.1](01-binary-ast.md).

### 1. Include `<map>`

At the top of `kaleidoscope.cpp`, add `#include <map>` right after `#include <iostream>`:

```cpp
#include <iostream>
#include <map>
#include <memory>
```

### 2. Add `BinopPrecedence` and `GetTokPrecedence()`

In the `Parser` section, right below `getNextToken()` and above `LogError()`, define the precedence table and a helper that queries the precedence of `CurTok`:

```cpp
static std::map<char, int> BinopPrecedence;

static int GetTokPrecedence() {
  if (!isascii(CurTok))
    return -1;

  int TokPrec = BinopPrecedence[static_cast<char>(CurTok)];
  if (TokPrec <= 0)
    return -1;
  return TokPrec;
}
```

Check how `GetTokPrecedence()` handles every possible `CurTok` value:
* Multi-character tokens like `tok_eof` (`-1`), `tok_def` (`-100`), or `tok_number` (`-201`) fail `isascii(CurTok)` and immediately return `-1`.
* Punctuation that is *not* a registered binary operator (such as `';'`, `')'`, or `','`) looks up `0` in `BinopPrecedence`, triggers `TokPrec <= 0`, and returns `-1`.
* Registered binary operators (`<`, `+`, `-`, `*`, `/`) return their positive precedence (`10`, `20`, or `40`).

### 3. Drive `ParseExpression()` from `GetTokPrecedence()`

Update the `while` loop in `ParseExpression()` to use `GetTokPrecedence() > 0` instead of the hardcoded `CurTok == '+' || ...` list, and log each operator's precedence to `stderr` so we can inspect our table at work:

```cpp
// Recognises operators via GetTokPrecedence(), still grouping left-to-right.
static std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParsePrimary();
  if (!LHS)
    return nullptr;

  while (GetTokPrecedence() > 0) {
    char Op = static_cast<char>(CurTok);
    int Prec = GetTokPrecedence();
    fprintf(stderr, "  [Operator '%c' has precedence %d]\n", Op, Prec);
    getNextToken(); // eat operator
    auto RHS = ParsePrimary();
    if (!RHS)
      return nullptr;
    LHS = std::make_unique<BinaryExprAST>(Op, std::move(LHS), std::move(RHS));
  }
  return LHS;
}
```

### 4. Register the Initial Operators in `main()`

At the very start of `main()`, populate `BinopPrecedence` (including `<` at precedence `10`!) and update the banner to Step 3.2:

```cpp
int main() {
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;
  BinopPrecedence['/'] = 40;

  printf("Kaleidoscope AST Inspector (Step 3.2: Precedence Table)\n");
  printf("Type math expressions followed by ';' and press Enter (Ctrl+D to "
         "exit):\n\n");

  Prompt();
  getNextToken();
  MainLoop();
  return 0;
}
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

// Recognises operators via GetTokPrecedence(), still grouping left-to-right.
static std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParsePrimary();
  if (!LHS)
    return nullptr;

  while (GetTokPrecedence() > 0) {
    char Op = static_cast<char>(CurTok);
    int Prec = GetTokPrecedence();
    fprintf(stderr, "  [Operator '%c' has precedence %d]\n", Op, Prec);
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
  BinopPrecedence['<'] = 10;
  BinopPrecedence['+'] = 20;
  BinopPrecedence['-'] = 20;
  BinopPrecedence['*'] = 40;
  BinopPrecedence['/'] = 40;

  printf("Kaleidoscope AST Inspector (Step 3.2: Precedence Table)\n");
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

Run `./kaleidoscope` and test `1 + 2 * 3;` and `3 < 5;`:

```bash
./kaleidoscope
```

```text
Kaleidoscope AST Inspector (Step 3.2: Precedence Table)
Type math expressions followed by ';' and press Enter (Ctrl+D to exit):

ready> 1 + 2 * 3;
  [Operator '+' has precedence 20]
  [Operator '*' has precedence 40]
Parsed AST: (* (+ (Number 1) (Number 2)) (Number 3))
ready> 3 < 5;
  [Operator '<' has precedence 10]
Parsed AST: (< (Number 3) (Number 5))
ready>
```

Look at those diagnostic lines: the parser *knows* `'+'` has precedence `20` and `'*'` has precedence `40` (and `<` works automatically simply because we added `BinopPrecedence['<'] = 10`!), yet the flat loop still built `(* (+ (Number 1) (Number 2)) (Number 3))`.

Now we have everything in place for the magic moment: in [Step 3.3](03-precedence-climbing.md), we will let `ParseBinOpRHS()` compare `20 < 40` and climb!

---

[Previous: Step 3.1 - The Binary AST Node](01-binary-ast.md) | [Section 3 Overview](index.md) | [Next: Step 3.3 - Precedence Climbing](03-precedence-climbing.md)
