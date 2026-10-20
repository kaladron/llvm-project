# Step 7.2: Emitting Loop Basic Blocks and PHI Nodes

In an imperative language, a loop counter is a variable in memory updated on each iteration (`i++`).  In pure SSA form, how can a loop counter change its value when values cannot be reassigned?

The answer is once again the **PHI node**.  On the very first iteration, the counter receives the starting value from outside the loop (the preheader).  On all subsequent iterations, the counter receives the incremented value from the end of the previous iteration!

---

## The Loop Control Flow Structure

A standard SSA loop consists of three regions:

```
        [ Preheader ]
        (compute Start)
              |
              v
     +---> [ LoopBB ] <----------------------+
     |     (PHI: Start from Preheader,       |
     |           NextVar from LoopEnd)       |
     |     (compute Body)                    |
     |     (compute NextVar = Variable + Step)|
     |     (check EndCond)                   |
     |        /        \                     |
(true)-------+          +----(false)         |
                              |              |
                              v              |
                       [ AfterBB ]           |
                       (return 0.0)          |
```

---

## Implementing `ForExprAST::codegen()`

Replace your stub `ForExprAST::codegen()` in `kaleidoscope.cpp` with the full implementation:

```cpp
llvm::Value *ForExprAST::codegen() {
  // 1. Evaluate the start expression.
  llvm::Value *StartVal = Start->codegen();
  if (!StartVal)
    return nullptr;

  // 2. Set up basic blocks.
  llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();
  llvm::BasicBlock *PreheaderBB = Builder->GetInsertBlock();
  llvm::BasicBlock *LoopBB =
      llvm::BasicBlock::Create(*TheContext, "loop", TheFunction);

  // Jump from preheader into the loop header.
  Builder->CreateBr(LoopBB);
  Builder->SetInsertPoint(LoopBB);

  // 3. Create the PHI node for the loop induction variable.
  llvm::PHINode *Variable =
      Builder->CreatePHI(llvm::Type::getDoubleTy(*TheContext), 2, VarName);
  Variable->addIncoming(StartVal, PreheaderBB);

  // Shadow any existing variable with this name in NamedValues.
  llvm::Value *OldVal = NamedValues[VarName];
  NamedValues[VarName] = Variable;

  // 4. Emit the loop body.
  if (!Body->codegen())
    return nullptr;

  // 5. Compute the step value (defaulting to 1.0 if omitted).
  llvm::Value *StepVal = nullptr;
  if (Step) {
    StepVal = Step->codegen();
    if (!StepVal)
      return nullptr;
  } else {
    StepVal = llvm::ConstantFP::get(*TheContext, llvm::APFloat(1.0));
  }

  llvm::Value *NextVar = Builder->CreateFAdd(Variable, StepVal, "nextvar");

  // 6. Evaluate the termination condition.
  llvm::Value *EndCond = End->codegen();
  if (!EndCond)
    return nullptr;

  EndCond = Builder->CreateFCmpONE(
      EndCond, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)),
      "loopcond");

  // Record where the loop ended (in case nested expressions branched).
  llvm::BasicBlock *LoopEndBB = Builder->GetInsertBlock();
  llvm::BasicBlock *AfterBB =
      llvm::BasicBlock::Create(*TheContext, "afterloop", TheFunction);

  // Branch back to LoopBB if EndCond is true, otherwise jump to AfterBB.
  Builder->CreateCondBr(EndCond, LoopBB, AfterBB);
  Builder->SetInsertPoint(AfterBB);

  // Add the incoming edge from the end of the loop to the induction PHI node.
  Variable->addIncoming(NextVar, LoopEndBB);

  // Restore the shadowed variable.
  if (OldVal)
    NamedValues[VarName] = OldVal;
  else
    NamedValues.erase(VarName);

  // For expressions always return 0.0.
  return llvm::Constant::getNullValue(llvm::Type::getDoubleTy(*TheContext));
}
```

Notice how `NamedValues[VarName]` is carefully saved and restored.  If an outer scope has a variable named `i`, the inner loop shadows it cleanly, and upon exiting `AfterBB`, the outer `i` is safely restored.

---

Next: [Step 7.3: Terminal Graphics and Host Calls](03-terminal-graphics.md) - Connect C host functions and draw patterns from the JIT.
