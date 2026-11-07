# Step 3.4: Immediate Evaluation and Milestone 1 Review

In [Step 3.3](03-precedence-climbing.md), our parser assembled mathematically accurate syntax trees.  Now let us cash in that tree structure right away by adding a `double eval() const` method to `ExprAST`, turning Kaleidoscope into an interactive calculator in pure C++.

---

## Tree-Walking Evaluation

Executing a program by recursively calling a method on each AST node is called a **tree-walking interpreter**.  Values flow **bottom-up** from the leaves to the root:

```text
                (+)  ======> returns 14.0
               /   \
              /     \
    (Number 4)       (*)  ==> returns 10.0
     returns 4.0    /   \
                   /     \
         (Number 5)       (Number 2)
          returns 5.0      returns 2.0
```

Leaf `NumberExprAST` nodes simply return their stored `double`, while each `BinaryExprAST` recursively evaluates its left and right children and applies its operator.

---

## Incremental Code Changes

Open `kaleidoscope.cpp` and apply three updates on top of [Step 3.3](03-precedence-climbing.md).

### 1. Add Pure Virtual `eval()` to `ExprAST`

Add `virtual double eval() const = 0;` right below `print()` on `ExprAST`:

```cpp
class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual void print(std::ostream &OS) const = 0;
  virtual double eval() const = 0;
};
```

### 2. Implement `eval()` Across the Four AST Classes (With a Guided Stretch!)

Add `eval()` overrides to each concrete AST subclass:

1.  **`NumberExprAST`**: Return `Val` directly:
```cpp
  double eval() const override { return Val; }
```

2.  **`VariableExprAST`**: Variables need function parameter scopes, which arrive in Section 4.  Print a clear diagnostic naming the variable and return `0.0`:
```cpp
  double eval() const override {
    fprintf(stderr,
            "Error: variables require function scopes (coming in Section 4): "
            "%s\n",
            Name.c_str());
    return 0.0;
  }
```

3.  **`BinaryExprAST` (Your Stretch!)**: Here is the `eval()` method with `'+'` and `'-'` filled in:
```cpp
  double eval() const override {
    double Left = LHS->eval();
    double Right = RHS->eval();
    switch (Op) {
    case '+':
      return Left + Right;
    case '-':
      return Left - Right;
    // Your stretch: implement '*', '/', and '<' (returning 1.0 or 0.0)!
    default:
      fprintf(stderr, "Error: unknown binary operator: %c\n", Op);
      return 0.0;
    }
  }
```

**Your Stretch**: Fill in the three remaining `case` arms inside `BinaryExprAST::eval()`:
* `case '*':` returns `Left * Right;`.
* `case '/':` returns `Left / Right; // Follows IEEE-754 floating-point semantics (inf/nan)`.
* `case '<':` returns `Left < Right ? 1.0 : 0.0;` (since Kaleidoscope represents booleans as `1.0` for true and `0.0` for false!).

:::{note}
**IEEE-754 Floating-Point Division by Zero**:
Why do we not need a special `if (Right == 0.0)` guard before `Left / Right`?  Unlike C++ *integer* division by zero (which is undefined behaviour and traps with `SIGFPE` on x86-64), IEEE-754 `double` division by zero is completely well-defined in hardware: `1.0 / 0.0` evaluates to `+inf`, `-1.0 / 0.0` evaluates to `-inf`, and `0.0 / 0.0` evaluates to `nan`!  Best of all, LLVM's `fdiv` instruction in Section 4 and our JIT in Section 5 follow these exact same IEEE-754 rules.
:::

4.  **`CallExprAST`**: Function calls will be compiled to LLVM IR in Section 4 and executed by our JIT in Section 5.  Print a helpful message naming `Callee` and return `0.0`:
```cpp
  double eval() const override {
    fprintf(stderr,
            "Error: function calls will be compiled by LLVM in Section 4/5: "
            "%s\n",
            Callee.c_str());
    return 0.0;
  }
```

### 3. Print Evaluated Results in `MainLoop()` and Update `main()`

Inside `MainLoop()`, add one line right after printing the AST so both the tree and its computed value appear together:

```cpp
      if (auto AST = ParseExpression()) {
        std::cout << "Parsed AST: ";
        AST->print(std::cout);
        std::cout << "\n";
        std::cout << "Evaluated:  " << AST->eval() << "\n";
      } else {
        // Skip tokens for error recovery until the next semicolon.
        while (CurTok != ';' && CurTok != tok_eof)
          getNextToken();
      }
```

