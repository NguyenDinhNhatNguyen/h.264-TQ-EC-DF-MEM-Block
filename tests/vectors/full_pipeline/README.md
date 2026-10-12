# Full-pipeline harness vectors

These synthetic vectors are generated within the project, not H.264 data encoded by a reference encoder.
source.hex contains 768 bytes: two 16x16 YUV420 frames, with byte i = i mod 256.

stub_expected.hex: little-endian 32-bit FNV-1a for each 384-byte tile, followed by the EOS fixture
00 00 01 0B. reference.hex is the second frame, placed at offset 768 (slot 2).
contract_expected.hex: 20 consecutive bytes 01..14 from the variable-length test adapter.
bad_expected.hex deliberately changes the first byte for a negative test; it is not valid golden data.

Each case runs two activations to check that counters/output pointers reset correctly.
Do not use these fixture vectors as golden data for the real backend. Real cases require independent
source/golden data and an explicit release configuration/revision, as described in the testbench guide.
