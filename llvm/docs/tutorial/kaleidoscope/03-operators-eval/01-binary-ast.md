# Step 3.1: The Binary AST Node

Welcome to Step 3.1.  In this step, we create an AST node to represent binary expressions like `a + b` or `x * 2.0`.

---

## The BinaryExprAST Class

A binary expression consists of three components:
1.  An operator character (`Op`), such as `'+'` or `'*'`.
2.  A left-hand side expression (`LHS`).
3.  A right-hand side expression (`RHS`).

Add `BinaryExprAST` to `kaleidoscope.cpp`:

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
};
```

Notice that `LHS` and `RHS` are `std::unique_ptr<ExprAST>`.  Because an expression node can hold another binary expression as one of its children, this simple recursive definition allows us to represent arbitrary mathematical expression trees.

---

## Compiling and Verifying

Recompile your binary:

```bash
clang++ -Wall -Wextra kaleidoscope.cpp -o kaleidoscope
```

In [Step 3.2](02-precedence-table.md), we will set up an operator precedence table to determine which operators bind first.
