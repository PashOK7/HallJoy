[CmdletBinding()]
param(
    [Parameter(Mandatory = $false)]
    [string]$SoupRoot = (Join-Path (Split-Path -Parent $PSScriptRoot) 'Soup')
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$source = Join-Path $SoupRoot 'soup\AnalogueKeyboard.cpp'
$header = Join-Path $SoupRoot 'soup\AnalogueKeyboard.hpp'
$handleHeader = Join-Path $SoupRoot 'soup\HandleRaii.hpp'
$hidSource = Join-Path $SoupRoot 'soup\hwHid.cpp'
$hidHeader = Join-Path $SoupRoot 'soup\hwHid.hpp'
if (-not (Test-Path -LiteralPath $source) -or -not (Test-Path -LiteralPath $header) -or
    -not (Test-Path -LiteralPath $handleHeader) -or -not (Test-Path -LiteralPath $hidSource) -or
    -not (Test-Path -LiteralPath $hidHeader)) {
    throw "Soup source tree is incomplete below: $SoupRoot\soup"
}

$sourceText = [System.IO.File]::ReadAllText($source)
$fixedMarker = 'HallJoy Madlions fix v7'

# Soup's constructor used to clear only nuphy.buffer. That does not necessarily
# cover the complete union when the Madlions member contains state, a pointer,
# and a full key buffer. Match semantically, not by line endings or indentation.
$fixedInitialiser = 'memset(&razer, 0, sizeof(madlions)); // HallJoy: zero the complete analogue-keyboard state union'
if (-not $sourceText.Contains($fixedInitialiser)) {
    $initialiserPattern = 'memset\s*\(\s*nuphy\.buffer\s*,\s*0\s*,\s*sizeof\s*\(\s*nuphy\.buffer\s*\)\s*\)\s*;[^\r\n]*'
    $initialiserMatch = [System.Text.RegularExpressions.Regex]::Match($sourceText, $initialiserPattern)
    if (-not $initialiserMatch.Success) {
        throw 'Could not locate the AnalogueKeyboard union initialiser in AnalogueKeyboard.cpp.'
    }
    $sourceText = $sourceText.Remove($initialiserMatch.Index, $initialiserMatch.Length).Insert($initialiserMatch.Index, $fixedInitialiser)
}

if (-not $sourceText.Contains($fixedMarker)) {
    $startPattern = '(?m)^[\t ]*std::vector<ActiveKey>[\t ]+AnalogueKeyboard::getActiveKeysMadlions\(\)[\t ]*\r?$'
    $startMatch = [System.Text.RegularExpressions.Regex]::Match($sourceText, $startPattern)
    if (-not $startMatch.Success) {
        throw 'Could not locate the getActiveKeysMadlions() signature in AnalogueKeyboard.cpp.'
    }

    $tail = $sourceText.Substring($startMatch.Index)
    $endPattern = '(?m)^[\t ]*uint8_t[\t ]+AnalogueKeyboard::ActiveKey::getHidScancode\(\)[\t ]+const[\t ]+noexcept[\t ]*\r?$'
    $endMatch = [System.Text.RegularExpressions.Regex]::Match($tail, $endPattern)
    if (-not $endMatch.Success) {
        throw 'Could not locate the function following getActiveKeysMadlions().' 
    }

    $start = $startMatch.Index
    $end = $start + $endMatch.Index

    $fixedFunction = @'
	extern "C" void halljoy_plugin_checkpoint(int);
	extern "C" void halljoy_plugin_transport_error(uint32_t);

	std::vector<ActiveKey> AnalogueKeyboard::getActiveKeysMadlions()
	{
		halljoy_plugin_checkpoint(300); // madlions_entry
		// HallJoy Madlions fix v7:
		// - use a dedicated request/response transport with unique manual-reset
		//   events and fully drained OVERLAPPED lifetimes;
		// - never index the incomplete final layout chunk out of bounds;
		// - reject short HID responses before parsing them;
		// - keep transient failure state in the individual keyboard object;
		// - clamp travel values and avoid leaving stale pressed keys.
		std::vector<ActiveKey> keys{};

#if SOUP_WINDOWS
		halljoy_plugin_checkpoint(310); // madlions_before_mutex
		struct HallJoyNamedMutexGuard
		{
			NamedMutex& mutex;
			explicit HallJoyNamedMutexGuard(NamedMutex& value) : mutex(value) { mutex.lock(); }
			~HallJoyNamedMutexGuard() { mutex.unlock(); }
		};
		static NamedMutex mtx("MadlionsMtx");
		HallJoyNamedMutexGuard mutex_guard(mtx);
#endif

#if SOUP_DIGITALKEYBOARD_AVAILABLE && !defined(UAP_MADLIONS_DISABLE_DIGITAL_ASSIST)
		static DigitalKeyboard dkbd;
#if SOUP_WINDOWS
		static bool dkbd_okay = false;
#endif
		dkbd.update();
#endif

		if (madlions.layout == nullptr || madlions.layout_size == 0)
		{
			return keys;
		}

		uint8_t report[33];
		memset(report, 0, sizeof(report));
		report[1] = 0x02;
		report[2] = 0x96;
		report[3] = 0x1C;
		report[8] = 4;

		for (uint16_t offset = 0; offset < madlions.layout_size; offset += 4)
		{
			bool should_request_this_chunk = false;
			for (uint8_t i = 0; i != 4; ++i)
			{
				const uint16_t index = static_cast<uint16_t>(offset) + i;
				if (index >= madlions.layout_size)
				{
					break;
				}

				const auto sk = madlions.layout[index];
				if (sk != KEY_NONE)
				{
					if (
#if SOUP_DIGITALKEYBOARD_AVAILABLE && !defined(UAP_MADLIONS_DISABLE_DIGITAL_ASSIST)
						dkbd.keys[sk] ||
#endif
						madlions.buffer[sk] || (offset >> 4) == madlions.state
						)
					{
						should_request_this_chunk = true;
					}
				}
			}

			if (!should_request_this_chunk)
			{
				for (uint8_t i = 0; i != 4; ++i)
				{
					const uint16_t index = static_cast<uint16_t>(offset) + i;
					if (index >= madlions.layout_size)
					{
						break;
					}
					const auto sk = madlions.layout[index];
					if (sk != KEY_NONE && madlions.buffer[sk] != 0)
					{
						keys.emplace_back(ActiveKey{ sk, static_cast<float>(madlions.buffer[sk]) / 255.0f });
					}
				}
				continue;
			}

			report[7] = static_cast<uint8_t>(offset);
			halljoy_plugin_checkpoint(320); // madlions_transport_begin
			const Buffer<>& resp = hid.transactReport(report, sizeof(report), 100);
			halljoy_plugin_checkpoint(350); // madlions_after_transaction

			// 7 header bytes + 4 records * (3 metadata bytes + u16 travel) = 27.
			if (resp.size() < 27)
			{
#if SOUP_WINDOWS
				halljoy_plugin_transport_error(13u); // ERROR_INVALID_DATA
#endif
				if (madlions.consecutive_failed_reports != 0xff)
				{
					++madlions.consecutive_failed_reports;
				}

				for (uint8_t i = 0; i != 4; ++i)
				{
					const uint16_t index = static_cast<uint16_t>(offset) + i;
					if (index >= madlions.layout_size)
					{
						break;
					}
					const auto sk = madlions.layout[index];
					if (sk != KEY_NONE)
					{
						madlions.buffer[sk] = 0;
					}
				}

				// A persistent transport failure is handled by the isolated host:
				// read_full_buffer returns an error and the supervisor starts a
				// completely fresh process/handle. A single transient timeout is
				// tolerated without interrupting gameplay.
				if (madlions.consecutive_failed_reports >= 8)
				{
					disconnected = true;
				}
				break;
			}
			madlions.consecutive_failed_reports = 0;
			halljoy_plugin_checkpoint(360); // madlions_parse

			for (uint8_t i = 0; i != 4; ++i)
			{
				const uint16_t index = static_cast<uint16_t>(offset) + i;
				if (index >= madlions.layout_size)
				{
					break;
				}

				const size_t value_offset = 7 + static_cast<size_t>(i) * 5 + 3;
				const uint16_t travel = (static_cast<uint16_t>(resp.at(value_offset)) << 8)
					| static_cast<uint16_t>(resp.at(value_offset + 1));
				const uint16_t clamped_travel = travel > 350 ? 350 : travel;

				const auto sk = madlions.layout[index];
				if (sk != KEY_NONE)
				{
					const auto fvalue = static_cast<float>(clamped_travel) / 350.0f;
					madlions.buffer[sk] = static_cast<uint8_t>(fvalue * 255.0f);
					if (clamped_travel != 0)
					{
						keys.emplace_back(ActiveKey{ sk, fvalue });

#if SOUP_WINDOWS && SOUP_DIGITALKEYBOARD_AVAILABLE && !defined(UAP_MADLIONS_DISABLE_DIGITAL_ASSIST)
						if (!dkbd_okay && clamped_travel == 350)
						{
							if (dkbd.keys[sk])
							{
								dkbd_okay = true;
							}
							else
							{
								dkbd.deinit();
							}
						}
#endif
					}
				}
			}
		}

		const uint8_t state_count = static_cast<uint8_t>((static_cast<uint16_t>(madlions.layout_size) + 15) / 16);
		if (state_count != 0 && ++madlions.state >= state_count)
		{
			madlions.state = 0;
		}

		halljoy_plugin_checkpoint(370); // madlions_return
		return keys;
	}
'@

    if (-not $fixedFunction.Contains($fixedMarker)) {
        throw 'Internal error: the embedded Madlions patch marker does not match fixedMarker.'
    }

    $sourceText = $sourceText.Substring(0, $start) + $fixedFunction + "`r`n`r`n" + $sourceText.Substring($end)
}

# Store transient failure state in each Madlions object. thread_local was not
# per-device once polling moved into the single SDK/host thread.
$headerText = [System.IO.File]::ReadAllText($header)
if ($headerText -notmatch 'uint8_t\s+consecutive_failed_reports') {
    $madlionsPattern = '(uint8_t\s+state\s*;\s*)(uint8_t\s+layout_size\s*;)'
    $madlionsMatch = [System.Text.RegularExpressions.Regex]::Match($headerText, $madlionsPattern)
    if (-not $madlionsMatch.Success) {
        throw 'Could not locate the Madlions state block in AnalogueKeyboard.hpp.'
    }
    $replacement = $madlionsMatch.Groups[1].Value + "uint8_t consecutive_failed_reports;`r`n`t`t`t`t" + $madlionsMatch.Groups[2].Value
    $headerText = $headerText.Remove($madlionsMatch.Index, $madlionsMatch.Length).Insert($madlionsMatch.Index, $replacement)
}

# Fix explicit-destructor move assignment in Soup HandleRaii. Calling the
# destructor and then assigning members without placement-new ends the object
# lifetime and is undefined C++. This object owns every Windows HID handle.
$handleText = [System.IO.File]::ReadAllText($handleHeader)
if ($handleText -notmatch 'operator=\s*\(\s*HANDLE\s+new_handle') {
    $handleStart = [System.Text.RegularExpressions.Regex]::Match($handleText, '(?m)^[\t ]*void\s+operator=\s*\(\s*HANDLE\s+h\s*\)\s*noexcept\s*\r?$')
    if (-not $handleStart.Success) { throw 'Could not locate HandleRaii HANDLE assignment.' }
    $afterHandle = $handleText.Substring($handleStart.Index)
    $moveStartRelative = [System.Text.RegularExpressions.Regex]::Match($afterHandle, '(?m)^[\t ]*void\s+operator=\s*\(\s*HandleRaii&&\s+b\s*\)\s*noexcept\s*\r?$')
    if (-not $moveStartRelative.Success) { throw 'Could not locate HandleRaii move assignment.' }
    $moveStart = $handleStart.Index + $moveStartRelative.Index

    $assignHandleReplacement = @'
		void operator=(HANDLE new_handle) noexcept
		{
			if (h != new_handle)
			{
				if (isValid())
				{
					CloseHandle(h);
				}
				h = new_handle;
			}
		}

'@
    $handleText = $handleText.Substring(0, $handleStart.Index) + $assignHandleReplacement + $handleText.Substring($moveStart)
}

if ($handleText -notmatch 'operator=\s*\(\s*HandleRaii&&\s+b\s*\)[\s\S]*?this\s*!=\s*&b') {
    $moveStart = [System.Text.RegularExpressions.Regex]::Match($handleText, '(?m)^[\t ]*void\s+operator=\s*\(\s*HandleRaii&&\s+b\s*\)\s*noexcept\s*\r?$')
    if (-not $moveStart.Success) { throw 'Could not locate HandleRaii move assignment after first patch.' }
    $afterMove = $handleText.Substring($moveStart.Index)
    $boolStartRelative = [System.Text.RegularExpressions.Regex]::Match($afterMove, '(?m)^[\t ]*\[\[nodiscard\]\]\s+operator\s+bool\(\)\s+const\s+noexcept\s*\r?$')
    if (-not $boolStartRelative.Success) { throw 'Could not locate function following HandleRaii move assignment.' }
    $boolStart = $moveStart.Index + $boolStartRelative.Index

    $assignMoveReplacement = @'
		void operator=(HandleRaii&& b) noexcept
		{
			if (this != &b)
			{
				if (isValid())
				{
					CloseHandle(h);
				}
				h = b.h;
				b.h = INVALID_HANDLE_VALUE;
			}
		}

'@
    $handleText = $handleText.Substring(0, $moveStart.Index) + $assignMoveReplacement + $handleText.Substring($boolStart)
}
# Apply the same lifetime fix to the POSIX handle branch so the patched Soup
# remains valid for future Linux builds as well.
if ($handleText -notmatch 'operator=\s*\(\s*int\s+new_handle') {
    $posixStart = [System.Text.RegularExpressions.Regex]::Match($handleText, '(?m)^[\t ]*void\s+operator=\s*\(\s*int\s+handle\s*\)\s*noexcept\s*\r?$')
    if (-not $posixStart.Success) { throw 'Could not locate POSIX HandleRaii assignment.' }
    $afterPosix = $handleText.Substring($posixStart.Index)
    $posixMoveRelative = [System.Text.RegularExpressions.Regex]::Match($afterPosix, '(?m)^[\t ]*void\s+operator=\s*\(\s*HandleRaii&&\s+b\s*\)\s*noexcept\s*\r?$')
    if (-not $posixMoveRelative.Success) { throw 'Could not locate POSIX HandleRaii move assignment.' }
    $posixMoveStart = $posixStart.Index + $posixMoveRelative.Index
    $posixAssignReplacement = @'
		void operator=(int new_handle) noexcept
		{
			if (handle != new_handle)
			{
				if (isValid())
				{
					::close(handle);
				}
				handle = new_handle;
			}
		}

'@
    $handleText = $handleText.Substring(0, $posixStart.Index) + $posixAssignReplacement + $handleText.Substring($posixMoveStart)
}

# Locate the remaining POSIX move assignment after the already-patched Windows
# one and replace it through the following int conversion operator.
$posixMoveMatches = [System.Text.RegularExpressions.Regex]::Matches($handleText, '(?m)^[\t ]*void\s+operator=\s*\(\s*HandleRaii&&\s+b\s*\)\s*noexcept\s*\r?$')
if ($posixMoveMatches.Count -ge 2) {
    $posixMoveStart = $posixMoveMatches[$posixMoveMatches.Count - 1]
    $afterPosixMove = $handleText.Substring($posixMoveStart.Index)
    if ($afterPosixMove -notmatch 'this\s*!=\s*&b') {
        $intConversionRelative = [System.Text.RegularExpressions.Regex]::Match($afterPosixMove, '(?m)^[\t ]*operator\s+int\(\)\s+const\s+noexcept\s*\r?$')
        if (-not $intConversionRelative.Success) { throw 'Could not locate function following POSIX HandleRaii move assignment.' }
        $intConversionStart = $posixMoveStart.Index + $intConversionRelative.Index
        $posixMoveReplacement = @'
		void operator=(HandleRaii&& b) noexcept
		{
			if (this != &b)
			{
				if (isValid())
				{
					::close(handle);
				}
				handle = b.handle;
				b.handle = -1;
			}
		}

'@
        $handleText = $handleText.Substring(0, $posixMoveStart.Index) + $posixMoveReplacement + $handleText.Substring($intConversionStart)
    }
}
if ($handleText -match 'this->~HandleRaii\(\)') {
    throw 'Internal error: unsafe HandleRaii explicit-destructor assignment remains.'
}

# Windows APIs use both INVALID_HANDLE_VALUE (CreateFile) and NULL
# (CreateEvent/CreateMutex) as invalid handle values.
$handleText = [System.Text.RegularExpressions.Regex]::Replace(
    $handleText,
    'return\s+h\s*!=\s*INVALID_HANDLE_VALUE\s*;',
    'return h != INVALID_HANDLE_VALUE && h != NULL;',
    1)
if ($handleText -notmatch 'h\s*!=\s*INVALID_HANDLE_VALUE\s*&&\s*h\s*!=\s*NULL') {
    throw 'Internal error: Windows HandleRaii NULL-handle validation is missing.'
}

# Add a dedicated, formally-correct request/response operation to hwHid.
# It uses one manual-reset event per OVERLAPPED operation, does not return
# while an I/O request still references its context/buffer, and safely drains
# queued stale reports before sending the Madlions request.
$hidHeaderText = [System.IO.File]::ReadAllText($hidHeader)
if ($hidHeaderText -notmatch 'transactReport\s*\(') {
    $sendDecl = [System.Text.RegularExpressions.Regex]::Match(
        $hidHeaderText,
        '(?m)^[\t ]*bool\s+sendReport\s*\(\s*Buffer<>\s*&&\s*buf\s*\)\s*const\s+noexcept\s*;\s*$')
    if (-not $sendDecl.Success) {
        throw 'Could not locate hwHid::sendReport declaration in hwHid.hpp.'
    }
    $decl = @'
		// HallJoy: safe request/response transaction for poll-only vendor HID
		// protocols. On Windows every operation owns a distinct manual-reset event
		// and is completed or cancelled-and-drained before the context is reused.
		[[nodiscard]] const Buffer<>& transactReport(const void* data, size_t size, uint32_t timeout_ms) noexcept;

'@
    $hidHeaderText = $hidHeaderText.Insert($sendDecl.Index, $decl)
}

$hidSourceText = [System.IO.File]::ReadAllText($hidSource)

# The native MAD68 backend must be the only process that ever opens the Pro R
# vendor interface. AnalogueKeyboard::getAll() filters supported devices only
# after hwHid::getAll() has already opened every HID path, so a filter in the
# plugin main loop is too late. Insert the exact VID/PID path exclusion before
# CreateFileW. The block is compile-time gated and therefore changes only the
# dedicated *-mad68native Sun targets; ordinary UAP builds enumerate normally.
$preOpenMarker = 'HallJoy native analogue pre-open exclusion'
if (-not $hidSourceText.Contains($preOpenMarker)) {
    $enumerationLoop = [System.Text.RegularExpressions.Regex]::Match(
        $hidSourceText,
        '(?m)^(?<indent>[\t ]*)for\s*\(\s*const\s+wchar_t\*\s+device_interface\s*=\s*device_interface_list\s*;[^\r\n]*\)\s*\r?$')
    if (-not $enumerationLoop.Success) {
        throw 'Could not locate the Windows hwHid device-interface enumeration loop.'
    }

    $lineEnd = $hidSourceText.IndexOf("`n", $enumerationLoop.Index + $enumerationLoop.Length)
    if ($lineEnd -lt 0) {
        throw 'Could not locate the body of the Windows hwHid enumeration loop.'
    }
    $braceStart = $hidSourceText.IndexOf('{', $enumerationLoop.Index + $enumerationLoop.Length)
    if ($braceStart -lt 0 -or $braceStart -gt ($lineEnd + 8)) {
        # Soup places the opening brace on the next line.
        $braceStart = $hidSourceText.IndexOf('{', $lineEnd)
    }
    if ($braceStart -lt 0) {
        throw 'Could not locate the opening brace of the Windows hwHid enumeration loop.'
    }

    $indent = $enumerationLoop.Groups['indent'].Value + "`t"
    $preOpenBlock = @"

${indent}#if defined(UAP_EXCLUDE_HALLJOY_NATIVE)
${indent}// HallJoy native analogue pre-open exclusion.
${indent}// The parent process performs protocol-specific capability proofs and
${indent}// publishes exact VID/PID tokens.
${indent}// This gate runs before CreateFileW, so UAP never opens a native-owned HID.
${indent}const auto halljoy_path_contains_ci = [](const wchar_t* value, const wchar_t* token) noexcept
${indent}{
${indent}`tif (value == nullptr || token == nullptr || *token == L'\0') return false;
${indent}`tconst size_t token_length = wcslen(token);
${indent}`tfor (; *value != L'\0'; ++value)
${indent}`t{
${indent}`t`tif (_wcsnicmp(value, token, token_length) == 0) return true;
${indent}`t}
${indent}`treturn false;
${indent}};
${indent}wchar_t halljoy_native_ids[2048]{};
${indent}const DWORD halljoy_native_chars = GetEnvironmentVariableW(
${indent}`tL"HALLJOY_UAP_NATIVE_HID_IDS", halljoy_native_ids,
${indent}`tstatic_cast<DWORD>(sizeof(halljoy_native_ids) / sizeof(halljoy_native_ids[0])));
${indent}if (halljoy_native_chars != 0 &&
${indent}`thalljoy_native_chars < (sizeof(halljoy_native_ids) / sizeof(halljoy_native_ids[0])))
${indent}{
${indent}`tbool halljoy_exclude_path = false;
${indent}`twchar_t* context = nullptr;
${indent}`tfor (wchar_t* token = wcstok_s(halljoy_native_ids, L";", &context);
${indent}`t`ttoken != nullptr; token = wcstok_s(nullptr, L";", &context))
${indent}`t{
${indent}`t`tif (halljoy_path_contains_ci(device_interface, token))
${indent}`t`t{
${indent}`t`t`thalljoy_exclude_path = true;
${indent}`t`t`tbreak;
${indent}`t`t}
${indent}`t}
${indent}`tif (halljoy_exclude_path) continue;
${indent}}
${indent}#endif
"@
    $hidSourceText = $hidSourceText.Insert($braceStart + 1, $preOpenBlock)
}
if ($hidSourceText -notmatch 'HallJoySafeHidContext') {
    $sendBufferSignature = [System.Text.RegularExpressions.Regex]::Match(
        $hidSourceText,
        '(?m)^[\t ]*bool\s+hwHid::sendReport\s*\(\s*Buffer<>\s*&&\s*buf\s*\)\s*const\s+noexcept\s*\r?$')
    if (-not $sendBufferSignature.Success) {
        throw 'Could not locate hwHid::sendReport(Buffer&&) in hwHid.cpp.'
    }

    $safeImplementation = @'
#if SOUP_WINDOWS
	extern "C" void halljoy_plugin_checkpoint(int);
	extern "C" void halljoy_plugin_transport_error(uint32_t);

	namespace
	{
		struct HallJoySafeHidContext
		{
			HandleRaii read_event{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
			HandleRaii write_event{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
			OVERLAPPED read_overlapped{};
			OVERLAPPED write_overlapped{};
			DWORD last_error = ERROR_SUCCESS;
			bool poisoned = false;

			void set_error(DWORD error) noexcept
			{
				last_error = error;
				halljoy_plugin_transport_error(error);
			}

			[[nodiscard]] bool valid() const noexcept
			{
				return !poisoned && read_event.isValid() && write_event.isValid();
			}

			[[nodiscard]] static bool prepare(OVERLAPPED& overlapped, HANDLE event) noexcept
			{
				ZeroMemory(&overlapped, sizeof(overlapped));
				overlapped.hEvent = event;
				return ResetEvent(event) != FALSE;
			}
		};

		[[nodiscard]] static bool halljoy_finish_io(
			HANDLE device,
			OVERLAPPED& overlapped,
			HANDLE event,
			DWORD timeout_ms,
			DWORD& transferred,
			int wait_checkpoint,
			int cancel_checkpoint,
			HallJoySafeHidContext& context) noexcept
		{
			halljoy_plugin_checkpoint(wait_checkpoint);
			const DWORD wait_result = WaitForSingleObject(event, timeout_ms);
			if (wait_result == WAIT_OBJECT_0)
			{
				if (GetOverlappedResult(device, &overlapped, &transferred, FALSE))
				{
					return true;
				}

				const DWORD completion_error = GetLastError();
				context.set_error(completion_error);
				if (completion_error != ERROR_IO_INCOMPLETE)
				{
					// The operation completed with an error; the context is no longer
					// owned by the kernel and may be reused.
					return false;
				}
			}
			else
			{
				context.set_error(wait_result == WAIT_TIMEOUT ? ERROR_TIMEOUT : GetLastError());
			}

			halljoy_plugin_checkpoint(cancel_checkpoint);

			// CancelIoEx only requests cancellation. Do not clear, reuse, destroy, or
			// return from the context while the kernel may still reference the
			// OVERLAPPED structure or its buffer.
			if (!CancelIoEx(device, &overlapped))
			{
				const DWORD cancel_error = GetLastError();
				if (cancel_error != ERROR_NOT_FOUND)
				{
					context.set_error(cancel_error);
				}
			}

			DWORD completion_bytes = 0;
			if (GetOverlappedResult(device, &overlapped, &completion_bytes, TRUE))
			{
				transferred = completion_bytes;
			}
			else
			{
				const DWORD completion_error = GetLastError();
				if (completion_error != ERROR_OPERATION_ABORTED)
				{
					context.set_error(completion_error);
					context.poisoned = true;
				}
			}
			return false;
		}

		[[nodiscard]] static bool halljoy_start_and_finish_write(
			HANDLE device,
			const void* data,
			DWORD size,
			HallJoySafeHidContext& context,
			DWORD timeout_ms) noexcept
		{
			halljoy_plugin_checkpoint(330); // madlions_write_begin
			if (!HallJoySafeHidContext::prepare(context.write_overlapped, context.write_event))
			{
				context.set_error(GetLastError());
				context.poisoned = true;
				return false;
			}

			DWORD written = 0;
			if (WriteFile(device, data, size, &written, &context.write_overlapped))
			{
				halljoy_plugin_checkpoint(332); // madlions_write_complete
				if (written != size)
				{
					context.set_error(ERROR_WRITE_FAULT);
					return false;
				}
				return true;
			}

			const DWORD error = GetLastError();
			if (error != ERROR_IO_PENDING)
			{
				context.set_error(error);
				return false;
			}

			const bool completed = halljoy_finish_io(device, context.write_overlapped,
				context.write_event, timeout_ms, written, 331, 343, context);
			halljoy_plugin_checkpoint(332); // madlions_write_complete
			if (!completed)
			{
				return false;
			}
			if (written != size)
			{
				context.set_error(ERROR_WRITE_FAULT);
				return false;
			}
			return true;
		}

		[[nodiscard]] static bool halljoy_start_and_finish_read(
			HANDLE device,
			uint8_t* data,
			DWORD capacity,
			HallJoySafeHidContext& context,
			DWORD timeout_ms,
			DWORD& read_bytes) noexcept
		{
			halljoy_plugin_checkpoint(321); // madlions_read_arm
			if (!HallJoySafeHidContext::prepare(context.read_overlapped, context.read_event))
			{
				context.set_error(GetLastError());
				context.poisoned = true;
				return false;
			}

			read_bytes = 0;
			if (ReadFile(device, data, capacity, &read_bytes, &context.read_overlapped))
			{
				halljoy_plugin_checkpoint(341); // madlions_read_complete
				return true;
			}

			const DWORD error = GetLastError();
			if (error != ERROR_IO_PENDING)
			{
				context.set_error(error);
				return false;
			}

			halljoy_plugin_checkpoint(323); // madlions_read_pending
			const bool ok = halljoy_finish_io(device, context.read_overlapped,
				context.read_event, timeout_ms, read_bytes, 340, 342, context);
			if (ok)
			{
				halljoy_plugin_checkpoint(341); // madlions_read_complete
			}
			return ok;
		}

		[[nodiscard]] static bool halljoy_drain_stale_reports(
			HANDLE device,
			uint8_t* data,
			DWORD capacity,
			HallJoySafeHidContext& context) noexcept
		{
			for (uint8_t stale = 0; stale != 32; ++stale)
			{
				if (!HallJoySafeHidContext::prepare(context.read_overlapped, context.read_event))
				{
					context.set_error(GetLastError());
					context.poisoned = true;
					return false;
				}

				DWORD ignored = 0;
				halljoy_plugin_checkpoint(321); // madlions_read_arm
				if (ReadFile(device, data, capacity, &ignored, &context.read_overlapped))
				{
					halljoy_plugin_checkpoint(322); // madlions_stale_report_discarded
					continue;
				}

				const DWORD error = GetLastError();
				if (error != ERROR_IO_PENDING)
				{
					context.set_error(error);
					return false;
				}

				// No stale report was queued. Cancel and fully drain this probe before
				// starting the request. Therefore the request write and response read
				// are never concurrent and never share a completion signal.
				halljoy_plugin_checkpoint(342); // madlions_cancel_read
				if (!CancelIoEx(device, &context.read_overlapped))
				{
					const DWORD cancel_error = GetLastError();
					if (cancel_error != ERROR_NOT_FOUND)
					{
						context.set_error(cancel_error);
					}
				}
				if (!GetOverlappedResult(device, &context.read_overlapped, &ignored, TRUE))
				{
					const DWORD completion_error = GetLastError();
					if (completion_error != ERROR_OPERATION_ABORTED)
					{
						context.set_error(completion_error);
						context.poisoned = true;
						return false;
					}
				}
				return true;
			}

			context.set_error(ERROR_BUSY);
			context.poisoned = true;
			return false;
		}
	}
#endif

	const Buffer<>& hwHid::transactReport(const void* data, size_t size, uint32_t timeout_ms) noexcept
	{
#if SOUP_WINDOWS
		static thread_local HallJoySafeHidContext context{};
		read_buffer.clear();
		context.set_error(ERROR_SUCCESS);

		if (!context.valid() || disconnected || data == nullptr || size == 0
			|| size > MAXDWORD || read_buffer.capacity() == 0
			|| read_buffer.capacity() > MAXDWORD || pending_read != 0)
		{
			context.set_error(ERROR_INVALID_STATE);
			halljoy_plugin_checkpoint(349); // madlions_transport_rejected
			return read_buffer;
		}

		if (!halljoy_drain_stale_reports(handle, read_buffer.data(),
			static_cast<DWORD>(read_buffer.capacity()), context))
		{
			const DWORD error = context.last_error;
			if (error == ERROR_DEVICE_NOT_CONNECTED || error == ERROR_INVALID_HANDLE)
			{
				disconnected = true;
			}
			halljoy_plugin_checkpoint(349); // madlions_transport_failed
			return read_buffer;
		}
		halljoy_plugin_checkpoint(322); // stale queue drained

		// The vendor request is completed before the response read is started.
		// Windows HID queues input reports, and this strictly serial state machine
		// avoids every shared-handle or overlapping-lifetime ambiguity.
		if (!halljoy_start_and_finish_write(handle, data, static_cast<DWORD>(size),
			context, timeout_ms))
		{
			const DWORD error = context.last_error;
			if (error == ERROR_DEVICE_NOT_CONNECTED || error == ERROR_INVALID_HANDLE)
			{
				disconnected = true;
			}
			halljoy_plugin_checkpoint(349);
			return read_buffer;
		}

		DWORD read_bytes = 0;
		if (!halljoy_start_and_finish_read(handle, read_buffer.data(),
			static_cast<DWORD>(read_buffer.capacity()), context, timeout_ms, read_bytes))
		{
			const DWORD error = context.last_error;
			if (error == ERROR_DEVICE_NOT_CONNECTED || error == ERROR_INVALID_HANDLE)
			{
				disconnected = true;
			}
			halljoy_plugin_checkpoint(349);
			return read_buffer;
		}

		if (read_bytes > read_buffer.capacity())
		{
			context.set_error(ERROR_INVALID_DATA);
			context.poisoned = true;
			halljoy_plugin_checkpoint(349);
			return read_buffer;
		}
		read_buffer.resize(read_bytes);

		// Match receiveReport() semantics: Windows prepends report-id zero for
		// unnumbered HID reports, while Soup callers expect it removed.
		if (!read_buffer.empty() && read_buffer.at(0) == 0)
		{
			read_buffer.erase(0, 1);
		}

		context.set_error(ERROR_SUCCESS);
		halljoy_plugin_checkpoint(348); // madlions_transport_return
		return read_buffer;
#else
		discardStaleReports();
		if (!sendReport(data, size))
		{
			read_buffer.clear();
			return read_buffer;
		}
		return receiveReport();
#endif
	}


'@
    $hidSourceText = $hidSourceText.Insert($sendBufferSignature.Index, $safeImplementation)
}

# Fix generic Windows writes as well. A unique manual-reset event removes the
# ambiguity of waiting on a file handle while a read and write are outstanding.
$rawSendStart = [System.Text.RegularExpressions.Regex]::Match(
    $hidSourceText,
    '(?m)^[\t ]*bool\s+hwHid::sendReport\s*\(\s*const\s+void\*\s+data\s*,\s*size_t\s+size\s*\)\s*const\s+noexcept\s*\r?$')
if (-not $rawSendStart.Success) {
    throw 'Could not locate hwHid::sendReport(const void*, size_t).'
}
$afterRawSend = $hidSourceText.Substring($rawSendStart.Index)
$featureStartRelative = [System.Text.RegularExpressions.Regex]::Match(
    $afterRawSend,
    '(?m)^[\t ]*bool\s+hwHid::sendFeatureReport\s*\(\s*Buffer<>\s*&&\s*buf\s*\)\s*const\s+noexcept\s*\r?$')
if (-not $featureStartRelative.Success) {
    throw 'Could not locate function following raw hwHid::sendReport.'
}
$featureStart = $rawSendStart.Index + $featureStartRelative.Index
$rawSendReplacement = @'
	bool hwHid::sendReport(const void* data, size_t size) const noexcept
	{
#if SOUP_WINDOWS
		if (data == nullptr || size == 0)
		{
			return false;
		}
		HandleRaii event(CreateEventW(nullptr, TRUE, FALSE, nullptr));
		if (!event.isValid())
		{
			return false;
		}
		OVERLAPPED overlapped{};
		overlapped.hEvent = event;
		DWORD bytesWritten = 0;
		BOOL result = WriteFile(handle, data, static_cast<DWORD>(size), &bytesWritten, &overlapped);
		if (result == FALSE && GetLastError() == ERROR_IO_PENDING)
		{
			result = GetOverlappedResult(handle, &overlapped, &bytesWritten, TRUE);
		}
		return result != FALSE && bytesWritten == size;
#elif SOUP_LINUX
		return write(handle, data, size) == size;
#elif SOUP_MACOS
		if (!device || size == 0)
		{
			return false;
		}
		const uint8_t* bytes = static_cast<const uint8_t*>(data);
		uint8_t report_id = bytes[0];
		const uint8_t* send_data = bytes;
		size_t send_size = size;
		if (report_id == 0)
		{
			send_data = bytes + 1;
			send_size = size - 1;
		}
		return IOHIDDeviceSetReport((IOHIDDeviceRef)device, kIOHIDReportTypeOutput, report_id, send_data, send_size) == kIOReturnSuccess;
#else
		return false;
#endif
	}

'@
$hidSourceText = $hidSourceText.Substring(0, $rawSendStart.Index) + $rawSendReplacement + $hidSourceText.Substring($featureStart)


if (-not $sourceText.Contains($fixedMarker)) {
    throw 'Internal error: the Madlions fix marker was not written.'
}
if ($sourceText -notmatch 'index\s*>=\s*madlions\.layout_size') {
    throw 'Internal error: the Madlions layout bounds check is missing.'
}
if ($sourceText -notmatch 'resp\.size\(\)\s*<\s*27') {
    throw 'Internal error: the Madlions HID response length check is missing.'
}
if ($sourceText -notmatch 'madlions\.consecutive_failed_reports') {
    throw 'Internal error: per-device Madlions failure state is missing.'
}
if ($headerText -notmatch 'uint8_t\s+consecutive_failed_reports') {
    throw 'Internal error: Madlions header state patch is missing.'
}
if ($hidHeaderText -notmatch 'transactReport\s*\(' -or $hidSourceText -notmatch 'HallJoySafeHidContext') {
    throw 'Internal error: safe Windows HID transaction patch is missing.'
}
if ($sourceText -notmatch 'hid\.transactReport\s*\(') {
    throw 'Internal error: Madlions does not use the safe HID transaction.'
}
if ($hidSourceText -notmatch 'CreateEventW\(nullptr, TRUE, FALSE, nullptr\)' -or
    $hidSourceText -notmatch 'halljoy_drain_stale_reports' -or
    $hidSourceText -notmatch 'halljoy_start_and_finish_write' -or
    $hidSourceText -notmatch 'halljoy_start_and_finish_read') {
    throw 'Internal error: SafeHID serial event-per-operation state machine is incomplete.'
}
if ($handleText -notmatch 'h\s*!=\s*INVALID_HANDLE_VALUE\s*&&\s*h\s*!=\s*NULL') {
    throw 'Internal error: HandleRaii still accepts a NULL Windows handle.'
}

$preOpenIndex = $hidSourceText.IndexOf($preOpenMarker)
$firstCreateFileIndex = $hidSourceText.IndexOf('hid.handle = CreateFileW')
if ($preOpenIndex -lt 0 -or $firstCreateFileIndex -lt 0 -or
    $preOpenIndex -gt $firstCreateFileIndex -or
    $hidSourceText -notmatch 'UAP_EXCLUDE_HALLJOY_NATIVE' -or
        $hidSourceText -notmatch 'HALLJOY_UAP_NATIVE_HID_IDS') {
    throw 'Internal error: native analogue exclusion is not applied before CreateFileW.'
}

# Commit the patch only after every in-memory regression check has passed.
# This prevents a failed validation from leaving a partially patched Soup tree.
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($source, $sourceText, $utf8NoBom)
[System.IO.File]::WriteAllText($header, $headerText, $utf8NoBom)
[System.IO.File]::WriteAllText($handleHeader, $handleText, $utf8NoBom)
[System.IO.File]::WriteAllText($hidHeader, $hidHeaderText, $utf8NoBom)
[System.IO.File]::WriteAllText($hidSource, $hidSourceText, $utf8NoBom)

Write-Host "Soup Madlions SafeHID fix v7 applied: $source" -ForegroundColor Green
