#pragma once

// Early exact-EXE simulator gate. The command is intercepted in production as
// unsupported and never falls through to normal application startup.
bool VigemOutputSelfHostTest_TryRunCommand(int& exitCode) noexcept;
