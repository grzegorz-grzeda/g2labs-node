#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Build, flash, monitor and provision a g2labs-node Node.

Run it from the activated workspace venv (~/zephyrproject/.venv). The shell
subcommands mirror the Node's own `node ...` UART shell commands; `provision`
applies a Provisioning file (see `provision --help`).
"""

import argparse
import getpass
import glob
import re
import subprocess
import sys
import time
import tomllib
from pathlib import Path

try:
    import serial
except ImportError:
    sys.exit("pyserial is missing: activate ~/zephyrproject/.venv first")

APP_DIR = Path(__file__).resolve().parent.parent
BAUD = 115200
BOARDS = {
    "c6": ("esp32c6_devkitc/esp32c6/hpcore", "build-c6"),
    "s3": ("esp32s3_devkitc/esp32s3/procpu", "build-s3"),
}
KNOWN_NETWORKS_MAX = 5  # NODE_KNOWN_NETWORKS_MAX in core/include/node/core.h
BROKER_DEFAULT_PORT = 1883

PROMPT = "uart:~$ "
ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
RED = "\x1b[1;31m"  # Zephyr's shell_error() colour
LOG_LINE = re.compile(r"^\[\d{2}:\d{2}:\d{2}\.\d{3},\d{3}\] <\w+> ")


class NodeError(Exception):
    """The Node answered with an error, or didn't answer."""


def quote(arg):
    """Quote one argument for the Zephyr shell."""
    if arg and re.fullmatch(r"[A-Za-z0-9_.:@/+=-]+", arg):
        return arg
    return '"' + arg.replace("\\", "\\\\").replace('"', '\\"') + '"'


def node_command(*args):
    return " ".join(["node", *map(quote, args)])


def default_port():
    ports = sorted(glob.glob("/dev/ttyACM*") + glob.glob("/dev/ttyUSB*"))
    return ports[0] if len(ports) == 1 else "/dev/ttyACM0"


class Shell:
    """The Node's UART shell, driven one command at a time."""

    def __init__(self, port, timeout):
        self.timeout = timeout
        # On the devkits DTR/RTS drive reset and boot mode. Leave both at their
        # open-time default (asserted together = run): changing either resets the Node.
        try:
            self.serial = serial.Serial(port, BAUD, timeout=0.05)
        except serial.SerialException as e:
            raise NodeError(f"Cannot open {port}: {e}") from e
        self.serial.reset_input_buffer()
        # An empty line brings the prompt back and clears any half-typed input.
        self.serial.write(b"\x15\r")  # Ctrl-U, Enter
        self._read_until_prompt()

    def close(self):
        self.serial.close()

    def _read_until_prompt(self):
        raw = ""
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            raw += self.serial.read(256).decode("utf-8", "replace")
            if ANSI.sub("", raw).rstrip().endswith(PROMPT.rstrip()) and \
                    not self.serial.in_waiting:
                return raw
        raise NodeError(f"No shell prompt from the Node on {self.serial.port} "
                        f"within {self.timeout:g} s")

    def run(self, command):
        """Send one command; return its output lines, raise NodeError on an error."""
        self.serial.write(command.encode() + b"\r")
        raw = self._read_until_prompt()
        received = []  # (clean text, printed in red)
        for raw_line in raw.replace("\r", "").split("\n"):
            line = ANSI.sub("", raw_line).rstrip()
            if line.startswith(PROMPT.rstrip()):
                line = line[len(PROMPT.rstrip()):].strip()
            if line and not LOG_LINE.match(line):
                received.append((line, RED in raw_line))

        # Drop the shell's echo of the command, which it wraps over several lines
        # when long; it would otherwise show passwords.
        text = "\n".join(line for line, _ in received)
        echo = re.search(r"\s*".join(re.escape(c) for c in command if not c.isspace()), text)
        lines, failed, offset = [], False, 0
        for line, red in received:
            start, offset = offset, offset + len(line) + 1
            if echo and start < echo.end() and offset - 1 > echo.start():
                continue
            failed |= red
            lines.append(line)
        if failed:
            raise NodeError("\n".join(lines))
        return lines


