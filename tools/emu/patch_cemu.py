#!/usr/bin/env python3
"""Patch a CEmu checkout so the headless runner prints the calculator's debug console
(CE toolchain dbg_printf -> writes to 0xFB0000/0xFC0000) WITHOUT building the full debugger
(-DDEBUG_SUPPORT segfaults in cemu-headless because the debugger is never initialized).
Idempotent. Usage: patch_cemu.py <CEmu checkout dir>
"""
import sys

MARK = '/* khicas-review: minimal debug console */'
ANCHOR = '#ifdef DEBUG_SUPPORT\n                if (debug_get_flags() & DBG_SOFT_COMMANDS) {'
BLOCK = MARK + '''
#ifndef DEBUG_SUPPORT
                if (addr >= DBGOUT_PORT_RANGE && addr < DBGEXT_PORT) {
                    static char khbuf[2][SIZEOF_DBG_BUFFER];
                    static unsigned khpos[2];
                    const int e = addr >= DBGERR_PORT_RANGE;
                    khbuf[e][khpos[e]] = (char)value;
                    khpos[e] = (khpos[e] + 1) % (SIZEOF_DBG_BUFFER - 1);
                    if (!value) {
                        if (e) { gui_console_err_printf("%s", khbuf[e]); } else { gui_console_printf("%s", khbuf[e]); }
                        khpos[e] = 0;
                    }
                    break;
                }
#endif
'''

def patch(path, mark, anchor, block):
    src = open(path).read()
    if mark in src:
        print('already patched:', path)
    elif src.count(anchor) != 1:
        sys.exit('anchor not found exactly once in ' + path)
    else:
        open(path, 'w').write(src.replace(anchor, block + anchor))
        print('patched:', path)


root = sys.argv[1].rstrip('/')
patch(root + '/core/mem.c', MARK, ANCHOR, BLOCK)

# 2) headless runner: `peek <hexaddr> [n]` and `poke <hexaddr> <hexbytes>` (direct physical
#    memory access, bypassing flash protection; used e.g. to make the emulated OS report the
#    user's OS version after boot - see tools/emu/cemu.py `osfake`).
HL_MARK = '/* khicas-review: peek/poke */'   # mem_peek_byte/mem_poke_byte come from core/mem.h via autotester.h
patch(root + '/tests/autotester/headless_cli.cpp', HL_MARK,
      '    respond("ERR unknown command " + command);',
      '''    if (command == "peek" || command == "poke") { ''' + HL_MARK + '''
        std::string a; input >> a;
        const uint32_t addr = static_cast<uint32_t>(std::stoul(a, nullptr, 16));
        if (command == "peek") {
            unsigned n = 1; input >> n;
            std::string out; char b[4];
            for (unsigned i = 0; i < n; ++i) { std::snprintf(b, sizeof b, "%02X", cemucore::mem_peek_byte(addr + i)); out += b; }
            respond("OK peek " + out);
        } else {
            std::string hex; input >> hex;
            for (size_t i = 0; i + 1 < hex.size(); i += 2) {
                cemucore::mem_poke_byte(addr + static_cast<uint32_t>(i / 2), static_cast<uint8_t>(std::stoul(hex.substr(i, 2), nullptr, 16)));
            }
            respond("OK poke");
        }
        return true;
    }
''')

# 3) headless runner: `keydown <name>` / `keyup <name>` (press and release separately, to sample
#    the calculator's state with peek while a key is held - see tools/emu/phases.py).
KD_MARK = '/* khicas-review: keydown/keyup */'
patch(root + '/tests/autotester/headless_cli.cpp', KD_MARK,
      '    respond("ERR unknown command " + command);',
      '''    if (command == "keydown" || command == "keyup") { ''' + KD_MARK + '''
        std::string name; input >> name;
        autotester::key_coord_t coord{};
        if (!autotester::keyCoordForName(name, coord)) {
            respond("ERR unknown key " + name);
        } else {
            cemucore::emu_keypad_event(coord.y, coord.x, command == "keydown");
            respond("OK " + command);
        }
        return true;
    }
''')

# 4) headless runner: `regs` prints PC and SP (where a hung calculator is stuck).
RG_MARK = '/* khicas-review: regs */'
patch(root + '/tests/autotester/headless_cli.cpp', RG_MARK,
      '    respond("ERR unknown command " + command);',
      '''    if (command == "regs") { ''' + RG_MARK + '''
        char b[96];
        std::snprintf(b, sizeof b, "OK regs pc=%06X sp=%06X halted=%d", (unsigned)cemucore::cpu.registers.PC,
                      (unsigned)cemucore::cpu.registers.SPL, (int)cemucore::cpu.halted);
        respond(b);
        return true;
    }
''')
