"""Test Sarek configuration precedence and the default compilation mode."""
from pathlib import Path
import argparse
import os
import re
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--compiler', default='/ucrt64/bin/g++.exe')
args = parser.parse_args()
root = Path(__file__).resolve().parents[3]
source = root/'build-nx/deps/dxvk-sarek'
output = root/'build-nx/sarek-work/config-tests'
output.mkdir(parents=True, exist_ok=True)
exe = output/'sarek-config.exe'
manager = (source/'src/dxvk/dxvk_pipemanager.cpp').read_text()
header = (source/'src/dxvk/dxvk_pipemanager.h').read_text()
mode = re.search(r'  enum class DxvkShaderCompileMethod.*?\n  };', header, re.S).group()
functions = manager[manager.index('    const char* method_name('):manager.index('    bool is_state_cache_enabled(')]
(output/'compile-method.inc').write_text(mode+'\n'+functions)
subprocess.run([
    args.compiler, '-std=c++20', '-O2', '-Wall', '-Wextra',
    '-I'+str(output),
    '-I'+str(source/'src'), '-I'+str(source/'include/vulkan/include'),
    str(root/'tools/nx/tests/sarek-config.cpp'),
    str(source/'src/util/config/config.cpp'),
    str(source/'src/dxvk/dxvk_options.cpp'),
    str(root/'src/nx/dxvk/sarek_string.cpp'), '-o', str(exe),
], check=True)

custom = ('dxvk.maxMemoryBudget = 768\nd3d9.maxAvailableMemory = 768\n'
          'dxvk.shaderCompilationMethod = none\ndxvk.numCompilerThreads = 2\n')
cases = [
    ('file-options', custom, '', '', (768, 768, 'none', 2, 'none')),
    ('env-options', '', custom.replace('\n', ';'), '', (768, 768, 'none', 2, 'none')),
    ('env-async', custom, 'dxvk.shaderCompilationMethod=async', '', (768, 768, 'async', 2, 'async')),
    ('env-dyasync', custom, 'dxvk.shaderCompilationMethod=dyasync', '', (768, 768, 'dyasync', 2, 'dyasync')),
    ('custom-budget', custom.replace('Budget = 768', 'Budget = 1536'), '', '', (1536, 768, 'none', 2, 'none')),
    ('custom-report', custom.replace('Memory = 768', 'Memory = 2048'), '', '', (768, 2048, 'none', 2, 'none')),
    ('env-override', custom, 'dxvk.maxMemoryBudget=1024;dxvk.numCompilerThreads=3', '', (1024, 768, 'none', 3, 'none')),
    ('inactive-section', '[OtherGame.nro]\n'+custom, '', '', (0, 4096, '', 0, 'async')),
    ('active-section', '[OpenBLOPS-NX.nro]\n'+custom, '', '', (768, 768, 'none', 2, 'none')),
    ('shipped', (root/'switch-meta/dxvk-sarek.conf').read_text(), '', '', (0, 4096, 'async', 2, 'async')),
    ('defaults', '', '', '', (0, 4096, '', 0, 'async')),
    ('shader-env-override', custom, '', 'async', (768, 768, 'none', 2, 'async')),
    ('unknown-mode', 'dxvk.shaderCompilationMethod = invalid\n', '', 'invalid', (0, 4096, 'invalid', 0, 'async')),
]
for name, contents, overrides, shader_override, expected in cases:
    config = output/(name+'.conf')
    config.write_text(contents)
    env = dict(os.environ, DXVK_CONFIG_FILE=str(config.relative_to(root)), DXVK_CONFIG=overrides,
               DXVK_SHADER_COMPILATION_METHOD=shader_override)
    subprocess.run([str(exe), *map(str, expected)], cwd=root, env=env, check=True)
    print('PASS:', name)
