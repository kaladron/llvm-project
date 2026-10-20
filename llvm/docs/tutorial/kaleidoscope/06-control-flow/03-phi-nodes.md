# Step 6.3: SSA PHI Nodes

In traditional imperative languages, when two branches compute a result, programmers often write to a mutable variable:

```c
// C style:
double result;
if (cond)
  result = 10.0;
else
  result = 20.0;
// use result
```

In LLVM Intermediate Representation, values are in **Static Single Assignment (SSA)** form.  In pure SSA form, there are no mutable variables.  Every register is assigned exactly once.  Once `%val1` is produced, its value can never be modified.

How, then, can code in the merge block know whether to use the result from the `then` branch or the `else` branch?

---

## The PHI Node

LLVM solves this with a special instruction called a **PHI node** (`llvm::PHINode`).

A PHI node selects a value dynamically at runtime based on **which basic block control flowed from** immediately before reaching the current block:

```llvm
ifcont:
  %iftmp = phi double [ %then_val, %then ], [ %else_val, %else ]
```

If the program branched from `%then`, `%iftmp` receives `%then_val`.  If the program branched from `%else`, `%iftmp` receives `%else_val`.

---

## 1. Emitting the PHI Node

In `kaleidoscope.cpp`, replace the stub `IfExprAST::codegen()` with the full implementation:

```cpp
llvm::Value *IfExprAST::codegen() {
  llvm::Value *CondV = Cond->codegen();
  if (!CondV)
    return nullptr;

  // Convert condition to a boolean by comparing non-equal to 0.0.
  CondV = Builder->CreateFCmpONE(
      CondV, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)), "ifcond");

  llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

  // Create blocks for the then and else cases.
  llvm::BasicBlock *ThenBB =
      llvm::BasicBlock::Create(*TheContext, "then", TheFunction);
  llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(*TheContext, "else");
  llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*TheContext, "ifcont");

  Builder->CreateCondBr(CondV, ThenBB, ElseBB);

  // Emit 'then' block.
  Builder->SetInsertPoint(ThenBB);
  llvm::Value *ThenV = Then->codegen();
  if (!ThenV)
    return nullptr;

  Builder->CreateBr(MergeBB);
  ThenBB = Builder->GetInsertBlock();

  // Emit 'else' block.
  TheFunction->insert(TheFunction->end(), ElseBB);
  Builder->SetInsertPoint(ElseBB);

  llvm::Value *ElseV = Else->codegen();
  if (!ElseV)
    return nullptr;

  Builder->CreateBr(MergeBB);
  ElseBB = Builder->GetInsertBlock();

  // Emit 'merge' block.
  TheFunction->insert(TheFunction->end(), MergeBB);
  Builder->SetInsertPoint(MergeBB);

  llvm::PHINode *PN =
      Builder->CreatePHI(llvm::Type::getDoubleTy(*TheContext), 2, "iftmp");

  PN->addIncoming(ThenV, ThenBB);
  PN->addIncoming(ElseV, ElseBB);
  return PN;
}
```

---

## 2. Compile and Test

Compile your compiler:

```bash
clang++ -Iinclude -Wall -Wextra kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native) -o kaleidoscope
```

Launch `./kaleidoscope` and inspect the generated IR:

```
ready> def choose(c x y) if c then x else y;
Parsed a function definition.
define double @choose(double %c, double %x, double %y) {
entry:
  %ifcond = fcmp one double %c, 0.000000e+00
  br i1 %ifcond, label %then, label %else

then:                                             ; preds = %entry
  br label %ifcont

else:                                             ; preds = %entry
  br label %ifcont

ifcont:                                           ; preds = %else, %then
  %iftmp = phi double [ %x, %then ], [ %y, %else ]
  ret double %iftmp
}
```

Notice how clean the emitted IR is.  The PHI node seamlessly merges the two paths into a single unified SSA value.

---

Next: [Step 6.4: Recursive Fibonacci in the JIT](04-fibonacci-jit.md) - Execute recursive control flow in your interactive JIT REPL.
