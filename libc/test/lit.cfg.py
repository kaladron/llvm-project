# -*- Python -*-
#
# Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#
#===----------------------------------------------------------------------===##
#
# This is the lit configuration for LLVM libc tests.
#
#===----------------------------------------------------------------------===##

import os
import platform
import re
import subprocess
import sys

import lit.formats
import lit.util

from lit.llvm import llvm_config
from lit.llvm.subst import ToolSubst

# Configuration file for the 'lit' test runner.

# name: The name of this test suite.
config.name = 'libc'

# testFormat: The test format to use to interpret tests.
# We use the ShTest format which runs tests as shell scripts.
config.test_format = lit.formats.ShTest(execute_external=False)

# suffixes: A list of file extensions to treat as test files.
config.suffixes = ['.c', '.cpp']

# excludes: A list of directories to exclude from the testsuite.
config.excludes = ['Inputs', 'CMakeLists.txt', 'README.txt', 'LICENSE.txt']

# test_source_root: The root path where tests are located.
config.test_source_root = os.path.dirname(__file__)

# test_exec_root: The root path where tests should be run.
config.test_exec_root = os.path.join(config.libc_obj_root, 'test')

# Propagate some variables from the config
llvm_config.use_default_substitutions()

# Add tool directories to PATH
config.environment['PATH'] = os.path.pathsep.join([
    config.llvm_tools_dir,
    config.environment.get('PATH', '')
])

# Substitutions for running libc tests
# %libc_test_runner - the test runner executable
config.substitutions.append(('%libc_test_runner', config.libc_test_runner))

# Feature detection based on target
if config.target_triple:
    # Add target-based features
    pass

# Mark whether this is a full build or overlay build
if config.libc_full_build:
    config.available_features.add('libc-full-build')
else:
    config.available_features.add('libc-overlay-build')
