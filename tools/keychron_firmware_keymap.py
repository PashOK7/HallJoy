"""Reviewed, hash-pinned factory tables from official HE / HE 8K images.

This reads constant data, never executes firmware. Addresses are image offsets,
not a heuristic applied to arbitrary binaries. Unknown key actions fail closed.
"""
import hashlib
import json
import struct
import layout_import as m

TABLES = [
    ('q1_8k_ansi',0x1010,0x3964,'b80b4ba0c4593b526c20571001a37280cc1e590a030f07eee9c66ea0368bad93'),
    ('q1_8k_iso',0x1011,0x3b20,'a32a5e998e9041cc858a4bc4b5a50a9e5dc6b0edd43d13f7318b86481c654926'),
    ('q1_8k_jis',0x1012,0x39b8,'2b8b2b0ca872e7ed1d93478ba9733d48d065fa65fee60b8ec7144cede921b2a5'),
    ('q3_8k_ansi',0x1030,0x3804,'db617367ed70b41ffbdcffd97f68402ae0486a02f24091e5df73cf570ea933ba'),
    ('q3_8k_iso',0x1031,0x3808,'c5dc1cb7b0804306ee4cc8c34723b562e8dafe3e3e82f8aa62d6cec256de918d'),
    ('q3_8k_jis',0x1032,0x3a40,'678512d3462fd5e9371a318b1030822bdce09d3d093bc6378e6c7edec6dc4a96'),
    ('q6_8k_ansi',0x1060,0x45b74,'11446905bb36452deb81f3923b830cb564643892ed979b80e89bd97f38b48878'),
    ('q5_8k_ansi',0x1050,0x3a94,'5acd0342a9aff6ae38bc6767c95a68a8690c4802873bf7dee91e72eaa56b604c'),
    ('k3_ansi',0x0e30,0x16d80,'ff785c5a4f0f814a2bf3bba23c83a5532e7699415fae01c1b7b0b0ff15ab7a37'),
    ('k3_iso',0x0e31,0x16d64,'709a18e622e83ae6bfe08caa515ddf3bab3eadae2cd5de64cb7e7f0165c7d62f'),
    ('k3_jis',0x0e32,0x1719c,'33af5c6c8a4080c47d881096756caa2b011c161a4c9ccf2bc4808d7ea2902da7'),
]


def prepare(source, model, pid):
    _,_,offset,digest = next(t for t in TABLES if t[0] == model and t[1] == pid)
    inputs = {'launcher':source/(str(0x34340000+pid)+'-launcher.json'),
              'firmware':source/'firmware'/f'{pid:04x}-{digest}.bin',
              'metadata':source/'firmware'/f'{pid:04x}-{digest}.json'}
    raw = inputs['firmware'].read_bytes()
    m.require(hashlib.sha256(raw).hexdigest() == digest, 'Firmware hash mismatch')
    metadata = m.read_json(inputs['metadata'])[0]['data']['product']
    m.require(int(metadata['pid'],0) == pid and int(metadata['vid'],0) == 0x3434, 'Firmware product mismatch')
    definition = m.read_json(inputs['launcher'])[0]
    rows,cols = definition['matrix']['rows'],definition['matrix']['cols']
    matrix = struct.unpack_from('<'+str(rows*cols)+'H',raw,offset)
    anchor = [41,0]+list(range(58,70)) if pid == 0x1050 else [41]+list(range(58,70))
    m.require(matrix[:len(anchor)] == tuple(anchor), 'Invalid Windows base-table anchor')
    used = {k['row']*cols+k['col'] for k in definition['layouts']['keys']}
    m.require(all(value == 0 for i,value in enumerate(matrix) if i not in used), 'Nonzero unmapped matrix position')
    reverse = {hid:token for token,(hid,label) in m.CODES.items()}
    layout, tokens, fixes = [], [], {}
    for key in definition['layouts']['keys']:
        row,col = key['row'],key['col']
        value = matrix[row*cols+col]
        token = reverse.get(value, f'FW_{value:04X}')
        layout.append({'matrix':[row,col]}); tokens.append(token)
        if pid == 0x1050 and row == 0 and col in (15,16,17):
            m.require(value == 104+col-15,'Q5 physical OEM key changed')
            fixes[f'{row},{col}'] = (token,0x403+col-15,'F'+str(col-2))
        if value not in reverse:
            if value == 0x00A8 and row == 0: # QMK KC_AUDIO_MUTE, mechanical encoder.
                hid,label = None,'Knob'
            elif value == 0x5223 and row == 5: # QMK MO(3), Windows Fn layer.
                hid,label = 0x409,'Fn'
            elif value == 0x7821 and row == 0: # QMK underglow next mode.
                hid,label = 0x404,'RGB'
            elif value in (0x7E09,0x7E0C) and row == 0: # Physical assistant position, OEM 1.
                hid,label = 0x403,'Assistant'
            else:
                raise m.ImportErrorDetail(f'Unreviewed firmware key {model} {row},{col}: {value:04X}')
            fixes[f'{row},{col}'] = (token,hid,label)
    info = {'usb':{'vid':'0x3434','pid':hex(pid)},'layouts':{'LAYOUT_EXTRACTED':{'layout':layout}}}
    code = '[WIN_BASE] = LAYOUT_EXTRACTED('+','.join(tokens)+')'
    return inputs,definition,info,code,fixes,offset
