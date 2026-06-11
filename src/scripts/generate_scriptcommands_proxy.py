#!/usr/bin/env python3
"""Generate ScriptCommandsProxy for MinGW (default args on function pointers)."""
import re
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
HEADER_IN = os.path.join(SCRIPT_DIR, 'scriptcommands.h')
HEADER_OUT = os.path.join(SCRIPT_DIR, 'scriptcommands_proxy.h')
CPP_OUT = os.path.join(SCRIPT_DIR, 'scriptcommands_proxy.cpp')


def parse_methods(text):
    methods = []
    for line in text.splitlines():
        line = line.split('//')[0].rstrip()
        if not line:
            continue
        m = re.match(
            r'^\s+(.+?)\s*\(\s*\*\s*(\w+)\s*\)\s*\((.*)\)\s*;\s*$',
            line,
        )
        if not m:
            continue
        ret, name, args = m.group(1).strip(), m.group(2), m.group(3).strip()
        if name in ('Size', 'Version'):
            continue
        methods.append((ret, name, args))
    return methods


def emit_proxy(methods):
    h = []
    h.append('/* Auto-generated for MinGW scripts.dll - do not edit by hand. */')
    h.append('#ifndef SCRIPTCOMMANDS_PROXY_H')
    h.append('#define SCRIPTCOMMANDS_PROXY_H')
    h.append('')
    h.append('#include "scriptcommands.h"')
    h.append('')
    h.append('class ScriptCommandsProxy {')
    h.append('public:')
    h.append('\tScriptCommandsProxy() : m_p(NULL) {}')
    h.append('\tvoid Attach(ScriptCommands *p) { m_p = p; }')
    h.append('\tScriptCommands *Raw() const { return m_p; }')
    h.append('\tunsigned int Size() const { return m_p ? m_p->Size : 0; }')
    h.append('\tunsigned int Version() const { return m_p ? m_p->Version : 0; }')
    h.append('')

    cpp = []
    cpp.append('/* Auto-generated for MinGW scripts.dll - do not edit by hand. */')
    cpp.append('#include "scriptcommands_proxy.h"')
    cpp.append('')
    cpp.append('#include <stdio.h>')
    cpp.append('#include <stdarg.h>')
    cpp.append('')
    cpp.append('ScriptCommandsProxy g_ScriptCommandsProxy;')
    cpp.append('')

    for ret, name, args in methods:
        arg_names = []
        depth = 0
        cur = ''
        for ch in args + ',':
            if ch == ',' and depth == 0:
                part = cur.strip()
                if part:
                    if part == '...':
                        arg_names.append('...')
                    else:
                        tok = part.split('=')[0].strip()
                        pname = re.sub(r'[&*]', ' ', tok).split()[-1]
                        arg_names.append(pname)
                cur = ''
            else:
                if ch == '(':
                    depth += 1
                elif ch == ')':
                    depth -= 1
                cur += ch
        if '...' in arg_names:
            h.append(f'\t{ret} {name}({args});')
            continue

        forward = ', '.join(arg_names)
        if forward == 'void':
            forward = ''
        call = f'{name}({forward})' if forward else f'{name}()'

        args_decl = re.sub(r'\s*=\s*[^,()]+', '', args)
        h.append(f'\t{ret} {name}({args});')

        ret_stripped = ret.strip()
        if ret_stripped == 'void':
            cpp.append(f'void ScriptCommandsProxy::{name}({args_decl})')
            cpp.append('{')
            cpp.append('\tif (!m_p) return;')
            cpp.append(f'\tm_p->{call};')
            cpp.append('}')
        elif ret_stripped in ('int', 'unsigned int'):
            cpp.append(f'{ret} ScriptCommandsProxy::{name}({args_decl})')
            cpp.append('{')
            cpp.append('\tif (!m_p) return 0;')
            cpp.append(f'\treturn m_p->{call};')
            cpp.append('}')
        elif ret_stripped == 'bool':
            cpp.append(f'{ret} ScriptCommandsProxy::{name}({args_decl})')
            cpp.append('{')
            cpp.append('\tif (!m_p) return false;')
            cpp.append(f'\treturn m_p->{call};')
            cpp.append('}')
        elif ret_stripped == 'float':
            cpp.append(f'{ret} ScriptCommandsProxy::{name}({args_decl})')
            cpp.append('{')
            cpp.append('\tif (!m_p) return 0.0f;')
            cpp.append(f'\treturn m_p->{call};')
            cpp.append('}')
        elif '*' in ret:
            cpp.append(f'{ret} ScriptCommandsProxy::{name}({args_decl})')
            cpp.append('{')
            cpp.append('\tif (!m_p) return NULL;')
            cpp.append(f'\treturn m_p->{call};')
            cpp.append('}')
        else:
            cpp.append(f'{ret} ScriptCommandsProxy::{name}({args_decl})')
            cpp.append('{')
            cpp.append('\tif (!m_p) return 0;')
            cpp.append(f'\treturn m_p->{call};')
            cpp.append('}')
        cpp.append('')

    h.append('private:')
    h.append('\tScriptCommands *m_p;')
    h.append('};')
    h.append('')
    h.append('')
    h.append('extern ScriptCommandsProxy g_ScriptCommandsProxy;')
    h.append('#endif')

    cpp.append('void ScriptCommandsProxy::Debug_Message(char *format, ...)')
    cpp.append('{')
    cpp.append('\tif (!m_p) return;')
    cpp.append('\tva_list ap;')
    cpp.append('\tva_start(ap, format);')
    cpp.append('\tchar buf[4096];')
    cpp.append('\tvsnprintf(buf, sizeof(buf), format, ap);')
    cpp.append('\tva_end(ap);')
    cpp.append('\tm_p->Debug_Message(buf);')
    cpp.append('}')
    cpp.append('')
    cpp.append('void ScriptCommandsProxy::Debug_Message_2(char *format, ...)')
    cpp.append('{')
    cpp.append('\tva_list ap;')
    cpp.append('\tva_start(ap, format);')
    cpp.append('\tchar buf[4096];')
    cpp.append('\tvsnprintf(buf, sizeof(buf), format, ap);')
    cpp.append('\tva_end(ap);')
    cpp.append('\tm_p->Debug_Message(buf);')
    cpp.append('}')
    cpp.append('')

    return '\n'.join(h) + '\n', '\n'.join(cpp) + '\n'


def main():
    with open(HEADER_IN, 'r', encoding='latin-1') as f:
        text = f.read()
    start = text.find('typedef struct {')
    end = text.find('} ScriptCommands;')
    block = text[start:end]
    methods = parse_methods(block)
    if len(methods) < 50:
        raise SystemExit(f'expected many methods, got {len(methods)}')
    h, cpp = emit_proxy(methods)
    with open(HEADER_OUT, 'w', encoding='latin-1') as f:
        f.write(h)
    with open(CPP_OUT, 'w', encoding='latin-1') as f:
        f.write(cpp)
    print(f'generated {len(methods)} wrappers -> {HEADER_OUT}')


if __name__ == '__main__':
    main()
