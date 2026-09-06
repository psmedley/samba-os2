
# a waf tool to add autoconf-like macros to the configure section
# and for SAMBA_ macros for building libraries, binaries etc

import os, sys, re, shutil, fnmatch, types
from waflib import Build, Options, Task, Utils, TaskGen, Logs, Context, Errors
from waflib.Configure import conf
from waflib.Logs import debug
from samba_utils import SUBST_VARS_RECURSIVE
TaskGen.task_gen.apply_verif = Utils.nada

# bring in the other samba modules
from samba_utils import *
from samba_utils import symlink
from samba_version import *
from samba_autoconf import *
from samba_patterns import *
from samba_pidl import *
from samba_autoproto import *
from samba_python import *
from samba_perl import *
from samba_deps import *
from samba_bundled import *
from samba_third_party import *
import samba_cross
import samba_install
import samba_conftests
import samba_abi
import samba_headers
import generic_cc
import samba_dist
import samba_wildcard
import symbols
import pkgconfig
import configure_file
import samba_waf18

LIB_PATH="shared"

os.environ['PYTHONUNBUFFERED'] = '1'

if Context.HEXVERSION not in (0x2001900,):
    Logs.error('''
Please use the version of waf that comes with Samba, not
a system installed version. See http://wiki.samba.org/index.php/Waf
for details.

Alternatively, please run ./configure and make as usual. That will
call the right version of waf.''')
    sys.exit(1)

@conf
def SAMBA_BUILD_ENV(conf):
    '''create the samba build environment'''
    conf.env.BUILD_DIRECTORY = conf.bldnode.abspath()
    mkdir_p(os.path.join(conf.env.BUILD_DIRECTORY, LIB_PATH))
    mkdir_p(os.path.join(conf.env.BUILD_DIRECTORY, LIB_PATH, "private"))
    mkdir_p(os.path.join(conf.env.BUILD_DIRECTORY, "modules"))
    mkdir_p(os.path.join(conf.env.BUILD_DIRECTORY, "plugins"))
    mkdir_p(os.path.join(conf.env.BUILD_DIRECTORY, 'python/samba/dcerpc'))
    # this allows all of the bin/shared and bin/python targets
    # to be expressed in terms of build directory paths
    mkdir_p(os.path.join(conf.env.BUILD_DIRECTORY, 'default'))
    for (source, target) in [('shared', 'shared'), ('modules', 'modules'), ('plugins', 'plugins'), ('python', 'python')]:
        link_target = os.path.join(conf.env.BUILD_DIRECTORY, 'default/' + target)
        if not os.path.lexists(link_target):
            symlink('../' + source, link_target)

    # get perl to put the blib files in the build directory
    blib_bld = os.path.join(conf.env.BUILD_DIRECTORY, 'default/pidl/blib')
    blib_src = os.path.join(conf.srcnode.abspath(), 'pidl/blib')
    mkdir_p(blib_bld + '/man1')
    mkdir_p(blib_bld + '/man3')
    if os.path.islink(blib_src):
        os.unlink(blib_src)
    elif os.path.exists(blib_src):
        shutil.rmtree(blib_src)


OS2_DLL_MAP = {
    'samba-credentials': 'smbcred',
    'samba-util':        'smbutil',
    'samba-hostconfig':  'smbhcfg',
    'samba-debug':       'smbdbg',
    'samba-sockets':     'smbsock',
    'samba-security':    'smbsec',
    'samba3-util':       'smb3utl',
    'secrets3':          'secrets',
    'messages-dgm':      'msgdgm',
    'messages-util':     'msgutl',
    'libsmb':            'libsmb',
    'msrpc3':            'msrpc3',
    'ndr-samba':         'ndrsmb',
    'ndr-standard':      'ndrstd',
    'dcerpc-samba':      'dcerpc',
}

def os2_shorten_dll_name(name):
    """Truncate or map library names to <= 8 characters for OS/2."""
    
    # 1. Strip Samba's private library suffix before any truncation
    if name.endswith('-samba4'):
        name = name[:-7]
        
    if name in OS2_DLL_MAP:
        return OS2_DLL_MAP[name]
    
    clean_name = name
    for prefix in ['lib', 'samba-']:
        if clean_name.startswith(prefix):
            clean_name = clean_name[len(prefix):]
            
    clean_name = clean_name.replace('-', '').replace('_', '')
    return clean_name[:8]

# Define a custom Waf task to run emxexp
class os2_def(Task.Task):
    color = 'CYAN'
    def run(self):
        target_name = self.outputs[0].name.replace('.def', '')
        def_file = self.outputs[0].abspath()
        
        with open(def_file, 'w') as f:
            f.write("LIBRARY %s INITINSTANCE TERMINSTANCE\n" % target_name)
            f.write("DATA MULTIPLE NONSHARED\n")
            f.write("EXPORTS\n")
            
        seen_symbols = set()
        
        for node in self.inputs:
            cmd = ['emxexp.exe', node.abspath()]
            try:
                out = self.generator.bld.cmd_and_log(cmd, quiet=Context.BOTH)
            except Exception:
                continue 
                
            lines = [
                line.strip() for line in out.splitlines() 
                if line.strip() and line.strip().upper() != 'EXPORTS'
            ]
            
            with open(def_file, 'a') as f:
                for line in lines:
                    if line not in seen_symbols:
                        f.write("%s\n" % line)
                        seen_symbols.add(line)
                        
        return 0

@TaskGen.feature('cshlib')
@TaskGen.after_method('apply_link')
def os2_shared_lib_hook(self):
    if not sys.platform.startswith('os2'):
        return
    self.vnum = None
    self.link_task.env.LINKFLAGS = [
        f for f in self.link_task.env.LINKFLAGS 
        if not f.startswith('-Wl,-os2-dummy-soname') and f not in ['-lcx', '-lat-funcs', '-lpthread']
    ]
    
    self.link_task.env.RPATH = []
    
    short_name = os2_shorten_dll_name(self.target)
    old_dll_node = self.link_task.outputs[0]
    
    new_dll_node = old_dll_node.parent.find_or_declare(short_name + '.dll')
    implib_node  = old_dll_node.parent.find_or_declare('lib%s.a' % self.target)
    
    self.link_task.outputs = [new_dll_node, implib_node]
    if '-Zdll' not in self.link_task.env.LINKFLAGS:
        self.link_task.env.append_value('LINKFLAGS', '-Zdll')
    if '-Zomf' not in self.link_task.env.LINKFLAGS:
        self.link_task.env.append_value('LINKFLAGS', '-Zomf')
    objs = [node for node in self.link_task.inputs if node.name.endswith('.o') or node.name.endswith('.a')]
    # --- PERFECT TOPOLOGICAL STATIC LIBRARY INJECTOR ---
    def get_all_deps(tg_name, seen=None, ordered=None, visiting=None):
        if seen is None: seen = set()
        if ordered is None: ordered = []
        if visiting is None: visiting = set()
        
        if tg_name in seen or tg_name in visiting:
            return ordered
            
        visiting.add(tg_name)
        try:
            tg = self.bld.get_tgen_by_name(tg_name)
            direct = []
            for attr in ['use', 'samba_deps', 'deps', 'public_deps', 'samba_modules', 'samba_subsystem']:
                if hasattr(tg, attr):
                    v = getattr(tg, attr)
                    if isinstance(v, str):
                        direct.extend(v.split())
                    elif isinstance(v, list):
                        direct.extend(v)
            for d in direct:
                get_all_deps(d, seen, ordered, visiting)
        except Exception:
            pass
            
        visiting.remove(tg_name)
        seen.add(tg_name)
        ordered.append(tg_name)
        return ordered
    seen_set = set()
    ordered_list = []
    get_all_deps(self.name, seen_set, ordered_list, set())
    all_transitive = ordered_list[::-1]
    
    hidden_seen = set()
    hidden_ordered = []
    # Explicitly append invisible Linux dependencies to the absolute bottom of the DAG
    hidden_libs = ['samba-sockets', 'tsocket', 'tevent-util', 'wbclient', 'samba-credentials', 'samba-security', 'gensec', 'authkrb5', 'cli-auth', 'smb-crypto']
    for hidden in hidden_libs:
        get_all_deps(hidden, hidden_seen, hidden_ordered, set())
        
    for dep in hidden_ordered[::-1]:
        if dep not in all_transitive:
            all_transitive.append(dep)
    if self.name in all_transitive:
        all_transitive.remove(self.name)
        
    transitive_archives = []
    for dep_name in all_transitive:
        try:
            dep_tg = self.bld.get_tgen_by_name(dep_name)
            if hasattr(dep_tg, 'link_task'):
                for out in dep_tg.link_task.outputs:
                    if out.name.endswith('.a') and out.abspath() not in transitive_archives:
                        transitive_archives.append(out.abspath())
        except Exception:
            continue
            
    self.link_task.os2_transitive_archives = transitive_archives
    self.link_task.os2_rsp_file = self.link_task.outputs[0].abspath() + '.rsp'
    if not objs:
        return
        
    def_node = self.path.find_or_declare(short_name + '.def')
    self.create_task('os2_def', objs, def_node)
    
    if not hasattr(self.link_task, 'dep_nodes'):
        self.link_task.dep_nodes = []
    self.link_task.dep_nodes.append(def_node)
    
    self.link_task.env.append_value('LINKFLAGS', def_node.path_from(self.bld.bldnode))
    self.link_task.env.OS2_DEF_FILE = def_node.abspath()
    self.link_task.env.OS2_IMPLIB_FILE = implib_node.abspath()
    
    orig_exec = self.link_task.exec_command
    def os2_exec_command(task, cmd, **kw):
        import os
        rsp_file = task.os2_rsp_file
        
        args = cmd[1:] if isinstance(cmd, list) else cmd.split()[1:]
        base_cmd = cmd[0] if isinstance(cmd, list) else cmd.split()[0]
        
        pre_flags, post_flags = [], []
        skip_next = False
        
        for arg in args:
            if skip_next:
                pre_flags.append(arg)
                skip_next = False
                continue
            if arg == '-o':
                pre_flags.append(arg)
                skip_next = True
                continue
            if arg.startswith('-l') or arg.startswith('-L') or arg.startswith('-Wl,'):
                post_flags.append(arg)
            else:
                pre_flags.append(arg)
                
        def write_item(f, item):
            item = item.replace('\\', '/')
            if ' ' in item and not item.startswith('-'):
                f.write('"%s"\n' % item)
            else:
                f.write('%s\n' % item)
        with open(rsp_file, 'w') as f:
            for item in pre_flags:
                write_item(f, item)
            if hasattr(task, 'os2_transitive_archives'):
                # Reduced to 2 loops of mathematically ordered archives to fit inside OS/2 32KB spawn limit
                for i in range(2):
                    for arch in task.os2_transitive_archives:
                        if os.path.exists(arch):
                            write_item(f, arch)
            for item in post_flags:
                write_item(f, item)
                
            f.write('-lcx\n-lat-funcs\n-lpthread\n')
            
        final_cmd = [base_cmd, '@' + rsp_file]
        if not isinstance(cmd, list):
            final_cmd = " ".join(final_cmd)
            
        ret = orig_exec(final_cmd, **kw)
        if ret != 0: return ret
            
        emximp_cmd = 'emximp.exe -o "%s" "%s"' % (task.env.OS2_IMPLIB_FILE, task.env.OS2_DEF_FILE)
        return orig_exec(emximp_cmd, **kw)
        
    self.link_task.exec_command = types.MethodType(os2_exec_command, self.link_task)

