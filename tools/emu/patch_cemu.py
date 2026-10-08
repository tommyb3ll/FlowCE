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

# 5) headless runner: `stats` prints the CPU's counters since power-on: cycles, halted cycles,
#    cycles lost to the LCD's DMA, flash reads, flash cache misses (serial flash, revision M+:
#    8 KB 2-way cache, a miss costs ~197 cycles) and the flash delay cycles.
ST_MARK = '/* khicas-review: stats */'
patch(root + '/tests/autotester/headless_cli.cpp', ST_MARK,
      '    respond("ERR unknown command " + command);',
      '''    if (command == "stats") { ''' + ST_MARK + '''
        char b[256];
        std::snprintf(b, sizeof b, "OK stats cycles=%llu halt=%llu dma=%llu flash=%lu misses=%lu delay=%lld",
                      (unsigned long long)(cemucore::cpu.baseCycles + cemucore::cpu.cycles),
                      (unsigned long long)cemucore::cpu.haltCycles, (unsigned long long)cemucore::cpu.dmaCycles,
                      (unsigned long)cemucore::cpu.flashTotalAccesses, (unsigned long)cemucore::cpu.flashCacheMisses,
                      (long long)cemucore::cpu.flashDelayCycles);
        respond(b);
        return true;
    }
''')

# 6) flash line profile: per 32-byte flash line, how often the CPU entered it (an access to another
#    line than the previous one) and how often that missed the cache. `lineprof on` (zeroed),
#    `lineprof dump <path>` (2 x 131072 little-endian u32: entries, misses), `lineprof off`.
patch(root + '/core/flash.c', '/* khicas-review: lineprof decl */',
      'uint32_t flash_touch_cache(uint32_t addr) {',
      'uint32_t *khicas_lineprof; /* khicas-review: lineprof decl */\n')
patch(root + '/core/flash.c', '/* khicas-review: lineprof entry */',
      '        flash_cache_set_t* set = &flash.cacheTags[line & (FLASH_CACHE_SETS - 1)];',
      '        if (khicas_lineprof) { khicas_lineprof[2 * (line & 0x1FFFF)]++; } /* khicas-review: lineprof entry */\n')
patch(root + '/core/flash.c', '/* khicas-review: lineprof miss */',
      '            cpu.flashCacheMisses++;',
      '            if (khicas_lineprof) { khicas_lineprof[2 * (line & 0x1FFFF) + 1]++; } /* khicas-review: lineprof miss */\n')
LP_MARK = '/* khicas-review: lineprof */'
patch(root + '/tests/autotester/headless_cli.cpp', LP_MARK,
      '    respond("ERR unknown command " + command);',
      '''    if (command == "lineprof") { ''' + LP_MARK + '''
        std::string what; input >> what;
        const size_t n = 2 * 0x20000;
        if (what == "on") {
            if (!cemucore::khicas_lineprof) { cemucore::khicas_lineprof = static_cast<uint32_t *>(std::calloc(n, 4)); }
            else { std::memset(cemucore::khicas_lineprof, 0, n * 4); }
            respond("OK lineprof on");
        } else if (what == "dump" && cemucore::khicas_lineprof) {
            std::string path; input >> path;
            FILE *f = std::fopen(path.c_str(), "wb");
            if (f) { std::fwrite(cemucore::khicas_lineprof, 4, n, f); std::fclose(f); respond("OK lineprof dump"); }
            else { respond("ERR cannot write " + path); }
        } else if (what == "off") {
            std::free(cemucore::khicas_lineprof); cemucore::khicas_lineprof = nullptr;
            respond("OK lineprof off");
        } else {
            respond("ERR lineprof on|dump <path>|off");
        }
        return true;
    }
''')
patch(root + '/tests/autotester/headless_cli.cpp', '/* khicas-review: lineprof extern */',
      '        #include "../../core/usb/usb.h"',
      '        extern uint32_t *khicas_lineprof; /* khicas-review: lineprof extern */\n')

# 7) headless runner: `lcddma 1|0` emulates the LCD's DMA (it steals RAM cycles from the CPU while
#    the screen refreshes; the headless runner leaves it off unless asked).
DM_MARK = '/* khicas-review: lcddma */'
patch(root + '/tests/autotester/headless_cli.cpp', DM_MARK,
      '    respond("ERR unknown command " + command);',
      '''    if (command == "lcddma") { ''' + DM_MARK + '''
        int on = 1; input >> on;
        cemucore::emu_set_lcd_dma(on);
        respond(on ? "OK lcddma on" : "OK lcddma off");
        return true;
    }
''')

# 8) flash line trace: every entry into a 32-byte flash line (an access to another line than the
#    previous one), as little-endian u32 line numbers (address >> 5), for cache simulations.
#    `linetrace on <path>`, `linetrace off`.
patch(root + '/core/flash.c', '/* khicas-review: linetrace decl */',
      'uint32_t flash_touch_cache(uint32_t addr) {',
      '''FILE *khicas_linetrace; /* khicas-review: linetrace decl */
uint32_t khicas_linebuf[65536];
unsigned khicas_linepos;
''')
patch(root + '/core/flash.c', '/* khicas-review: linetrace entry */',
      '        flash_cache_set_t* set = &flash.cacheTags[line & (FLASH_CACHE_SETS - 1)];',
      '''        if (khicas_linetrace) { /* khicas-review: linetrace entry */
            khicas_linebuf[khicas_linepos++] = line;
            if (khicas_linepos == 65536) { fwrite(khicas_linebuf, 4, 65536, khicas_linetrace); khicas_linepos = 0; }
        }
''')
LT_MARK = '/* khicas-review: linetrace */'
patch(root + '/tests/autotester/headless_cli.cpp', LT_MARK,
      '    respond("ERR unknown command " + command);',
      '''    if (command == "linetrace") { ''' + LT_MARK + '''
        std::string what; input >> what;
        if (cemucore::khicas_linetrace) {
            std::fwrite(cemucore::khicas_linebuf, 4, cemucore::khicas_linepos, cemucore::khicas_linetrace);
            std::fclose(cemucore::khicas_linetrace); cemucore::khicas_linetrace = nullptr; cemucore::khicas_linepos = 0;
        }
        if (what == "on") {
            std::string path; input >> path;
            cemucore::khicas_linetrace = std::fopen(path.c_str(), "wb");
            respond(cemucore::khicas_linetrace ? "OK linetrace on" : "ERR cannot write " + path);
        } else {
            respond("OK linetrace off");
        }
        return true;
    }
''')
patch(root + '/tests/autotester/headless_cli.cpp', '/* khicas-review: linetrace extern */',
      '        #include "../../core/usb/usb.h"',
      '''        extern FILE *khicas_linetrace; /* khicas-review: linetrace extern */
        extern uint32_t khicas_linebuf[65536];
        extern unsigned khicas_linepos;
''')
