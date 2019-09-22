#!/usr/bin/python
# usage: $0 bindings.c colors.c
import sys, re
import argparse
RE = lambda regex: re.compile(regex.replace(' ', r'\s*'))

# =============================================================================
# Bindable commands ===========================================================
# =============================================================================

CMD_MIXER = [
    # (C_CONSTANT, COMMAND, DESCRIPTION)
    ('HELP', 'help',
        'Display a window with keybindings'),
    ('SYSTEM_INFORMATION', 'system_information',
        'Display a menu with files containing information about alsa'),
    ('SELECT_CARD', 'select_card',
        'Display a menu for selecting the soundcard'),
    ('CLOSE', 'close',
        'Close the application'),

    ('SET_VIEW_MODE', 'mode_playback',
        'Display playback controls'),
    ('SET_VIEW_MODE', 'mode_capture',
        'Display capture controls'),
    ('SET_VIEW_MODE', 'mode_all',
        'Display all controls'),
    ('TOGGLE_VIEW_MODE', 'mode_toggle',
        'Switch between displaying playback/capture/all controls'),

    ('REFRESH', 'refresh',
        'Refresh the screen'),
    ('NEXT', 'next',
        'Select next control'),
    ('PREVIOUS', 'previous',
        'Select previous control'),
    ('BALANCE_CONTROL', 'balance_control',
        'Balance control volumes'),

    ('CONTROL_DOWN_N', 'control_down_<N>',
        'Change control by <N> percent down'),
    ('CONTROL_DOWN_LEFT_N', 'control_down_left_<N>',
        'Change left channel by <N> percent down'),
    ('CONTROL_DOWN_RIGHT_N', 'control_down_right_<N>',
        'Change right channel by <N> percent down'),

    ('CONTROL_UP_N', 'control_up_<N>',
        'Change control by <N> percent up'),
    ('CONTROL_UP_LEFT_N', 'control_up_left_<N>',
        'Change left channel by <N> percent up'),
    ('CONTROL_UP_RIGHT_N', 'control_up_right_<N>',
        'Change right channel by <N> percent up'),

    ('TOGGLE_MUTE', 'toggle_mute',
        'Toggle muting of control'),
    ('TOGGLE_MUTE_LEFT', 'toggle_mute_left',
        'Toggle muting left channel of control'),
    ('TOGGLE_MUTE_RIGHT', 'toggle_mute_right',
        'Toggle muting right channel of control'),

    ('TOGGLE_CAPTURE', 'toggle_capture',
        'Toggle capturing of control'),
    ('TOGGLE_CAPTURE_LEFT', 'toggle_capture_left',
            'Toggle capturing left channel of control'),
    ('TOGGLE_CAPTURE_RIGHT', 'toggle_capture_right',
            'Toggle capturing right channel of control'),

    ('CONTROL_N_PERCENT', 'control_set_<N>',
            'Set control to <N> percent'),

    ('CONTROL_FOCUS_N', 'control_focus_<N>',
            'Focus control number <N>')
]

CMD_TEXTBOX = [
    ('TOP', 'top', 'Go to first line'),
    ('BOTTOM', 'bottom', 'Go to last line'),
    ('UP', 'up', 'Scroll text up by one line'),
    ('DOWN', 'down', 'Scroll text down by one line'),
    ('LEFT', 'left', 'Scroll text left by one column'),
    ('RIGHT', 'right', 'Scroll text right by one column'),
    ('PAGE_UP', 'page_up', 'Scroll text up by half a page'),
    ('PAGE_DOWN', 'page_down', 'Scroll text down by half a page'),
    ('PAGE_LEFT', 'page_left', 'Scroll text left by half a page'),
    ('PAGE_RIGHT', 'page_right', 'Scroll text right by half a page'),
    ('CLOSE', 'close', 'Close textbox')
]

# =============================================================================
# THEME ELEMENTS ==============================================================
# =============================================================================