@TaskGen.feature('cshlib')
@TaskGen.after_method('apply_link')
def os2_shared_lib_hook(self):
    if not sys.platform.startswith('os2'):
        return
    self.vnum = None
    self.link_task.env.LINKFLAGS = [
        f for f in self.link_task.env.LINKFLAGS 
        if not f.startswith('-Wl,-os2-dummy-soname') and f not in ['-lcx', '-lat-funcs', '-lattr', '-lpthread']
    ]
    self.link_task.env.RPATH = []
    
    short_name = os2_shorten_dll_name(self.target)
    old_dll_node = self.link_task.outputs[0]
    new_dll_node = old_dll_node.parent.find_or_declare(short_name + '.dll')
    implib_node  = old_dll_node.parent.find_or_declare('lib%s.a' % self.target)
    self.link_task.outputs = [new_dll_node, implib_node]
    if '-Zdll' not in self.link_task.env.LINKFLAGS: self.link_task.env.append_value('LINKFLAGS', '-Zdll')
    if '-Zomf' not in self.link_task.env.LINKFLAGS: self.link_task.env.append_value('LINKFLAGS', '-Zomf')
    def resolve_tgen(name):
        if not name: return None
        candidates = [
            name,
            name + '.objlist',
            name[:-8] if name.endswith('.objlist') else name,
            name[:-7] if name.endswith('-samba4') else name + '-samba4',
            (name[:-7] + '.objlist') if name.endswith('-samba4') else (name + '-samba4.objlist'),
            name.replace('-', '_'),
            name.replace('_', '-'),
            name.replace('-', '_') + '.objlist',
            name.replace('_', '-') + '.objlist',
        ]
        for c in candidates:
            try: return self.bld.get_tgen_by_name(c)
            except Exception: pass
        clean_name = name.lower().replace('_', '-').replace('.objlist', '')
        for group in self.bld.groups:
            for tg in group:
                tg_clean = getattr(tg, 'name', '').lower().replace('_', '-').replace('.objlist', '')
                if tg_clean == clean_name:
                    return tg
        return None
    def get_all_deps(tg_name, seen=None, ordered=None, visiting=None):
        if seen is None: seen = set()
        if ordered is None: ordered = []
        if visiting is None: visiting = set()
        if tg_name in seen or tg_name in visiting: return ordered
        visiting.add(tg_name)
        tg = resolve_tgen(tg_name)
        if tg:
            direct = []
            for attr in ['use', 'samba_deps', 'deps', 'public_deps', 'samba_modules', 'samba_subsystem']:
                if hasattr(tg, attr):
                    v = getattr(tg, attr)
                    if isinstance(v, str): direct.extend(v.split())
                    elif isinstance(v, list): direct.extend(v)
            for d in direct: get_all_deps(d, seen, ordered, visiting)
        visiting.remove(tg_name)
        seen.add(tg_name)
        ordered.append(tg_name)
        return ordered
    seen_set, ordered_list = set(), []
    get_all_deps(self.name, seen_set, ordered_list, set())
    all_transitive = ordered_list[::-1]
    
    hidden_libs = [
        'samba-sockets', 'tsocket', 'tevent-util', 'wbclient', 
        'samba-credentials', 'samba-security', 'gensec', 'authkrb5', 
        'cli-auth', 'smb-crypto',
        'RPC_NDR_NETLOGON', 'RPC_NDR_SPOOLSS', 'RPC_NDR_SAMR', 
        'RPC_NDR_SRVSVC', 'RPC_NDR_EPMAPPER', 'RPC_NDR_WKSSVC',
        'RPC_NDR_INITSHUTDOWN', 'RPC_NDR_WINREG', 'RPC_NDR_LSA',
        'RPC_NDR_DFS', 'XATTR_TDB',
        'ndr-wkssvc', 'dcerpc-wkssvc', 'ndr-initshutdown', 'dcerpc-initshutdown',
        'dcerpc-samba4', 'dcerpc-binding', 'dcerpc'
    ]
    hidden_seen, hidden_ordered = set(), []
    for hidden in hidden_libs: get_all_deps(hidden, hidden_seen, hidden_ordered, set())
        
    for dep in hidden_ordered[::-1]:
        if dep not in all_transitive: all_transitive.append(dep)
    if self.name in all_transitive: all_transitive.remove(self.name)
    seen_sources = set()
    for t in getattr(self, 'compiled_tasks', []):
        if getattr(t, 'inputs', None):
            seen_sources.add(os.path.normcase(t.inputs[0].abspath()).replace('\\', '/'))
    transitive_archives = []
    transitive_objects = []
    for dep_name in all_transitive:
        tg = resolve_tgen(dep_name)
        if not tg: continue
        has_archive = False
        if hasattr(tg, 'link_task') and getattr(tg.link_task, 'outputs', None):
            for out in tg.link_task.outputs:
                if out.name.endswith('.a'):
                    has_archive = True
                    p = out.abspath().replace('\\', '/')
                    if p not in transitive_archives:
                        transitive_archives.append(p)
        if not has_archive and hasattr(tg, 'compiled_tasks'):
            for t in tg.compiled_tasks:
                src_node = t.inputs[0] if getattr(t, 'inputs', None) else None
                if src_node:
                    src_path = os.path.normcase(src_node.abspath()).replace('\\', '/')
                    if src_path in seen_sources or src_node.name == 'os2helper.c':
                        continue
                    seen_sources.add(src_path)
                for out in t.outputs:
                    if out.name.endswith('.o') or out.name.endswith('.obj'):
                        p = out.abspath().replace('\\', '/')
                        if p not in transitive_objects:
                            transitive_objects.append(p)
            
    self.link_task.os2_transitive_archives = transitive_archives
    self.link_task.os2_transitive_objects = transitive_objects
    self.link_task.os2_rsp_file = self.link_task.outputs[0].abspath() + '.rsp'
    objs = [node for node in self.link_task.inputs if node.name.endswith('.o') or node.name.endswith('.a')]
    if not objs: return
        
    def_node = self.path.find_or_declare(short_name + '.def')
    self.create_task('os2_def', objs, def_node)
    
    if not hasattr(self.link_task, 'dep_nodes'): self.link_task.dep_nodes = []
    self.link_task.dep_nodes.append(def_node)
    self.link_task.env.append_value('LINKFLAGS', def_node.path_from(self.bld.bldnode))
    self.link_task.env.OS2_DEF_FILE = def_node.abspath()
    self.link_task.env.OS2_IMPLIB_FILE = implib_node.abspath()
    
    orig_exec = self.link_task.exec_command
    def os2_exec_command(task, cmd, **kw):
        import os
        rsp_file = task.os2_rsp_file
        args = cmd[1:] if isinstance(cmd, list) else cmd.split()[1:]
        base_cmd = cmd[0] if isinstance(cmd, list) else cmd.split()[0]
        bld_dir = task.generator.bld.bldnode.abspath().replace('\\', '/')
        
        internal_lib_map = {}
        for group in task.generator.bld.groups:
            for tgen in group:
                if hasattr(tgen, 'link_task') and getattr(tgen.link_task, 'outputs', None):
                    for out in tgen.link_task.outputs:
                        if out.name.startswith('lib') and out.name.endswith('.a'):
                            internal_lib_map['-l' + out.name[3:-2]] = out.abspath().replace('\\', '/')
                            
        def get_canon(p):
            if not os.path.isabs(p):
                p = os.path.join(bld_dir, p)
            return os.path.normcase(os.path.normpath(p)).replace('\\', '/')
        seen_written = set()
        rsp_objects = []
        def add_object_file(p):
            if not p: return
            canon = get_canon(p)
            if canon not in seen_written:
                if not os.path.isabs(p):
                    full_path = os.path.normpath(os.path.join(bld_dir, p))
                else:
                    full_path = os.path.normpath(p)
                if os.path.exists(full_path):
                    seen_written.add(canon)
                    rsp_objects.append(full_path.replace('\\', '/'))
        cmd_flags = []
        skip_next = False
        extra_archives = []
        
        for arg in args:
            if skip_next:
                cmd_flags.append(arg)
                skip_next = False
                continue
            if arg == '-o':
                cmd_flags.append(arg)
                skip_next = True
                continue
                
            arg_norm = arg.replace('\\', '/')
            
            if arg_norm.endswith('.o') or arg_norm.endswith('.obj'):
                add_object_file(arg_norm)
            elif arg_norm.endswith('.a') or arg_norm.endswith('.lib'):
                if arg_norm in internal_lib_map:
                    arch_path = internal_lib_map[arg_norm]
                    if arch_path not in extra_archives:
                        extra_archives.append(arch_path)
                else:
                    add_object_file(arg_norm)
            elif arg_norm.startswith('-L') and ('bin/default' in arg_norm or bld_dir in arg_norm):
                continue
            elif arg in internal_lib_map:
                arch_path = internal_lib_map[arg]
                if arch_path not in extra_archives:
                    extra_archives.append(arch_path)
            elif arg not in ['-lcx', '-lat-funcs', '-lattr', '-lpthread']:
                cmd_flags.append(arg)
        if hasattr(task, 'os2_transitive_objects'):
            for obj in task.os2_transitive_objects:
                add_object_file(obj)
        seen_archives = set()
        unique_archives = []
        for arch in list(getattr(task, 'os2_transitive_archives', [])) + extra_archives:
            if not os.path.exists(arch): continue
            canon = os.path.normcase(os.path.abspath(arch)).replace('\\', '/')
            if canon not in seen_archives:
                seen_archives.add(canon)
                unique_archives.append(arch.replace('\\', '/'))
        with open(rsp_file, 'w') as f:
            for obj in rsp_objects:
                f.write('"%s"\n' % obj if ' ' in obj else '%s\n' % obj)
                
            for i in range(2):
                for arch in unique_archives:
                    f.write('"%s"\n' % arch if ' ' in arch else '%s\n' % arch)
        # 1. Static libarchive dependencies
        if any(a == '-larchive' or a.endswith('archive.lib') or a.endswith('libarchive.a') for a in cmd_flags):
            archive_deps = ['-lxml2_dll', '-llzma_dll', '-lbz2_dll', '-lcrypto', '-lz', '-liconv']
            for dep in archive_deps:
                if dep in cmd_flags:
                    cmd_flags.remove(dep)
                cmd_flags.append(dep)
        # 2. Static ncurses dependencies (tinfo/termcap)
        if any(a in ['-lncurses', '-lncursesw'] or a.endswith('ncurses.lib') or a.endswith('libncurses.a') for a in cmd_flags):
            search_dirs = ['/usr/local/lib', '/usr/lib']
            for a in args:
                if a.startswith('-L'):
                    search_dirs.append(a[2:])
            unixroot = os.environ.get('UNIXROOT', 'C:')
            search_dirs.extend([unixroot + '/usr/local/lib', unixroot + '/usr/lib', 'C:/usr/local/lib', 'C:/usr/lib'])
            tinfo_lib = None
            for candidate in ['tinfo', 'tinfow', 'termcap']:
                found = False
                for d in search_dirs:
                    for ext in ['.lib', '.a', '.dll.a']:
                        for prefix in ['', 'lib']:
                            if os.path.exists(os.path.join(d, prefix + candidate + ext)):
                                tinfo_lib = candidate
                                found = True
                                break
                        if found: break
                    if found: break
                if found: break
            if not tinfo_lib:
                tinfo_lib = 'tinfo'
            dep = '-l' + tinfo_lib
            if dep in cmd_flags:
                cmd_flags.remove(dep)
            cmd_flags.append(dep)
            
        # 3. Static gettext/intl dependencies (-lintl before -liconv)
        for dep in ['-lintl', '-liconv']:
            if dep in cmd_flags:
                cmd_flags.remove(dep)
            cmd_flags.append(dep)

        final_cmd = [base_cmd, '@' + rsp_file] + cmd_flags + ['-lattr', '-lcx', '-lat-funcs', '-lpthread']
        if not isinstance(cmd, list): final_cmd = " ".join(final_cmd)
            
        ret = orig_exec(final_cmd, **kw)
        if ret != 0: return ret
        emximp_cmd = 'emximp.exe -o "%s" "%s"' % (task.env.OS2_IMPLIB_FILE, task.env.OS2_DEF_FILE)
        return orig_exec(emximp_cmd, **kw)
        
    self.link_task.exec_command = types.MethodType(os2_exec_command, self.link_task)