def open_shell(args):
    return Shell(args.port, args.timeout)


def run_and_print(args, *node_args):
    shell = open_shell(args)
    try:
        for line in shell.run(node_command(*node_args)):
            print(line)
    finally:
        shell.close()


def password_from(args, what, empty_flag):
    if args.password is not None:
        return args.password
    if getattr(args, empty_flag.lstrip("-").replace("-", "_")):
        return ""
    password = getpass.getpass(f"{what} password: ")
    if not password:
        raise NodeError(f"Empty password; pass {empty_flag} if that is intended")
    return password


# --- host-side subcommands ---------------------------------------------------

def west(*args):
    print("+ west " + " ".join(args), flush=True)
    if subprocess.call(["west", *args], cwd=APP_DIR) != 0:
        raise NodeError(f"west {args[0]} failed")


def do_build(args):
    target, build_dir = BOARDS[args.board]
    west("build", "--sysbuild", *(["-p", "always"] if args.pristine else []),
         "-d", build_dir, "-b", target, ".")


def do_flash(args):
    do_build(args)
    west("flash", "-d", BOARDS[args.board][1], "--esp-device", args.port)
    if not args.no_monitor:
        do_monitor(args)


def do_monitor(args):
    print(f"Monitor on {args.port}; quit with Ctrl-]", flush=True)
    subprocess.call([sys.executable, "-m", "serial.tools.miniterm",
                     "--quiet", args.port, str(BAUD)])


# --- shell subcommands -------------------------------------------------------

def do_show(args):
    shell = open_shell(args)
    try:
        sections = [
            ("Firmware version", ("version",)),
            ("Node", ("id",)),
            ("Known Networks", ("network", "list")),
            ("Network status", ("network", "status")),
            ("Broker", ("broker", "show")),
        ]
        for i, (title, node_args) in enumerate(sections):
            print(("\n" if i else "") + f"== {title} ==")
            for line in shell.run(node_command(*node_args)):
                print(line)
    finally:
        shell.close()


def do_network_add(args):
    password = password_from(args, f'"{args.ssid}"', "--open")
    run_and_print(args, "network", "add", args.ssid, *([password] if password else []))


def do_broker_set(args):
    run_and_print(args, "broker", "set", args.host, str(args.broker_port))


def do_broker_login(args):
    password = password_from(args, "Broker", "--no-password")
    run_and_print(args, "broker", "login", args.username, *([password] if password else []))


# --- provision ---------------------------------------------------------------

def load_provisioning_file(path):
    """Read and check a Provisioning file; return (node_id, name, networks, broker)."""
    try:
        data = tomllib.loads(Path(path).read_text())
    except (OSError, tomllib.TOMLDecodeError) as e:
        raise NodeError(f"{path}: {e}") from e

    def check_keys(table, allowed, where):
        unknown = set(table) - set(allowed)
        if unknown:
            raise NodeError(f"{path}: unknown key(s) in {where}: {', '.join(sorted(unknown))}")

    def check_str(table, key, where, required=False):
        if key not in table:
            if required:
                raise NodeError(f"{path}: {where} needs {key}")
            return None
        if not isinstance(table[key], str):
            raise NodeError(f"{path}: {where}.{key} must be a string")
        return table[key]

    check_keys(data, ("node_id", "name", "network", "broker"), "the file")
    node_id = check_str(data, "node_id", "the file")
    if node_id is not None and not re.fullmatch(r"[0-9a-f]{12}", node_id):
        raise NodeError(f"{path}: node_id must be 12 lowercase hex digits")
    name = check_str(data, "name", "the file")

    networks = None
    if "network" in data:
        if not isinstance(data["network"], list):
            raise NodeError(f"{path}: write Known Networks as [[network]] tables")
        networks = []
        for i, net in enumerate(data["network"], 1):
            where = f"network #{i}"
            check_keys(net, ("ssid", "password"), where)
            ssid = check_str(net, "ssid", where, required=True)
            if any(ssid == s for s, _ in networks):
                raise NodeError(f'{path}: Known Network "{ssid}" is listed twice')
            networks.append((ssid, check_str(net, "password", where) or ""))
        if len(networks) > KNOWN_NETWORKS_MAX:
            raise NodeError(f"{path}: at most {KNOWN_NETWORKS_MAX} Known Networks")

    broker = None
    if "broker" in data:
        b = data["broker"]
        check_keys(b, ("host", "port", "username", "password"), "[broker]")
        port = b.get("port", BROKER_DEFAULT_PORT)
        if not isinstance(port, int) or isinstance(port, bool) or not 0 < port < 65536:
            raise NodeError(f"{path}: [broker].port must be 1-65535")
        broker = {
            "host": check_str(b, "host", "[broker]", required=True),
            "port": port,
            "username": check_str(b, "username", "[broker]"),
            "password": check_str(b, "password", "[broker]"),
        }
        if broker["password"] and not broker["username"]:
            raise NodeError(f"{path}: [broker].password needs a username")
    return node_id, name, networks, broker


