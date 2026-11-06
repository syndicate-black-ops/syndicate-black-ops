"""Compiling a suite's map and running it on a headless stock HLDS."""

import hashlib
import os
import shutil
import subprocess
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ENTITY = os.path.dirname(HERE)
ROOT = os.path.dirname(os.path.dirname(ENTITY))
SNIPER_IMAGE = os.environ.get('SNIPER_IMAGE',
                              'registry.gitlab.steamos.cloud/steamrt/sniper/sdk:3.0.20260805.254768@sha256:1c33c507bc75d012e77df5727f93b0d5b8c3f7c8d4142ba5f7a16882cc92e014')


def log(msg):
    print(msg, flush=True)


def compile_map(text, mapname, out_dir, sdhlt_dir, threads=2):
    """CSG, BSP, VIS -fast (no RAD: the tests need no light). Cached by the .map's hash."""
    os.makedirs(out_dir, exist_ok=True)
    h = hashlib.sha256(text.encode()).hexdigest()[:16]
    bsp = os.path.join(out_dir, mapname + '.bsp')
    stamp = os.path.join(out_dir, mapname + '.hash')
    if os.path.exists(bsp) and os.path.exists(stamp) and open(stamp).read() == h:
        return bsp, False
    with open(os.path.join(out_dir, mapname + '.map'), 'w') as f:
        f.write(text)
    wad = os.path.join(out_dir, 'anzymut.wad')
    if not os.path.exists(wad):
        os.symlink(os.path.join(ROOT, 'gamedir', 'syndicate', 'anzymut.wad'), wad)
    for leftover in ('.err', '.pts', '.lin', '.bsp'):
        p = os.path.join(out_dir, mapname + leftover)
        if os.path.exists(p):
            os.remove(p)
    for tool, args in (('sdHLCSG', []), ('sdHLBSP', []), ('sdHLVIS', ['-fast'])):
        cmd = ['nice', '-n', '10', 'timeout', '300', os.path.join(sdhlt_dir, tool)] + args + \
              ['-threads', str(threads), '-chart', mapname]
        with open(os.path.join(out_dir, tool + '.out'), 'w') as f:
            res = subprocess.run(cmd, cwd=out_dir, stdout=f, stderr=subprocess.STDOUT,
                                 preexec_fn=_limit_memory)
        if res.returncode != 0 or os.path.exists(os.path.join(out_dir, mapname + '.err')):
            raise RuntimeError('%s failed (see %s)' % (tool, os.path.join(out_dir, tool + '.out')))
        if tool == 'sdHLBSP' and os.path.exists(os.path.join(out_dir, mapname + '.pts')):
            raise RuntimeError('map leak (%s.pts)' % os.path.join(out_dir, mapname))
    with open(stamp, 'w') as f:
        f.write(h)
    return bsp, True


def _limit_memory():
    import resource
    resource.setrlimit(resource.RLIMIT_AS, (4 << 30, 4 << 30))


def timescale_lib(build_dir):
    """tests/entity/timescale/timescale.c, built in the sniper image (its glibc 2.31 runs on every host)."""
    out = os.path.join(build_dir, 'timescale.so')
    src = os.path.join(ENTITY, 'timescale', 'timescale.c')
    if os.path.exists(out) and os.path.getmtime(out) >= os.path.getmtime(src):
        return out
    os.makedirs(build_dir, exist_ok=True)
    subprocess.run(['timeout', '300', 'docker', 'run', '--rm', '--network', 'none', '--memory=1g',
                    '-u', '%d:%d' % (os.getuid(), os.getgid()), '-v', '%s:%s' % (ROOT, ROOT), '-w', ROOT, SNIPER_IMAGE,
                    'gcc', '-m32', '-O2', '-shared', '-fPIC', '-o', out, src, '-ldl'], check=True)
    return out


def _unlink_dir(path):
    """A link to a repo folder becomes a folder of links to its entries (so files can be added beside them)."""
    if not os.path.islink(path):
        return
    target = os.path.realpath(path)
    os.remove(path)
    os.makedirs(path)
    for f in os.listdir(target):
        os.symlink(os.path.join(target, f), os.path.join(path, f))