@TaskGen.feature('cprogram')
@TaskGen.after_method('apply_link')
def os2_cprogram_hook(self):
    if not sys.platform.startswith('os2'):
        return
    self.link_task.env.LINKFLAGS = [
        f for f in self.link_task.env.LINKFLAGS 
        if f not in ['-lcx', '-lat-funcs', '-lattr', '-lpthread']
    ]
    if '-Zexe' not in self.link_task.env.LINKFLAGS: self.link_task.env.append_value('LINKFLAGS', '-Zexe')
    if '-Zomf' not in self.link_task.env.LINKFLAGS: self.link_task.env.append_value('LINKFLAGS', '-Zomf')
    def resolve_tgen(name):
        if not name: return None
        candidates = [
            name,
            name + '.objlist',
            name[:-8] if name.endswith('.objlist') else name,
            name[:-7] if name.endswith('-samba4') else name + '-samba4',
            (name[:-7] + '.objlist') if name.endswith('-samba4') else (name + '-samba4.objlist'),
            name.replace('-', '_'),
            name.replace('_', '-'),
            name.replace('-', '_') + '.objlist',
            name.replace('_', '-') + '.objlist',
        ]
        for c in candidates:
            try: return self.bld.get_tgen_by_name(c)
            except Exception: pass
        clean_name = name.lower().replace('_', '-').replace('.objlist', '')
        for group in self.bld.groups:
            for tg in group:
                tg_clean = getattr(tg, 'name', '').lower().replace('_', '-').replace('.objlist', '')
                if tg_clean == clean_name:
                    return tg
        return None
    def get_all_deps(tg_name, seen=None, ordered=None, visiting=None):
        if seen is None: seen = set()
        if ordered is None: ordered = []
        if visiting is None: visiting = set()
        if tg_name in seen or tg_name in visiting: return ordered
        visiting.add(tg_name)
        tg = resolve_tgen(tg_name)
        if tg:
            direct = []
            for attr in ['use', 'samba_deps', 'deps', 'public_deps', 'samba_modules', 'samba_subsystem']:
                if hasattr(tg, attr):
                    v = getattr(tg, attr)
                    if isinstance(v, str): direct.extend(v.split())
                    elif isinstance(v, list): direct.extend(v)
            for d in direct: get_all_deps(d, seen, ordered, visiting)
        visiting.remove(tg_name)
        seen.add(tg_name)
        ordered.append(tg_name)
        return ordered
    seen_set, ordered_list = set(), []
    get_all_deps(self.name, seen_set, ordered_list, set())
    all_transitive = ordered_list[::-1]
    
    hidden_libs = [
        'samba-sockets', 'tsocket', 'tevent-util', 'wbclient', 
        'samba-credentials', 'samba-security', 'gensec', 'authkrb5', 
        'cli-auth', 'smb-crypto',
        'RPC_NDR_NETLOGON', 'RPC_NDR_SPOOLSS', 'RPC_NDR_SAMR', 
        'RPC_NDR_SRVSVC', 'RPC_NDR_EPMAPPER', 'RPC_NDR_WKSSVC',
        'RPC_NDR_INITSHUTDOWN', 'RPC_NDR_WINREG', 'RPC_NDR_LSA',
        'RPC_NDR_DFS', 'XATTR_TDB',
        'ndr-wkssvc', 'dcerpc-wkssvc', 'ndr-initshutdown', 'dcerpc-initshutdown',
        'dcerpc-samba4', 'dcerpc-binding', 'dcerpc'
    ]
    hidden_seen, hidden_ordered = set(), []
    for hidden in hidden_libs: get_all_deps(hidden, hidden_seen, hidden_ordered, set())
        
    for dep in hidden_ordered[::-1]:
        if dep not in all_transitive: all_transitive.append(dep)
    if self.name in all_transitive: all_transitive.remove(self.name)
    seen_sources = set()
    for t in getattr(self, 'compiled_tasks', []):
        if getattr(t, 'inputs', None):
            seen_sources.add(os.path.normcase(t.inputs[0].abspath()).replace('\\', '/'))
    transitive_archives = []
    transitive_objects = []
    for dep_name in all_transitive:
        tg = resolve_tgen(dep_name)
        if not tg: continue
        has_archive = False
        if hasattr(tg, 'link_task') and getattr(tg.link_task, 'outputs', None):
            for out in tg.link_task.outputs:
                if out.name.endswith('.a'):
                    has_archive = True
                    p = out.abspath().replace('\\', '/')
                    if p not in transitive_archives:
                        transitive_archives.append(p)
        if not has_archive and hasattr(tg, 'compiled_tasks'):
            for t in tg.compiled_tasks:
                src_node = t.inputs[0] if getattr(t, 'inputs', None) else None
                if src_node:
                    src_path = os.path.normcase(src_node.abspath()).replace('\\', '/')
                    if src_path in seen_sources or src_node.name == 'os2helper.c':
                        continue
                    seen_sources.add(src_path)
                for out in t.outputs:
                    if out.name.endswith('.o') or out.name.endswith('.obj'):
                        p = out.abspath().replace('\\', '/')
                        if p not in transitive_objects:
                            transitive_objects.append(p)
            
    self.link_task.os2_transitive_archives = transitive_archives
    self.link_task.os2_transitive_objects = transitive_objects
    self.link_task.os2_rsp_file = self.link_task.outputs[0].abspath() + '.rsp'
    
    orig_exec = self.link_task.exec_command
    def os2_exec_command(task, cmd, **kw):
        import os
        rsp_file = task.os2_rsp_file
        args = cmd[1:] if isinstance(cmd, list) else cmd.split()[1:]
        base_cmd = cmd[0] if isinstance(cmd, list) else cmd.split()[0]
        bld_dir = task.generator.bld.bldnode.abspath().replace('\\', '/')
        
        internal_lib_map = {}
        for group in task.generator.bld.groups:
            for tgen in group:
                if hasattr(tgen, 'link_task') and getattr(tgen.link_task, 'outputs', None):
                    for out in tgen.link_task.outputs:
                        if out.name.startswith('lib') and out.name.endswith('.a'):
                            internal_lib_map['-l' + out.name[3:-2]] = out.abspath().replace('\\', '/')
                            
        def get_canon(p):
            if not os.path.isabs(p):
                p = os.path.join(bld_dir, p)
            return os.path.normcase(os.path.normpath(p)).replace('\\', '/')
        seen_written = set()
        rsp_objects = []
        def add_object_file(p):
            if not p: return
            canon = get_canon(p)
            if canon not in seen_written:
                if not os.path.isabs(p):
                    full_path = os.path.normpath(os.path.join(bld_dir, p))
                else:
                    full_path = os.path.normpath(p)
                if os.path.exists(full_path):
                    seen_written.add(canon)
                    rsp_objects.append(full_path.replace('\\', '/'))
        cmd_flags = []
        skip_next = False
        extra_archives = []
        
        for arg in args:
            if skip_next:
                cmd_flags.append(arg)
                skip_next = False
                continue
            if arg == '-o':
                cmd_flags.append(arg)
                skip_next = True
                continue
                
            arg_norm = arg.replace('\\', '/')
            
            if arg_norm.endswith('.o') or arg_norm.endswith('.obj'):
                add_object_file(arg_norm)
            elif arg_norm.endswith('.a') or arg_norm.endswith('.lib'):
                if arg_norm in internal_lib_map:
                    arch_path = internal_lib_map[arg_norm]
                    if arch_path not in extra_archives:
                        extra_archives.append(arch_path)
                else:
                    add_object_file(arg_norm)
            elif arg_norm.startswith('-L') and ('bin/default' in arg_norm or bld_dir in arg_norm):
                continue
            elif arg in internal_lib_map:
                arch_path = internal_lib_map[arg]
                if arch_path not in extra_archives:
                    extra_archives.append(arch_path)
            elif arg not in ['-lcx', '-lat-funcs', '-lattr', '-lpthread']:
                cmd_flags.append(arg)
        if hasattr(task, 'os2_transitive_objects'):
            for obj in task.os2_transitive_objects:
                add_object_file(obj)
        seen_archives = set()
        unique_archives = []
        for arch in list(getattr(task, 'os2_transitive_archives', [])) + extra_archives:
            if not os.path.exists(arch): continue
            canon = os.path.normcase(os.path.abspath(arch)).replace('\\', '/')
            if canon not in seen_archives:
                seen_archives.add(canon)
                unique_archives.append(arch.replace('\\', '/'))
        with open(rsp_file, 'w') as f:
            for obj in rsp_objects:
                f.write('"%s"\n' % obj if ' ' in obj else '%s\n' % obj)
                
            for i in range(2):
                for arch in unique_archives:
                    f.write('"%s"\n' % arch if ' ' in arch else '%s\n' % arch)
        # 1. Static libarchive dependencies
        if any(a == '-larchive' or a.endswith('archive.lib') or a.endswith('libarchive.a') for a in cmd_flags):
            archive_deps = ['-lxml2_dll', '-llzma_dll', '-lbz2_dll', '-lcrypto', '-lz', '-liconv']
            for dep in archive_deps:
                if dep in cmd_flags:
                    cmd_flags.remove(dep)
                cmd_flags.append(dep)
        # 2. Static ncurses dependencies (tinfo/termcap)
        if any(a in ['-lncurses', '-lncursesw'] or a.endswith('ncurses.lib') or a.endswith('libncurses.a') for a in cmd_flags):
            search_dirs = ['/usr/local/lib', '/usr/lib']
            for a in args:
                if a.startswith('-L'):
                    search_dirs.append(a[2:])
            unixroot = os.environ.get('UNIXROOT', 'C:')
            search_dirs.extend([unixroot + '/usr/local/lib', unixroot + '/usr/lib', 'C:/usr/local/lib', 'C:/usr/lib'])
            tinfo_lib = None
            for candidate in ['tinfo', 'tinfow', 'termcap']:
                found = False
                for d in search_dirs:
                    for ext in ['.lib', '.a', '.dll.a']:
                        for prefix in ['', 'lib']:
                            if os.path.exists(os.path.join(d, prefix + candidate + ext)):
                                tinfo_lib = candidate
                                found = True
                                break
                        if found: break
                    if found: break
                if found: break
            if not tinfo_lib:
                tinfo_lib = 'tinfo'
            dep = '-l' + tinfo_lib
            if dep in cmd_flags:
                cmd_flags.remove(dep)
            cmd_flags.append(dep)
            
        # 3. Static gettext/intl dependencies (-lintl before -liconv)
        for dep in ['-lintl', '-liconv']:
            if dep in cmd_flags:
                cmd_flags.remove(dep)
            cmd_flags.append(dep)

        final_cmd = [base_cmd, '@' + rsp_file] + cmd_flags + ['-lattr', '-lcx', '-lat-funcs', '-lpthread']
        if not isinstance(cmd, list): final_cmd = " ".join(final_cmd)
            
        return orig_exec(final_cmd, **kw)
        
    self.link_task.exec_command = types.MethodType(os2_exec_command, self.link_task)

