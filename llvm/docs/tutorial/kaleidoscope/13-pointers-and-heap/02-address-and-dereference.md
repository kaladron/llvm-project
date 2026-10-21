# Step 13.2: Address-Of and Dereferencing

In C and C++, the address-of operator (`&`) and the dereference operator (`*`) are two sides of the same coin:
* `&x` yields the memory address where variable `x` is stored.
* `*p` inspects the address in pointer `p` and accesses the value stored at that location.

How do these concepts map to LLVM Intermediate Representation?

---

## 1. The Address-Of Operator (`&`)

Remember how local variables are represented in `NamedValues` from Section 9:

```cpp
static std::map<std::string, llvm::AllocaInst *> NamedValues;
```

In LLVM IR, an `AllocaInst` allocates stack memory and produces a pointer (`ptr`) pointing to that allocated memory.

This means **taking the address of a variable requires zero instructions**:

```cpp
// Address-of &x:
llvm::AllocaInst *VariableAlloca = NamedValues["x"];
// VariableAlloca IS already the pointer (ptr) to x!
return VariableAlloca;
```

Because variables already exist as stack pointers in the frontend, `&x` simply returns the `AllocaInst*` directly.

---

## 2. The Dereference Operator (`*`)

The dereference operator behaves differently depending on whether it appears on the right-hand side (RHS, reading) or the left-hand side (LHS, writing) of an assignment.

### Reading Through a Pointer (`*p`)
To read the value referenced by a pointer, emit a `load` instruction:

```cpp
llvm::Value *PtrVal = PtrExpr->codegen(); // Evaluates to ptr
llvm::Value *LoadedVal = Builder->CreateLoad(
    PointeeType, PtrVal, "deref_val");
```

### Writing Through a Pointer (`*p = val`)
To write through a pointer, emit a `store` instruction into the target address:

```cpp
llvm::Value *PtrVal = TargetPtrExpr->codegen(); // Evaluates to ptr
llvm::Value *ValToStore = ValueExpr->codegen();
Builder->CreateStore(ValToStore, PtrVal);
```

---

## 3. Multi-Level Pointers (`int **`)

Consider a pointer to a pointer:

```c
int x = 42;
int *p = &x;
int **pp = &p;
int result = **pp;
```

In LLVM IR, each dereference is simply another `load` instruction:

```llvm
; 1. Load pointer 'p' from 'pp':
%p = load ptr, ptr %pp

; 2. Load integer 'x' from 'p':
%result = load i32, ptr %p
```

---

Next: [Step 13.3: Heap Allocation with 'malloc' and 'free'](03-heap-malloc-free.md) - Allocate dynamic memory on the heap.
