#!/usr/bin/env python3
"""Fake Discord IPC server for ipc_driver.cpp: answers the handshake with READY, records SET_ACTIVITY frames, sends a
PING and expects the PONG, and checks the protocol. Usage: fake_discord.py <socket path> <result file>"""
import json, socket, struct, sys

path, result = sys.argv[1], sys.argv[2]
srv = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
srv.bind(path)
srv.listen(1)
srv.settimeout(20)
log, errors = [], []


def recv_frame(conn):
    hdr = b''
    while len(hdr) < 8:
        d = conn.recv(8 - len(hdr))
        if not d:
            return None
        hdr += d
    op, ln = struct.unpack('<II', hdr)
    body = b''
    while len(body) < ln:
        d = conn.recv(ln - len(body))
        if not d:
            return None
        body += d
    return op, json.loads(body.decode('utf-8'))


def send_frame(conn, op, obj):
    b = json.dumps(obj).encode()
    try:
        conn.sendall(struct.pack('<II', op, len(b)) + b)
    except (BrokenPipeError, ConnectionResetError):
        pass  # the client closed (e.g. right after clearing the activity)


for session in range(2):
    conn, _ = srv.accept()
    conn.settimeout(20)
    op, hs = recv_frame(conn)
    if op != 0 or hs.get('v') != 1 or hs.get('client_id') != '123456789012345678':
        errors.append(f'bad handshake {op} {hs}')
    send_frame(conn, 1, {'cmd': 'DISPATCH', 'evt': 'READY', 'data': {'v': 1, 'user': {'id': '1', 'username': 'test'}}})
    pinged = False
    while True:
        f = recv_frame(conn)
        if f is None:
            log.append(('closed', session))
            break
        op, msg = f
        if op == 4:
            log.append(('pong', session, msg))
            continue
        if op == 1 and msg.get('cmd') == 'SUBSCRIBE':
            log.append(('subscribe', session, msg.get('evt')))
            send_frame(conn, 1, {'cmd': 'SUBSCRIBE', 'nonce': msg.get('nonce'), 'evt': None, 'data': {'evt': msg.get('evt')}})
            if session == 0 and msg.get('evt') == 'ACTIVITY_JOIN_REQUEST':
                send_frame(conn, 1, {'cmd': 'DISPATCH', 'evt': 'ACTIVITY_JOIN', 'data': {'secret': '203.0.113.4:27015'}})
                send_frame(conn, 1, {'cmd': 'DISPATCH', 'evt': 'ACTIVITY_JOIN_REQUEST',
                                     'data': {'user': {'id': '987654321', 'username': 'friend'}}})
            continue
        if op == 1 and msg.get('cmd') == 'SEND_ACTIVITY_JOIN_INVITE':
            log.append(('invite', session, msg['args'].get('user_id')))
            continue
        if op != 1 or msg.get('cmd') != 'SET_ACTIVITY' or 'nonce' not in msg or not isinstance(msg['args'].get('pid'), int):
            errors.append(f'bad frame {op} {msg}')
        log.append(('activity', session, msg['args']['activity']))
        send_frame(conn, 1, {'cmd': 'SET_ACTIVITY', 'nonce': msg['nonce'], 'evt': None, 'data': msg['args']['activity']})
        if not pinged:
            send_frame(conn, 3, {'ping': 42})
            pinged = True
    conn.close()

acts = [(s, a) for k, s, *rest in log if k == 'activity' for a in rest]
expect = [(0, {'details': 'Srv "quoted" \\ tab\t\x01 é'}), (0, {'details': 'B'}), (0, None), (1, {'details': 'C'})]
if acts != expect:
    errors.append(f'activities {acts!r} != {expect!r}')
if [x for x in log if x[0] == 'subscribe'] != [('subscribe', 0, 'ACTIVITY_JOIN'), ('subscribe', 0, 'ACTIVITY_JOIN_REQUEST'),
                                             ('subscribe', 1, 'ACTIVITY_JOIN'), ('subscribe', 1, 'ACTIVITY_JOIN_REQUEST')]:
    errors.append('subscriptions: ' + repr([x for x in log if x[0] == 'subscribe']))
if [x for x in log if x[0] == 'invite'] != [('invite', 0, '987654321')]:
    errors.append('invite: ' + repr([x for x in log if x[0] == 'invite']))
if not any(k == 'pong' and m == {'ping': 42} for k, s, *r in log for m in r if k == 'pong'):
    errors.append('no PONG')
open(result, 'w').write('\n'.join(errors) if errors else 'OK')
print('\n'.join(map(str, log)))