def ADD_INIT_FUNCTION(bld, subsystem, target, init_function):
    '''add an init_function to the list for a subsystem'''
    if init_function is None:
        return
    bld.ASSERT(subsystem is not None, "You must specify a subsystem for init_function '%s'" % init_function)
    cache = LOCAL_CACHE(bld, 'INIT_FUNCTIONS')
    if not subsystem in cache:
        cache[subsystem] = []
    cache[subsystem].append( { 'TARGET':target, 'INIT_FUNCTION':init_function } )
Build.BuildContext.ADD_INIT_FUNCTION = ADD_INIT_FUNCTION


def generate_empty_file(task):
    task.outputs[0].write('')
    return 0

#################################################################
def SAMBA_LIBRARY(bld, libname, source,
                  deps='',
                  public_deps='',
                  includes='',
                  public_headers=None,
                  public_headers_install=True,
                  private_headers=None,
                  header_path=None,
                  pc_files=None,
                  vnum=None,
                  soname=None,
                  cflags='',
                  cflags_end=None,
                  ldflags='',
                  external_library=False,
                  realname=None,
                  keep_underscore=False,
                  autoproto=None,
                  autoproto_extra_source='',
                  group='main',
                  depends_on='',
                  local_include=True,
                  global_include=True,
                  vars=None,
                  subdir=None,
                  install_path=None,
                  install=True,
                  pyembed=False,
                  pyext=False,
                  target_type='LIBRARY',
                  bundled_name=None,
                  link_name=None,
                  abi_directory=None,
                  abi_match=None,
                  orig_vscript_map=None,
                  hide_symbols=False,
                  manpages=None,
                  private_library=False,
                  grouping_library=False,
                  require_builtin_deps=False,
                  provide_builtin_linking=False,
                  builtin_cflags='',
                  allow_undefined_symbols=False,
                  allow_warnings=False,
                  enabled=True):
    '''define a Samba library'''

    # We support:
    # - LIBRARY: this can be use to link via -llibname
    # - MODULE:  this is module from SAMBA_MODULE()
    # - PLUGIN:  this is plugin for external consumers to be
    #            loaded via dlopen()
    # - PYTHON:  a python C binding library
    #

    if target_type not in ['LIBRARY', 'MODULE', 'PLUGIN', 'PYTHON']:
        raise Errors.WafError("target_type[%s] not supported in SAMBA_LIBRARY('%s')" %
                              (target_type, libname))

    if require_builtin_deps:
        # For now we only support require_builtin_deps only for libraries, plugins
        if target_type not in ['LIBRARY', 'PLUGIN']:
            raise Errors.WafError("target_type[%s] not supported SAMBA_LIBRARY('%s', require_builtin_deps=True)" %
                                  (target_type, libname))

    if private_library and public_headers:
        raise Errors.WafError("private library '%s' must not have public header files" %
                             libname)

    if orig_vscript_map and not private_library:
        raise Errors.WafError("public library '%s' must not have orig_vscript_map" %
                             libname)

    if orig_vscript_map and abi_directory:
        raise Errors.WafError("private library '%s' with orig_vscript_map must not have abi_directory" %
                             libname)
    if orig_vscript_map and abi_match:
        raise Errors.WafError("private library '%s' with orig_vscript_map must not have abi_match" %
                             libname)

    if LIB_MUST_BE_PRIVATE(bld, libname) and target_type not in ['PLUGIN']:
        private_library = True

    if not enabled:
        SET_TARGET_TYPE(bld, libname, 'DISABLED')
        return

    source = bld.EXPAND_VARIABLES(source, vars=vars)
    if subdir:
        source = bld.SUBDIR(subdir, source)

    # remember empty libraries, so we can strip the dependencies
    if ((source == '') or (source == [])):
        if deps == '' and public_deps == '':
            SET_TARGET_TYPE(bld, libname, 'EMPTY')
            return
        empty_c = libname + '.empty.c'
        bld.SAMBA_GENERATOR('%s_empty_c' % libname,
                            rule=generate_empty_file,
                            target=empty_c)
        source=empty_c

    samba_deps = deps + ' ' + public_deps
    samba_deps = TO_LIST(samba_deps)

    if BUILTIN_LIBRARY(bld, libname):
        builtin_target = libname + '.builtin.objlist'
        builtin_cflags_end = '-D_PUBLIC_=_PRIVATE_'
        empty_target = libname
        obj_target = None
    else:
        if provide_builtin_linking:
            builtin_target = libname + '.builtin.objlist'
            builtin_cflags_end = '-D_PUBLIC_=_PRIVATE_'
        else:
            builtin_target = None
        empty_target = None
        obj_target = libname + '.objlist'
        if require_builtin_deps:
            # hide the builtin deps from the callers
            samba_deps = TO_LIST('')
        dep_target = obj_target

    if group == 'libraries':
        subsystem_group = 'main'
    else:
        subsystem_group = group

    # first create a target for building the object files for this library
    # by separating in this way, we avoid recompiling the C files
    # separately for the install library and the build library
    if builtin_target:
        __t = __SAMBA_SUBSYSTEM_BUILTIN(bld, builtin_target, source,
                                        deps=deps,
                                        public_deps=public_deps,
                                        includes=includes,
                                        header_path=header_path,
                                        builtin_cflags=builtin_cflags,
                                        builtin_cflags_end=builtin_cflags_end,
                                        group=group,
                                        depends_on=depends_on,
                                        local_include=local_include,
                                        global_include=global_include,
                                        allow_warnings=allow_warnings)
        builtin_subsystem = __t
    else:
        builtin_subsystem = None
    if obj_target:
        bld.SAMBA_SUBSYSTEM(obj_target,
                            source         = source,
                            deps           = deps,
                            public_deps    = public_deps,
                            includes       = includes,
                            public_headers = public_headers,
                            public_headers_install = public_headers_install,
                            private_headers= private_headers,
                            header_path    = header_path,
                            cflags         = cflags,
                            cflags_end     = cflags_end,
                            group          = subsystem_group,
                            autoproto      = autoproto,
                            autoproto_extra_source=autoproto_extra_source,
                            depends_on     = depends_on,
                            hide_symbols   = hide_symbols,
                            allow_warnings = allow_warnings,
                            pyembed        = pyembed,
                            pyext          = pyext,
                            local_include  = local_include,
                            __require_builtin_deps=require_builtin_deps,
                            global_include = global_include)
    else:
        et = bld.SAMBA_SUBSYSTEM(empty_target,
                                 source=[],
                                 __force_empty=True,
                                 __require_builtin_deps=True)
        et.samba_builtin_subsystem = builtin_subsystem

    if BUILTIN_LIBRARY(bld, libname):
        return

    if not SET_TARGET_TYPE(bld, libname, target_type):
        return

    # the library itself will depend on that object target
    samba_deps.append(dep_target)

    realname = bld.map_shlib_extension(realname, python=(target_type=='PYTHON'))
    link_name = bld.map_shlib_extension(link_name, python=(target_type=='PYTHON'))

    # we don't want any public libraries without version numbers
    if (not private_library and target_type != 'PYTHON' and not realname):
        if vnum is None and soname is None:
            raise Errors.WafError("public library '%s' must have a vnum" %
                    libname)
        if pc_files is None:
            raise Errors.WafError("public library '%s' must have pkg-config file" %
                       libname)
        if public_headers is None:
            raise Errors.WafError("public library '%s' must have header files" %
                       libname)

    abi_vnum = vnum

    if bundled_name is not None:
        pass
    elif target_type == 'PYTHON' or realname or not private_library:
        if keep_underscore:
            bundled_name = libname
        else:
            bundled_name = libname.replace('_', '-')
    else:
        assert (private_library == True and realname is None)
        bundled_name = PRIVATE_NAME(bld, libname.replace('_', '-'))
        vnum = None

    ldflags = TO_LIST(ldflags)
    if bld.env['ENABLE_RELRO'] is True:
        ldflags.extend(TO_LIST('-Wl,-z,relro,-z,now'))

    # Force private libs, libs with undefined symbols, and known circular libs to be static
    force_static_os2 = ['samba-util', 'samba-credentials']
    
    if sys.platform.startswith('os2') and (private_library or allow_undefined_symbols or libname in force_static_os2):
        features = 'c cstlib install_lib'
    elif bld.env.ENABLE_SHARED:
        features = 'c cshlib install_lib'
    else:
        features = 'c cstlib install_lib'

    if pyext:
        features += ' pyext'
    if pyembed:
        features += ' pyembed'

    if abi_directory:
        features += ' abi_check'

    if pyembed and bld.env['PYTHON_SO_ABI_FLAG']:
        # For ABI checking, we don't care about the Python version.
        # Remove the Python ABI tag (e.g. ".cpython-35m")
        abi_flag = bld.env['PYTHON_SO_ABI_FLAG']
        replacement = ''
        version_libname = libname.replace(abi_flag, replacement)
    else:
        version_libname = libname

    vscript = None
    if bld.env.HAVE_LD_VERSION_SCRIPT:
        if private_library:
            version = bld.env.PRIVATE_VERSION
        elif vnum:
            version = "%s_%s" % (libname, vnum)
        else:
            version = None
        if version:
            vscript = "%s.vscript" % libname
            if orig_vscript_map:
                bld.VSCRIPT_MAP_PRIVATE(version_libname, orig_vscript_map, version, vscript)
            else:
                bld.ABI_VSCRIPT(version_libname, abi_directory, version, vscript,
                                abi_match, private_library)
            fullname = apply_pattern(bundled_name, bld.env.cshlib_PATTERN)
            fullpath = bld.path.find_or_declare(fullname)
            vscriptpath = bld.path.find_or_declare(vscript)
            if not fullpath:
                raise Errors.WafError("unable to find fullpath for %s" % fullname)
            if not vscriptpath:
                raise Errors.WafError("unable to find vscript path for %s" % vscript)
            bld.add_manual_dependency(fullpath, vscriptpath)
            if bld.is_install:
                # also make the .inst file depend on the vscript
                instname = apply_pattern(bundled_name + '.inst', bld.env.cshlib_PATTERN)
                bld.add_manual_dependency(bld.path.find_or_declare(instname), bld.path.find_or_declare(vscript))
            vscript = os.path.join(bld.path.abspath(bld.env), vscript)

    bld.SET_BUILD_GROUP(group)
    t = bld(
        features        = features,
        source          = [],
        target          = bundled_name,
        depends_on      = depends_on,
        samba_ldflags   = ldflags,
        samba_deps      = samba_deps,
        samba_includes  = includes,
        version_script  = vscript,
        version_libname = version_libname,
        local_include   = local_include,
        global_include  = global_include,
        vnum            = vnum,
        soname          = soname,
        install_path    = None,
        samba_inst_path = install_path,
        name            = libname,
        samba_realname  = realname,
        samba_install   = install,
        abi_directory   = "%s/%s" % (bld.path.abspath(), abi_directory),
        abi_match       = abi_match,
        abi_vnum        = abi_vnum,
        private_library = private_library,
        grouping_library=grouping_library,
        allow_undefined_symbols=allow_undefined_symbols,
        samba_require_builtin_deps=False,
        samba_builtin_subsystem=builtin_subsystem,
        )

    if realname and not link_name:
        link_name = 'shared/%s' % realname

    if link_name:
        if 'waflib.extras.compat15' in sys.modules:
            link_name = 'default/' + link_name
        t.link_name = link_name

    if pc_files is not None and not private_library:
        if pyembed:
            bld.PKG_CONFIG_FILES(pc_files, vnum=vnum, extra_name=bld.env['PYTHON_SO_ABI_FLAG'])
        else:
            bld.PKG_CONFIG_FILES(pc_files, vnum=vnum)

    if (manpages is not None and 'XSLTPROC_MANPAGES' in bld.env and
        bld.env['XSLTPROC_MANPAGES']):
        bld.MANPAGES(manpages, install)


