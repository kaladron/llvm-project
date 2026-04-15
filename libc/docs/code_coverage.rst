.. _code_coverage:

=============
Code Coverage
=============

LLVM-libc supports generating code coverage reports using Clang's source-based code coverage instrumentation.

Enabling Coverage
=================

To enable code coverage, pass the ``-DLIBC_ENABLE_COVERAGE=ON`` flag to CMake when configuring the build. It is recommended to also use a debug build for better source mapping.

.. code-block:: sh

  cmake -G Ninja ../runtimes -DLLVM_ENABLE_RUNTIMES="libc;compiler-rt" -DLLVM_LIBC_FULL_BUILD=ON -DCMAKE_BUILD_TYPE=Debug -DLIBC_ENABLE_COVERAGE=ON

Building and Running Tests
==========================

Once configured, build and run the tests as usual.

For unit tests:

.. code-block:: sh

  ninja libc-unit-tests

For integration tests:

.. code-block:: sh

  ninja libc-integration-tests

Handling Profile Data
=====================

Test execution will generate raw profile data files (``.profraw``).

Unit Tests
----------

Unit tests run in parallel and are configured to write unique profile files named ``default-%p.profraw`` (where ``%p`` is the process ID) to avoid corruption from concurrent writes.

Due to the large number of unit tests, hundreds or thousands of ``profraw`` files may be generated. You can merge them using ``llvm-profdata``:

.. code-block:: sh

  llvm-profdata merge -o unit_merged.profdata $(find libc/test -name "default-*.profraw")

.. note::
  If the number of files exceeds the command line length limit, you may need to use a script to merge them in chunks.

Integration Tests
-----------------

Integration tests produce ``default.profraw`` files in their respective build directories. They can be merged similarly:

.. code-block:: sh

  llvm-profdata merge -o integration_merged.profdata $(find libc/test/integration -name "default.profraw")

Generating Reports
==================

Once you have a merged ``.profdata`` file, you can generate a coverage report using ``llvm-cov``. You need to pass the test binaries to ``llvm-cov``.

For a single binary:

.. code-block:: sh

  llvm-cov report <path-to-binary> -instr-profile=unit_merged.profdata

For multiple binaries (e.g., all unit tests), you can pass them using the ``-object`` flag or a response file if there are too many:

.. code-block:: sh

  llvm-cov report <first-binary> -instr-profile=unit_merged.profdata -object <second-binary> -object <third-binary> ...

A helper script can be useful to automate finding all binaries and passing them to ``llvm-cov``.
