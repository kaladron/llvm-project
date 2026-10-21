# Step 14.3: Arrays and Member Access in Practice

Now that you understand the dual-index mechanics of `getelementptr`, you can combine structs, arrays, and memory operations into complete data pipelines.

---

## 1. Array Types in LLVM

To define a fixed-size array type in LLVM, use `llvm::ArrayType::get`:

```cpp
// Define an array of 10 integers: [10 x i32]
llvm::ArrayType *Array10Ty = llvm::ArrayType::get(Builder->getInt32Ty(), 10);
```

To allocate this array on the stack:

```cpp
llvm::AllocaInst *ArrayAlloca = Builder->CreateAlloca(Array10Ty, nullptr, "my_array");
```

---

## 2. Indexing an Array with GEP

To compute a pointer to element `i` of your stack-allocated array (`&my_array[i]`):

```cpp
// my_array has type [10 x i32]
// Index 0: index through the pointer (to the first [10 x i32] array)
// Index i: index into the array (to the i-th i32 element)
llvm::Value *ElemPtr = Builder->CreateGEP(
    Array10Ty, ArrayAlloca, {Builder->getInt32(0), IndexVal}, "elem_ptr");
```

Once you have `ElemPtr`, write to it with `store`, or read from it with `load`:

```cpp
// Store 100 into my_array[i]:
Builder->CreateStore(Builder->getInt32(100), ElemPtr);

// Load value from my_array[i]:
llvm::Value *Val = Builder->CreateLoad(Builder->getInt32Ty(), ElemPtr, "elem_val");
```

---

## 3. Emitted IR Example

```llvm
%Point = type { i32, double }

define double @get_point_y(ptr %p) {
entry:
  %y_ptr = getelementptr inbounds %Point, ptr %p, i32 0, i32 1
  %y_val = load double, ptr %y_ptr, align 8
  ret double %y_val
}

define void @fill_array(ptr %arr, i32 %idx, i32 %val) {
entry:
  %elem_ptr = getelementptr inbounds i32, ptr %arr, i32 %idx
  store i32 %val, ptr %elem_ptr, align 4
  ret void
}
```

Notice the second function: when indexing a raw pointer `ptr %arr` (decayed array pointer), GEP only needs **one index** because the element type is specified directly as `i32`!

---

## Active Learning Challenge

<details>
<summary><b>Challenge: Array of Structs</b></summary>

Suppose you have an array of structs: `struct Point points[10]`.  How do you compute the pointer to `points[i].y` in LLVM IR?

<br>

**Solution**:
```cpp
// 1. Array type: [10 x %Point]
// Indices needed:
// 0: Through the pointer to points
// i: Index into the array to select points[i]
// 1: Index into the struct to select field .y
llvm::Value *YPtr = Builder->CreateGEP(
    ArrayOfPointsTy, PointsAlloca,
    {Builder->getInt32(0), IndexVal, Builder->getInt32(1)},
    "y_ptr");
```

GEP chains indices hierarchically: pointer $\to$ array element $\to$ struct member.

</details>

---

Next: [Section 15: Source-Level Debug Information (DWARF)](../15-debug-info/README.md) - Teach your compiler how to emit debug symbols for GDB and LLDB.