Build.BuildContext.SAMBA_LIBRARY = SAMBA_LIBRARY


#################################################################
def SAMBA_BINARY(bld, binname, source,
                 deps='',
                 includes='',
                 public_headers=None,
                 private_headers=None,
                 header_path=None,
                 modules=None,
                 ldflags=None,
                 cflags='',
                 cflags_end=None,
                 autoproto=None,
                 use_hostcc=False,
                 use_global_deps=True,
                 compiler=None,
                 group='main',
                 manpages=None,
                 local_include=True,
                 global_include=True,
                 subsystem_name=None,
                 allow_warnings=False,
                 pyembed=False,
                 vars=None,
                 subdir=None,
                 install=True,
                 install_path=None,
                 enabled=True,
                 fuzzer=False,
                 for_selftest=False):
    '''define a Samba binary'''

    if for_selftest:
        install=False
        if not bld.CONFIG_GET('ENABLE_SELFTEST'):
            enabled=False

    if not enabled:
        SET_TARGET_TYPE(bld, binname, 'DISABLED')
        return

    # Fuzzing builds do not build normal binaries
    # however we must build asn1compile etc

    if not use_hostcc and bld.env.enable_fuzzing != fuzzer:
        SET_TARGET_TYPE(bld, binname, 'DISABLED')
        return

    if fuzzer:
        install = False
        if ldflags is None:
            ldflags = bld.env['FUZZ_TARGET_LDFLAGS']

    if not SET_TARGET_TYPE(bld, binname, 'BINARY'):
        return

    features = 'c cprogram symlink_bin install_bin'
    if pyembed:
        features += ' pyembed'

    obj_target = binname + '.objlist'

    source = bld.EXPAND_VARIABLES(source, vars=vars)
    if subdir:
        source = bld.SUBDIR(subdir, source)
    source = unique_list(TO_LIST(source))

    if group == 'binaries':
        subsystem_group = 'main'
    elif group == 'build_compilers':
        subsystem_group = 'compiler_libraries'
    else:
        subsystem_group = group

    # only specify PIE flags for binaries
    pie_cflags = TO_LIST(cflags)
    pie_ldflags = TO_LIST(ldflags)
    if bld.env['ENABLE_PIE'] is True:
        pie_cflags.extend(TO_LIST('-fPIE'))
        pie_ldflags.extend(TO_LIST('-pie'))
    if bld.env['ENABLE_RELRO'] is True:
        pie_ldflags.extend(TO_LIST('-Wl,-z,relro,-z,now'))

    # first create a target for building the object files for this binary
    # by separating in this way, we avoid recompiling the C files
    # separately for the install binary and the build binary
    bld.SAMBA_SUBSYSTEM(obj_target,
                        source         = source,
                        deps           = deps,
                        includes       = includes,
                        cflags         = pie_cflags,
                        cflags_end     = cflags_end,
                        group          = subsystem_group,
                        autoproto      = autoproto,
                        subsystem_name = subsystem_name,
                        local_include  = local_include,
                        global_include = global_include,
                        use_hostcc     = use_hostcc,
                        pyext          = pyembed,
                        allow_warnings = allow_warnings,
                        use_global_deps= use_global_deps)

    bld.SET_BUILD_GROUP(group)

    # the binary itself will depend on that object target
    deps = TO_LIST(deps)
    deps.append(obj_target)

    t = bld(
        features       = features,
        source         = [],
        target         = binname,
        samba_deps     = deps,
        samba_includes = includes,
        local_include  = local_include,
        global_include = global_include,
        samba_modules  = modules,
        top            = True,
        samba_subsystem= subsystem_name,
        install_path   = None,
        samba_inst_path= install_path,
        samba_install  = install,
        samba_ldflags  = pie_ldflags
        )

    if manpages is not None and 'XSLTPROC_MANPAGES' in bld.env and bld.env['XSLTPROC_MANPAGES']:
        bld.MANPAGES(manpages, install)

