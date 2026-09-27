#!/usr/bin/env python3
"""abpanel (R24) - the PC test machine's status panel, drawn in a terminal on its standby monitor.

Runs as `foot --app-id abpanel abpanel` from the sway config next to it (tools/abpanel/sway.config); the VM's
live view (virt-viewer) sits beside it on the same monitor. Everything is read locally except the Teams block,
which is autobleem-main's public status.json on develop (no token). Standard library only.

  abpanel               the panel (redraws every 2 s, Ctrl-C to quit)
  abpanel --once        one frame to stdout, no logo, no screen control (a check over ssh)

Blocks: the logo, the host (IP, time in Irish local time), load (CPU, load average, RAM, temperature, disk),
the three runners (idle/busy and the job's repository), the VM (state, the stick's VERSION, launcher, padsim,
the DebugDriver forward), and the teams (status.json schema 1, docs/admin-roadmap-plan.md in autobleem-main).
"""
import json
import os
import re
import shutil
import signal
import subprocess
import sys
import threading
import time
import urllib.request
from datetime import datetime
from zoneinfo import ZoneInfo

TZ = ZoneInfo("Europe/Dublin")
VM = "pcusb-test"
VM_ADDR = "autobleem@192.168.122.119"
VM_KEY = os.path.expanduser("~/.ssh/pcusb-test-vm_ed25519")
RUNNERS = [("bleemmachine", "actions-runner"), ("bleemmachine-2", "actions-runner-2"),
           ("bleemmachine-3", "actions-runner-3")]
STATUS_URL = "https://raw.githubusercontent.com/autobleem2/autobleem-main/develop/status.json"
LOGO = os.path.join(os.path.dirname(os.path.realpath(__file__)), "ablogo.png")
TICK = 2
VM_EVERY = 30
TEAMS_EVERY = 60

RESET, BOLD, DIM = "\x1b[0m", "\x1b[1m", "\x1b[2m"
GREEN, YELLOW, RED, CYAN, GREY = "\x1b[32m", "\x1b[33m", "\x1b[31m", "\x1b[36m", "\x1b[90m"
STATE_COLOUR = {"working": GREEN, "busy": GREEN, "waiting": YELLOW, "asleep": GREY, "idle": CYAN}


def run(args, timeout=5):
    try:
        return subprocess.run(args, capture_output=True, text=True, timeout=timeout).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return ""