THEME_ELEMENTS = [
    ('mixer_frame', 'Frame around the mixer'),
    ('mixer_text', 'Default text color (used for upper labels __Card:__, __Chip:__, ...)'),
    ('mixer_active', 'Color of active labels (__[Playback]__)'),
    ('ctl_frame', 'Frame around the volume bar controls'),
    ('ctl_mute', 'Text color for indicating the mute state (__MM__)'),
    ('ctl_nomute', 'Text color for indicating the unmute state (__OO__)'),
    ('ctl_capture', 'Text color for the capture label (__CAPTURE__)'),
    ('ctl_nocapture', 'Text color for disabled capture label (__-------__)') ,
    ('ctl_label', 'Color of label underneath mixer controls (__Master__, __Headphone__, ...)'),
    ('ctl_label_focus', 'Color of label underneath focused mixer control'),
    ('ctl_mark_focus', 'Color of __<__ __>__ marks beside focused mixer label'),
    ('ctl_bar_lo', 'Lower volume bar'),
    ('ctl_bar_mi', 'Middle volume bar'),
    ('ctl_bar_hi', 'Upper volume bar'),
    ('ctl_inactive', 'Color of inactive control'),
    ('ctl_label_inactive', 'Color for inactive label'),
    ('errormsg', 'Color used for error message textbox'),
    ('infomsg', 'Color used for information message textbox'),
    ('textbox', 'Color used for textbox (help screen)'),
    ('textfield', 'Color used for user input'),
    ('menu', 'Color used for menu'),
    ('menu_selected', 'Color used for selected entry in menu')
]

# =============================================================================
# OPTIONS =====================================================================
# =============================================================================

OPTIONS = [
    ('mouse_wheel_step', 'Sets how many percent the volume changes using the mousewheel'),
    ('mouse_wheel_focuses_control', 'If enabled (__1__) the controls get refocused when a mousewheel event occurs. __0__ disables it.')
]

# Translate a C command enum with arguments to a configuration line
#   CMD_WITH_ARG(CMD_MIXER_CONTROL_UP_N, 10) -> ('', 'control_up_10')
#   CMD_TEXTBOX_PAGE_RIGHT                   -> ('textbox', 'page_right')
def c_enum_to_config_command(c_enum, c_arg):
    if c_enum.startswith('MIXER_'):
        widget = ''
        cmds = CMD_MIXER
    elif c_enum.startswith('TEXTBOX_'):
        widget = 'textbox'
        cmds = CMD_TEXTBOX
    else:
        raise Exception(c_enum)
    c_enum = str_remove(c_enum, 'MIXER_', 'TEXTBOX_')

    if c_enum == 'SET_VIEW_MODE':
        command = 'mode_' + {
            'VIEW_MODE_PLAYBACK': 'playback',
            'VIEW_MODE_CAPTURE': 'capture',
            'VIEW_MODE_ALL': 'all'
        }[c_arg]
    else:
        command = { cmd[0]:cmd[1] for cmd in cmds }[c_enum].replace('<N>', c_arg)

    return (widget, command)

def str_remove(string, *search):
    for i in search:
        string = string.replace(i,'')
    return string

def fix_key(k):
    prefix = ''
    k = str_remove(k, 'KEY_')

    if 'F(' in k:
        k = str_remove(k, '(', ')') 

    if 'CNTRL' in k:
        k = str_remove(k, 'CNTRL(', ')')
        prefix += 'C-'

    if 'ALT' in k:
        k = str_remove(k, 'ALT(', ')')
        prefix += 'A-'

    if k[0] == "'":
        k = eval(k)

    k = {
        ' ': 'SPACE',
        '\t': 'TAB',
        '\n': 'RETURN',
        '\r': 'RETURN',
        '27': 'ESCAPE',
    }.get(k, k)

    return prefix+k

def gen_options():
    yield 'set mouse_wheel_step 1'
    yield 'set mouse_wheel_focuses_control 1'

def gen_default_bindings(file):
    binding_re = RE(r'\[  (.+)  \]  =  ' +
        r'(?:CMD_WITH_ARG  \()?  CMD_([^,]+)  (?:,  )?  ([^\)]+)?')

    bindings = []
    with open(file, 'r') as fh:
        for line in fh:
            bindings.extend(binding_re.findall(line))

    bindings = [
            (fix_key(key), *c_enum_to_config_command(command, arg))
            for key, command, arg in bindings
    ]

    bindings.sort(key=lambda i: i[2]) # command
    bindings.sort(key=lambda i: i[1]) # widget

    for key, widget, command in bindings:
        yield 'bind %s\t\t%s %s' % (key, widget, command)

def gen_colors(file):
    color_pair_re = RE(r'get_color_pair  \(  (\w+)  ,  (\w+)  \)')
    element_re = RE(r'attrs.(\w+)  =  (A_\w+  \|  )*  COLOR_PAIR  \(  (\d+)  \)')

    color_pairs = [('none', 'none')]
    elements = []
    with open(file, 'r') as fh:
        for line in fh:
            color_pairs.extend(color_pair_re.findall(line))
            elements.extend(element_re.findall(line))

    color_pairs = [
        (
            fg.replace('COLOR_','').lower(),
            bg.replace('COLOR_','').lower()
        ) for fg, bg in color_pairs
    ]

    for e in elements:
        color_cmd = 'color\t%s\t%s\t%s\t%s' % (
                e[0],
                color_pairs[int(e[2])][0],
                color_pairs[int(e[2])][1],
                str_remove(e[1], 'A_', '|').lower()
        )
        yield color_cmd.strip()

