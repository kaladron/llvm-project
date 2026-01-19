# -*- Python -*-
#
# Local lit configuration for libc ctype tests.
# Overrides the parent's unsupported=True to enable these tests.
#

# Re-enable testing for this directory
config.unsupported = False

# These tests are compiled executables
# The test format expects the executable to be in the build directory
# with the same relative path structure

# Add ctype-specific features if needed
config.available_features.add('ctype')
