# Step 14.2: Demystifying GetElementPtr

The `getelementptr` (GEP) instruction is the single most infamous instruction in LLVM.  Newcomers frequently assume it loads data from memory.

It does not.

---

## The Dual-Index Mechanics of GEP

```mermaid
flowchart LR
    BasePtr["Base ptr %p"] --> Idx0["Index 0: Through the pointer<br>(%p + 0 * sizeof(Point))"]
    Idx0 --> Idx1["Index 1: Into the struct<br>(Field 1: offset +8 bytes for 'y')"]
    Idx1 --> OutPtr["Result ptr %y_ptr<br>(Direct address of member 'y')"]
```

> [!IMPORTANT]
> **Understanding the Leading Zero in GEP**:
> The first index always indexes through the base pointer.  Because in C and LLVM any pointer `p` can represent an array of elements (`p[0]`, `p[1]`, ...), the first index specifies which element of the array is selected.  Only subsequent indices navigate into the internal members or sub-elements of that element.

---

## The Golden Rule of GEP

> **`getelementptr` NEVER accesses memory.**

GEP does not read from memory (`load`).  It does not write to memory (`store`).

**`getelementptr` does one thing only: it performs address arithmetic.**  It calculates a pointer offset based on type sizes and layouts, and returns a new pointer (`ptr`).

---

## Incremental Code Changes

Open `kaleidoscope.cpp` to emit GEP address calculations.

### 1. The Mystery of the First Index

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

Here is the reason:

1.  **The First Index (`0`) indexes through the pointer**:
    In C, any pointer `p` can be treated as an array of structs (`p[0]`, `p[1]`, `p[2]`).  The first index specifies **which struct in the array** you are indexing into.  Index `0` means: stay at the struct pointed to by `%p` ($p + 0 \times \text{sizeof}(\text{Point})$).
2.  **The Second Index (`1`) indexes into the struct**:
    The second index specifies **which member inside that struct** you want.  Field `0` is `x`.  Field `1` is `y`.

If you were writing `p[3].y` in C, the GEP instruction would be:

```llvm
%y_ptr = getelementptr inbounds %Point, ptr %p, i32 3, i32 1
```

### 2. Using `CreateStructGEP`

Because accessing a field inside a struct is such a common pattern, `IRBuilder` provides a dedicated helper that automatically supplies the leading `0` index:

```cpp
// Computes pointer to field 1 (y) of Point:
llvm::Value *YPtr = Builder->CreateStructGEP(PointTy, PtrVal, 1, "y_ptr");
```

### 3. What Does `inbounds` Mean?

You will notice the `inbounds` keyword on most GEP instructions:

```llvm
getelementptr inbounds %Point, ptr %p, i32 0, i32 1
```

`inbounds` informs LLVM's optimiser that the computed address is guaranteed to lie within the allocated bounds of the underlying memory object (or one byte past the end).  If the calculation overflows or points outside the object, the value is poison.

This allows LLVM to perform aggressive loop vectorisation and alias analysis optimizations.

---

In [Step 14.3](03-arrays-indexing.md), we will work with arrays and structs in compiled code.
