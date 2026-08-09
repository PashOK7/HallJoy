CRITIQUE VERIFICATION MATERIALS
===============================

MAD68Pro_A8_A9_A0_Approach_Critique.md is an independent critique of the
approach. The resolution of every material claim against raw firmware bytes and
a physical hardware log is recorded in:

  ..\archive\historical-notes\MAD68PRO_R_NATIVE_V3_5_CRITIQUE_RESOLUTION.md

Reproducible automated verification:

  src\HallJoyProject\tests\mad68pr_a8_a9_state_verify.py

The verifier additionally requires:

- the exact firmware image with SHA-256
  2a7df4ffc491476b79da5333f51179f68fbbb5c5e146e987cfb5df764b79cead;
- the physical `HallJoyMAD68ProR.log` capture.
