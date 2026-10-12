# Module tests

This directory holds module unit tests. The regression currently exercises the top-level through
`tests/vp_tests.cpp`, including dma_32, dma_64, dma_128, completion, fault.
When adding a unit test, register its executable/test in this module's CMakeLists.txt.
