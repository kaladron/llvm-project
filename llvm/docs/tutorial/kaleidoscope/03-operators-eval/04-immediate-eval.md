# Step 3.4: Immediate Evaluation and Active Learning Challenge

In this step, we add direct mathematical evaluation to our AST, turning our parser into an interactive calculator in pure C++.

---

## Adding the eval() Method

Add a virtual `eval()` method to `ExprAST`:

```cpp
class ExprAST {
public:
  virtual ~ExprAST() = default;
  virtual void print(std::ostream &OS) const = 0;
  virtual double eval() const = 0;
};
```

Implement `eval()` on `NumberExprAST` and `BinaryExprAST`:

```cpp
class NumberExprAST : public ExprAST {
  double Val;

public:
  explicit NumberExprAST(double Val) : Val(Val) {}
  void print(std::ostream &OS) const override { OS << Val; }
  double eval() const override { return Val; }
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

Notice what `BinaryExprAST::eval()` does: it recursively computes the left and right values, applies the arithmetic operator, and returns the result.  For comparison operators like `<`, it returns `1.0` for true and `0.0` for false.

---

## Updating the REPL Loop

Update `MainLoop()` to display the evaluated result:

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

Type expressions and watch your calculator evaluate them in real time:

```text
Kaleidoscope Interactive Calculator (Step 3.4)
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

You now have a fully functioning arithmetic calculator with proper operator precedence running directly in C++.

---

## Active Learning Challenge: Exponentiation Operator

Most programming languages provide an exponentiation operator (`^` or `**`) to raise numbers to a power.  In mathematics, exponentiation has higher precedence than multiplication.

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

Here are the required changes:

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

## Next Steps

Congratulations!  You have built a complete lexer, an AST, an operator precedence parser, and an interactive calculator.

In **Section 4: Generating LLVM Intermediate Representation**, we will bridge this AST to the LLVM framework for the first time, emitting real LLVM IR instructions for expressions using `LLVMContext` and `IRBuilder`.
