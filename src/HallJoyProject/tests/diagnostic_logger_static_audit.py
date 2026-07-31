#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[1]
source = root / 'HallJoy' / 'debug_log.cpp'
text = source.read_text(encoding='utf-8-sig')
main_text = (root / 'HallJoy' / 'main.cpp').read_text(encoding='utf-8-sig')

writer_start = text.index('static DWORD DebugLogWriterThreadBody()')
writer_end = text.index('\nstatic void DebugLogWriterOnFault', writer_start)
writer = text[writer_start:writer_end]
write_start = text.index('void DebugLog_Write(const wchar_t* fmt')
write_end = text.index('\nvoid DebugLog_WriteBuffered', write_start)
normal_write = text[write_start:write_end]
shutdown_start = text.index('StopResult DebugLog_Shutdown()')
shutdown_end = text.index('\nvoid DebugLog_Write(', shutdown_start)
shutdown = text[shutdown_start:shutdown_end]

checks = {
    'normal log is queued': 'g_pendingLines.emplace_back(line)' in normal_write,
    'normal log has no WriteFile path': 'WriteUtf8Line' not in normal_write,
    'normal log has no forced flush': 'FlushFileBuffers(g_logFile)' not in normal_write and 'FlushFileBuffers(hFile)' not in normal_write,
    'writer owns forced flush': 'FlushFileBuffers(hFile)' in writer,
    'flush is bounded to 250 ms': 'kBufferedFlushIntervalMs = 250' in text,
    'filesystem write occurs after queue unlock': writer.index('ReleaseSRWLockExclusive(&g_logLock)') < writer.index('WriteUtf8Line(hFile, line.c_str())'),
    'shutdown closes producer gate before stop': shutdown.index('g_logReady.store(false') < shutdown.index('g_stopWriter.store(true'),
    'shutdown has no forced thread termination': 'TerminateThread' not in shutdown,
    'shutdown reports an exact lifecycle result': 'StopResult DebugLog_Shutdown()' in text,
    'shutdown uses the fault-injected join policy': 'ObserveWorkerJoin' in shutdown,
    'timeout retains writer resources': 'handles_retained=1' in shutdown and shutdown.index('if (!observedJoin.Completed())') < shutdown.index('g_writerThread = nullptr'),
    'timeout poisons restart': 'MarkPoisoned' in shutdown and 'restart_blocked=1' in shutdown,
    'runtime timeout injection is simulator-only': '#if defined(HALLJOY_ANALOG_SIMULATOR)' in writer and '--halljoy-test-debug-log-stop-timeout' in writer,
    'main skips CRT cleanup after logger poison': 'process_exit.log_poisoned' in main_text and 'TerminateProcess' in main_text,
    'all main shutdown paths inspect the logger result': '(void)DebugLog_Shutdown()' not in main_text and 'ShutdownDebugLogSafely()' in main_text,
    'ordinary production release logging remains disabled': '#if defined(NDEBUG) && !defined(HALLJOY_DIAGNOSTIC) && !defined(HALLJOY_ANALOG_SIMULATOR)' in text,
}
failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(('PASS' if ok else 'FAIL') + ': ' + name)
if failed:
    sys.exit(1)
print('DIAGNOSTIC_LOGGER_STATIC_AUDIT=PASS')
