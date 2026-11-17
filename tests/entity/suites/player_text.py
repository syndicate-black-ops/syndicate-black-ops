"""Player names and chat carry no color codes (client.cpp StripPlayerNameColorCodes / Host_Say, util.cpp
UTIL_RemoveColorCodes; docs/dev/CHAT_PORT_INTEGRATION.md "Player text: no codes"). A name's codes (^0..^9, ^xRGB,
^#RRGGBB) are removed and its first code becomes the player's name color while he has none; a typed chat message is
broadcast without codes; lines the server builds itself keep their codes.

Test players are fake clients: test_userinfo sets the keys and calls ClientUserInfoChanged itself, so the cleaned
name is read from the userinfo; the engine copies it into the client (netname) with its own userinfo pass a frame
later. Every test starts with both players named et1 / et2 without a name color."""

from etest import Suite

SUITE = Suite('player_text', 'Player names and chat: color codes removed, the first name code as the name color',
              players=2)
S = SUITE
DOC = 'dev/CHAT_PORT_INTEGRATION.md'


def reset(t):
    # the name first, then the name color emptied without a rename (a rename alone would get the kept one back)
    for n in (1, 2):
        t._step('test_userinfo', t.ref('player%d' % n), 'name', 'et%d' % n)
    t.wait(0.3)
    for n in (1, 2):
        t._step('test_userinfo', t.ref('player%d' % n), 'namecolor', '')
    t.wait(0.3)


def userinfo(t, player, *pairs):
    t._step('test_userinfo', t.ref(player), *pairs)
    t.wait(0.3)


def snap(t, label, player):
    t.snap(label, player, 'userinfo:name', 'userinfo:namecolor')


@S.test('name "^xF80Fed^1x": the name is "Fedx", namecolor xF80 (the first code)', players=2, doc=DOC)
def name_compact_code(t):
    reset(t)
    userinfo(t, 'player1', 'name', '^xF80Fed^1x')
    snap(t, 's', 'player1')
    t.expect.value('s', 'player1', 'userinfo:name', 'Fedx')
    t.expect.value('s', 'player1', 'userinfo:namecolor', 'xF80')


@S.test('name "^#3FA2E1Sha^2des": the name is "Shades", namecolor x3FA2E1', players=2, doc=DOC)
def name_exact_code(t):
    reset(t)
    userinfo(t, 'player1', 'name', '^#3FA2E1Sha^2des')
    snap(t, 's', 'player1')
    t.expect.value('s', 'player1', 'userinfo:name', 'Shades')
    t.expect.value('s', 'player1', 'userinfo:namecolor', 'x3FA2E1')


@S.test('name "^4Fed" with namecolor 0: namecolor 4', players=2, doc=DOC)
def name_table_code(t):
    reset(t)
    userinfo(t, 'player1', 'name', '^4Fed', 'namecolor', '0')
    snap(t, 's', 'player1')
    t.expect.value('s', 'player1', 'userinfo:name', 'Fed')
    t.expect.value('s', 'player1', 'userinfo:namecolor', '4')


@S.test('name "^4Cop" with namecolor 6 / x4AF (a connect\'s userinfo): the name color stays', players=2, doc=DOC)
def name_keeps_own_color(t):
    reset(t)
    userinfo(t, 'player1', 'name', '^4Cop', 'namecolor', '6')
    userinfo(t, 'player2', 'name', '^4Fed', 'namecolor', 'x4AF')
    snap(t, 's', 'player1')
    snap(t, 's2', 'player2')
    t.expect.value('s', 'player1', 'userinfo:name', 'Cop')
    t.expect.value('s', 'player1', 'userinfo:namecolor', '6')
    t.expect.value('s2', 'player2', 'userinfo:name', 'Fed')
    t.expect.value('s2', 'player2', 'userinfo:namecolor', 'x4AF')


@S.test('name "^0Fed^1x": the first code is ^0 (none): name "Fedx", no namecolor set', players=2, doc=DOC)
def name_reset_code(t):
    reset(t)
    userinfo(t, 'player1', 'name', '^0Fed^1x')
    snap(t, 's', 'player1')
    t.expect.value('s', 'player1', 'userinfo:name', 'Fedx')
    t.expect.value('s', 'player1', 'userinfo:namecolor', '')


@S.test('name of nothing but codes "^1^xF80": "unnamed"; "^1 #Fed " is trimmed to "Fed"', players=2, doc=DOC)
def name_only_codes(t):
    reset(t)
    userinfo(t, 'player1', 'name', '^1^xF80')
    userinfo(t, 'player2', 'name', '^1 #Fed ')
    snap(t, 's', 'player1')
    snap(t, 's2', 'player2')
    t.expect.value('s', 'player1', 'userinfo:name', 'unnamed')
    t.expect.value('s2', 'player2', 'userinfo:name', 'Fed')


@S.test('name "^1et2" while player2 is "et2": "(1)et2", as the engine names a duplicate', players=2, doc=DOC)
def name_duplicate(t):
    reset(t)
    userinfo(t, 'player1', 'name', '^1et2')
    snap(t, 's', 'player1')
    t.expect.value('s', 'player1', 'userinfo:name', '(1)et2')


