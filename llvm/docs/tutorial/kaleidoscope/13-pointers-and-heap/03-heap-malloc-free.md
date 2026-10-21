# Step 13.3: Heap Allocation with 'malloc' and 'free'

Stack allocation with `alloca` is fast, but stack frames have a fixed lifetime: they are automatically destroyed as soon as their enclosing function returns.

When a program needs data structures whose lifetimes extend beyond a single function call, or whose sizes cannot be known at compile time, it must allocate memory on the **heap**.

In this step, you will declare the standard C heap allocators `malloc` and `free` in your module and call them from compiled code.

---

## 1. Declaring `malloc` and `free`

In C, the prototypes are:

```c
void *malloc(size_t size);
void free(void *ptr);
```

On 64-bit systems, `size_t` is a 64-bit integer (`i64`), and `void*` is an opaque pointer (`ptr`).

Declare these signatures in your LLVM module:

```cpp
llvm::Type *PtrTy = Builder->getPtrTy();
llvm::Type *I64Ty = Builder->getInt64Ty();
llvm::Type *VoidTy = Builder->getVoidTy();

// Declare extern ptr @malloc(i64)
llvm::FunctionType *MallocFT = llvm::FunctionType::get(PtrTy, {I64Ty}, false);
llvm::FunctionCallee MallocFn = TheModule->getOrInsertFunction("malloc", MallocFT);

// Declare extern void @free(ptr)
llvm::FunctionType *FreeFT = llvm::FunctionType::get(VoidTy, {PtrTy}, false);
llvm::FunctionCallee FreeFn = TheModule->getOrInsertFunction("free", FreeFT);
```

---

## 2. Emitting Heap Allocations

To allocate heap memory, emit a call to `malloc`, passing the number of bytes:

```cpp
// Allocate space for one 32-bit integer (4 bytes):
llvm::Value *HeapPtr = Builder->CreateCall(
    MallocFn, Builder->getInt64(4), "heap_ptr");
```

Store data into the heap pointer, and read it back:

```cpp
// Store 42 into the allocated memory:
Builder->CreateStore(Builder->getInt32(42), HeapPtr);

// Read the value back:
llvm::Value *Val = Builder->CreateLoad(
    Builder->getInt32Ty(), HeapPtr, "val");
```

When finished with the heap memory, release it back to the operating system with `free`:

```cpp
Builder->CreateCall(FreeFn, HeapPtr);
```

---

## 3. Emitted IR Example

```llvm
define i32 @heap_example(i32 %x) {
entry:
  %heap_ptr = call ptr @malloc(i64 4)
  store i32 %x, ptr %heap_ptr, align 4
  %loaded = load i32, ptr %heap_ptr, align 4
  %res = add i32 %loaded, 10
  call void @free(ptr %heap_ptr)
  ret i32 %res
}

declare ptr @malloc(i64)
declare void @free(ptr)
```

Notice how clean the IR is.  Dynamic heap pointers interoperate directly with normal `load` and `store` instructions.

---

Next: [Section 14: Aggregate Structs, Arrays, and GetElementPtr](../14-structs-and-gep/index.md) - Learn how LLVM represents complex data structures and computes memory offsets.