def setup_server(hlds_dir, srv_dir, hl_so, bsp, mapname, seq_text, steps_text, steps_name, extra_cfg, compat=(),
                 lump_index=None, files=None):
    if os.path.exists(srv_dir):
        shutil.rmtree(srv_dir)      # a folder of symlinks we made (build/entity-tests/run/...)
    subprocess.run([os.path.join(ENTITY, 'setup_hlds.sh'), hlds_dir, srv_dir, hl_so], check=True,
                   stdout=subprocess.DEVNULL)
    g = os.path.join(srv_dir, 'syndicate')
    shutil.copy(bsp, os.path.join(g, 'maps', mapname + '.bsp'))
    if seq_text:
        seqdir = os.path.join(g, 'sequences')
        target = os.path.realpath(seqdir)
        os.remove(seqdir)       # the link to the repo's sequences/: a folder of links instead, plus the suite's file
        os.makedirs(seqdir)
        for f in os.listdir(target):
            os.symlink(os.path.join(target, f), os.path.join(seqdir, f))
        with open(os.path.join(seqdir, mapname + '.seq'), 'w') as f:
            f.write(seq_text)
    if compat:
        # maps/map_compat.txt: the shipped entries plus the suite's, for this map's CRC-32 (zlib's, of the .bsp)
        import zlib
        with open(bsp, 'rb') as f:
            crc = '%08x' % (zlib.crc32(f.read()) & 0xffffffff)
        path = os.path.join(g, 'maps', 'map_compat.txt')
        shipped = open(path).read() if os.path.exists(path) else ''
        if os.path.islink(path) or os.path.exists(path):
            os.remove(path)
        with open(path, 'w') as f:
            f.write(shipped.rstrip('\n') + '\n// entity tests\n')
            import re as _re
            for fixed, rule in compat:
                rule = _re.sub(r'#@(\S+)', lambda m_: '#%d' % lump_index[m_.group(1)], rule)
                f.write('%s %s %s\n' % (mapname, fixed or crc, rule))
    for rel, data in sorted((files or {}).items()):
        # the suite's test-only files (Suite.add_file): their folder becomes a folder of links, then the file
        parts = rel.replace('\\', '/').split('/')
        d = g
        for part in parts[:-1]:
            d = os.path.join(d, part)
            _unlink_dir(d)
            os.makedirs(d, exist_ok=True)
        dest = os.path.join(d, parts[-1])
        if os.path.lexists(dest):
            os.remove(dest)
        with open(dest, 'wb') as f:
            f.write(data)
    with open(os.path.join(g, steps_name), 'w') as f:
        f.write(steps_text)
    with open(os.path.join(g, 'server.cfg'), 'a') as f:
        f.write('\n// entity tests\nmp_timelimit 0\nmp_afk_time 0\nmp_afk_spectate_time 0\n' + extra_cfg)


def free_port(start=27961, end=28200):
    """A UDP port nobody listens on: other test runs or servers may be up on this machine."""
    import socket
    for port in range(start, end):
        sk = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            sk.bind(('0.0.0.0', port))
            return port
        except OSError:
            continue
        finally:
            sk.close()
    raise RuntimeError('no free UDP port in %d-%d' % (start, end))


def run_server(srv_dir, mapname, steps_name, timescale, timeout, console_path, asan=False, maxplayers=16, extra_args=()):
    """Runs the server until test_quit (or the timeout). Returns (exit status, wall seconds)."""
    lib = timescale_lib(os.path.dirname(console_path)) if timescale != 1 else None
    args = ['./hlds_linux', '-game', 'syndicate', '-nomaster', '-port', str(free_port()), '+sv_lan', '1',
            '+maxplayers', str(maxplayers), '+sys_ticrate', '1000'] + list(extra_args) + ['+map', mapname]
    env = dict(os.environ, LD_LIBRARY_PATH='.', ET_TIMESCALE=str(timescale))
    preload = []
    if asan:
        preload.append('/usr/lib32/libasan.so.6')
        env['ASAN_OPTIONS'] = 'detect_leaks=0:handle_segv=1:log_path=%s/asan' % srv_dir
    if lib:
        preload.append(lib)
    if asan:
        # in the sniper image: its 32-bit libasan
        inner = 'cd %s && LD_LIBRARY_PATH=. ET_TIMESCALE=%s ASAN_OPTIONS=%s LD_PRELOAD="%s" exec %s' % (
            srv_dir, timescale, env['ASAN_OPTIONS'], ' '.join(preload), ' '.join(args))
        cmd = ['docker', 'run', '--rm', '-i', '--network', 'none', '--memory=4g', '--cpus=2',
               '-u', '%d:%d' % (os.getuid(), os.getgid()), '-e', 'HOME=/tmp']
        for m in sorted({ROOT, os.path.dirname(srv_dir), os.path.realpath(os.path.join(srv_dir, 'hlds_linux', '..')),
                         os.path.dirname(os.path.realpath(os.path.join(srv_dir, 'syndicate', 'dlls', 'hl.so')))}):
            cmd += ['-v', '%s:%s' % (m, m)]
        cmd += [SNIPER_IMAGE, 'bash', '-c', inner]
        cwd = None
    else:
        if preload:
            env['LD_PRELOAD'] = ' '.join(preload)
        cmd = ['nice', '-n', '10'] + args
        cwd = srv_dir
    start = time.time()
    with open(console_path, 'w') as out:
        p = subprocess.Popen(cmd, cwd=cwd, env=env if not asan else None, stdin=subprocess.PIPE, stdout=out,
                             stderr=subprocess.STDOUT)
        # "+" commands run before the game library is loaded: test_run goes in on the console
        p.stdin.write(('test_run %s\n' % steps_name).encode())
        p.stdin.flush()
        try:
            status = p.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            p.kill()
            status = 'timeout'
            p.wait()
        try:
            p.stdin.close()
        except OSError:
            pass
    return status, time.time() - start