And update the first `printf` line in `main()`:

```cpp
  printf("Kaleidoscope Interactive Calculator (Step 3.4)\n");
```

---

## Verifying Your Code

Compare your `kaleidoscope.cpp` against the complete Milestone 1 reference file:

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
  virtual double eval() const = 0;
};

class NumberExprAST : public ExprAST {
  double Val;

public:
  explicit NumberExprAST(double Val) : Val(Val) {}

  void print(std::ostream &OS) const override {
    OS << "(Number " << Val << ")";
  }

  double eval() const override { return Val; }
};

class VariableExprAST : public ExprAST {
  std::string Name;

public:
  explicit VariableExprAST(std::string Name) : Name(std::move(Name)) {}

  void print(std::ostream &OS) const override { OS << "(Var " << Name << ")"; }

  double eval() const override {
    fprintf(stderr,
            "Error: variables require function scopes (coming in Section 4): "
            "%s\n",
            Name.c_str());
    return 0.0;
  }
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

  double eval() const override {
    double Left = LHS->eval();
    double Right = RHS->eval();
    switch (Op) {
    case '+':
      return Left + Right;
    case '-':
      return Left - Right;
    case '*':
      return Left * Right;
    case '/':
      return Left /
             Right; // Follows IEEE-754 floating-point semantics (inf/nan)
    case '<':
      return Left < Right ? 1.0 : 0.0;
    default:
      fprintf(stderr, "Error: unknown binary operator: %c\n", Op);
      return 0.0;
    }
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

  double eval() const override {
    fprintf(stderr,
            "Error: function calls will be compiled by LLVM in Section 4/5: "
            "%s\n",
            Callee.c_str());
    return 0.0;
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
        std::cout << "Evaluated:  " << AST->eval() << "\n";
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

  printf("Kaleidoscope Interactive Calculator (Step 3.4)\n");
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

Run `./kaleidoscope` and put your calculator through its paces, including `<` comparisons and `1 / 0`:

```bash
./kaleidoscope
```

```text
Kaleidoscope Interactive Calculator (Step 3.4)
Type math expressions followed by ';' and press Enter (Ctrl+D to exit):

ready> 4 + 5 * 2;
Parsed AST: (+ (Number 4) (* (Number 5) (Number 2)))
Evaluated:  14
ready> (4 + 5) * 2;
Parsed AST: (* (+ (Number 4) (Number 5)) (Number 2))
Evaluated:  18
ready> 10 / 2 - 3;
Parsed AST: (- (/ (Number 10) (Number 2)) (Number 3))
Evaluated:  2
ready> 3 < 5;
Parsed AST: (< (Number 3) (Number 5))
Evaluated:  1
ready> 1 / 0;
Parsed AST: (/ (Number 1) (Number 0))
Evaluated:  inf
ready>
```

---

## Milestone 1 Retrospective: A Complete Pure-C++ Frontend

Congratulations!  You have completed **Milestone 1** in roughly 350 lines of clean, dependency-free C++:
1.  **Lexer ([Section 1](../01-lexer/index.md))**: Scans raw characters into tokens while discarding whitespace and `#` comments.
2.  **AST ([Section 2](../02-parser-ast/index.md))**: Owns expression trees safely via `std::unique_ptr<ExprAST>`.
3.  **Parser ([Section 2](../02-parser-ast/index.md) and [Section 3](index.md))**: Combines recursive descent for primary atoms with table-driven precedence climbing for binary operators.
4.  **Tree-Walking Evaluator**: Computes arithmetic and comparisons directly on the AST.

### Looking Ahead to Section 4: From Tree Walking to LLVM IR

Keep your single `kaleidoscope.cpp` file right where it is: we will continue evolving this exact file without splitting anything out into separate headers!  In [Section 4: Generating LLVM IR](../04-llvm-ir/index.md), we will swap our toy `print()` and `eval()` methods on `ExprAST` for `llvm::Value *codegen()`, add `def` functions and `extern` prototypes to the parser, and watch LLVM emit Static Single Assignment (SSA) Intermediate Representation for your code.

---

[Previous: Step 3.3 - Precedence Climbing](03-precedence-climbing.md) | [Section 3 Overview](index.md) | [Next: Section 4 - Generating LLVM IR](../04-llvm-ir/index.md)
