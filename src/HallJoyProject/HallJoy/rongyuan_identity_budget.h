#pragma once
// Probe budget of the RongYuan stream backend for keyboards that answer a board this
// table does not admit on their VID/PID. The answer cannot change until the device
// re-enumerates, so a path is probed at most kAttempts times per device topology.
//
// Tester log 2026-10-24 (ATTACK SHARK X82HE, board 3086 on 3151:5029): after two
// rejections the ATTACK SHARK backend took the keyboard, and every later 0x8F reply of
// this backend was lost in its traffic (ERROR_INVALID_DATA, no board answered). Only
// answered rejections were counted, so the limit was never reached: 107 identity
// exchanges in two minutes during the other backend's analog session, and its
// exclusive open failed on this backend's handle (false communication warning).
// A failed pass therefore counts too once the path has answered a rejected board.
// Pure logic, unit-tested in rongyuan_stream_protocol_test.cpp.
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace halljoy::ry_stream {
class IdentityBudget {
public:
  static constexpr unsigned kAttempts = 3;
  void Reset() { paths.clear(); }
  bool Exhausted(const std::wstring &path) const {
    const auto entry = Find(path);
    return entry != paths.end() && entry->second >= kAttempts;
  }
  // Records a pass that ended without a session. Returns the attempts counted for
  // the path so far, or 0 when this pass does not count (no rejection seen yet).
  unsigned Failed(const std::wstring &path, bool identityRejected) {
    auto entry = Find(path);
    if (entry == paths.end()) {
      if (!identityRejected) return 0;
      paths.emplace_back(path, 1u);
      return 1;
    }
    return ++entry->second;
  }

private:
  using Entry = std::pair<std::wstring, unsigned>;
  std::vector<Entry>::const_iterator Find(const std::wstring &path) const {
    return std::find_if(paths.begin(), paths.end(), [&](const Entry &e) { return e.first == path; });
  }
  std::vector<Entry>::iterator Find(const std::wstring &path) {
    return std::find_if(paths.begin(), paths.end(), [&](const Entry &e) { return e.first == path; });
  }
  std::vector<Entry> paths;
};
}
