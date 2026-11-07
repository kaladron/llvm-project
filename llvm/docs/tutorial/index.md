# LLVM Tutorial: Table of Contents

## Kaleidoscope: Building a Compiler with LLVM

```{toctree}
:hidden:

kaleidoscope/index
```

{doc}`kaleidoscope/index`

: A sixteen-section hands-on tutorial that evolves a single-file toy calculator
  into a statically typed systems language with an ORCv2 JIT, ahead-of-time
  object emission, opaque pointers, structs, DWARF debug information, and a
  custom optimisation pass.

## Building a JIT in LLVM

```{toctree}
:glob:
:numbered:
:titlesonly:

BuildingAJIT*
```

## External Tutorials

[Tutorial: Creating an LLVM Backend for the Cpu0 Architecture](http://jonathan2251.github.io/lbd/)

: A step-by-step tutorial for developing an LLVM backend. Under
  active development at [https://github.com/Jonathan2251/lbd](https://github.com/Jonathan2251/lbd) (please
  contribute!).

[Howto: Implementing LLVM Integrated Assembler]

: A simple guide for how to implement an LLVM integrated assembler for an
  architecture.

## Advanced Topics

1. [Writing an Optimization for LLVM](https://llvm.org/pubs/2004-09-22-LCPCLLVMTutorial.html)

[howto: implementing llvm integrated assembler]: http://www.embecosm.com/appnotes/ean10/ean10-howto-llvmas-1.0.html