Build.BuildContext.SAMBA_BINARY = SAMBA_BINARY


#################################################################
def SAMBA_MODULE(bld, modname, source,
                 deps='',
                 includes='',
                 subsystem=None,
                 init_function=None,
                 module_init_name='samba_init_module',
                 autoproto=None,
                 autoproto_extra_source='',
                 cflags='',
                 cflags_end=None,
                 internal_module=True,
                 local_include=True,
                 global_include=True,
                 vars=None,
                 subdir=None,
                 enabled=True,
                 pyembed=False,
                 manpages=None,
                 allow_undefined_symbols=False,
                 allow_warnings=False,
                 install=True
                 ):
    '''define a Samba module.'''

    bld.ASSERT(subsystem, "You must specify a subsystem for SAMBA_MODULE(%s)" % modname)

    source = bld.EXPAND_VARIABLES(source, vars=vars)
    if subdir:
        source = bld.SUBDIR(subdir, source)

    if internal_module or BUILTIN_LIBRARY(bld, modname):
        # Do not create modules for disabled subsystems
        if GET_TARGET_TYPE(bld, subsystem) == 'DISABLED':
            return
        bld.SAMBA_SUBSYSTEM(modname, source,
                    deps=deps,
                    includes=includes,
                    autoproto=autoproto,
                    autoproto_extra_source=autoproto_extra_source,
                    cflags=cflags,
                    cflags_end=cflags_end,
                    local_include=local_include,
                    global_include=global_include,
                    allow_warnings=allow_warnings,
                    enabled=enabled)

        bld.ADD_INIT_FUNCTION(subsystem, modname, init_function)
        return

    if not enabled:
        SET_TARGET_TYPE(bld, modname, 'DISABLED')
        return

    # Do not create modules for disabled subsystems
    if GET_TARGET_TYPE(bld, subsystem) == 'DISABLED':
        return

    realname = modname
    deps += ' ' + subsystem
    while realname.startswith("lib"+subsystem+"_"):
        realname = realname[len("lib"+subsystem+"_"):]
    while realname.startswith(subsystem+"_"):
        realname = realname[len(subsystem+"_"):]

    build_name = "%s_module_%s" % (subsystem, realname)

    realname = bld.make_libname(realname)
    while realname.startswith("lib"):
        realname = realname[len("lib"):]

    build_link_name = "modules/%s/%s" % (subsystem, realname)

    if init_function:
        cflags += " -D%s=%s" % (init_function, module_init_name)

    bld.SAMBA_LIBRARY(modname,
                      source,
                      deps=deps,
                      includes=includes,
                      cflags=cflags,
                      cflags_end=cflags_end,
                      realname = realname,
                      autoproto = autoproto,
                      local_include=local_include,
                      global_include=global_include,
                      vars=vars,
                      bundled_name=build_name,
                      link_name=build_link_name,
                      install_path="${MODULESDIR}/%s" % subsystem,
                      pyembed=pyembed,
                      manpages=manpages,
                      allow_undefined_symbols=allow_undefined_symbols,
                      allow_warnings=allow_warnings,
                      private_library=True,
                      install=install
                      )


Build.BuildContext.SAMBA_MODULE = SAMBA_MODULE

#################################################################
def SAMBA_PLUGIN(bld, pluginname, source,
                 deps='',
                 includes='',
                 vnum=None,
                 soname=None,
                 cflags='',
                 ldflags='',
                 local_include=True,
                 global_include=True,
                 vars=None,
                 subdir=None,
                 realname=None,
                 keep_underscore=False,
                 autoproto=None,
                 autoproto_extra_source='',
                 install_path=None,
                 install=True,
                 manpages=None,
                 require_builtin_deps=True,
                 allow_undefined_symbols=False,
                 enabled=True):
    '''define an external plugin.'''

    bld.ASSERT(realname, "You must specify a realname for SAMBA_PLUGIN(%s)" % pluginname)

    source = bld.EXPAND_VARIABLES(source, vars=vars)
    if subdir:
        source = bld.SUBDIR(subdir, source)

    build_name = "_plugin_%s" % (pluginname)
    build_link_name = "plugins/%s" % (realname)

    bld.SAMBA_LIBRARY(pluginname,
                      source,
                      bundled_name=build_name,
                      link_name=build_link_name,
                      target_type='PLUGIN',
                      deps=deps,
                      includes=includes,
                      vnum=vnum,
                      soname=soname,
                      cflags=cflags,
                      ldflags=ldflags,
                      realname=realname,
                      autoproto=autoproto,
                      autoproto_extra_source=autoproto_extra_source,
                      local_include=local_include,
                      global_include=global_include,
                      vars=vars,
                      group='main',
                      install_path=install_path,
                      install=install,
                      manpages=manpages,
                      require_builtin_deps=require_builtin_deps,
                      builtin_cflags=cflags,
                      hide_symbols=True,
                      public_headers=[],
                      public_headers_install=False,
                      pc_files=[],
                      allow_undefined_symbols=allow_undefined_symbols,
                      allow_warnings=False,
                      enabled=enabled)
Build.BuildContext.SAMBA_PLUGIN = SAMBA_PLUGIN

