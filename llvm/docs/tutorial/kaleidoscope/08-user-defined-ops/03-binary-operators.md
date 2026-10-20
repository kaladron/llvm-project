# Step 8.3: User Binary Operators

While unary operators are straightforward function calls, binary operators have an additional property: **precedence**.  When a programmer defines `binary> 10`, the parser's precedence climbing table must dynamically learn that `>` binds with precedence 10.

---

## 1. Registering Precedence in `FunctionAST::codegen()`

When a user defines a binary operator function, register its operator name and precedence into `BinopPrecedence`:

```cpp
llvm::Function *FunctionAST::codegen() {
  auto &P = *Proto;
  FunctionProtos[Proto->getName()] = std::move(Proto);
  llvm::Function *TheFunction = getFunction(P.getName());
  if (!TheFunction)
    return nullptr;

  // If this is a binary operator, install its precedence into the parser table.
  if (P.isBinaryOp())
    BinopPrecedence[P.getOperatorName()] = P.getBinaryPrecedence();

  llvm::BasicBlock *BB =
      llvm::BasicBlock::Create(*TheContext, "entry", TheFunction);
  Builder->SetInsertPoint(BB);

  NamedValues.clear();
  for (auto &Arg : TheFunction->args())
    NamedValues[std::string(Arg.getName())] = &Arg;

  if (llvm::Value *RetVal = Body->codegen()) {
    Builder->CreateRet(RetVal);
    llvm::verifyFunction(*TheFunction);
    return TheFunction;
  }

  // Failed to compile body: remove function and unregister precedence.
  TheFunction->eraseFromParent();
  if (P.isBinaryOp())
    BinopPrecedence.erase(P.getOperatorName());
  return nullptr;
}
```

Because `BinopPrecedence` is updated as soon as the function definition compiles, subsequent expressions parsed in the REPL immediately recognise the new operator and respect its precedence.

---

## 2. Emitting Calls to User Binary Operators

In `BinaryExprAST::codegen()`, if the operator is not one of the built-in arithmetic instructions, look up the corresponding `binary<op>` function and emit a call:

```cpp
llvm::Value *BinaryExprAST::codegen() {
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
    break;
  }

  // Not a built-in operator: look up the user-defined binary function.
  llvm::Function *F = getFunction(std::string("binary") + Op);
  if (!F)
    return LogErrorV("binary operator not found");

  llvm::Value *Ops[2] = {L, R};
  return Builder->CreateCall(F, Ops, "binop");
}
```

---

## 3. Compile and Test

Compile your compiler:

```bash
clang++ -rdynamic -Iinclude -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -o kaleidoscope
```

Launch `./kaleidoscope` and define custom operators:

```
ready> def unary!(v) if v then 0 else 1;
Parsed a function definition.

ready> !0;
Evaluated to 1.000000

ready> !5;
Evaluated to 0.000000

ready> def binary> 10 (LHS RHS) RHS < LHS;
Parsed a function definition.

ready> 10 > 5;
Evaluated to 1.000000

ready> 5 > 10;
Evaluated to 0.000000
```

---

Next: [Step 8.4: The Mandelbrot Set](04-mandelbrot.md) - Put everything together to render a fractal in your terminal.
