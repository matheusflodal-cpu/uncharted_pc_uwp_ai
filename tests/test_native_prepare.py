"""Byte-for-byte comparison with the original Python tools, using invented executables."""
import contextlib
import io
import os
from pathlib import Path
import struct
import subprocess
import tempfile
import unittest

from prepare import prepare, parse_self
from link_modules import link, RUNTIME_SPELLING, RUNTIME_MODULE
from content_profile import prepare as content_profile
from test_prepare import make_test_sfo

HARNESS = os.environ.get('BB_TEST_NATIVE_PREPARE')


def executable(symbols=(), relocs=(), identities=(), tls=False, instruction=0xc3):
    """SELF with a load, independent dynlib blob, procparam, optional TLS and dynamic table."""
    strings = bytearray(b'\0')
    def string(text):
        offset = len(strings)
        strings.extend(text.encode('ascii') + b'\0')
        return offset
    tags = []
    for library, module, version, code in identities:
        tags.extend([(0x61000013, (code << 48) | (version << 32) | string(library)),
                     (0x6100000d, (code << 48) | (version << 32) | string(module))])
    syms = bytearray()
    for name, info, section, value, size in symbols:
        syms.extend(struct.pack('<IBBHQQ', string(name), info, 0, section, value, size))
    symoff = len(strings)
    reloff = symoff + len(syms)
    relocations = b''.join(struct.pack('<QQq', target, (sym << 32) | kind, addend)
                           for target, kind, sym, addend in relocs)
    dynoff = reloff + len(relocations)
    tags += [(0x61000035, 0), (0x61000037, len(strings)),
             (0x61000039, symoff), (0x6100003f, len(syms)),
             (0x61000029, reloff), (0x6100002d, len(relocations)),
             (0x6100002f, reloff), (0x61000031, 0), (12, 0x10), (0, 0)]
    dynamic = b''.join(struct.pack('<QQ', *tag) for tag in tags)
    blob = strings + syms + relocations + dynamic
    code = bytearray(4096)
    code[0] = instruction
    code[32:41] = bytes.fromhex('64488b042500000000')
    code[0x300:0x304] = b'TLS!'
    ph = [(1, 7, 0x1000, 0, 0, len(code), 8192, 4096),
          (0x61000000, 4, 0x2000, 0, 0, len(blob), 0, 16),
          (2, 4, 0x2000+dynoff, 0, 0, len(dynamic), len(dynamic), 8),
          (0x61000001, 4, 0x1200, 0x200, 0, 32, 32, 8)]
    if tls:
        ph.append((7, 4, 0x1300, 0x300, 0, 4, 32, 16))
    # Only LOAD and DYNLIBDATA have SELF payloads; other headers refer inside them.
    base = 32 + 2*32
    payload = 0x400
    data = bytearray(payload+len(code)+len(blob))
    data[:4] = b'O\x15=\x1d'
    struct.pack_into('<H', data, 24, 2)
    for slot, (index, body, offset) in enumerate([(0,code,payload), (1,blob,payload+len(code))]):
        struct.pack_into('<QQQQ', data, 32+slot*32, 0x800|(index<<20), offset, len(body), len(body))
        data[offset:offset+len(body)] = body
    struct.pack_into('<16sHHIQQQIHHHHHH', data, base, b'\x7fELF\x02\x01\x01\x09',
                     0xfe10, 62, 1, 0, 64, 0, 0, 64, 56, len(ph), 0, 0, 0)
    for i, header in enumerate(ph):
        struct.pack_into('<IIQQQQQQ', data, base+64+i*56, *header)
    return data


