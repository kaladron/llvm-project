# Chapter 3: Binary Operators and Direct AST Evaluation

Welcome to Chapter 3 of the Kaleidoscope tutorial.  In [Chapter 2](02-ast-and-parsing.md), you created an Abstract Syntax Tree that parses primary expressions, parentheses, variables, and function calls.

Now we need to parse binary operators like `+`, `-`, `*`, and `/`.

Parsing binary operators is notoriously tricky because mathematical operators have different precedence.  In the expression `1 + 2 * 3`, multiplication binds more tightly than addition.  The expression must be grouped as `1 + (2 * 3)`, not `(1 + 2) * 3`.

In this chapter, you will implement an elegant algorithm called **Operator Precedence Climbing**.  To give you an immediate win, you will also add an `eval()` method directly to the AST.  By the end of this chapter, your binary will function as an interactive mathematical calculator in pure C++, computing arithmetic results live in your terminal before we even touch LLVM.

---

## The Binary Expression AST Node

A binary expression consists of an operator character (like `'+'` or `'*'`) and two child subexpressions representing the left-hand side (LHS) and right-hand side (RHS).

Add the `BinaryExprAST` class to your AST definitions:

```cpp
/// BinaryExprAST - Expression class for binary operators like "a + b".
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
      if (Right == 0.0) {
        fprintf(stderr, "Warning: division by zero!\n");
        return 0.0;
      }
      return Left / Right;
    case '<':
      return Left < Right ? 1.0 : 0.0;
    default:
      fprintf(stderr, "Error: unknown binary operator: %c\n", Op);
      return 0.0;
    }
  }
};
```

Notice what `eval()` does: it recursively evaluates the left and right subtrees, applies the appropriate arithmetic operation, and returns the result.  For comparison operators like `<`, it returns `1.0` for true and `0.0` for false.

Also update `ExprAST` and `NumberExprAST` to declare and implement `eval()`:

```cpp
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
  void print(std::ostream &OS) const override { OS << Val; }
  double eval() const override { return Val; }
};
```

---

## Operator Precedence Climbing

To handle operator precedence without writing separate grammar rules for every operator, we use a precedence table.  Higher numbers mean tighter binding:

```cpp
/// BinopPrecedence - Precedence table for binary operators.
static std::map<char, int> BinopPrecedence;

/// GetTokPrecedence - Get the precedence of the pending binary operator token.
static int GetTokPrecedence() {
  if (!isascii(CurTok))
    return -1;

  int TokPrec = BinopPrecedence[static_cast<char>(CurTok)];
  if (TokPrec <= 0)
    return -1;
  return TokPrec;
}
```

In `main()`, we will configure the table:

```cpp
BinopPrecedence['<'] = 10;
BinopPrecedence['+'] = 20;
BinopPrecedence['-'] = 20;
BinopPrecedence['*'] = 40;
BinopPrecedence['/'] = 40;
```

Any character not in the map returns `-1`, signalling to the parser that the sequence of binary operators has ended.

### How Precedence Climbing Works

Consider parsing `a + b * c + d`:

1.  The parser reads the leading primary expression, `a`.
2.  It sees `+` (precedence 20), which is greater than the starting baseline precedence `0`.  It consumes `+` and parses primary `b`.
3.  Next, it looks ahead at `*` (precedence 40).  Because 40 is greater than 20, the right-hand side `b` binds more tightly to `*` than `a` binds to `+`.
4.  The parser recursively calls itself with minimum precedence 21 to parse `b * c` first.
5.  When it sees the second `+` (precedence 20), 20 is not greater than 21.  The recursive call returns `(b * c)` to the outer caller.
6.  The outer caller merges `a + (b * c)` and continues.

Here is the entire implementation in fourteen lines of C++:

