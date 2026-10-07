# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
"""Exercise source selection without requiring a BSP or installing SCons."""
import pathlib
import re
import runpy
import sys
import types

root = pathlib.Path(__file__).resolve().parents[3]
port = root / 'ports/rtthread'
config = {'AGENT_MAX_TOOLS': 12}
captured = {}
building = types.ModuleType('building')
building.GetCurrentDir = lambda: str(port)
building.GetDepend = lambda name: config.get(name, 0)

def define_group(name, sources, **kwargs):
    captured.update(sources=sources, **kwargs)
    return []

building.DefineGroup = define_group
sys.modules['building'] = building

def run():
    captured.clear()
    runpy.run_path(str(port / 'SConscript'), init_globals={'Return': lambda name: None})
    for source in captured['sources']:
        assert pathlib.Path(source).is_file(), source
    assert len(captured['sources']) == len(set(captured['sources']))
    return {str(pathlib.Path(p).relative_to(root)) for p in captured['sources']}

sources = run()
cmake_core = (root / 'CMakeLists.txt').read_text().split('set(AGENT_CORE_SOURCES', 1)[1].split(')', 1)[0]
assert {p for p in sources if p.startswith('src/')} == set(re.findall(r'src/\S+\.c', cmake_core))
assert 'codecs/json/reader.c' in sources and 'codecs/json/writer.c' not in sources
assert not any(p.startswith('ports/') for p in sources)
assert ('AGENT_BUILD_CONFIG_HEADER', '\\"rtconfig.h\\"') in captured['CPPDEFINES']
config.update(AGENT_MAX_TOOLS=0)
assert 'codecs/json/reader.c' not in run()
config.update(AGENT_PROVIDER_OPENAI=1, AGENT_SESSION_JSONL=1, AGENT_SESSION_RAM=1,
              AGENT_FILE_STORE=1, AGENT_POSIX_FILE_STORE=1, AGENT_MEMORY_MARKDOWN=1,
              AGENT_PORT_RTTHREAD_RUNTIME=1, AGENT_PORT_RTTHREAD_TRANSPORT=1,
              AGENT_PORT_RTTHREAD_FILE_STORE=1, AGENT_PORT_RTTHREAD_NO_SYMLINKS=1,
              PKG_USING_WEBCLIENT=1, RT_USING_HEAP=1)
sources = run()
assert 'providers/model/openai/src/openai_request.c' in sources
assert 'providers/storage/jsonl/src/file_store_bind.c' in sources
assert 'ports/rtthread/runtime/src/runtime.c' in sources
assert 'ports/rtthread/transport/src/transport.c' in sources
assert 'ports/rtthread/storage/src/file_store.c' in sources
assert ('AGENT_POSIX_FILE_STORE_NO_SYMLINKS', 1) in captured['CPPDEFINES']
config['AGENT_FILE_STORE'] = 0
try:
    run()
except ValueError:
    pass
else:
    raise AssertionError('Invalid file-store composition must fail')
