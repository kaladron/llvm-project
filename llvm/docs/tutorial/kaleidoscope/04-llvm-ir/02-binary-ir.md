# Step 4.2: Binary Math in IR

In this step, we generate LLVM instructions for mathematical operators (`+`, `-`, `*`, `/`, `<`).

---

## Floating-Point Instructions in LLVM

LLVM uses separate instructions for integer and floating-point arithmetic.  Because Kaleidoscope values are 64-bit doubles, we use LLVM floating-point builder methods:

- `Builder->CreateFAdd(L, R, "addtmp")`: Generates an `fadd` instruction.
- `Builder->CreateFSub(L, R, "subtmp")`: Generates an `fsub` instruction.
- `Builder->CreateFMul(L, R, "multmp")`: Generates an `fmul` instruction.
- `Builder->CreateFDiv(L, R, "divtmp")`: Generates an `fdiv` instruction.
- `Builder->CreateFCmpULT(L, R, "cmptmp")`: Performs an unordered less-than floating-point comparison, returning a 1-bit integer (`i1`).  We convert this `i1` back to a `double` (0.0 or 1.0) using `Builder->CreateUIToFP(...)`.

---

## Implementing BinaryExprAST::codegen()

Add the `codegen()` implementation to `BinaryExprAST` in `kaleidoscope.cpp`:

```cpp
class BinaryExprAST : public ExprAST {
  char Op;
  std::unique_ptr<ExprAST> LHS, RHS;

public:
  BinaryExprAST(char Op, std::unique_ptr<ExprAST> LHS,
                std::unique_ptr<ExprAST> RHS)
      : Op(Op), LHS(std::move(LHS)), RHS(std::move(RHS)) {}

  llvm::Value *codegen() override {
    llvm::Value *L = LHS->codegen();
    llvm::Value *R = RHS->codegen();
    if (!L || !R)
      return nullptr;

    switch (Op) {
    case '+':
      return Builder->CreateFAdd(L, R, "addtmp");
    case '-':
      return Builder->CreateFSub(L, R, "subtmp");
    case '*':
      return Builder->CreateFMul(L, R, "multmp");
    case '/':
      return Builder->CreateFDiv(L, R, "divtmp");
    case '<':
      L = Builder->CreateFCmpULT(L, R, "cmptmp");
      return Builder->CreateUIToFP(L, llvm::Type::getDoubleTy(*TheContext),
                                   "booltmp");
    default:
      fprintf(stderr, "invalid binary operator: %c\n", Op);
      return nullptr;
    }
  }
};
```

Update your driver loop to parse full expressions with `ParseExpression()` instead of only numbers.

---

## Compiling and Running

Compile:

```bash
clang++ -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core) -o kaleidoscope
```

Run the binary:

```bash
./kaleidoscope
```

Type a compound mathematical expression:

```text
Kaleidoscope LLVM IR Generator (Step 4.2: Binary Math)
Type math expressions followed by ';' and press Enter (Ctrl+D to exit):

ready> 1 + 2 * 3;
; ModuleID = 'Kaleidoscope'
source_filename = "Kaleidoscope"

define double @__anon_expr() {
entry:
  ret double 7.000000e+00
}
```

Notice what happened: because `1`, `2`, and `3` are all compile-time constants, LLVM's `IRBuilder` automatically performed **constant folding** during code generation, emitting `ret double 7.000000e+00` without generating runtime math instructions.

In [Step 4.3](03-functions-ir.md), we will introduce functions with parameters, where values are dynamic and full instructions are generated.
