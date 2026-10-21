# Step 11.4: Type Conversions and Casts

In a multi-type language, programs frequently convert values between types: widening an 8-bit character to a 32-bit integer, converting an integer to a floating-point number, or casting between signed and unsigned interpretations.

LLVM enforces strict type safety in its Intermediate Representation: **implicit conversions do not exist in LLVM IR**.  Every type change must be represented by an explicit conversion instruction.

---

## 1. Integer Width Conversions

When converting between integer types of different bit-widths:

### Narrowing: Truncation (`trunc`)
To convert a wider integer to a narrower integer (e.g. `i32` to `i8`), LLVM simply discards the leading bits:

```cpp
llvm::Value *ByteVal = Builder->CreateTrunc(Int32Val, Builder->getInt8Ty(), "trunc8");
```

### Widening: Sign-Extension (`sext`) vs. Zero-Extension (`zext`)
When expanding a narrower integer to a wider integer (e.g. `i8` to `i32`), the missing bits must be filled:

* **`zext` (Zero Extension)**: Fills the upper bits with zeroes.  Used for unsigned numbers.
* **`sext` (Sign Extension)**: Copies the most significant bit (sign bit) across all upper bits.  Used for signed two's-complement numbers.

```cpp
// If ByteVal is 0xFF (-1 signed, 255 unsigned):
// zext produces 0x000000FF (255)
llvm::Value *UnsignedWide = Builder->CreateZExt(ByteVal, Builder->getInt32Ty(), "zext32");

// sext produces 0xFFFFFFFF (-1)
llvm::Value *SignedWide   = Builder->CreateSExt(ByteVal, Builder->getInt32Ty(), "sext32");
```

---

## 2. Integer and Floating-Point Conversions

Converting between the general-purpose integer domain and the floating-point domain requires hardware conversion instructions:

```cpp
// Signed integer to double:
llvm::Value *AsDouble = Builder->CreateSIToFP(IntVal, Builder->getDoubleTy(), "todbl");

// Unsigned integer to double:
llvm::Value *AsDoubleU = Builder->CreateUIToFP(IntVal, Builder->getDoubleTy(), "todbl_u");

// Double to signed integer (truncates towards zero):
llvm::Value *AsInt = Builder->CreateFPToSI(DblVal, Builder->getInt32Ty(), "toint");

// Double to unsigned integer:
llvm::Value *AsIntU = Builder->CreateFPToUI(DblVal, Builder->getInt32Ty(), "toint_u");
```

---

## 3. Bitcasting

When two types have identical bit sizes and you wish to reinterpret the underlying bits without altering them (for example, reinterpreting the raw 64-bit IEEE pattern of a `double` as an `i64`), use `CreateBitCast`:

```cpp
llvm::Value *RawBits = Builder->CreateBitCast(DblVal, Builder->getInt64Ty(), "rawbits");
```

---

## Conversion Cheat Sheet

| Source | Destination | Signedness | LLVM Instruction | IRBuilder Method |
| :--- | :--- | :--- | :--- | :--- |
| `i32` | `i8` | Either | `trunc` | `CreateTrunc` |
| `i8` | `i32` | Unsigned | `zext` | `CreateZExt` |
| `i8` | `i32` | Signed | `sext` | `CreateSExt` |
| `i32` | `double` | Signed | `sitofp` | `CreateSIToFP` |
| `i32` | `double` | Unsigned | `uitofp` | `CreateUIToFP` |
| `double` | `i32` | Signed | `fptosi` | `CreateFPToSI` |
| `double` | `i32` | Unsigned | `fptoui` | `CreateFPToUI` |
| `i64` | `double` | Bit-level | `bitcast` | `CreateBitCast` |

---

## Active Learning Challenge

<details>
<summary><b>Challenge: Implementing an Absolute Value Function</b></summary>

Write a C++ function using `IRBuilder` that generates an integer absolute value function `abs(i32 %x)`.

Hint: Check if `x < 0` using `CreateICmpSLT`.  If true, subtract `x` from `0` with `CreateSub`, and use `CreateSelect` to choose between the original value and the negated value!

<br>

**Solution**:
```cpp
llvm::Value *EmitAbs(llvm::IRBuilder<> &Builder, llvm::Value *X) {
  llvm::Value *IsNeg = Builder.CreateICmpSLT(X, Builder.getInt32(0), "isneg");
  llvm::Value *NegX  = Builder.CreateSub(Builder.getInt32(0), X, "negx");
  return Builder.CreateSelect(IsNeg, NegX, X, "abstmp");
}
```

Notice `CreateSelect`: in LLVM, `select` is the ternary operator `cond ? trueval : falseval`.  It generates conditional moves (`cmov`) without needing basic blocks or branches!

</details>

---

Next: [Section 12: Global Variables and Constant Strings](../12-globals-and-strings/README.md) - Learn how LLVM represents global state and string literals.
