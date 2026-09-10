# RM-30-UAPDD — embedded UAP DrunkDeer safety review

Date: 2026-09-06

## Corrected frame safety boundary

The UAP DrunkDeer transaction remains the documented `04 B6 03 01` request and
three-response 6×21 sampling format. The receive path now accepts a matrix only
when the write completed and all three responses are exactly 64 bytes, are
`04 B7`, and contain each unique chunk number `0`, `1`, and `2`. The chunks are
normalized into chunk order before the 59-byte payload portions are joined.

This removes the former short-report underflow/out-of-bounds path and prevents
arrival order from silently changing the matrix. An invalid/incomplete response
uses the existing fail-closed disconnect path; no bytes from it are mapped or
published. The Soup overlay hash was updated in the dependency lock, and the
source and lock static checks passed. The independent Sun/Clang overlay build
also requires the explicit `Buffer<>` specialization for retained fragment
pointers; that compile-only correction does not change the wire validation or
publication behavior.

## Remaining scope boundary

This narrowly proves frame memory safety and correlation for the established
wire format. It does not prove a common physical map/scale for unrelated
DrunkDeer models, make unknown PIDs production-qualified, or replace the
separate native diagnostic/research path. Bounded transport and cross-process
mutex lifecycle remain separate UAP work; no executable or HID session ran.
