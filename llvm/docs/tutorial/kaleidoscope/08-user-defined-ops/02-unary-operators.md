# Step 8.2: Unary Operators

Now that prototypes can declare unary operators, your parser and code generator must be taught how to parse and evaluate unary prefix expressions like `!x` or `-(a + b)`.

---

## 1. Defining `UnaryExprAST`

Add `UnaryExprAST` to your AST hierarchy:

```cpp
class UnaryExprAST : public ExprAST {
  char Opcode;
  std::unique_ptr<ExprAST> Operand;

public:
  UnaryExprAST(char Opcode, std::unique_ptr<ExprAST> Operand)
      : Opcode(Opcode), Operand(std::move(Operand)) {}

  llvm::Value *codegen() override;
};
```

---

## 2. Parsing Unary Expressions

In traditional grammars, unary prefix operators bind tighter than binary operators, but looser than primary expressions like parentheses or function calls.

Add `ParseUnary()` right below `ParsePrimary()`:

```cpp
static std::unique_ptr<ExprAST> ParseUnary() {
  // If the current token is not an operator, it must be a primary expression.
  if (!isascii(CurTok) || CurTok == '(' || CurTok == ',')
    return ParsePrimary();

  // If this is a unary operator, read it.
  int Opc = CurTok;
  getNextToken();
  if (auto Operand = ParseUnary())
    return std::make_unique<UnaryExprAST>(Opc, std::move(Operand));
  return nullptr;
}
```

Notice that `ParseUnary()` calls itself recursively!  This allows chaining unary operators naturally, such as `!!x` or `!-x`.

Update `ParseBinOpRHS()` and `ParseExpression()` to parse unary expressions on the left-hand and right-hand sides instead of primary expressions:

```cpp
static std::unique_ptr<ExprAST> ParseBinOpRHS(int ExprPrec,
                                              std::unique_ptr<ExprAST> LHS) {
  while (true) {
    int TokPrec = GetTokPrecedence();
    if (TokPrec < ExprPrec)
      return LHS;

    int BinOp = CurTok;
    getNextToken();

    // Parse RHS as a unary expression instead of ParsePrimary().
    auto RHS = ParseUnary();
    if (!RHS)
      return nullptr;

    int NextPrec = GetTokPrecedence();
    if (TokPrec < NextPrec) {
      RHS = ParseBinOpRHS(TokPrec + 1, std::move(RHS));
      if (!RHS)
        return nullptr;
    }

    LHS =
        std::make_unique<BinaryExprAST>(BinOp, std::move(LHS), std::move(RHS));
  }
}

static std::unique_ptr<ExprAST> ParseExpression() {
  // Parse LHS as a unary expression instead of ParsePrimary().
  auto LHS = ParseUnary();
  if (!LHS)
    return nullptr;

  return ParseBinOpRHS(0, std::move(LHS));
}
```

---

## 3. Emitting IR for Unary Operators

Generating code for a unary operator is remarkably elegant: look up the user's function named `unary<op>` and emit a call to it:

```cpp
llvm::Value *UnaryExprAST::codegen() {
  llvm::Value *OperandV = Operand->codegen();
  if (!OperandV)
    return nullptr;

  llvm::Function *F = getFunction(std::string("unary") + Opcode);
  if (!F)
    return LogErrorV("Unknown unary operator");

  return Builder->CreateCall(F, OperandV, "unop");
}
```

---

Next: [Step 8.3: User Binary Operators](03-binary-operators.md) - Register dynamic precedence and emit user binary operators.
