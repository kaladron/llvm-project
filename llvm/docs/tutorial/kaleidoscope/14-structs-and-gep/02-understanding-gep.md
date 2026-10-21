# Step 14.2: Demystifying GetElementPtr

The `getelementptr` (GEP) instruction is the single most infamous instruction in LLVM.  Newcomers frequently assume it loads data from memory.

It does not.

---

## The Golden Rule of GEP

> **`getelementptr` NEVER accesses memory.**

GEP does not read from memory (`load`).  It does not write to memory (`store`).

**`getelementptr` does one thing only: it performs address arithmetic.**  It calculates a pointer offset based on type sizes and layouts, and returns a new pointer (`ptr`).

---

## The Mystery of the First Index

Consider accessing member `y` of a pointer to a struct in C:

```c
struct Point *p;
double val = p->y;
```

In LLVM IR, the GEP instruction looks like this:

```llvm
%y_ptr = getelementptr inbounds %Point, ptr %p, i32 0, i32 1
```

Why are there **two** indices (`0` and `1`) when we only asked for the second field (`y`)?

This confuses almost every beginner.  Here is the reason:

1.  **The First Index (`0`) indexes through the pointer**:
    In C, any pointer `p` can be treated as an array of structs (`p[0]`, `p[1]`, `p[2]`).  The first index specifies **which struct in the array** you are indexing into.  Index `0` means: stay at the struct pointed to by `%p` ($p + 0 \times \text{sizeof}(\text{Point})$).
2.  **The Second Index (`1`) indexes into the struct**:
    The second index specifies **which member inside that struct** you want.  Field `0` is `x`.  Field `1` is `y`.

If you were writing `p[3].y` in C, the GEP instruction would be:

```llvm
%y_ptr = getelementptr inbounds %Point, ptr %p, i32 3, i32 1
```

---

## Using `CreateStructGEP`

Because accessing a field inside a struct is such a common pattern, `IRBuilder` provides a dedicated helper that automatically supplies the leading `0` index:

```cpp
// Computes pointer to field 1 (y) of Point:
llvm::Value *YPtr = Builder->CreateStructGEP(PointTy, PtrVal, 1, "y_ptr");
```

---

## What Does `inbounds` Mean?

You will notice the `inbounds` keyword on most GEP instructions:

```llvm
getelementptr inbounds %Point, ptr %p, i32 0, i32 1
```

`inbounds` informs LLVM's optimiser that the computed address is guaranteed to lie within the allocated bounds of the underlying memory object (or one byte past the end).  If the calculation overflows or points outside the object, the value is poison.

This allows LLVM to perform aggressive loop vectorisation and alias analysis optimizations.

---

Next: [Step 14.3: Arrays and Member Access in Practice](03-arrays-indexing.md) - Work with arrays and structs in compiled code.
