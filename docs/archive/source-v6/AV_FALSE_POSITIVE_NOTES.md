# Antivirus false-positive notes

Changes in v5 that reduce suspicious heuristic indicators:

- the production child host starts without `DEBUG_ONLY_THIS_PROCESS`;
- the supervisor does not use the debugger API in a normal Release build;
- `TerminateThread` is no longer used to stop the analogue-host supervisor or
  snapshot bridge;
- the application is neither packed nor obfuscated;
- it does not download or launch external payloads;
- runtime DLLs are built from the included source code.

Normal properties that can still affect reputation-based detection:

- the application is unsigned;
- it creates an isolated child copy of itself to protect the main UI from
  plugin failures;
- it works with HID devices, hooks, a virtual gamepad, and shared memory;
- each new build hash initially has low prevalence.

When checking a release executable, preserve its exact SHA-256 hash and the
VirusTotal result. A single detection without a signature name is not, by
itself, proof of malware or proof that the detection is a false positive.
