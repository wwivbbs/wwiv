#!/usr/bin/env python3
"""End-to-end SSH test: a real OpenSSH client logs on to WWIV's cryptlib SSH server.

    ssh_logon_test.py --bbs PATH --wwivconfig PATH --source DIR [options]

Sets up a throwaway BBS in a temporary directory ("wwivconfig --initialize",
plus the text files, menus and ini files from the source tree's install/),
then does what wwivd does for one SSH call: listens on 127.0.0.1, accepts the
connection from "ssh", and starts "bbs -XS -H<socket>" with that socket. The
ssh client logs on as user WWIVTEST with a password (supplied through
SSH_ASKPASS, so no sshpass or pexpect is needed).

The test passes when:
  * the SSH handshake completes with OpenSSH's default algorithms (no
    legacy ciphers enabled on the client),
  * password authentication succeeds,
  * WWIV's logon prompt comes back over the channel with the SSH user name
    filled in ("NN: WWIVTEST"), and
  * bbs exits by itself after the client hangs up.

Options:
    --bbs PATH          the bbs executable
    --wwivconfig PATH   the wwivconfig executable
    --source DIR        top of the WWIV source tree (for install/)
    --ssh PATH          ssh client (default: ssh on PATH)
    --timeout SECS      seconds to wait for the logon prompt (default 60)
    --keep              keep the temporary BBS directory and logs

Exits 0 on success, 1 on failure (with the ssh and bbs logs printed), and 77
(ctest's SKIP_RETURN_CODE) if no ssh client is available.
"""
import argparse
import os
import re
import select
import shutil
import socket
import subprocess
import sys
import tempfile
import time

USER = "WWIVTEST"
PASSWORD = "testpass"
ANSI_RE = re.compile(rb"\x1b\[[0-9;?]*[A-Za-z]")


def fail(msg, workdir):
    print(f"FAIL: {msg}")
    for name in ("ssh-client.log", "bbs.log"):
        path = os.path.join(workdir, name)
        if os.path.exists(path):
            print(f"----- {name} -----")
            with open(path, errors="replace") as f:
                print(f.read()[-6000:])
    sys.exit(1)


def make_bbs(workdir, wwivconfig, source):
    bbsdir = os.path.join(workdir, "bbs")
    os.mkdir(bbsdir)
    init = subprocess.run([wwivconfig, "--initialize"], cwd=bbsdir,
                          stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
    if init.returncode != 0:
        print(init.stdout.decode(errors="replace"))
        fail(f"wwivconfig --initialize exited {init.returncode}", workdir)
    install = os.path.join(source, "install")
    # Release installs unzip these from gfiles.zip, menus.zip and data.zip.
    for sub in ("gfiles", "menus", "data"):
        shutil.copytree(os.path.join(install, sub), os.path.join(bbsdir, sub),
                        dirs_exist_ok=True)
    for name in os.listdir(os.path.join(install, "inifiles")):
        shutil.copy(os.path.join(install, "inifiles", name), bbsdir)
    return bbsdir


def main():
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument("-h", "--help", action="store_true")
    ap.add_argument("--bbs")
    ap.add_argument("--wwivconfig")
    ap.add_argument("--source")
    ap.add_argument("--ssh", default=shutil.which("ssh"))
    ap.add_argument("--timeout", type=float, default=60)
    ap.add_argument("--keep", action="store_true")
    args = ap.parse_args()
    if args.help or not (args.bbs and args.wwivconfig and args.source):
        print(__doc__.strip())
        sys.exit(0 if args.help else 2)
    if not args.ssh:
        print("SKIP: no ssh client found")
        sys.exit(77)

    workdir = tempfile.mkdtemp(prefix="wwiv-ssh-test-")
    print(f"Test directory: {workdir}")
    try:
        run(args, workdir)
    finally:
        if not args.keep:
            shutil.rmtree(workdir, ignore_errors=True)


def run(args, workdir):
    bbsdir = make_bbs(workdir, os.path.abspath(args.wwivconfig), args.source)

    askpass = os.path.join(workdir, "askpass.sh")
    with open(askpass, "w") as f:
        f.write(f"#!/bin/sh\necho {PASSWORD}\n")
    os.chmod(askpass, 0o700)

    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    port = listener.getsockname()[1]

    env = dict(os.environ, SSH_ASKPASS=askpass, SSH_ASKPASS_REQUIRE="force",
               DISPLAY=os.environ.get("DISPLAY", "none"))
    ssh_log = open(os.path.join(workdir, "ssh-client.log"), "w")
    ssh = subprocess.Popen(
        [args.ssh, "-tt", "-v", "-p", str(port),
         "-F", "/dev/null",  # ignore the user's ssh config
         "-o", "StrictHostKeyChecking=no", "-o", "UserKnownHostsFile=/dev/null",
         "-o", "PubkeyAuthentication=no",
         "-o", "PreferredAuthentications=password,keyboard-interactive",
         "-o", "ConnectTimeout=20", f"{USER}@127.0.0.1"],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=ssh_log,
        env=env, start_new_session=True)  # no controlling tty: use SSH_ASKPASS

    listener.settimeout(30)
    try:
        conn, _ = listener.accept()
    except socket.timeout:
        ssh.kill()
        fail("ssh never connected", workdir)
    listener.close()

    # Hand the accepted socket to bbs, like wwivd does.
    fd = conn.fileno()
    os.set_inheritable(fd, True)
    bbs_log = open(os.path.join(workdir, "bbs.log"), "w")
    bbs = subprocess.Popen([os.path.abspath(args.bbs), "-XS", f"-H{fd}", "-N2"],
                           cwd=bbsdir, pass_fds=(fd,), stdout=bbs_log,
                           stderr=subprocess.STDOUT)
    conn.close()

    received = b""
    deadline = time.monotonic() + args.timeout
    want = f"NN: {USER}".encode()
    while time.monotonic() < deadline:
        ready, _, _ = select.select([ssh.stdout], [], [], 1)
        if ready:
            chunk = os.read(ssh.stdout.fileno(), 4096)
            if not chunk:
                break
            received += chunk
            if want in ANSI_RE.sub(b"", received):
                break
        elif ssh.poll() is not None:
            break

    text = ANSI_RE.sub(b"", received)
    ssh.kill()
    ssh.wait()
    ssh_log.close()
    try:
        bbs.wait(timeout=30)
    except subprocess.TimeoutExpired:
        bbs.kill()
        fail("bbs did not exit within 30 seconds of the client hanging up", workdir)
    bbs_log.close()

    with open(os.path.join(workdir, "ssh-client.log"), errors="replace") as f:
        client_log = f.read()
    cipher = re.search(r"kex: server->client cipher: (\S+) MAC: (\S+)", client_log)
    if "Authenticated to" not in client_log:
        fail("ssh did not authenticate", workdir)
    if want not in text:
        tail = text[-300:].decode("cp437", errors="replace")
        fail(f"no '{want.decode()}' logon prompt; last output: {tail!r}", workdir)

    print(f"OK: cipher {cipher.group(1)}, MAC {cipher.group(2)}; "
          f"got the logon prompt; bbs exited with {bbs.returncode}")


if __name__ == "__main__":
    main()
