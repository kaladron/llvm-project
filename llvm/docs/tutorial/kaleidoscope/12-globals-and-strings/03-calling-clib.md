# Step 12.3: Calling the C Standard Library

With global string pointers available, your compiler can now interact with the rich ecosystem of standard C runtime library functions, including formatted printing and string inspection.

In this step, you will learn how to declare and call both standard and variadic C library functions in LLVM IR.

---

## 1. Calling `puts` (Fixed Parameter List)

The C standard library function `puts` has the prototype:

```c
int puts(const char *s);
```

In LLVM IR with opaque pointers, `const char *` is simply `ptr`.  To declare and call `puts`:

```cpp
// 1. Define function signature: int32_t(ptr)
llvm::FunctionType *PutsFT = llvm::FunctionType::get(
    Builder->getInt32Ty(), {Builder->getPtrTy()}, /*isVarArg=*/false);

// 2. Declare external function in module
llvm::FunctionCallee PutsFn = TheModule->getOrInsertFunction("puts", PutsFT);

// 3. Emit string literal and invoke puts
llvm::Value *StrPtr = Builder->CreateGlobalString("Hello from LLVM!\n", "str");
Builder->CreateCall(PutsFn, StrPtr);
```

---

## 2. Calling `printf` (Variadic Functions)

Functions like `printf` accept a variable number of arguments (`varargs`):

```c
int printf(const char *format, ...);
```

When defining a variadic function type in LLVM, set the third parameter `isVarArg` to `true`:

```cpp
// 1. Define variadic function signature: int32_t(ptr, ...)
llvm::FunctionType *PrintfFT = llvm::FunctionType::get(
    Builder->getInt32Ty(), {Builder->getPtrTy()}, /*isVarArg=*/true);

// 2. Declare external printf
llvm::FunctionCallee PrintfFn = TheModule->getOrInsertFunction("printf", PrintfFT);

// 3. Create format string and call printf with multiple arguments
llvm::Value *Fmt = Builder->CreateGlobalString("Result: %d, Float: %f\n", "fmt");
Builder->CreateCall(PrintfFn, {Fmt, Builder->getInt32(42), Builder->getDouble(3.14159)});
```

---

## 3. Emitted IR Example

```llvm
@str = private unnamed_addr constant [19 x i8] c"Hello from LLVM!\0A\00", align 1
@fmt = private unnamed_addr constant [24 x i8] c"Result: %d, Float: %f\0A\00", align 1

define i32 @main() {
entry:
  %0 = call i32 @puts(ptr @str)
  %1 = call i32 (ptr, ...) @printf(ptr @fmt, i32 42, double 3.141590e+00)
  ret i32 0
}

declare i32 @puts(ptr)
declare i32 @printf(ptr, ...)
```

---

## Active Learning Challenge

<details>
<summary><b>Challenge: Why Does printf Need Explicit VarArg Types?</b></summary>

Why does calling `printf` require specifying `(ptr, ...)` in the call instruction, whereas `puts` does not?

<br>

**Explanation**:
On x86-64 System V ABI, variadic functions use a different calling convention than non-variadic functions: the caller must set the `%al` register to the number of vector/floating-point registers used.  By marking the function `isVarArg=true`, LLVM knows to generate the appropriate ABI setup instructions before jumping to `printf`.

</details>

---

Next: [Section 13: Pointers, Indirection, and Heap Memory](../13-pointers-and-heap/README.md) - Master LLVM pointers and heap allocations with `malloc` and `free`.
