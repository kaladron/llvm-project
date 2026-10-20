# Step 6.2: Basic Blocks and Branching

In previous sections, your functions consisted of a single sequence of instructions executed straight through from top to bottom.  To implement decisions, your compiler must jump between different paths.  In LLVM IR, control flow is structured with **Basic Blocks**.

---

## What is a Basic Block?

A `BasicBlock` is a sequence of LLVM instructions with two fundamental properties:

1.  **Single Entry**: Execution always starts at the first instruction.  No instruction inside the block can be jumped into from the outside.
2.  **Single Exit**: Execution always proceeds to the end of the block.  The final instruction in every basic block **must** be a **terminator instruction** (such as a branch `br` or return `ret`).

Because basic blocks cannot contain internal branches, compilers can easily analyse and optimise them as indivisible units.

---

## The Control Flow Graph for 'if / then / else'

An `if / then / else` expression splits control flow into three basic blocks:

1.  **`then` Block**: Executed only if the condition evaluates to true (non-zero).
2.  **`else` Block**: Executed only if the condition evaluates to false (zero).
3.  **`ifcont` (Merge) Block**: Where both branches converge after completing their computations.

```
       [ Current Block ]
       (compute condition)
          /         \
   (true)/           \(false)
        v             v
   [ then ]        [ else ]
   (compute)       (compute)
        \             /
         \           /
          v         v
        [ ifcont (merge) ]
```

---

## 1. Converting the Condition to a Boolean

In Kaleidoscope, all numeric values are 64-bit IEEE 754 floating-point doubles.  However, conditional branch instructions in LLVM IR require a 1-bit integer (`i1`, boolean).

To convert a floating-point value into a boolean, compare it against `0.0`.  In IEEE floating-point math, non-equality is checked with `CreateFCmpONE` (Ordered and Not Equal):

```cpp
  llvm::Value *CondV = Cond->codegen();
  if (!CondV)
    return nullptr;

  CondV = Builder->CreateFCmpONE(
      CondV, llvm::ConstantFP::get(*TheContext, llvm::APFloat(0.0)), "ifcond");
```

---

## 2. Creating the Blocks and Branching

Next, retrieve the enclosing function from the builder's current insertion point, and construct the three blocks:

```cpp
  llvm::Function *TheFunction = Builder->GetInsertBlock()->getParent();

  llvm::BasicBlock *ThenBB =
      llvm::BasicBlock::Create(*TheContext, "then", TheFunction);
  llvm::BasicBlock *ElseBB = llvm::BasicBlock::Create(*TheContext, "else");
  llvm::BasicBlock *MergeBB = llvm::BasicBlock::Create(*TheContext, "ifcont");

  Builder->CreateCondBr(CondV, ThenBB, ElseBB);
```

Notice that only `ThenBB` is passed `TheFunction` upon creation.  This automatically appends `ThenBB` to the end of the function.  `ElseBB` and `MergeBB` are created unattached for now; we will attach them in order as we emit their contents.

---

## 3. Emitting the 'then' and 'else' Bodies

To emit instructions into `ThenBB`, move the builder's insertion point:

```cpp
  Builder->SetInsertPoint(ThenBB);
  llvm::Value *ThenV = Then->codegen();
  if (!ThenV)
    return nullptr;

  Builder->CreateBr(MergeBB);
  ThenBB = Builder->GetInsertBlock();
```

After generating code for `Then`, we emit an unconditional branch (`CreateBr`) to jump to `MergeBB`.

Notice this line:
```cpp
  ThenBB = Builder->GetInsertBlock();
```

Why update `ThenBB`?  Because the expression inside `then` might itself contain another nested `if` expression!  If it does, generating code for `Then` will create additional basic blocks, and the block that actually ends up branching to `MergeBB` will not be the original `ThenBB`.  Recording `Builder->GetInsertBlock()` ensures we know the true block where the branch originates.

Next, attach and emit `ElseBB`:

```cpp
  TheFunction->insert(TheFunction->end(), ElseBB);
  Builder->SetInsertPoint(ElseBB);
  llvm::Value *ElseV = Else->codegen();
  if (!ElseV)
    return nullptr;

  Builder->CreateBr(MergeBB);
  ElseBB = Builder->GetInsertBlock();
```

---

Next: [Step 6.3: SSA PHI Nodes](03-phi-nodes.md) - Learn how LLVM merges divergent values at control flow merge points.
