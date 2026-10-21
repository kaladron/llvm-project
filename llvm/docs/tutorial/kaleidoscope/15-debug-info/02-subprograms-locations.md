# Step 15.2: Subprograms and Instruction Locations

With the compile unit and source file configured, you can now attach debug metadata to individual functions and instructions.

---

## 1. Creating `DISubprogram` for Functions

In DWARF terminology, a function is a **Subprogram**.

Create the debug subroutine type and the subprogram metadata node, then attach it to the `llvm::Function`:

```cpp
// 1. Create subroutine type signature (e.g. returning int, taking int)
llvm::DISubroutineType *DIType = DIB.createSubroutineType(
    DIB.getOrCreateTypeArray({
        DIB.createBasicType("int", 32, llvm::dwarf::DW_ATE_signed)
    }));

// 2. Create the DISubprogram node
llvm::DISubprogram *SP = DIB.createFunction(
    File,                               // Enclosing file scope
    "add_one",                          // Function name in source
    "add_one",                          // Linkage / mangled name
    File,                               // Declared file
    5,                                  // Source line number
    DIType,                             // Type signature
    5,                                  // Scope line number
    llvm::DINode::FlagPrototyped,
    llvm::DISubprogram::SPFlagDefinition
);

// 3. Attach subprogram to the LLVM Function
TheFunction->setSubprogram(SP);
```

---

## 2. Attaching Source Locations to Instructions

To tell a debugger which line of source code an instruction corresponds to, configure the `IRBuilder`'s current debug location before creating the instruction:

```cpp
// Set source location to line 6, column 3 inside subprogram SP:
Builder->SetCurrentDebugLocation(
    llvm::DILocation::get(*TheContext, 6, 3, SP));

// Instructions emitted now will automatically carry this debug location:
llvm::Value *Sum = Builder->CreateAdd(A, B, "sum");
```

Whenever the debugger steps to `Sum`, it will highlight line 6 in the developer's source editor!

---

## 3. Emitted IR with Debug Attachments

Inspect the emitted LLVM IR:

```llvm
define i32 @add_one(i32 %x) !dbg !2 {
entry:
  %sum = add i32 %x, 1, !dbg !6
  ret i32 %sum, !dbg !7
}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "Kaleidoscope Compiler", ...)
!1 = !DIFile(filename: "fib.ks", directory: "/project")
!2 = distinct !DISubprogram(name: "add_one", scope: !1, file: !1, line: 5, unit: !0, ...)
!6 = !DILocation(line: 6, column: 3, scope: !2)
!7 = !DILocation(line: 7, column: 3, scope: !2)
```

Every instruction contains an attached `!dbg` tag pointing to metadata containing its exact line and column numbers.

---

Next: [Step 15.3: Stepping Through Code with LLDB](03-debugging-with-lldb.md) - Step through compiled functions in a native debugger.
