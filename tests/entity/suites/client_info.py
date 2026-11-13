"""Per-player client info (docs/dev/CLIENT_INFO.md, src/dlls/client_info.cpp): the server log line
`"name<uid><auth><team>" client "<version>" "<os>" "<engine>" "<renderer>"`. Test players are fake clients (the
engine's CreateFakeClient), so they are bots for it: "bot" at once, no queries. The answer mapping is checked by
handing in the answers a client's engine would give (test_clientinfo <player> start / reply): test players have no
network, so the engine's own queries never reach them."""

from etest import Suite

SUITE = Suite('client_info', 'Client info: bots, and the engine / renderer / OS from the cvar answers',
              requires=('ClientInfo_CvarValue2',))
S = SUITE
S.server_args = ('+log', 'on')     # the log lines also go to the console
DOC = 'dev/CLIENT_INFO.md'
BAD = 'Bad CVAR request'


def answers(t, version, hl25, xash, gl, os=BAD):
    """player1 gets a new wait with `version` in its userinfo, then the four answers (None: not handed in; `os` is
    cl_sbo_os, BAD: a client without it)."""
    t.console('test_userinfo player1 _cl_version %s' % version)
    t.console('test_clientinfo player1 start')
    for cvar, value in (('sv_allow_shaders', hl25), ('host_ver', xash), ('gl_fog', gl), ('cl_sbo_os', os)):
        if value is not None:
            t.console('test_clientinfo player1 reply %s %s' % (cvar, value))
    t.wait_frames(2)


@S.test('test players (fake clients) are bots: one line "bot" "none" "bot" "none" each, no queries', players=2, doc=DOC)
def bots(t):
    t.wait_frames(2)
    t.console('test_clientinfo player1')
    t.console('test_clientinfo player2')
    t.expect.console('"et1<')
    t.expect.console('><BOT><')
    t.expect.console('" client "bot" "none" "bot" "none"')
    t.expect.console('test_clientinfo: player1 "bot" "none" "bot" "none" complete')
    t.expect.console('test_clientinfo: player2 "bot" "none" "bot" "none" complete')


@S.test('HL25 OpenGL, a client without cl_sbo_os: sv_allow_shaders and gl_fog exist, host_ver does not -> "HL25" '
        '"OpenGL", OS unknown', players=1, doc=DOC)
def hl25_opengl(t):
    answers(t, '2.1.0', '1', BAD, '1')
    t.console('test_clientinfo player1')
    t.expect.console('" client "SBO 2.1.0" "unknown" "HL25" "OpenGL"')
    t.expect.console('test_clientinfo: player1 "SBO 2.1.0" "unknown" "HL25" "OpenGL" complete')


@S.test('SBO client on Linux: cl_sbo_os "linux" -> "Linux" "HL25" "OpenGL"', players=1, doc=DOC)
def os_linux(t):
    answers(t, '2.1.5', '1', BAD, '1', 'linux')
    t.console('test_clientinfo player1')
    t.expect.console('" client "SBO 2.1.5" "Linux" "HL25" "OpenGL"')
    t.expect.console('test_clientinfo: player1 "SBO 2.1.5" "Linux" "HL25" "OpenGL" complete')


@S.test('SBO client on Windows: cl_sbo_os "windows" -> "Windows"; another value -> "unknown"', players=1, doc=DOC)
def os_windows(t):
    answers(t, '2.1.6', '1', BAD, BAD, 'windows')
    t.expect.console('" client "SBO 2.1.6" "Windows" "HL25" "software"')
    answers(t, '2.1.7', '1', BAD, '1', 'macos')
    t.expect.console('" client "SBO 2.1.7" "unknown" "HL25" "OpenGL"')


@S.test('legacy software, a client without cl_sbo_os: none of the cvars exists -> "legacy" "software"', players=1, doc=DOC)
def legacy_software(t):
    answers(t, '2.1.1', BAD, BAD, BAD)
    t.expect.console('" client "SBO 2.1.1" "unknown" "legacy" "software"')


@S.test('HL25 software: "CVAR is privileged" counts as existing', players=1, doc=DOC)
def hl25_software(t):
    answers(t, '2.1.2', 'CVAR is privileged', BAD, BAD)
    t.expect.console('" client "SBO 2.1.2" "unknown" "HL25" "software"')


@S.test('Xash3D: host_ver exists; its OS word gives the OS (not cl_sbo_os) -> "Linux" "Xash3D"', players=1, doc=DOC)
def xash_linux(t):
    answers(t, '2.1.3', BAD, '4529 0.21 linux i386 b8c3e3a1', '1', 'windows')
    t.expect.console('" client "SBO 2.1.3" "Linux" "Xash3D" "OpenGL"')


@S.test('an answer that is not all in: the line comes after the 30 s wait with what came ("unknown" for the rest)',
        players=1, doc=DOC)
def timeout(t):
    answers(t, '2.1.4', None, None, '1', None)
    t.console('test_clientinfo player1')
    t.expect.console('test_clientinfo: player1 "SBO 2.1.4" "unknown" "unknown" "unknown" pending')   # sorted at the line
    t.expect.no_console('" client "SBO 2.1.4" "unknown" "HL25"')
    t.wait(31.5)
    t.expect.console('" client "SBO 2.1.4" "unknown" "unknown" "OpenGL"')


@S.test('a second answer for the same cvar and answers after the line change nothing; no version -> "none"',
        players=1, doc=DOC)
def repeated(t):
    t.console('test_userinfo player1 _cl_version ""')
    t.console('test_clientinfo player1 start')
    t.console('test_clientinfo player1 reply gl_fog Bad CVAR request')
    t.console('test_clientinfo player1 reply gl_fog 1')
    t.console('test_clientinfo player1 reply sv_allow_shaders 1')
    t.console('test_clientinfo player1 reply host_ver Bad CVAR request')
    t.console('test_clientinfo player1 reply host_ver 4529 0.21 win32 i386 1')
    t.console('test_clientinfo player1 reply cl_sbo_os Bad CVAR request')
    t.wait_frames(2)
    t.expect.console('" client "none" "unknown" "HL25" "software"')
    t.expect.console('test_clientinfo: player1 "none" "unknown" "HL25" "software" complete')
