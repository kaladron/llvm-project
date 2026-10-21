# Step 11.1: Integer Widths and Primitive Types

In hardware, processors do not treat integers and floating-point numbers identically.  They reside in separate register files (general-purpose registers vs. vector/floating-point registers) and use distinct arithmetic execution pipelines.

Furthermore, integers have explicit bit-widths.  An array of bytes requires 8-bit integers (`i8`), loop counters and lengths often use 32-bit integers (`i32`), and memory pointers on 64-bit machines require 64-bit integers (`i64`).

---

## 1. LLVM's Arbitrary-Precision Integer Types

In LLVM Intermediate Representation, integer types are written as `iN`, where `N` is an arbitrary bit-width from 1 to $2^{23}-1$:

* `i1`: A 1-bit integer, used for booleans and branch conditions.
* `i8`: An 8-bit byte (equivalent to C `char` or `uint8_t`).
* `i16`: A 16-bit halfword (equivalent to C `short`).
* `i32`: A 32-bit word (equivalent to standard C `int`).
* `i64`: A 64-bit doubleword (equivalent to C `long long` or `intptr_t`).

To retrieve these types from the LLVM context in C++:

```cpp
llvm::Type *I1Ty = llvm::Type::getInt1Ty(*TheContext);
llvm::Type *I8Ty = llvm::Type::getInt8Ty(*TheContext);
llvm::Type *I32Ty = llvm::Type::getInt32Ty(*TheContext);
llvm::Type *I64Ty = llvm::Type::getInt64Ty(*TheContext);
```

Or, through the IRBuilder convenience helpers:

```cpp
llvm::Type *I32Ty = Builder->getInt32Ty();
```

---

## 2. Emitting Integer Constants

Previously, you emitted floating-point constants using `llvm::ConstantFP::get`:

```cpp
// 64-bit floating point constant:
llvm::Value *F = llvm::ConstantFP::get(*TheContext, llvm::APFloat(42.0));
```

To emit an integer constant, use `llvm::ConstantInt::get` or the `IRBuilder` helper:

```cpp
// 32-bit integer constant:
llvm::Value *Val32 = Builder->getInt32(42);

// 64-bit integer constant:
llvm::Value *Val64 = Builder->getInt64(10000000000LL);

// Arbitrary-precision APInt constant:
llvm::Value *Custom = llvm::ConstantInt::get(
    *TheContext, llvm::APInt(32, 42, /*isSigned=*/true));
```

---

## 3. Creating `IntExprAST`

Add `IntExprAST` alongside `NumberExprAST` in your AST hierarchy:

```cpp
class IntExprAST : public ExprAST {
  int64_t Val;

public:
  IntExprAST(int64_t Val) : Val(Val) {}

  llvm::Value *codegen() override {
    return Builder->getInt32(static_cast<int32_t>(Val));
  }

  llvm::Type *getType() override {
    return Builder->getInt32Ty();
  }
};
```

---

## Active Learning Challenge

<details>
<summary><b>Challenge: Why Does LLVM Support Arbitrary Bit-Widths?</b></summary>

Why does LLVM support arbitrary bit-widths like `i3`, `i7`, or `i24`, even though physical CPU registers only come in 8, 16, 32, and 64 bits?

<br>

**Explanation**:
Arbitrary-width integers allow LLVM to accurately represent hardware descriptions (like FPGA synthesis and Verilog), graphics shaders, packed bitfields, and specialized DSP architectures without premature padding.  When targeting a standard CPU like x86-64 or ARM, LLVM's target backend automatically promotes or legalises odd integer widths (e.g. promoting `i7` into an `i8` or `i32` register).

</details>

---

Next: [Step 11.2: Signless Registers and Integer Arithmetic](02-signless-integers.md) - Learn why LLVM integer registers are signless.
