"""Compiling a suite's map and running it on a headless stock HLDS."""

import hashlib
import os
import shutil
import subprocess
import threading
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ENTITY = os.path.dirname(HERE)
ROOT = os.path.dirname(os.path.dirname(ENTITY))
SNIPER_IMAGE = os.environ.get('SNIPER_IMAGE',
                              'registry.gitlab.steamos.cloud/steamrt/sniper/sdk:3.0.20260805.254768@sha256:1c33c507bc75d012e77df5727f93b0d5b8c3f7c8d4142ba5f7a16882cc92e014')


# Address-space cap of every map compiler and server (prlimit: a preexec_fn is not safe with the runner's threads).
# A server peaks at ~280 MB of address space and ~85 MB resident (stock HLDS 10211 + the test-layer hl.so, monster
# suites included); the cap stops a runaway one.
COMPILE_MEM = 4 << 30
SERVER_MEM = int(os.environ.get('ET_SERVER_MEM_MB', '1024')) << 20

# the servers running now, killed by kill_servers() when the run is stopped (SIGTERM, Ctrl-C)
_running = set()
_running_lock = threading.Lock()


def log(msg):
    print(msg, flush=True)


def limited(cmd, mem):
    return ['prlimit', '--as=%d' % mem, '--'] + cmd


def kill_servers():
    with _running_lock:
        for p in list(_running):
            try:
                p.kill()
            except OSError:
                pass


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
        cmd = limited(['nice', '-n', '10', 'timeout', '300', os.path.join(sdhlt_dir, tool)] + args +
                      ['-threads', str(threads), '-chart', mapname], COMPILE_MEM)
        with open(os.path.join(out_dir, tool + '.out'), 'w') as f:
            res = subprocess.run(cmd, cwd=out_dir, stdout=f, stderr=subprocess.STDOUT)
        if res.returncode != 0 or os.path.exists(os.path.join(out_dir, mapname + '.err')):
            raise RuntimeError('%s failed (see %s)' % (tool, os.path.join(out_dir, tool + '.out')))
        if tool == 'sdHLBSP' and os.path.exists(os.path.join(out_dir, mapname + '.pts')):
            raise RuntimeError('map leak (%s.pts)' % os.path.join(out_dir, mapname))
    with open(stamp, 'w') as f:
        f.write(h)
    return bsp, True


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


def free_port(start=27961, end=28200, slot=0, slots=1):
    """A UDP port nobody listens on: other test runs or servers may be up on this machine. The search starts at a
    random port of the range, so two runs started together do not pick the same one. With -j, each parallel server
    (slot) searches its own part of the range."""
    import random
    import socket
    part = (end - start) // slots
    start, end = start + slot * part, start + (slot + 1) * part
    span = end - start
    first = random.randrange(span)
    for i in range(span):
        port = start + (first + i) % span
        sk = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        try:
            sk.bind(('0.0.0.0', port))
            return port
        except OSError:
            continue
        finally:
            sk.close()
    raise RuntimeError('no free UDP port in %d-%d' % (start, end))


PORT_TAKEN = "Couldn't allocate dedicated server IP port"


def run_server(srv_dir, mapname, steps_name, timescale, timeout, console_path, asan=False, maxplayers=16, extra_args=(),
               timescale_so=None, slot=(0, 1), say=log):
    """Runs the server until test_quit (or the timeout). Returns (exit status, wall seconds, peak RSS in bytes or
    None). A server that could not bind its port (another run took it between free_port and the bind) is started
    again on another port. slot: (this server's slot, number of parallel servers) for the port range."""
    for attempt in range(3):
        status, secs, rss = _run_server_once(srv_dir, mapname, steps_name, timescale, timeout, console_path, asan,
                                             maxplayers, extra_args, timescale_so, slot)
        try:
            taken = PORT_TAKEN in open(console_path, errors='replace').read()
        except OSError:
            taken = False
        if not taken:
            break
        say('port taken by another run: starting the server again (%d)' % (attempt + 1))
    return status, secs, rss


def _peak_rss(pid):
    try:
        with open('/proc/%d/status' % pid) as f:
            for line in f:
                if line.startswith('VmHWM:'):
                    return int(line.split()[1]) << 10
    except OSError:
        pass
    return None


def _run_server_once(srv_dir, mapname, steps_name, timescale, timeout, console_path, asan, maxplayers, extra_args,
                     timescale_so, slot):
    lib = None
    if timescale != 1:
        lib = timescale_so or timescale_lib(os.path.dirname(console_path))
    port = free_port(slot=slot[0], slots=slot[1])
    args = ['./hlds_linux', '-game', 'syndicate', '-nomaster', '-port', str(port), '+sv_lan', '1',
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
        cmd = limited(['nice', '-n', '10'] + args, SERVER_MEM)
        cwd = srv_dir
    start = time.time()
    rss = None
    with open(console_path, 'w') as out:
        p = subprocess.Popen(cmd, cwd=cwd, env=env if not asan else None, stdin=subprocess.PIPE, stdout=out,
                             stderr=subprocess.STDOUT)
        with _running_lock:
            _running.add(p)
        try:
            # "+" commands run before the game library is loaded: test_run goes in on the console
            p.stdin.write(('test_run %s\n' % steps_name).encode())
            p.stdin.flush()
            status = None
            while status is None:
                # prlimit and nice exec the server: the process is hlds_linux (the docker client with --asan)
                if not asan:
                    rss = max(rss or 0, _peak_rss(p.pid) or 0) or None
                try:
                    status = p.wait(timeout=min(1.0, max(0.0, start + timeout - time.time())))
                except subprocess.TimeoutExpired:
                    if time.time() >= start + timeout:
                        p.kill()
                        status = 'timeout'
                        p.wait()
        except BaseException:       # the run is stopped (SIGTERM, Ctrl-C): this server goes too
            p.kill()
            p.wait()
            raise
        finally:
            with _running_lock:
                _running.discard(p)
            try:
                p.stdin.close()
            except OSError:
                pass
    return status, time.time() - start, rss