def known_network_ssids(shell):
    ssids = []
    for line in shell.run(node_command("network", "list")):
        m = re.fullmatch(r"\d+: (.*) \((?:password set|open)\)", line)
        if m:
            ssids.append(m.group(1))
    return ssids


def do_provision(args):
    node_id, name, networks, broker = load_provisioning_file(args.file)
    if node_id is None and name is None and networks is None and broker is None:
        raise NodeError(f"{args.file}: nothing to provision")

    # Each step is (command to send, command to print): passwords never get printed.
    steps = []

    def step(*node_args, secret=None):
        shown = [("****" if a == secret and secret else a) for a in node_args]
        steps.append((node_command(*node_args), node_command(*shown)))

    if name is not None:
        step(*(("name", "set", name) if name else ("name", "clear")))

    shell = None if args.dry_run else open_shell(args)
    try:
        if node_id is not None and shell:
            lines = shell.run(node_command("id"))
            actual = next((l.split(": ", 1)[1] for l in lines if l.startswith("Node ID: ")), None)
            if actual != node_id:
                raise NodeError(f"This is Node {actual}, but {args.file} is for {node_id}")

        if networks is not None:
            wanted = {ssid for ssid, _ in networks}
            if shell:
                for ssid in known_network_ssids(shell):
                    if ssid not in wanted:
                        step("network", "remove", ssid)
            else:
                print("# dry run: Known Networks not in the file would be removed first")
            # Re-add every network: the Node never shows a stored password to compare.
            for ssid, password in networks:
                step("network", "add", ssid, *([password] if password else []), secret=password)

        if broker is not None:
            step("broker", "set", broker["host"], str(broker["port"]))
            if broker["username"]:
                password = broker["password"] or ""
                step("broker", "login", broker["username"], *([password] if password else []),
                     secret=password)
            else:
                step("broker", "logout")

        for command, shown in steps:
            print(f"> {shown}", flush=True)
            if shell:
                for line in shell.run(command):
                    print(f"  {line}")
    finally:
        if shell:
            shell.close()


# --- command line ------------------------------------------------------------

