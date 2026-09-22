"""Execute the production FAR receive loop with finite mocked HID reports."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
from run_native_backend_checks import find_cxx


class FarTests(unittest.TestCase):
    def test_two_fn_and_factory_key_counts(self):
        for model, count in [('k6_ansi', 68), ('q2_ansi', 66), ('q4_ansi', 61)]:
            report = json.loads((ROOT / 'docs/exports/keychron-he' / (model+'-review.json')).read_text())
            keys = report['keys']
            self.assertEqual(len(keys), count)
            by_position = {tuple(k['matrix']): k['hid'] for k in keys}
            self.assertEqual(by_position[4, 10], 0x409)
            self.assertEqual(by_position[4, 11], 0x408)
            self.assertEqual(len({k['hid'] for k in keys}), count)

    def test_production_receive_loop(self):
        compiler = find_cxx()
        self.assertIsNotNone(compiler, 'C++ compiler required')
        source = (ROOT / 'third_party/UniversalAnalogPluginFixed/overlay/Soup/soup/AnalogueKeyboard.cpp').read_text()
        start = source.index('const size_t slots = layout_get_size(keychron.layout);')
        end = source.index('if (!valid)', start)
        loop = source[start:end]
        program = r'''
#include <vector>
#include <cstdint>
#include <cassert>
#include <cstddef>
using std::size_t;
struct Buffer : std::vector<uint8_t> {
    using std::vector<uint8_t>::vector;
    void append(const uint8_t* p, size_t n) { insert(end(), p, p+n); }
};
struct Keyboard { size_t layout; } keychron;
size_t layout_get_size(size_t slots) { return slots; }
std::vector<Buffer> input;
size_t calls;
Buffer safeReceiveReport(int, int, int) {
    assert(calls < input.size());
    return input[calls++];
}
bool run(size_t count, bool request_sent, int bad = -1) {
    keychron.layout = count; calls = 0; input.clear();
    // Independent firmware producer: flush full payloads, then final reply.
    Buffer packet(32, 0); size_t pos = 2;
    for (size_t i = 0; i < count; ++i) {
        packet[pos++] = static_cast<uint8_t>(i);
        if (pos == 32) { input.push_back(packet); packet = Buffer(32, 0); pos = 2; }
    }
    input.push_back(packet);
    if (bad >= 0) input[bad].resize(31);
    int hid = 0; uint8_t data[3] = {0, 0xa9, 0x31};
''' + loop + r'''
    if (request_sent && bad < 0) {
        assert(valid && calls == input.size() && combined.size() >= count);
        for (size_t i = 0; i < count; ++i) assert(combined[i] == i);
    } else {
        assert(!valid);
        assert(calls == (request_sent ? size_t(bad + 1) : 0));
    }
    return valid;
}
int main() {
    for (size_t count : {70, 75, 90, 96, 114, 120, 126, 132}) {
        assert(run(count, true));
        assert(!run(count, false));
        for (size_t i = 0; i <= count / 30; ++i) assert(!run(count, true, int(i)));
    }
}
'''
        with tempfile.TemporaryDirectory(prefix='halljoy-far-') as temporary:
            path = Path(temporary)
            source_file = path / 'far.cpp'
            with source_file.open('x') as stream:
                stream.write(program)
            exe = path / 'far.exe'
            subprocess.run([compiler, '-std=c++20', str(source_file), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    unittest.main()