def __SAMBA_SUBSYSTEM_BUILTIN(bld, builtin_target, source,
                              deps='',
                              public_deps='',
                              includes='',
                              public_headers=None,
                              public_headers_install=True,
                              private_headers=None,
                              header_path=None,
                              builtin_cflags='',
                              builtin_cflags_end=None,
                              group='main',
                              autoproto=None,
                              autoproto_extra_source='',
                              depends_on='',
                              local_include=True,
                              global_include=True,
                              allow_warnings=False):

    bld.ASSERT(builtin_target.endswith('.builtin.objlist'),
               "builtin_target[%s] does not end with '.builtin.objlist'" %
               (builtin_target))
    return bld.SAMBA_SUBSYSTEM(builtin_target, source,
                               deps=deps,
                               public_deps=public_deps,
                               includes=includes,
                               public_headers=public_headers,
                               public_headers_install=public_headers_install,
                               private_headers=private_headers,
                               header_path=header_path,
                               cflags=builtin_cflags,
                               cflags_end=builtin_cflags_end,
                               hide_symbols=True,
                               group=group,
                               target_type='BUILTIN',
                               autoproto=autoproto,
                               autoproto_extra_source=autoproto_extra_source,
                               depends_on=depends_on,
                               local_include=local_include,
                               global_include=global_include,
                               allow_warnings=allow_warnings,
                               __require_builtin_deps=True)

#################################################################
def SAMBA_SUBSYSTEM(bld, modname, source,
                    deps='',
                    public_deps='',
                    __force_empty=False,
                    includes='',
                    public_headers=None,
                    public_headers_install=True,
                    private_headers=None,
                    header_path=None,
                    cflags='',
                    cflags_end=None,
                    group='main',
                    target_type='SUBSYSTEM',
                    init_function_sentinel=None,
                    autoproto=None,
                    autoproto_extra_source='',
                    depends_on='',
                    local_include=True,
                    local_include_first=True,
                    global_include=True,
                    subsystem_name=None,
                    enabled=True,
                    use_hostcc=False,
                    use_global_deps=True,
                    vars=None,
                    subdir=None,
                    hide_symbols=False,
                    __require_builtin_deps=False,
                    provide_builtin_linking=False,
                    builtin_cflags='',
                    allow_warnings=False,
                    pyext=False,
                    pyembed=False):
    '''define a Samba subsystem'''

    # We support:
    # - SUBSYSTEM: a normal subsystem from SAMBA_SUBSYSTEM()
    # - BUILTIN:   a hidden subsystem from __SAMBA_SUBSYSTEM_BUILTIN()
    if target_type not in ['SUBSYSTEM', 'BUILTIN']:
        raise Errors.WafError("target_type[%s] not supported in SAMBA_SUBSYSTEM('%s')" %
                              (target_type, modname))

    if not enabled:
        SET_TARGET_TYPE(bld, modname, 'DISABLED')
        return

    # remember empty subsystems, so we can strip the dependencies
    if ((source == '') or (source == [])):
        if not __force_empty and deps == '' and public_deps == '':
            SET_TARGET_TYPE(bld, modname, 'EMPTY')
            return
        empty_c = modname + '.empty.c'
        bld.SAMBA_GENERATOR('%s_empty_c' % modname,
                            rule=generate_empty_file,
                            target=empty_c)
        source=empty_c

    if not SET_TARGET_TYPE(bld, modname, target_type):
        return

    source = bld.EXPAND_VARIABLES(source, vars=vars)
    if subdir:
        source = bld.SUBDIR(subdir, source)
    source = unique_list(TO_LIST(source))

    deps += ' ' + public_deps

    bld.SET_BUILD_GROUP(group)

    features = 'c'
    if pyext:
        features += ' pyext'
    if pyembed:
        features += ' pyembed'

    t = bld(
        features       = features,
        source         = source,
        target         = modname,
        samba_cflags   = CURRENT_CFLAGS(bld, modname, cflags,
                                        allow_warnings=allow_warnings,
                                        use_hostcc=use_hostcc,
                                        hide_symbols=hide_symbols),
        depends_on     = depends_on,
        samba_deps     = TO_LIST(deps),
        samba_includes = includes,
        local_include  = local_include,
        local_include_first  = local_include_first,
        global_include = global_include,
        samba_subsystem= subsystem_name,
        samba_use_hostcc = use_hostcc,
        samba_use_global_deps = use_global_deps,
        samba_require_builtin_deps = __require_builtin_deps,
        samba_builtin_subsystem = None,
        )

    if cflags_end is not None:
        t.samba_cflags.extend(TO_LIST(cflags_end))

    if autoproto is not None:
        bld.SAMBA_AUTOPROTO(autoproto, source + TO_LIST(autoproto_extra_source))
    if public_headers is not None:
        bld.PUBLIC_HEADERS(public_headers, header_path=header_path,
                           public_headers_install=public_headers_install)

    if provide_builtin_linking:

        if use_hostcc:
            raise Errors.WafError("subsystem[%s] provide_builtin_linking=True " +
                                  "not allowed with use_hostcc=True" %
                                  modname)

        if pyext or pyembed:
            raise Errors.WafError("subsystem[%s] provide_builtin_linking=True " +
                                  "not allowed with pyext=True nor pyembed=True" %
                                  modname)

        if __require_builtin_deps:
            raise Errors.WafError("subsystem[%s] provide_builtin_linking=True " +
                                  "not allowed with __require_builtin_deps=True" %
                                  modname)

        builtin_target = modname + '.builtin.objlist'
        tbuiltin = __SAMBA_SUBSYSTEM_BUILTIN(bld, builtin_target, source,
                                             deps=deps,
                                             public_deps=public_deps,
                                             includes=includes,
                                             header_path=header_path,
                                             builtin_cflags=builtin_cflags,
                                             builtin_cflags_end='-D_PUBLIC_=_PRIVATE_',
                                             group=group,
                                             depends_on=depends_on,
                                             local_include=local_include,
                                             global_include=global_include,
                                             allow_warnings=allow_warnings)
        t.samba_builtin_subsystem = tbuiltin

    return t


Build.BuildContext.SAMBA_SUBSYSTEM = SAMBA_SUBSYSTEM


def SAMBA_GENERATOR(bld, name, rule, source='', target='',
                    group='generators', enabled=True,
                    public_headers=None,
                    public_headers_install=True,
                    private_headers=None,
                    header_path=None,
                    vars=None,
                    dep_vars=None,
                    always=False):
    '''A generic source generator target'''

    if dep_vars is None:
        dep_vars = []
    if not SET_TARGET_TYPE(bld, name, 'GENERATOR'):
        return

    if not enabled:
        return

    dep_vars = TO_LIST(dep_vars)
    dep_vars.append('ruledeps')
    dep_vars.append('SAMBA_GENERATOR_VARS')

    shell=isinstance(rule, str)

    # This ensures that if the command (executed in the shell) fails
    # (returns non-zero), the build fails
    if shell:
        rule = "set -e; " + rule

    bld.SET_BUILD_GROUP(group)
    t = bld(
        rule=rule,
        source=bld.EXPAND_VARIABLES(source, vars=vars),
        shell=shell,
        target=target,
        update_outputs=True,
        before='c',
        ext_out='.c',
        samba_type='GENERATOR',
        dep_vars = dep_vars,
        name=name)

    if vars is None:
        vars = {}
    t.env.SAMBA_GENERATOR_VARS = vars

    if always:
        t.always = True

    if public_headers is not None:
        bld.PUBLIC_HEADERS(public_headers, header_path=header_path,
                           public_headers_install=public_headers_install)
    return t
Build.BuildContext.SAMBA_GENERATOR = SAMBA_GENERATOR



@Utils.run_once
def SETUP_BUILD_GROUPS(bld):
    '''setup build groups used to ensure that the different build
    phases happen consecutively'''
    bld.p_ln = bld.srcnode # we do want to see all targets!
    bld.env['USING_BUILD_GROUPS'] = True
    bld.add_group('setup')
    bld.add_group('generators')
    bld.add_group('hostcc_base_build_source')
    bld.add_group('hostcc_base_build_main')
    bld.add_group('hostcc_build_source')
    bld.add_group('hostcc_build_main')
    bld.add_group('vscripts')
    bld.add_group('base_libraries')
    bld.add_group('build_source')
    bld.add_group('prototypes')
    bld.add_group('headers')
    bld.add_group('main')
    bld.add_group('symbolcheck')
    bld.add_group('syslibcheck')
    bld.add_group('final')
Build.BuildContext.SETUP_BUILD_GROUPS = SETUP_BUILD_GROUPS


def SET_BUILD_GROUP(bld, group):
    '''set the current build group'''
    if not 'USING_BUILD_GROUPS' in bld.env:
        return
    bld.set_group(group)
Build.BuildContext.SET_BUILD_GROUP = SET_BUILD_GROUP



def SAMBA_SCRIPT(bld, name, pattern, installdir, installname=None):
    '''used to copy scripts from the source tree into the build directory
       for use by selftest'''

    source = bld.path.ant_glob(pattern, flat=True)

    bld.SET_BUILD_GROUP('build_source')
    for s in TO_LIST(source):
        iname = s
        if installname is not None:
            iname = installname
        target = os.path.join(installdir, iname)
        tgtdir = os.path.dirname(os.path.join(bld.srcnode.abspath(bld.env), '..', target))
        mkdir_p(tgtdir)
        link_src = os.path.normpath(os.path.join(bld.path.abspath(), s))
        link_dst = os.path.join(tgtdir, os.path.basename(iname))
        if os.path.islink(link_dst) and os.readlink(link_dst) == link_src:
            continue
        if os.path.islink(link_dst):
            os.unlink(link_dst)
        Logs.info("symlink: %s -> %s/%s" % (s, installdir, iname))
        symlink(link_src, link_dst)