documentation = '''\

## FILES
Configuration is read from the following files:

__$XDG_CONFIG_HOME__/alsamixer.rc

__$HOME__/.config/alsamixer.rc

__$HOME__/.alsamixer.rc

After a file has been successfully read no further files are processed.

## CONFIGURATION

Comment char is '#'.

Everything after comment char is ignored.

No quoting allowed.

.TP
**color** __element__ __foreground__ __background__ [__attribute__...]

__element__: A theme element as listed in **THEME ELEMENTS**.

__foreground__/__background__: __red__, __green__, __yellow__, __blue__, __magenta__, __cyan__, __white__, __black__, __default__.

__attribute__:  __bold__, __normal__, __reverse__, __underline__, __dim__, __italic__, __blink__.

.TP
**set** __option__ __value__

__mouse_wheel_step__ = __<number>__

.TP
**bind** __key__ [__widget__] __command__

__key__:
    - a single character
    - a combination with control and/or meta/alt key: __C-x__, __M-x__, __A-x__, __C-M-x__
    - a curses key constant as found in **getch(3)** (like __Up__, __Left__, __Home__, __F1__, __F12__, ...)
    - one of these aliases: __Escape__, __Del__, __Delete__, __Insert__, __PageDown__, __PageUp__, __Space__, __Tab__

__widget__:
    - __mixer__ (default),
    - __textbox__

### OPTIONS
{options}

### THEME ELEMENTS
{theme_elements}

### MIXER COMMANDS

{mixer_commands}

### TEXTBOX COMMANDS

{textbox_commands}
'''

default_config = '''\
# alsamixer.rc - Default configuration file for alsamixer(1)

### Options ###
{options}

### Key bindings ###
{bindings}

### Colors ###
{colors}
'''

def replace_markup(string, replace):
    r = replace
    s = string
    s = re.sub(r'\*\*(.+?)\*\*', lambda m: r['**'][0] + m[1] + r['**'][1], s)
    s = re.sub(r'__(.+?)__', lambda m: r['__'][0] + m[1] + r['__'][1], s)
    s = s.replace('###', replace['###'])
    s = s.replace('##', replace['##'])
    return s

def make_manpage():
    replace = {
        '**': (r'\fB', r'\fP'),
        '__': (r'\fI', r'\fP'),
        '##': r'.SH',
        '###': r'.SS'
    }

    theme_elements = '\n\n'.join(map(
        lambda i: "__%s__\n%s" % (i[0], i[1]), THEME_ELEMENTS))

    mixer_commands = '\n\n'.join(map(
        lambda i: "__%s__\n%s" % (i[1], i[2]), CMD_MIXER))

    textbox_commands = '\n\n'.join(map(
        lambda i: "__%s__\n%s" % (i[1], i[2]), CMD_TEXTBOX))

    options = '\n\n'.join(map(
        lambda i: "__%s__\n%s" % (i[0], i[1]), OPTIONS))

    doc = documentation.format(
            options=options,
            theme_elements=theme_elements,
            mixer_commands=mixer_commands,
            textbox_commands=textbox_commands
    )

    print(replace_markup(doc, replace))

def make_configuration(bindings_c, colors_c, comment=False):
    if comment:
        nl = '\n#'
        beg = '#'
    else:
        nl = '\n'
        beg = ''

    print(default_config.format(
        options=beg + (nl.join(gen_options())),
        bindings=beg + (nl.join(gen_default_bindings(bindings_c))),
        colors=beg + (nl.join(gen_colors(colors_c)))
    ))

# =============================================================================
# Command line handling =======================================================
# =============================================================================
argp = argparse.ArgumentParser()
subp = argp.add_subparsers()

cfgp = subp.add_parser('mkconfig')
cfgp.set_defaults(cmd='mkconfig')
cfgp.add_argument('bindings_source_file')
cfgp.add_argument('colors_source_file')
cfgp.add_argument('--comment', action='store_true')
manp = subp.add_parser('mkman')
manp.set_defaults(cmd='mkman')

args = argp.parse_args()
if not 'cmd' in args:
    argp.print_help()
    sys.exit(1)
if args.cmd == 'mkman':
    make_manpage()
elif args.cmd == 'mkconfig':
    make_configuration(args.bindings_source_file, args.colors_source_file, args.comment)
else:
    argp.print_help()
    sys.exit(1)
