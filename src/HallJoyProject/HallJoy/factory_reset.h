#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

enum class FactoryResetApplyStatus
{
    None,
    Applied,
    Failed,
};

struct FactoryResetApplyResult
{
    FactoryResetApplyStatus status = FactoryResetApplyStatus::None;
    DWORD nativeError = ERROR_SUCCESS;
    bool rollbackComplete = true;
    std::wstring backupRoot;
};

// Persists an atomic restart-time request. No user data is touched here.
bool FactoryReset_Request(DWORD* errorOut = nullptr);

// Runs before settings/bindings/layouts are loaded. Existing state is moved to
// a recoverable backup as one rollback-capable transaction.
FactoryResetApplyResult FactoryReset_ApplyPending();