Build.BuildContext.SAMBA_SCRIPT = SAMBA_SCRIPT


def copy_and_fix_python_path(task):
    pattern='sys.path.insert(0, "bin/python")'
    if task.env["PYTHONARCHDIR"] in sys.path and task.env["PYTHONDIR"] in sys.path:
        replacement = ""
    elif task.env["PYTHONARCHDIR"] == task.env["PYTHONDIR"]:
        replacement="""sys.path.insert(0, "%s")""" % task.env["PYTHONDIR"]
    else:
        replacement="""sys.path.insert(0, "%s")
sys.path.insert(1, "%s")""" % (task.env["PYTHONARCHDIR"], task.env["PYTHONDIR"])

    if task.env["PYTHON"][0].startswith("/"):
        replacement_shebang = "#!%s\n" % task.env["PYTHON"][0]
    else:
        replacement_shebang = "#!/usr/bin/env %s\n" % task.env["PYTHON"][0]

    installed_location=task.outputs[0].bldpath(task.env)
    source_file = open(task.inputs[0].srcpath(task.env))
    installed_file = open(installed_location, 'w')
    lineno = 0
    for line in source_file:
        newline = line
        if (lineno == 0 and
                line[:2] == "#!"):
            newline = replacement_shebang
        elif pattern in line:
            newline = line.replace(pattern, replacement)
        installed_file.write(newline)
        lineno = lineno + 1
    installed_file.close()
    os.chmod(installed_location, 0o755)
    return 0

def copy_and_fix_perl_path(task):
    pattern='use lib "$RealBin/lib";'

    replacement = ""
    if not task.env["PERL_LIB_INSTALL_DIR"] in task.env["PERL_INC"]:
         replacement = 'use lib "%s";' % task.env["PERL_LIB_INSTALL_DIR"]

    if task.env["PERL"][0] == "/":
        replacement_shebang = "#!%s\n" % task.env["PERL"]
    else:
        replacement_shebang = "#!/usr/bin/env %s\n" % task.env["PERL"]

    installed_location=task.outputs[0].bldpath(task.env)
    source_file = open(task.inputs[0].srcpath(task.env))
    installed_file = open(installed_location, 'w')
    lineno = 0
    for line in source_file:
        newline = line
        if lineno == 0 and task.env["PERL_SPECIFIED"] == True and line[:2] == "#!":
            newline = replacement_shebang
        elif pattern in line:
            newline = line.replace(pattern, replacement)
        installed_file.write(newline)
        lineno = lineno + 1
    installed_file.close()
    os.chmod(installed_location, 0o755)
    return 0


def install_file(bld, destdir, file, chmod=MODE_644, flat=False,
                 python_fixup=False, perl_fixup=False,
                 destname=None, base_name=None):
    '''install a file'''
    if not isinstance(file, str):
        file = file.abspath()
    destdir = bld.EXPAND_VARIABLES(destdir)
    if not destname:
        destname = file
        if flat:
            destname = os.path.basename(destname)
    dest = os.path.join(destdir, destname)
    if python_fixup:
        # fix the path python will use to find Samba modules
        inst_file = file + '.inst'
        bld.SAMBA_GENERATOR('python_%s' % destname,
                            rule=copy_and_fix_python_path,
                            dep_vars=["PYTHON","PYTHON_SPECIFIED","PYTHONDIR","PYTHONARCHDIR"],
                            source=file,
                            target=inst_file)
        file = inst_file
    if perl_fixup:
        # fix the path perl will use to find Samba modules
        inst_file = file + '.inst'
        bld.SAMBA_GENERATOR('perl_%s' % destname,
                            rule=copy_and_fix_perl_path,
                            dep_vars=["PERL","PERL_SPECIFIED","PERL_LIB_INSTALL_DIR"],
                            source=file,
                            target=inst_file)
        file = inst_file
    if base_name:
        file = os.path.join(base_name, file)
    bld.install_as(dest, file, chmod=chmod)


def INSTALL_FILES(bld, destdir, files, chmod=MODE_644, flat=False,
                  python_fixup=False, perl_fixup=False,
                  destname=None, base_name=None):
    '''install a set of files'''
    for f in TO_LIST(files):
        install_file(bld, destdir, f, chmod=chmod, flat=flat,
                     python_fixup=python_fixup, perl_fixup=perl_fixup,
                     destname=destname, base_name=base_name)
Build.BuildContext.INSTALL_FILES = INSTALL_FILES


def INSTALL_WILDCARD(bld, destdir, pattern, chmod=MODE_644, flat=False,
                     python_fixup=False, exclude=None, trim_path=None):
    '''install a set of files matching a wildcard pattern'''
    files=TO_LIST(bld.path.ant_glob(pattern, flat=True))
    if trim_path:
        files2 = []
        for f in files:
            files2.append(os.path.relpath(f, trim_path))
        files = files2

    if exclude:
        for f in files[:]:
            if fnmatch.fnmatch(f, exclude):
                files.remove(f)
    INSTALL_FILES(bld, destdir, files, chmod=chmod, flat=flat,
                  python_fixup=python_fixup, base_name=trim_path)
Build.BuildContext.INSTALL_WILDCARD = INSTALL_WILDCARD

def INSTALL_DIR(bld, path, chmod=0o755, env=None):
    """Install a directory if it doesn't exist, always set permissions."""

    if not path:
        return []

    destpath = bld.EXPAND_VARIABLES(path)
    if Options.options.destdir:
        destpath = os.path.join(Options.options.destdir, destpath.lstrip(os.sep))

    if bld.is_install > 0:
        if not os.path.isdir(destpath):
            try:
                Logs.info('* create %s', destpath)
                os.makedirs(destpath)
                os.chmod(destpath, chmod)
            except OSError as e:
                if not os.path.isdir(destpath):
                    raise Errors.WafError("Cannot create the folder '%s' (error: %s)" % (path, e))
Build.BuildContext.INSTALL_DIR = INSTALL_DIR

def INSTALL_DIRS(bld, destdir, dirs, chmod=0o755, env=None):
    '''install a set of directories'''
    destdir = bld.EXPAND_VARIABLES(destdir)
    dirs = bld.EXPAND_VARIABLES(dirs)
    for d in TO_LIST(dirs):
        INSTALL_DIR(bld, os.path.join(destdir, d), chmod, env)
Build.BuildContext.INSTALL_DIRS = INSTALL_DIRS


def MANPAGES(bld, manpages, install):
    '''build and install manual pages'''
    bld.env.MAN_XSL = 'http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl'
    for m in manpages.split():
        source = m + '.xml'
        bld.SAMBA_GENERATOR(m,
                            source=source,
                            target=m,
                            group='final',
                            rule='${XSLTPROC} --xinclude -o ${TGT} --nonet ${MAN_XSL} ${SRC}'
                            )
        if install:
            bld.INSTALL_FILES('${MANDIR}/man%s' % m[-1], m, flat=True)
Build.BuildContext.MANPAGES = MANPAGES

def SAMBAMANPAGES(bld, manpages, extra_source=None):
    '''build and install manual pages'''
    bld.env.SAMBA_EXPAND_XSL = bld.srcnode.abspath() + '/docs-xml/xslt/expand-sambadoc.xsl'
    bld.env.SAMBA_MAN_XSL = bld.srcnode.abspath() + '/docs-xml/xslt/man.xsl'
    bld.env.SAMBA_CATALOG = bld.bldnode.abspath() + '/docs-xml/build/catalog.xml'
    bld.env.SAMBA_CATALOGS = os.getenv('XML_CATALOG_FILES', 'file:///etc/xml/catalog file:///usr/local/share/xml/catalog') + ' file://' + bld.env.SAMBA_CATALOG

    for m in manpages.split():
        source = [m + '.xml']
        if extra_source is not None:
            source = [source, extra_source]
        # ${SRC[1]}, ${SRC[2]} and ${SRC[3]} are not referenced in the
        # SAMBA_GENERATOR but trigger the dependency calculation so
        # ensures that manpages are rebuilt when these change.
        source += ['build/DTD/samba.build.pathconfig', 'build/DTD/samba.entities', 'build/DTD/samba.build.version']
        bld.SAMBA_GENERATOR(m,
                            source=source,
                            target=m,
                            group='final',
                            dep_vars=['SAMBA_MAN_XSL', 'SAMBA_EXPAND_XSL', 'SAMBA_CATALOG'],
                            rule='''XML_CATALOG_FILES="${SAMBA_CATALOGS}"
                                    export XML_CATALOG_FILES
                                    ${XSLTPROC} --xinclude --stringparam noreference 0 -o ${TGT}.xml --nonet ${SAMBA_EXPAND_XSL} ${SRC[0].abspath(env)}
                                    ${XSLTPROC} --nonet -o ${TGT} ${SAMBA_MAN_XSL} ${TGT}.xml'''
                            )
        bld.INSTALL_FILES('${MANDIR}/man%s' % m[-1], m, flat=True)
Build.BuildContext.SAMBAMANPAGES = SAMBAMANPAGES

@after('apply_link')
@feature('cshlib')
def apply_bundle_remove_dynamiclib_patch(self):
    if self.env['MACBUNDLE'] or getattr(self,'mac_bundle',False):
        if not getattr(self,'vnum',None):
            try:
                self.env['LINKFLAGS'].remove('-dynamiclib')
                self.env['LINKFLAGS'].remove('-single_module')
            except ValueError:
                pass
