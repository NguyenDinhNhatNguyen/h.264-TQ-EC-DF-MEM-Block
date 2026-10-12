# Encoder top-level

Owner: Huy Nguyen Huynh Quoc.

Chapter 3: top-level assembly, pipeline stub and multi-module verification.

CMake target: `h264_top`. Public headers: h264/top/encoder_vp.h, h264/top/pipeline_factory.h.

After refactoring, `h264_top` contains only the top-level wiring; `h264_pipeline_stub` is a separate target containing the stub.
`FrameExecutorIf` lives in interfaces; `PipelineFactory` selects the assembly during elaboration.
The stub header is exported by h264_pipeline_stub and is not part of the h264_top API.
Instructions for adding released modules and the full-pipeline testbench:
[full_pipeline_testbench.md](docs/full_pipeline_testbench.md).

Regression tests currently live in tests because they require host + DDR + top. Run from components/h264:
`ctest --test-dir build/vp --output-on-failure`. Relevant tests: the complete CTest regression.

Read `interfaces/docs/interface_contracts.md` before
changing an interface. All headers use .h. Do not duplicate source files across tasks.

