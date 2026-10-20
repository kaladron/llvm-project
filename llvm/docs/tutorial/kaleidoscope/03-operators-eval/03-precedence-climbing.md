# Step 3.3: Precedence Climbing

In this step, we implement the core parsing algorithm: **Operator Precedence Climbing**.

---

## How Precedence Climbing Works

Consider parsing `a + b * c + d`:

1.  The parser reads the leading primary expression, `a`.
2.  It sees `+` (precedence 20), which is greater than the starting baseline precedence `0`.  It consumes `+` and parses primary `b`.
3.  Next, it looks ahead at `*` (precedence 40).  Because 40 is greater than 20, the right-hand side `b` binds more tightly to `*` than `a` binds to `+`.
4.  The parser recursively calls itself with minimum precedence 21 to parse `b * c` first.
5.  When it sees the second `+` (precedence 20), 20 is not greater than 21.  The recursive call returns `(b * c)` to the outer caller.
6.  The outer caller merges `a + (b * c)` and continues.

---

## The Code Changes

Add `ParseBinOpRHS()` to `kaleidoscope.cpp` and update `ParseExpression()`:

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

## Compiling and Running

Recompile:

```bash
clang++ -Wall -Wextra kaleidoscope.cpp -o kaleidoscope
```

Run the binary:

```bash
./kaleidoscope
```

Type mathematical expressions with mixed operators and parentheses:

```text
Kaleidoscope AST Inspector (Step 3.3: Precedence Climbing)
Type math expressions followed by ';' and press Enter (Ctrl+D to exit):

ready> 1 + 2 * 3;
Parsed AST: (+ 1 (* 2 3))
ready> (1 + 2) * 3;
Parsed AST: (* (+ 1 2) 3)
ready> 10 - 4 - 2;
Parsed AST: (- (- 10 4) 2)
```

Look closely at `1 + 2 * 3`.  The tree is `(+ 1 (* 2 3))`.  Multiplication was grouped as the right child of addition, preserving standard arithmetic order of operations.

In [Step 3.4](04-immediate-eval.md), we will add direct AST evaluation so that your terminal prints calculated numeric results immediately.
