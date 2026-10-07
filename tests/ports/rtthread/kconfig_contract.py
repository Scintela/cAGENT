# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: 2026 tanglinjie
"""Optional Kconfiglib check; SDK symbols are fixtures, not a full BSP."""
import os
import pathlib
import kconfiglib

root = pathlib.Path(__file__).resolve().parents[3]
os.environ['CAGENT_SOURCE_ROOT'] = str(root)
kconf = kconfiglib.Kconfig(str(root / 'tests/ports/rtthread/kconfig/Kconfig'))
symbols = kconf.syms
assert not kconf.warnings, kconf.warnings
for part in ['RUNTIME', 'TRANSPORT', 'FILE_STORE']:
    assert symbols['AGENT_PORT_RTTHREAD_' + part].str_value == 'n'
symbols['AGENT_PORT_RTTHREAD_RUNTIME'].set_value('y')
assert symbols['AGENT_PORT_RTTHREAD_RUNTIME'].str_value == 'y'
symbols['AGENT_PORT_RTTHREAD_TRANSPORT'].set_value('y')
assert symbols['AGENT_PORT_RTTHREAD_TRANSPORT'].str_value == 'n'
for name in ['RT_USING_HEAP', 'PKG_USING_WEBCLIENT']:
    symbols[name].set_value('y')
assert symbols['AGENT_PORT_RTTHREAD_TRANSPORT'].str_value == 'y'
symbols['WEBCLIENT_DEBUG'].set_value('y')
assert symbols['AGENT_PORT_RTTHREAD_TRANSPORT'].str_value == 'n'
symbols['AGENT_PORT_RTTHREAD_FILE_STORE'].set_value('y')
assert symbols['AGENT_PORT_RTTHREAD_FILE_STORE'].str_value == 'n'
for name in ['RT_USING_DFS', 'RT_USING_POSIX_FS']:
    symbols[name].set_value('y')
assert symbols['AGENT_PORT_RTTHREAD_FILE_STORE'].str_value == 'y'
assert symbols['AGENT_FILE_STORE'].str_value == 'y'
assert symbols['AGENT_POSIX_FILE_STORE'].str_value == 'y'
assert symbols['AGENT_PORT_RTTHREAD_NO_SYMLINKS'].str_value == 'n'
symbols['AGENT_PROFILE_TINY'].set_value('y')
assert symbols['AGENT_MAX_TOOLS'].str_value == '4'
assert not kconf.warnings, kconf.warnings
print('PASS: RT-Thread Kconfig dependencies, safe defaults and shared Core Profile.')