def read(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as f:
            return f.read()
    except OSError:
        return ""


# ---------------------------------------------------------------- the host

class Cpu:
    def __init__(self):
        self.last = self.sample()

    @staticmethod
    def sample():
        f = [int(x) for x in read("/proc/stat").split("\n", 1)[0].split()[1:]]
        idle = f[3] + (f[4] if len(f) > 4 else 0)
        return sum(f), idle

    def percent(self):
        now = self.sample()
        total, idle = now[0] - self.last[0], now[1] - self.last[1]
        self.last = now
        return 0.0 if total <= 0 else 100.0 * (total - idle) / total


def host_ip():
    for line in run(["ip", "-4", "-o", "addr", "show", "scope", "global"]).splitlines():
        p = line.split()
        if len(p) > 3 and not p[1].startswith(("virbr", "docker", "br-", "veth")):
            return p[3].split("/")[0]
    return "no network"


def temperature():
    base = "/sys/class/thermal"
    zones = sorted(os.listdir(base)) if os.path.isdir(base) else []
    for want in ("x86_pkg_temp", "acpitz"):
        for z in zones:
            if read(f"{base}/{z}/type").strip() == want:
                t = read(f"{base}/{z}/temp").strip()
                if t.isdigit():
                    return int(t) / 1000
    return None


def memory():
    m = {}
    for line in read("/proc/meminfo").splitlines():
        k, _, v = line.partition(":")
        m[k] = int(v.split()[0]) if v.split() else 0
    total, avail = m.get("MemTotal", 0), m.get("MemAvailable", 0)
    return (total - avail) / 1048576, total / 1048576


def gib(n):
    return f"{n / 1073741824:.0f}G"


# ---------------------------------------------------------------- the runners

def gha_processes():
    """(pid, cmdline) of every gha-runner process - /proc/<pid>/cmdline is readable by everyone."""
    try:
        import pwd
        uid = pwd.getpwnam("gha-runner").pw_uid
    except (ImportError, KeyError):
        return []
    out = []
    for pid in filter(str.isdigit, os.listdir("/proc")):
        try:
            if os.stat(f"/proc/{pid}").st_uid != uid:
                continue
            with open(f"/proc/{pid}/cmdline", "rb") as f:
                out.append((int(pid), f.read().replace(b"\0", b" ").decode(errors="replace")))
        except OSError:
            continue
    return out


def runners():
    procs = gha_processes()
    rows = []
    for name, folder in RUNNERS:
        active = run(["systemctl", "is-active", f"actions.runner.autobleem2.{name}.service"])
        busy = any(f"/{folder}/bin/Runner.Worker" in c for _, c in procs)
        repo = ""
        for _, c in procs:
            m = re.search(rf"/{re.escape(folder)}/_work/([^/ ]+)/", c)
            if m and not m.group(1).startswith("_"):
                repo = m.group(1)
                break
        state = "busy" if busy else ("idle" if active == "active" else (active or "unknown"))
        rows.append((name, state, repo))
    return rows


# ---------------------------------------------------------------- the VM (slow, in a thread)

class Slow(threading.Thread):
    """Refreshes a value every `every` seconds off the drawing loop."""

    def __init__(self, every, fn):
        super().__init__(daemon=True)
        self.every, self.fn, self.value, self.at = every, fn, None, 0.0

    def run(self):
        while True:
            try:
                self.value = self.fn()
            except Exception as e:  # a panel must keep drawing whatever one block does
                self.value = {"error": str(e)}
            self.at = time.time()
            time.sleep(self.every)


def vm_status():
    st = run(["virsh", "-c", "qemu:///system", "domstate", VM]) or "unknown"
    fwd = run(["systemctl", "--user", "is-active", "pcusb-test-debugdriver.service"])
    v = {"state": st, "forward": fwd}
    if st == "running":
        out = run(["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=4", "-i", VM_KEY, VM_ADDR,
                   "cat /media/autobleem/VERSION; systemctl is-active autobleem.service padsim.service;"
                   " ss -ltn | grep -c 127.0.0.1:6900"], timeout=10).splitlines()
        if len(out) >= 4:
            v.update(version=out[0], launcher=out[1], padsim=out[2], driver="listening" if out[3] != "0" else "off")
        else:
            v["error"] = "guest not reachable over ssh"
    return v


def teams_status():
    req = urllib.request.Request(STATUS_URL, headers={"User-Agent": "abpanel", "Cache-Control": "no-cache"})
    with urllib.request.urlopen(req, timeout=10) as r:
        return json.loads(r.read().decode("utf-8"))


# ---------------------------------------------------------------- drawing

def ago(iso):
    try:
        mins = int((datetime.now(TZ) - datetime.fromisoformat(iso)).total_seconds() // 60)
    except (TypeError, ValueError):
        return "?", True
    if mins < 60:
        return f"{max(mins, 0)} min ago", False
    return f"{mins // 60} h {mins % 60} min ago", True


def hhmm(iso):
    try:
        return datetime.fromisoformat(iso).astimezone(TZ).strftime("%a %H:%M")
    except (TypeError, ValueError):
        return "?"


def colour(state):
    return STATE_COLOUR.get(state, RED) + state + RESET


def frame(cpu, vm, teams, width):
    L = []
    now = datetime.now(TZ)
    L.append(f"{BOLD}bleemmachine{RESET}  {host_ip()}   {now:%a %d %b  %H:%M:%S} {now.tzname()}")
    L.append("")
    used, total = memory()
    t = temperature()
    du = shutil.disk_usage("/")
    load = read("/proc/loadavg").split()[:3]
    L.append(f"{BOLD}Load{RESET}    CPU {cpu.percent():3.0f}%   load {' '.join(load)}   RAM {used:.1f}/{total:.1f}G"
             f"   {'%.0f°C' % t if t is not None else ''}   disk {gib(du.free)} free of {gib(du.total)}")
    L.append("")
    L.append(f"{BOLD}Runners{RESET}")
    for name, state, repo in runners():
        L.append(f"  {name:<16}{colour(state):<20}{repo}")
    L.append("")
    v = vm.value or {}
    state = v.get("state", "...")
    L.append(f"{BOLD}VM{RESET}      {VM}  {(GREEN if state == 'running' else RED) + state + RESET}")
    if "version" in v:
        L.append(f"  stick     {v['version']}")
        L.append(f"  launcher  {v['launcher']}   padsim {v['padsim']}   DebugDriver {v['driver']}"
                 f" (forward {v.get('forward', '?')})")
    elif v:
        L.append(f"  {RED}{v.get('error', '')}{RESET}  forward {v.get('forward', '?')}")
    L.append("")
    s = teams.value
    if not isinstance(s, dict) or s.get("schema") != 1:
        err = s.get("error") if isinstance(s, dict) else None
        L.append(f"{BOLD}Teams{RESET}   {DIM}no status yet{' (' + err + ')' if err else ''}{RESET}")
    else:
        age, stale = ago(s.get("written_at"))
        g = DIM if stale else ""
        L.append(f"{BOLD}Teams{RESET}   {g}written {age} by {s.get('written_by', '?')}{RESET}")
        u = s.get("usage") or {}
        if u:
            L.append(f"{g}  usage 5h {u.get('five_hour_percent', '?')}% (resets {hhmm(u.get('five_hour_resets_at'))})"
                     f"   week {u.get('weekly_percent', '?')}% of {u.get('weekly_cap_today_percent', '?')}% today"
                     f" (resets {hhmm(u.get('weekly_resets_at'))}){RESET}")
        for tm in s.get("teams") or []:
            L.append(f"{g}  {BOLD}{tm.get('name', '?')}{RESET}{g} ({tm.get('team', '')})  {colour(tm.get('state', '?'))}")
            for it in tm.get("items") or []:
                L.append(f"{g}      {it.get('id', '')}  {it.get('what', '')}{RESET}")
            if tm.get("note"):
                L.append(f"{DIM}      {tm['note']}{RESET}")
        needs = s.get("needs_owner") or []
        if needs:
            L.append("")
            L.append(f"{BOLD}{YELLOW}Needs the owner{RESET}")
            for n in needs:
                L.append(f"  {n.get('id', '')}  [{n.get('kind', '')}]  {n.get('what', '')}")
    return [fit(x, width) for x in L]


def fit(line, width):
    """Cuts a line to `width` visible columns, keeping its escape codes."""
    out, n, i = [], 0, 0
    while i < len(line):
        m = re.match(r"\x1b\[[0-9;]*m", line[i:])
        if m:
            out.append(m.group(0))
            i += len(m.group(0))
            continue
        if n >= width - 1:
            out.append("…")
            break
        out.append(line[i])
        n += 1
        i += 1
    return "".join(out) + RESET


def logo(cols):
    """The logo as sixel (foot draws it), rendered once by chafa; empty if chafa or the file is missing."""
    if not shutil.which("chafa") or not os.path.exists(LOGO):
        return "", 0
    rows = 7
    out = run(["chafa", "-f", "sixels", "--size", f"{min(cols, 40)}x{rows}", LOGO], timeout=10)
    return out, rows if out else 0


def main():
    once = "--once" in sys.argv
    cpu, vm, teams = Cpu(), Slow(VM_EVERY, vm_status), Slow(TEAMS_EVERY, teams_status)
    if once:
        for slow in (vm, teams):
            try:
                slow.value = slow.fn()
            except Exception as e:  # the same as Slow.run
                slow.value = {"error": str(e)}
        time.sleep(0.5)
        print("\n".join(frame(cpu, vm, teams, 200)))
        return
    vm.start()
    teams.start()
    redraw = [True]
    signal.signal(signal.SIGWINCH, lambda *_: redraw.__setitem__(0, True))
    sys.stdout.write("\x1b[?25l")  # no cursor
    top = 1
    try:
        while True:
            cols, rows = shutil.get_terminal_size()
            if redraw[0]:
                redraw[0] = False
                img, h = logo(cols)
                sys.stdout.write("\x1b[2J\x1b[H" + img)
                top = h + 2
            lines = frame(cpu, vm, teams, cols)[: max(rows - top, 1)]
            sys.stdout.write(f"\x1b[{top};1H" + "".join(x + "\x1b[K\n" for x in lines) + "\x1b[J")
            sys.stdout.flush()
            time.sleep(TICK)
    except KeyboardInterrupt:
        pass
    finally:
        sys.stdout.write("\x1b[?25h" + RESET + "\n")


if __name__ == "__main__":
    main()
