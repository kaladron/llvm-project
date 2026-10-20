# Step 9.2: Local Variables and Assignment

Now that your compiler manages stack allocations with `alloca`, we can introduce two key language features:
1.  Declaring local variables with `var / in`.
2.  Mutating variables using the assignment operator `=`.

---

## 1. Syntax for `var / in` and Assignment

A `var` expression declares one or more local variables, optionally initialises them, and scopes them to a body expression:

```kaleidoscope
var a = 1, b = 2 in
  a + b
```

The assignment operator `=` updates an existing variable:

```kaleidoscope
var a = 1 in (
  a = a + 10 :
  a
)
```

---

## 2. Defining `VarExprAST`

Add `VarExprAST` to represent local variable declarations:

```cpp
class VarExprAST : public ExprAST {
  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;
  std::unique_ptr<ExprAST> Body;

public:
  VarExprAST(
      std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames,
      std::unique_ptr<ExprAST> Body)
      : VarNames(std::move(VarNames)), Body(std::move(Body)) {}

  llvm::Value *codegen() override;
};
```

---

## 3. Parsing `var` Expressions

Add `tok_var = -13` to `Token`, and teach `gettok()` to recognise `"var"`.

Write `ParseVarExpr()`:

```cpp
static std::unique_ptr<ExprAST> ParseVarExpr() {
  getNextToken(); // eat 'var'

  std::vector<std::pair<std::string, std::unique_ptr<ExprAST>>> VarNames;

  if (CurTok != tok_identifier)
    return LogError("expected identifier after var");

  while (true) {
    std::string Name = IdentifierStr;
    getNextToken(); // eat identifier

    std::unique_ptr<ExprAST> Init = nullptr;
    if (CurTok == '=') {
      getNextToken(); // eat '='
      Init = ParseExpression();
      if (!Init)
        return nullptr;
    }

    VarNames.push_back(std::make_pair(Name, std::move(Init)));

    if (CurTok != ',')
      break;
    getNextToken(); // eat ','

    if (CurTok != tok_identifier)
      return LogError("expected identifier list after var");
  }

  if (CurTok != tok_in)
    return LogError("expected 'in' keyword after 'var'");
  getNextToken(); // eat 'in'

  auto Body = ParseExpression();
  if (!Body)
    return nullptr;

  return std::make_unique<VarExprAST>(std::move(VarNames), std::move(Body));
}
```

Add `case tok_var: return ParseVarExpr();` to `ParsePrimary()`.

---

## 4. Emitting IR for `var / in` and Assignment

In `VarExprAST::codegen()`, create an alloca for each variable, store its initial value (defaulting to `0.0`), shadow any existing variable with the same name, emit the body, and restore previous bindings upon exit:

```cpp
llvm::Value *VarExprAST::codegen() {
  std::vector<llvm::AllocaInst *> OldBindings;
  llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

  for (unsigned i = 0, e = VarNames.size(); i != e; ++i) {
    const std::string &VarName = VarNames[i].first;
    ExprAST *Init = VarNames[i].second.get();

    llvm::Value *InitVal;
    if (Init) {
      InitVal = Init->codegen();
      if (!InitVal)
        return nullptr;
    } else {
      InitVal = llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0));
    }

    llvm::AllocaInst *Alloca = CreateEntryBlockAlloca(TheFunction, VarName);
    Builder->CreateStore(InitVal, Alloca);

    OldBindings.push_back(NamedValues[VarName]);
    NamedValues[VarName] = Alloca;
  }

  llvm::Value *BodyVal = Body->codegen();
  if (!BodyVal)
    return nullptr;

  for (unsigned i = 0, e = VarNames.size(); i != e; ++i)
    NamedValues[VarNames[i].first] = OldBindings[i];

  return BodyVal;
}
```

In `BinaryExprAST::codegen()`, implement `=`:

```cpp
  if (Op == '=') {
    VariableExprAST *LHSE = dynamic_cast<VariableExprAST *>(LHS.get());
    if (!LHSE)
      return LogErrorV("destination of '=' must be a variable");

    llvm::Value *Val = RHS->codegen();
    if (!Val)
      return nullptr;

    llvm::AllocaInst *Variable = NamedValues[LHSE->getName()];
    if (!Variable)
      return LogErrorV("unknown variable name");

    Builder->CreateStore(Val, Variable);
    return Val;
  }
```

In `main()`, set the lowest precedence for `=`:

```cpp
  BinopPrecedence['='] = 2;
```

---

Next: [Step 9.3: Automatic SSA Promotion with 'mem2reg'](03-mem2reg-optimization.md) - Turn stack memory back into SSA registers automatically.