@unittest.skipUnless(HARNESS, 'set BB_TEST_NATIVE_PREPARE to the compiled native harness')
class NativePrepareTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.game = self.root/'game'
        (self.game/'sce_sys').mkdir(parents=True)
        (self.game/'sce_module').mkdir()
        self.eboot = executable()
        self.libc = executable([('bzQExy189ZI#H#H',0x12,1,0,1)], identities=[('libc','libc',1,7)])
        self.fios = executable()
        self.sfo = make_test_sfo('UNCHARTED: The Nathan Drake Collection\u2122')

    def write_inputs(self):
        files = [('eboot.bin',self.eboot),('sce_module/libc.prx',self.libc),
                 ('sce_module/libSceFios2.prx',self.fios),('sce_sys/param.sfo',self.sfo)]
        for path, data in files:
            (self.game/path).write_bytes(data)
        return [str(self.game/path) for path,_ in files]

    def native(self):
        return subprocess.run([HARNESS,*self.write_inputs(),str(self.root/'native.bin'),
                               str(self.root/'native-content.bin')],capture_output=True,text=True)

    def parity(self):
        result = self.native()
        self.assertEqual(result.returncode,0,result.stderr)
        output = self.root/'python'
        with contextlib.redirect_stdout(io.StringIO()):
            prepare(self.game,output,'eboot.bin')
            link(self.game,output)
            content_profile(self.game,output)
        self.assertEqual((output/'boot-linked.bin').read_bytes(),(self.root/'native.bin').read_bytes())
        self.assertEqual((output/'content.bin').read_bytes(),(self.root/'native-content.bin').read_bytes())

    def test_minimal_dump_and_ret_flag(self):
        self.parity()
        self.assertEqual(struct.unpack_from('<Q',(self.root/'native.bin').read_bytes(),48)[0],1)
        self.libc = executable([('bzQExy189ZI#H#H',0x12,1,0,1)],identities=[('libc','libc',1,7)],instruction=0x90)
        self.parity()
        self.assertEqual(struct.unpack_from('<Q',(self.root/'native.bin').read_bytes(),48)[0],0)

    def test_all_host_spellings_and_module_mismatch(self):
        syms,relocs,ids = [],[],[]
        for code,library in enumerate(RUNTIME_SPELLING,1):
            from link_libc import encode_id
            spelling = encode_id(code)
            syms.append((f'Nid{code}#{spelling}#{spelling}',0x12,0,0,0))
            relocs.append((0x400+code*8,7,len(syms)-1,0))
            ids.append((library,RUNTIME_MODULE.get(library,library),1,code))
        syms.append(('Foreign#++#++',0x12,0,0,0))
        ids.append(('libScePad','different-module',1,4030))
        relocs.append((0x800,7,len(syms)-1,0))
        self.eboot = executable(syms,relocs,ids)
        self.parity()
        raw = (self.root/'native.bin').read_bytes()
        self.assertIn(b'Nid27#A#B',raw)
        self.assertIn(b'Foreign#libScePad',raw)

    def test_native_bindings_tls_weak_absolute_and_relative_relocations(self):
        self.eboot = executable([('bzQExy189ZI#E#E',0x12,0,0,0),('same#F#F',0x11,0,0,0)],
            [(0x400,7,0,0),(0x408,1,1,8),(0x410,8,0,0x100)],
            [('libSceLibcInternal','libSceLibcInternal',1,4),('libSceFios2','libSceFios2',1,5)],tls=True)
        self.fios = executable([('same#D#D',0x11,1,0x180,16),('',3,1,0,0),
                                ('module_start',0x22,0,0,0),('absolute',0x11,0xfff1,99,8)],
            [(0x400,16,1,0),(0x408,7,2,0),(0x410,1,3,1),(0x418,8,0,0x40)],
            [('libSceFios2','libSceFios2',1,3)],tls=True)
        self.parity()

    def test_version_identity_mismatch_is_unresolved(self):
        self.eboot=executable([('same#F#F',0x12,0,0,0)],[(0x400,7,0,0)],
                               [('libSceFios2','libSceFios2',2,5)])
        self.fios=executable([('same#D#D',0x12,1,0x180,1)],identities=[('libSceFios2','libSceFios2',1,3)])
        self.parity()

    def test_rejects_incomplete_encrypted_wrong_title_and_bad_relocations(self):
        original = self.eboot
        cases = []
        bad=bytearray(original); struct.pack_into('<Q',bad,32,0x802); cases.append(bad)
        cases += [original[:-1], executable([('x#A#A',0x12,0,0,0)],[(0x400,7,99,0)], [('libc','libc',1,0)]),
                  executable(relocs=[(0xfffffff0,8,0,0)]), executable(relocs=[(0x400,999,0,0)])]
        for bad in cases:
            with self.subTest(size=len(bad)):
                self.eboot=bad
                self.assertNotEqual(self.native().returncode,0)
                self.assertFalse((self.root/'native.bin').exists())
        self.eboot=original
        self.sfo=make_test_sfo('Another game')
        self.assertNotEqual(self.native().returncode,0)

    def test_plain_elf_has_same_output_as_self(self):
        self.parity()
        expected=(self.root/'native.bin').read_bytes()
        self.eboot=parse_self(self.eboot)[0]
        self.libc=parse_self(self.libc)[0]
        self.fios=parse_self(self.fios)[0]
        result=self.native()
        self.assertEqual(result.returncode,0,result.stderr)
        self.assertEqual(expected,(self.root/'native.bin').read_bytes())
