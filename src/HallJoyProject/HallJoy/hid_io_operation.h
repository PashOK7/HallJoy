#pragma once

// Owns the lifetime of a single overlapped Win32 I/O request.
//
// CancelIoEx() only requests cancellation; it does not guarantee that the
// operation has completed when it returns. The OVERLAPPED structure, event and
// caller-owned buffer therefore have to remain alive until GetOverlappedResult
// observes the final completion (normally ERROR_OPERATION_ABORTED after a
// successful cancellation).
class HidIoOperation final
{
public:
    enum class StartResult
    {
        Completed,
        Pending,
        Failed,
    };

    explicit HidIoOperation(HANDLE handle, HANDLE reusableEvent = nullptr)
        : handle_(handle), event_(reusableEvent), ownsEvent_(reusableEvent == nullptr)
    {
        if (ownsEvent_)
            event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    }

    HidIoOperation(const HidIoOperation&) = delete;
    HidIoOperation& operator=(const HidIoOperation&) = delete;

    ~HidIoOperation()
    {
        // This is a last-resort safety net. Normal paths should explicitly call
        // Finish() or CancelAndDrain() so the final error can be handled.
        if (active_)
            CancelAndDrain(nullptr, nullptr);
        if (ownsEvent_ && event_)
            CloseHandle(event_);
    }

    bool IsValid() const
    {
        return handle_ && handle_ != INVALID_HANDLE_VALUE && event_;
    }

    HANDLE Event() const
    {
        return event_;
    }

    StartResult StartRead(void* buffer, DWORD length, DWORD* outError)
    {
        return StartIo(false, buffer, length, outError);
    }

    StartResult StartWrite(const void* buffer, DWORD length, DWORD* outError)
    {
        return StartIo(true, const_cast<void*>(buffer), length, outError);
    }

    DWORD Wait(DWORD timeoutMs) const
    {
        if (!active_ || !event_)
            return WAIT_FAILED;
        return WaitForSingleObject(event_, timeoutMs);
    }

    bool Finish(DWORD* outTransferred, DWORD* outError, bool wait)
    {
        if (outTransferred) *outTransferred = 0;
        if (outError) *outError = ERROR_SUCCESS;
        if (!active_)
        {
            if (outError) *outError = ERROR_INVALID_FUNCTION;
            return false;
        }

        DWORD transferred = 0;
        BOOL ok = GetOverlappedResult(handle_, &overlapped_, &transferred, wait ? TRUE : FALSE);
        DWORD err = ok ? ERROR_SUCCESS : GetLastError();
        if (!ok && err == ERROR_IO_INCOMPLETE)
        {
            // The caller asked for a non-blocking reap before completion. Keep
            // ownership active so the destructor can still cancel and drain.
            if (outError) *outError = err;
            return false;
        }
        active_ = false;

        if (outTransferred) *outTransferred = transferred;
        if (outError) *outError = err;
        return ok != FALSE;
    }

    // Requests cancellation and then waits until the request has reached a
    // terminal state. Returning false is still safe: the OVERLAPPED is no
    // longer active; outError contains the final completion error.
    bool CancelAndDrain(DWORD* outTransferred, DWORD* outError)
    {
        if (outTransferred) *outTransferred = 0;
        if (outError) *outError = ERROR_SUCCESS;
        if (!active_)
            return true;

        if (!CancelIoEx(handle_, &overlapped_))
        {
            DWORD cancelErr = GetLastError();
            // ERROR_NOT_FOUND means the operation already completed between the
            // wait result and CancelIoEx. It still must be reaped below.
            if (cancelErr != ERROR_NOT_FOUND && outError)
                *outError = cancelErr;
        }

        DWORD transferred = 0;
        BOOL ok = GetOverlappedResult(handle_, &overlapped_, &transferred, TRUE);
        DWORD finalErr = ok ? ERROR_SUCCESS : GetLastError();
        active_ = false;

        if (outTransferred) *outTransferred = transferred;
        if (outError) *outError = finalErr;
        return ok != FALSE || finalErr == ERROR_OPERATION_ABORTED;
    }

private:
    StartResult StartIo(bool write, void* buffer, DWORD length, DWORD* outError)
    {
        if (outError) *outError = ERROR_SUCCESS;
        if (!IsValid() || !buffer || length == 0 || active_)
        {
            if (outError) *outError = ERROR_INVALID_PARAMETER;
            return StartResult::Failed;
        }

        if (!ResetEvent(event_))
        {
            if (outError) *outError = GetLastError();
            return StartResult::Failed;
        }
        overlapped_ = OVERLAPPED{};
        overlapped_.hEvent = event_;

        BOOL ok = write
            ? WriteFile(handle_, buffer, length, nullptr, &overlapped_)
            : ReadFile(handle_, buffer, length, nullptr, &overlapped_);

        if (ok)
        {
            active_ = true;
            return StartResult::Completed;
        }

        DWORD err = GetLastError();
        if (err == ERROR_IO_PENDING)
        {
            active_ = true;
            return StartResult::Pending;
        }

        if (outError) *outError = err;
        return StartResult::Failed;
    }

    HANDLE handle_ = nullptr;
    HANDLE event_ = nullptr;
    bool ownsEvent_ = false;
    bool active_ = false;
    OVERLAPPED overlapped_{};
};