def parser():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--port", default=default_port(),
                   help="serial port (default: the only /dev/ttyACM*|/dev/ttyUSB*, "
                        "else /dev/ttyACM0; now %(default)s)")
    p.add_argument("--timeout", type=float, default=5.0,
                   help="seconds to wait for the Node's shell (default %(default)s)")
    sub = p.add_subparsers(dest="cmd", required=True, metavar="command")

    def cmd(parent, name, func, help_):
        c = parent.add_parser(name, help=help_, description=help_)
        c.set_defaults(func=func)
        return c

    def group(name, help_):
        g = sub.add_parser(name, help=help_, description=help_)
        return g.add_subparsers(dest="sub", required=True, metavar="command")

    def board_args(c):
        c.add_argument("--board", required=True, choices=BOARDS, help="ESP32-C6 or ESP32-S3")
        c.add_argument("--pristine", action="store_true", help="rebuild from scratch")

    board_args(cmd(sub, "build", do_build, "Build MCUboot and the Node image (sysbuild)"))
    c = cmd(sub, "flash", do_flash, "Build, flash MCUboot and the Node image, then monitor")
    board_args(c)
    c.add_argument("--no-monitor", action="store_true", help="don't open the monitor after")
    cmd(sub, "monitor", do_monitor, "Open the Node's serial shell (quit with Ctrl-])")
    cmd(sub, "show", do_show, "Show firmware version, Node ID, Known Networks and Broker")

    cmd(sub, "version", lambda a: run_and_print(a, "version"), "Print the firmware version")
    cmd(sub, "id", lambda a: run_and_print(a, "id"), "Print the Node ID and friendly name")

    name = group("name", "Friendly name")
    c = cmd(name, "set", lambda a: run_and_print(a, "name", "set", a.name), "Set the friendly name")
    c.add_argument("name")
    cmd(name, "clear", lambda a: run_and_print(a, "name", "clear"), "Clear the friendly name")

    net = group("network", "Known Networks")
    c = cmd(net, "add", do_network_add,
            "Add a Known Network (or change its password); prompts for the password")
    c.add_argument("ssid")
    pw = c.add_mutually_exclusive_group()
    pw.add_argument("--password", help="password, instead of being prompted")
    pw.add_argument("--open", action="store_true", help="open network, no password")
    c = cmd(net, "remove", lambda a: run_and_print(a, "network", "remove", a.ssid),
            "Remove a Known Network")
    c.add_argument("ssid")
    cmd(net, "list", lambda a: run_and_print(a, "network", "list"),
        "List Known Networks (passwords hidden)")
    cmd(net, "status", lambda a: run_and_print(a, "network", "status"),
        "Show the current Known Network, RSSI and IP address")

    brk = group("broker", "Broker connection")
    c = cmd(brk, "set", do_broker_set, "Set the Broker host and port")
    c.add_argument("host")
    c.add_argument("broker_port", metavar="port", nargs="?", type=int,
                   default=BROKER_DEFAULT_PORT, help="default %(default)s")
    c = cmd(brk, "login", do_broker_login,
            "Log in to the Broker as a user; prompts for the password")
    c.add_argument("username")
    pw = c.add_mutually_exclusive_group()
    pw.add_argument("--password", help="password, instead of being prompted")
    pw.add_argument("--no-password", action="store_true", help="user without a password")
    cmd(brk, "logout", lambda a: run_and_print(a, "broker", "logout"),
        "Connect to the Broker anonymously")
    cmd(brk, "clear", lambda a: run_and_print(a, "broker", "clear"), "Forget the Broker")
    cmd(brk, "show", lambda a: run_and_print(a, "broker", "show"),
        "Show the Broker settings (password hidden) and connection")

    c = cmd(sub, "provision", do_provision, "Apply a Provisioning file to the Node")
    c.epilog = PROVISION_HELP
    c.formatter_class = argparse.RawDescriptionHelpFormatter
    c.add_argument("file", help="TOML Provisioning file, e.g. kitchen.provision.toml")
    c.add_argument("--dry-run", action="store_true",
                   help="print the node commands (passwords masked) without sending them")
    return p


PROVISION_HELP = """\
A Provisioning file is TOML; every part is optional:

  node_id = "aabbccddeeff"   # refuse to provision any other Node
  name = "kitchen"           # friendly name; "" clears it

  [[network]]                # one per Known Network, at most 5
  ssid = "home"
  password = "secret"        # omit for an open network

  [broker]
  host = "192.168.1.10"
  port = 1883                # default 1883
  username = "node"          # omit to connect anonymously
  password = "secret"

Each part present replaces that setting on the Node: [[network]] leaves the
Node with exactly the listed Known Networks, [broker] sets the Broker exactly
as written. Parts left out are not touched. The file holds plaintext
passwords: name it *.provision.toml, which git ignores.
"""


def main():
    args = parser().parse_args()
    try:
        args.func(args)
    except NodeError as e:
        sys.exit(str(e))
    except KeyboardInterrupt:
        sys.exit(130)


if __name__ == "__main__":
    main()
