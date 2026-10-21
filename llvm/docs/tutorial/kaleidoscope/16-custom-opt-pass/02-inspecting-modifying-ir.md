# Step 16.2: Inspecting and Modifying IR

Writing an optimization pass requires traversing LLVM's in-memory data structures, identifying redundant or inefficient patterns, and safely replacing them.

In this step, you will implement an algebraic simplification pass: detecting and eliminating addition by zero ($x + 0 \to x$).

---

## 1. Safe Iterator Traversal

When modifying or deleting instructions in a basic block, **you must never use a standard range-based for loop** (`for (auto &I : BB)`).

If you delete an instruction while holding an iterator to it, the iterator is invalidated, causing undefined behaviour and compiler crashes.

Instead, advance the iterator *before* inspecting or mutating the instruction:

```cpp
for (auto &BB : F) {
  for (auto Inst = BB.begin(), E = BB.end(); Inst != E; ) {
    llvm::Instruction &I = *Inst++; // Advance iterator FIRST!

    // Safely transform or delete I here...
  }
}
```

---

## 2. Pattern Matching Instructions

To match an instruction of the form `x + 0`:

```cpp
// 1. Check if the instruction is an integer addition
if (I.getOpcode() == llvm::Instruction::Add) {
  // 2. Check if the second operand is a constant integer
  if (auto *CI = llvm::dyn_cast<llvm::ConstantInt>(I.getOperand(1))) {
    // 3. Check if the constant is zero
    if (CI->isZero()) {
      // Pattern matched!
    }
  }
}
```

Notice `llvm::dyn_cast<T>`: this is LLVM's custom RTTI casting system.  It is faster than standard C++ `dynamic_cast` and works across compilers with RTTI disabled (`-fno-rtti`).

---

## 3. Replacing Uses and Erasing Instructions

Once the pattern is detected, perform the transformation in two steps:

1.  **`replaceAllUsesWith(NewVal)`**: Updates every other instruction in the function that was using the result of `I`, pointing them directly to the first operand (`x`) instead.
2.  **`eraseFromParent()`**: Unlinks the dead instruction from the basic block and deletes it from memory.

```cpp
llvm::Value *OriginalX = I.getOperand(0);

// Point all users of (x + 0) directly to x:
I.replaceAllUsesWith(OriginalX);

// Delete the dead add instruction:
I.eraseFromParent();
Modified = true;
```

---

Next: [Step 16.3: Running Your Pass and Verifying Optimisations](03-running-pass.md) - Register and execute your pass in a real pipeline.