```cpp
/// binoprhs
///   ::= ('+' primary)*
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
  while (true) {
    int TokPrec = GetTokPrecedence();

    // If this binary operator binds at least as tightly as the current operator,
    // consume it; otherwise we are done.
    if (TokPrec < ExprPrec)
      return LHS;

    int BinOp = CurTok;
    getNextToken(); // eat binary operator

    auto RHS = ParsePrimary();
    if (!RHS)
      return nullptr;

    // If BinOp binds less tightly with RHS than the operator after RHS, let
    // the pending operator take RHS as its LHS.
    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }

    // Merge LHS/RHS into a binary expression node.
    LHS = std::make_unique<BinaryExprAST>(static_cast<char>(BinOp),
                                          std::move(LHS), std::move(RHS));
  }
}

/// expression
///   ::= primary binoprhs
static std::unique_ptr<ExprAST> ParseExpression() {
  auto LHS = ParsePrimary();
  if (!LHS)
    return nullptr;

  return ParseBinOpRHS(0, std::move(LHS));
}
```

---

## Running the Calculator

Update `MainLoop()` to print both the parsed AST and the computed evaluation:

```cpp
static void MainLoop() {
  while (true) {
    fprintf(stderr, "ready> ");
    switch (CurTok) {
    case tok_eof:
      return;
    case ';':
      getNextToken();
      break;
    default:
      if (auto AST = ParseExpression()) {
        std::cout << "Parsed AST: ";
        AST->print(std::cout);
        std::cout << "\n";
        std::cout << "Evaluated:  " << AST->eval() << "\n";
      } else {
        getNextToken();
      }
      break;
    }
  }
}
```

### Compiling and Testing

Compile your updated parser:

```bash
clang++ -std=c++17 -Wall -Wextra evaluator.cpp -o evaluator
```

Run the binary:

```bash
./evaluator
```

Try typing mathematical expressions with mixed operators and parentheses:

```text
Kaleidoscope Calculator & AST Inspector (Chapter 3)
Type math expressions followed by ';' and press Enter (Ctrl+D to exit):

ready> 1 + 2 * 3;
Parsed AST: (+ 1 (* 2 3))
Evaluated:  7
ready> (1 + 2) * (10 - 4);
Parsed AST: (* (+ 1 2) (- 10 4))
Evaluated:  18
ready> 50 / 2 - 5;
Parsed AST: (- (/ 50 2) 5)
Evaluated:  20
ready> 10 < 20;
Parsed AST: (< 10 20)
Evaluated:  1
```

Look at the output for `1 + 2 * 3`.  The AST structure shows `(+ 1 (* 2 3))`, proving that the precedence climbing algorithm grouped multiplication before addition.  And the computed result is `7`, calculated right in front of you.

---

## Active Learning Challenge: Adding an Exponentiation Operator

Most programming languages use `^` or `**` for power or exponentiation.  In mathematics, exponentiation has higher precedence than multiplication and division.

### The Challenge

Add the `^` exponentiation operator to your calculator:
1.  Assign `^` a precedence of `50` in `BinopPrecedence`.
2.  Update `BinaryExprAST::eval()` to handle `'^'` using `<cmath>`'s `std::pow()`.

### Testing Your Changes

When you recompile and type `2 + 3 * 2 ^ 3;`, your calculator should group the expression as `2 + (3 * (2 ^ 3))` and evaluate to `26`:

```text
ready> 2 + 3 * 2 ^ 3;
Parsed AST: (+ 2 (* 3 (^ 2 3)))
Evaluated:  26
```

Try implementing this addition yourself before expanding the solution fold below.

<details>
<summary>Click here to reveal the solution</summary>

Here are the required additions:

1.  Include `<cmath>` at the top of your file:
```cpp
#include <cmath>
```

2.  Add `'^'` to `BinaryExprAST::eval()`:
```cpp
    case '^':
      return std::pow(Left, Right);
```

3.  Register the operator in `main()` with precedence `50`:
```cpp
  BinopPrecedence['^'] = 50;
```
</details>

---

## Complete Reference Code

The complete reference implementation for this chapter is available at [code/chapter03/evaluator.cpp](code/chapter03/evaluator.cpp).

In [Chapter 4](04-llvm-ir.md), we will connect our AST to LLVM for the first time.  You will use the LLVM IR builder to translate this AST into real LLVM Intermediate Representation instructions.
