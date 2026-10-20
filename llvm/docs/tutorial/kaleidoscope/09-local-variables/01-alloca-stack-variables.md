# Step 9.1: Stack Allocation with 'alloca'

In pure SSA form, register values are immutable.  However, machine architectures have memory (the stack and heap), and memory is mutable.

LLVM provides the `alloca` instruction to allocate memory on the execution stack of the current function.  Memory allocated with `alloca` is automatically freed when the function returns.

By storing mutable variables into stack locations:
* Assignment becomes a `store` instruction.
* Reading a variable becomes a `load` instruction.

Your compiler does not need to worry about SSA rules or PHI nodes when generating code for mutable variables.

---

## 1. Creating Allocas in the Entry Block

A critical rule in LLVM: **always create alloca instructions in the function's entry basic block**.

Allocas placed in the entry block are allocated once when the function is entered.  Allocas placed inside loops or branches reallocate stack memory every iteration, leading to stack overflow.  Placing allocas in the entry block also allows LLVM's optimiser to recognise and eliminate them easily.

Add this helper function to `kaleidoscope.cpp`:

```cpp
static llvm::AllocaInst *CreateEntryBlockAlloca(llvm::Function *TheFunction,
                                                llvm::StringRef VarName) {
  llvm::IRBuilder<> TmpB(&TheFunction->getEntryBlock(),
                         TheFunction->getEntryBlock().begin());
  return TmpB.CreateAlloca(llvm::Type::getDoubleTy(*TheContext), nullptr,
                           VarName);
}
```

Notice that `TmpB` positions itself at the very beginning of the entry block, guaranteeing the allocation happens before any other instructions.

---

## 2. Updating `NamedValues` and Function Arguments

Previously, `NamedValues` mapped variable names directly to `llvm::Value*`.  Change `NamedValues` to map to stack allocations (`llvm::AllocaInst*`):

```cpp
static std::map<std::string, llvm::AllocaInst *> NamedValues;
```

In `FunctionAST::codegen()`, create an alloca for every function argument in the entry block, store the incoming argument value into it, and store the alloca pointer in `NamedValues`:

```cpp
  NamedValues.clear();
  for (auto &Arg : TheFunction->args()) {
    llvm::AllocaInst *Alloca =
        CreateEntryBlockAlloca(TheFunction, Arg.getName());
    Builder->CreateStore(&Arg, Alloca);
    NamedValues[std::string(Arg.getName())] = Alloca;
  }
```

---

## 3. Reading Variables with `load`

Update `VariableExprAST::codegen()` to read the value from its stack alloca with `CreateLoad`:

```cpp
llvm::Value *VariableExprAST::codegen() {
  llvm::AllocaInst *A = NamedValues[Name];
  if (!A)
    return LogErrorV("unknown variable name");
  return Builder->CreateLoad(llvm::Type::getDoubleTy(*TheContext), A,
                             Name.c_str());
}
```

Now, every variable reference in your language loads its current value from stack memory.

---

Next: [Step 9.2: Local Variables and Assignment](02-var-in-expressions.md) - Implement `var / in` syntax and the assignment operator `=`.