@S.test('name "^xZZ Fed^": no code, the name stays as typed and no namecolor is set', players=2, doc=DOC)
def name_no_code(t):
    reset(t)
    userinfo(t, 'player1', 'name', '^xZZ Fed^')
    snap(t, 's', 'player1')
    t.expect.value('s', 'player1', 'userinfo:name', '^xZZ Fed^')
    t.expect.value('s', 'player1', 'userinfo:namecolor', '')


@S.test('name "^1et1" while named "et1": the same name after the strip, no "changed name to" line', players=2,
        doc=DOC)
def name_same_after_strip(t):
    reset(t)
    t.mark('m')
    userinfo(t, 'player1', 'name', '^1et1')
    t.wait(0.5)
    snap(t, 's', 'player1')
    t.expect.value('s', 'player1', 'userinfo:name', 'et1')
    t.expect.no_msg('SayText', after='m')


@S.test('renamed with a code: the server\'s "changed name to" line keeps its codes (the name color as ^xF80)',
        players=2, doc=DOC)
def server_line_keeps_codes(t):
    reset(t)
    t.mark('m')
    userinfo(t, 'player1', 'name', '^xF80Fed')
    t.expect.msg('SayText', fields={'text': '^xF80et1^0 changed name to ^xF80Fed^0\n', 'codes': True}, min=1,
                 after='m')


@S.test('say "^1hello ^xF80big ^#3FA2E1world^0": broadcast as "hello big world", to both players', players=2,
        doc=DOC)
def chat_codes_removed(t):
    reset(t)
    t.wait(1.0)
    t.mark('m')
    t.cmd('player1', 'say', '^1hello ^xF80big ^#3FA2E1world^0')
    t.wait(0.3)
    t.expect.msg('SayText', fields={'sender': 1, 'body': 'hello big world', 'codes': False}, count=2, after='m')


@S.test('say of nothing but codes "^1^xF80": not sent', players=2, doc=DOC)
def chat_only_codes(t):
    reset(t)
    t.wait(1.0)
    t.mark('m')
    t.cmd('player1', 'say', '^1^xF80')
    t.wait(0.3)
    t.expect.no_msg('SayText', after='m')


@S.test('say "^xZZ text ^": no code, sent as typed (both players get it)', players=2, doc=DOC)
def chat_no_code(t):
    reset(t)
    t.wait(1.0)
    t.mark('m')
    t.cmd('player1', 'say', '^xZZ text ^')
    t.wait(0.3)
    t.expect.msg('SayText', fields={'body': '^xZZ text ^', 'codes': False}, count=2, after='m')


# A rename on Valve's engine removes the namecolor key when it comes before "name" in the userinfo (Info_RemoveKey
# matches by prefix), which it does after any earlier rename (the new name goes to the end); the server keeps the
# last value and puts it back (client.cpp NameColor_KeepThroughRename). test_userinfo sets the name with the
# engine's own SetClientKeyValue, so the key is really gone before ClientUserInfoChanged: the first rename puts
# "name" behind "namecolor", the second one is the one that loses it without the fix.
def rename_twice(t, name):
    userinfo(t, 'player1', 'name', 'Tmp')
    userinfo(t, 'player1', 'name', name)
    t.wait(0.3)


@S.test('namecolor x4AF, then a rename to "Cop": the server still has namecolor x4AF', players=2, doc=DOC)
def rename_keeps_name_color(t):
    reset(t)
    userinfo(t, 'player1', 'namecolor', 'x4AF')
    rename_twice(t, 'Cop')
    snap(t, 's', 'player1')
    t.expect.value('s', 'player1', 'userinfo:name', 'Cop')
    t.expect.value('s', 'player1', 'userinfo:namecolor', 'x4AF')


@S.test('namecolor 0 (none chosen), then a rename: namecolor stays 0', players=2, doc=DOC)
def rename_keeps_zero(t):
    reset(t)
    userinfo(t, 'player1', 'namecolor', '0')
    rename_twice(t, 'Cop')
    snap(t, 's', 'player1')
    t.expect.value('s', 'player1', 'userinfo:namecolor', '0')


@S.test('namecolor x4AF, then emptied without a rename: it stays empty, also after a later rename', players=2,
        doc=DOC)
def emptied_stays_empty(t):
    reset(t)
    userinfo(t, 'player1', 'namecolor', 'x4AF')
    userinfo(t, 'player1', 'namecolor', '')
    snap(t, 's', 'player1')
    rename_twice(t, 'Cop')
    snap(t, 's2', 'player1')
    t.expect.value('s', 'player1', 'userinfo:namecolor', '')
    t.expect.value('s2', 'player1', 'userinfo:namecolor', '')


@S.test('namecolor x4AF, then a rename with a code "^1Fed": the name is "Fed", namecolor stays x4AF', players=2,
        doc=DOC)
def rename_code_keeps_color(t):
    reset(t)
    userinfo(t, 'player1', 'namecolor', 'x4AF')
    rename_twice(t, '^1Fed')
    snap(t, 's', 'player1')
    t.expect.value('s', 'player1', 'userinfo:name', 'Fed')
    t.expect.value('s', 'player1', 'userinfo:namecolor', 'x4AF')
