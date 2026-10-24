#!/usr/bin/env python3
"""cemu_dusbkey.py CEMU_DIR: adds remote keys over D-USB to a COPY of the CEmu tree, the packets the FlowCE web
installer sends (dusb.js pressKey: EXECUTE 0,0,3,0,code, then the calculator's DATA_ACK): the dusb device's
"key:<hex>[,<hex>...]" command (it reuses the clock command's handshake and states), emu_press_keys() in
core/link.c, and the headless CLI's "dusb-key <hex>[,<hex>...]"."""
import sys

root = sys.argv[1]

def edit(rel, pairs):
    p = f'{root}/{rel}'
    s = open(p, encoding='utf-8').read()
    for old, new in pairs:
        assert s.count(old) == 1, (rel, old[:70])
        s = s.replace(old, new)
    open(p, 'w', encoding='utf-8').write(s)

edit('core/usb/dusb.c', [
    ('    DUSB_CLOCK_COMMAND,\n    DUSB_NUM_COMMANDS,', '    DUSB_CLOCK_COMMAND,\n    DUSB_KEY_COMMAND,\n    DUSB_NUM_COMMANDS,'),
    ('"done", "screenshot", "romdump", "list", "send", "receive", "clock"',
     '"done", "screenshot", "romdump", "list", "send", "receive", "clock", "key"'),
    ('                    case DUSB_CLOCK_COMMAND:\n                        context->position = 0;',
     '                    case DUSB_KEY_COMMAND:\n                    case DUSB_CLOCK_COMMAND:\n                        context->position = 0;'),
    ('''            uint16_t size;
            uint8_t value[4];
            (void)dusb_clock_parameter(context, &size, value);
            return DUSB_RPKT_HEADER_SIZE + DUSB_VPKT_HEADER_SIZE + 4 + size;''',
     '''            uint16_t size;
            uint8_t value[4];
            if (context->command->type == DUSB_KEY_COMMAND) {
                return DUSB_RPKT_HEADER_SIZE + DUSB_VPKT_HEADER_SIZE + 5;
            }
            (void)dusb_clock_parameter(context, &size, value);
            return DUSB_RPKT_HEADER_SIZE + DUSB_VPKT_HEADER_SIZE + 4 + size;'''),
    ('''                    *buffer++ = DUSB_VPKT_PARM_SET >> 8 & 0xFF;
                    *buffer++ = DUSB_VPKT_PARM_SET >> 0 & 0xFF;

                    *buffer++ = parameter >> 8;''',
     '''                    if (context->command->type == DUSB_KEY_COMMAND) {
                        /* a remote key (khicas-review): EXECUTE 0,0,3,0,code, as TI Connect and the FlowCE page */
                        *buffer++ = DUSB_VPKT_EXECUTE >> 8 & 0xFF;
                        *buffer++ = DUSB_VPKT_EXECUTE >> 0 & 0xFF;
                        *buffer++ = 0;
                        *buffer++ = 0;
                        *buffer++ = 3;
                        *buffer++ = 0;
                        *buffer++ = context->command->varname[context->position];
                        (void)parameter;
                        break;
                    }
                    *buffer++ = DUSB_VPKT_PARM_SET >> 8 & 0xFF;
                    *buffer++ = DUSB_VPKT_PARM_SET >> 0 & 0xFF;

                    *buffer++ = parameter >> 8;'''),
    ('''                case DUSB_CLOCK_PAUSE_STATE:
                    /* TI-OS starts''',
     '''                case DUSB_CLOCK_PAUSE_STATE:
                    if (context->command->type == DUSB_KEY_COMMAND) {
                        if (context->position < context->command->varname_length) {
                            return dusb_transition(event, DUSB_CLOCK_SET_WAIT_STATE);
                        }
                        context->clock_complete = true;
                        return dusb_transition(event, DUSB_NEXT_COMMAND_STATE);
                    }
                    /* TI-OS starts'''),
    ('''    if (context->state == DUSB_VAR_MODE_SET_ACK_STATE &&
        context->command->type == DUSB_CLOCK_COMMAND) {''',
     '''    if (context->state == DUSB_VAR_MODE_SET_ACK_STATE &&
        (context->command->type == DUSB_CLOCK_COMMAND || context->command->type == DUSB_KEY_COMMAND)) {'''),
    ('''                    context->clock_operation = true;
                    continue;
                }
                while (*arg == '-') {''',
     '''                    context->clock_operation = true;
                    continue;
                }
                if (command->type == DUSB_KEY_COMMAND) {
                    /* key:<hex>[,<hex>...] (khicas-review): TI-OS key codes pressed one after another */
                    unsigned int code;
                    int used;
                    command->varname_length = 0;
                    while ((*arg == ':' || *arg == ',') && command->varname_length < 8 &&
                           sscanf(arg + 1, "%x%n", &code, &used) == 1 && code <= 0xFF) {
                        command->varname[command->varname_length++] = (uint8_t)code;
                        arg += 1 + used;
                    }
                    if (*arg || !command->varname_length) {
                        return EINVAL;
                    }
                    context->clock_operation = true;
                    continue;
                }
                while (*arg == '-') {'''),
])

edit('core/link.c', [('''static inline size_t write_le16(''', '''/* khicas-review: TI-OS key codes ("0a,9a"), pressed over D-USB as a computer's remote keys */
int emu_press_keys(const char *codes, usb_progress_handler_t *progress_handler, void *progress_context) {
    char command[64];
    const char *argv[] = { "dusb", command };

    snprintf(command, sizeof command, "key:%s", codes);
    return usb_plug_device(2, argv, progress_handler, progress_context) == USB_SUCCESS
        ? LINK_GOOD : LINK_ERR;
}

static inline size_t write_le16(''')])
edit('core/link.h', [('int emu_receive_variable(', '''int emu_press_keys(const char *codes, usb_progress_handler_t *progress_handler, void *progress_context);
int emu_receive_variable(''')])

edit('tests/autotester/headless_cli.cpp', [('''void printUsage(const char *program)''', '''// khicas-review: remote keys over D-USB (the FlowCE web installer's pressKey): true once the calculator acked
bool pressKeys(const std::string& codes)
{
    transfer_progress_t progress;
    if (cemucore::emu_press_keys(codes.c_str(), transferProgress, &progress) != cemucore::LINK_GOOD) {
        return false;
    }
    for (uint32_t elapsed = 0; !progress.finished && elapsed < 30000; elapsed += 10) {
        cemucore::emu_run(10);
    }
    cemucore::emu_run(100);
    return progress.finished && !progress.failed;
}

void printUsage(const char *program)'''),
    ('''    if (command == "usb") {''', '''    if (command == "dusb-key") {
        std::string codes;
        std::getline(input >> std::ws, codes);
        if (codes.empty()) {
            respond("ERR usage: dusb-key <hex>[,<hex>...]");
        } else if (pressKeys(codes)) {
            respond("OK dusb-key " + codes);
        } else {
            respond("ERR dusb-key " + codes);
        }
        return true;
    }
    if (command == "usb") {'''),
])
print('patched', root)
