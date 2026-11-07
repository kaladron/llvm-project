# Section 13: Pointers, Indirection, and Heap Memory

```{toctree}
:hidden:

01-opaque-pointers
02-address-and-dereference
03-heap-malloc-free
```

In Section 12, you passed `.rodata` string addresses into `puts` and `printf`.  Now it is time to unlock the full power of **first-class pointers, indirection, and dynamic heap allocation** inside Kaleidoscope itself.

Up to this point, every variable lived either in a function's stack frame (`alloca`, destroyed the instant the function returns) or at a single fixed global address (`global`).  Systems software needs two capabilities neither of those provides:

1.  **Indirection (`&` and `*`)**: Passing the address of a variable (`&x`) to another function such as `swap(&x, &y)` so the callee can read and mutate the caller's memory directly.
2.  **Dynamic Heap Allocation (`malloc`, `free`, `sizeof`)**: Allocating memory buffers at runtime whose lifetimes outlive the function that created them.

---

## Stack Frames vs. Dynamic Heap Lifetimes

```text
  Stack Allocation (alloca)                 Heap Allocation (malloc / free)
  +-------------------------------+         +-----------------------------------+
  | Caller Frame: test_swap()     |         | Producer: make_box(init)          |
  |   %x = alloca i32  [ 10 ] <---+--+      |   %p = call ptr @malloc(i64 8)    |
  |   %y = alloca i32  [ 99 ] <---+--|---+  |   store i64 %init, ptr %p         |
  +-------------------------------+  |   |  |   ret ptr %p  (frame popped!)     |
  | Callee Frame: swap(%a, %b)    |  |   |  +-----------------+-----------------+
  |   %a holds address of %x -----+--+   |                    |
  |   %b holds address of %y -----+------+                    v
  |   Popped automatically on ret |         | Process Heap: [ 8-byte i64 box ]  |
  +-------------------------------+         |   Survives across arbitrary calls |
  (Returning &x from a callee would         |   until explicitly reclaimed via  |
   leave a dangling stack pointer!)         |   `call void @free(ptr %p)`!      |
                                            +-----------------------------------+
```

:::{note}
**Stack Speed vs. Heap Flexibility**:
Stack allocation via `alloca` costs almost nothing at runtime: the CPU prologue adjusts the stack pointer register (`%rsp` on x86-64) once for the whole function.  Heap allocation via `malloc` searches the process allocator's free lists, costing slightly more CPU work in exchange for a buffer that persists until your program explicitly hands it back to `free`.
:::

---

## What You Will Build in This Section

1.  [Step 13.1: Understanding LLVM Opaque Pointers (`ptr<T>`)](01-opaque-pointers.md): Learn why modern LLVM replaced typed pointers (`i32*`, `double*`) with a single unified `ptr` type, parse parameterised `ptr<T>` types and `null`, and emit `ptrtoint`, `inttoptr`, and zero-instruction pointer casts.
2.  [Step 13.2: Address-Of (`&`) and Dereference (`*`)](02-address-and-dereference.md): Build `AddrOfExprAST` and `DerefExprAST` on top of `codegenAddress()` from [Step 9.2](../09-local-variables/02-var-in-expressions.md) so `&x`, `*p`, `*p = val`, and multi-level `**pp = val` work seamlessly, and write a working `swap` function in Kaleidoscope.
3.  [Step 13.3: `sizeof` and Heap Allocation (`malloc` / `free`)](03-heap-malloc-free.md): Add `sizeof(<type>)` backed by `DataLayout::getTypeAllocSize()`, declare `malloc` and `free`, and pass heap-allocated values safely across function boundaries.

---

## Compiling Your Living Compiler

Compile `kaleidoscope.cpp` with the standard build command:

```bash
clang++ -std=c++17 -fno-rtti -Wall -Wextra -rdynamic -Iinclude kaleidoscope.cpp $(llvm-config --cxxflags --ldflags --system-libs --libs core orcjit native passes all-targets) -isystem $(llvm-config --includedir) -o kaleidoscope
```

Complete reference snapshots for every step live in [code/index.md](../code/index.md) (`s13-01-opaque-pointers.cpp` through `s13-03-heap-malloc-free.cpp`).

---

[Previous: Step 12.3](../12-globals-and-strings/03-calling-clib.md) | [Tutorial Index](../index.md) | [Start Step 13.1](01-opaque-pointers.md)
