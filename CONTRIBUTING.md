# Contributing

Thanks for contributing to HallJoy.

## License Terms for Contributions

By submitting any contribution (pull request, patch, or direct commit), you agree that:

1. Your contribution is licensed under `AGPL-3.0` for the open-source version of HallJoy.
2. You grant the project owner a perpetual, worldwide, non-exclusive, royalty-free right to use, modify, relicense, and distribute your contribution under commercial license terms.

If you do not agree with these terms, do not submit contributions.

## Practical Workflow

1. Fork the repo.
2. Create a feature branch.
3. Open a pull request with a clear description of the change.

## Optional Sign-off

You may include a sign-off line in commits:

`Signed-off-by: Your Name <you@example.com>`

## Native analogue protocols

Do not add protocol-specific lifecycle, raw-read, UI, curve or ViGEm branches.
Start with `docs/development/ARCHITECTURE_OVERVIEW.md`, generate a standalone
module with `tools/new_native_backend.py`, and complete the new-protocol pull request
checklist. Run `python tools/run_native_backend_checks.py --require-compiler` and a
Windows Release x64 `BUILD.cmd` before submitting.
