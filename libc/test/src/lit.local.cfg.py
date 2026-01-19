# -*- Python -*-
#
# Local lit configuration for libc source tests.
# These are compiled test executables, not FileCheck-based tests.
#

# This directory contains compiled test executables
# The test format for these is different from standard ShTest

# For now, we skip automatic discovery in this directory
# Tests here are registered via CMake and run directly
config.unsupported = True
