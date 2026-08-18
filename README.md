# CS695 Assignment 3 — Linux Namespaces & Container Technology

**Student:** Veenu | **Roll No:** 24m2117

---

## Overview

This assignment explores Linux container primitives from the ground up — namespaces, cgroups, overlay filesystems, and network isolation — across four progressive tasks, culminating in a working container runtime and a two-service orchestrated deployment.

---

## Repository Structure

```
24m2117_assignment3/
├── task1/
│   ├── namespace_prog.c        # Namespace isolation with clone/setns
│   └── namespace_prog          # Compiled binary
├── task2/
│   ├── container_prog.c        # Benchmark program run inside container
│   ├── container_prog          # Compiled binary
│   └── simple_container.sh     # chroot + unshare + cgroup setup script
├── task3/
│   ├── conductor.sh            # Container runtime (Docker-like)
│   ├── setup.sh                # Configuration and environment setup
│   ├── Conductorfile           # Default build spec (debian:bookworm)
│   └── Conductorfiles/
│       ├── debian              # debian:bookworm + htop + Conductorfiles/
│       ├── ubuntu-1            # ubuntu:jammy + Conductorfiles/ + htop
│       └── ubuntu-2            # ubuntu:focal + Conductorfiles/ + htop
└── task4/
    ├── service-orchestrator.sh # Two-container deployment script
    ├── csfile                  # Conductorfile for counter-service image
    └── esfile                  # Conductorfile for external-service image
```

---

## Task 1 — Linux Namespace Isolation

**File:** `task1/namespace_prog.c`

### What it does

Demonstrates PID and UTS namespace isolation using low-level Linux syscalls — `clone`, `setns`, `fork`, and a pipe for synchronization.

### Execution Flow

```
Parent (host namespaces)
│
├─ clone(CLONE_NEWPID | CLONE_NEWUTS) ──► Child1
│   └─ Sets hostname → "Child1Hostname"
│   └─ Sends ready signal via pipe, then loops (sleep)
│
├─ Reads pipe, then setns() into Child1's PID namespace
│
├─ fork() ──► Child2
│   └─ Opens Child1's UTS namespace, calls setns() to join it
│   └─ Prints PID and hostname (both match Child1's namespace)
│   └─ exits
│
├─ wait(Child2)
├─ kill(Child1)
└─ wait(Child1) — cleanup
```

### Key Syscalls Used

| Syscall | Purpose |
|--------|---------|
| `clone(fn, stack, CLONE_NEWPID \| CLONE_NEWUTS \| SIGCHLD, args)` | Create Child1 in new PID + UTS namespaces |
| `sethostname("Child1Hostname", ...)` | Set isolated hostname inside Child1 |
| `setns(pid_ns_fd, 0)` | Parent joins Child1's PID namespace |
| `setns(uts_ns_fd, 0)` | Child2 joins Child1's UTS namespace |
| `pipe(pipefd)` | Synchronize: Child1 signals parent when ready |

### Build & Run

```bash
cd task1
gcc -o namespace_prog namespace_prog.c
sudo ./namespace_prog
```

> Requires root (`clone` with namespace flags needs `CAP_SYS_ADMIN`).

### Expected Output

```
----------------------------------------
Parent Process PID: <host-pid>
Parent Hostname:    <host-hostname>
----------------------------------------
Child1 Process PID: 1
Child1 Hostname:    Child1Hostname
----------------------------------------
Parent Process PID: <host-pid>
Parent Hostname:    <host-hostname>
----------------------------------------
Child2 Process PID: 2
Child2 Hostname:    Child1Hostname
----------------------------------------
Parent Process PID: <host-pid>
Parent Hostname:    <host-hostname>
----------------------------------------
```

---

## Task 2 — Simple Container with chroot, unshare, and cgroups

**Files:** `task2/container_prog.c`, `task2/simple_container.sh`

### What it does

Runs a benchmark program inside progressively more isolated environments, demonstrating chroot, namespace isolation, and CPU cgroup resource control.

### `container_prog.c` — the workload

Compiled into `container_prog` and placed inside `container_root/`. Accepts one argument:

| Argument | Actions |
|----------|---------|
| `subtask1` | Print PID, fork a child (shows PID isolation), list `/` directory |
| `subtask2` | Above + change hostname to `new_hostname` |
| `subtask3` | Above + run a compute benchmark (10000×10000 loop, reports time in µs) |

### `simple_container.sh` — the runner

#### Subtask 2a — chroot only

```bash
sudo chroot container_root ./container_prog subtask1
```

- Copies `container_prog` and all its shared library dependencies into `container_root/`
- `ldd` is used to discover and copy libraries automatically
- Process sees an isolated root filesystem but shares host PID and UTS namespaces

#### Subtask 2b — chroot + new namespaces

```bash
sudo unshare --fork --uts --pid --mount-proc \
    chroot container_root ./container_prog subtask2
```

- New UTS namespace: hostname change is isolated from host
- New PID namespace: `container_prog` sees itself as PID 1
- `--mount-proc`: `/proc` reflects only container processes
- Host hostname is unaffected (verified with `hostname` after exit)

#### Subtask 2c — chroot + namespaces + CPU cgroup (50% limit)

```bash
# Create cgroup and apply CPU quota
sudo mkdir -p /sys/fs/cgroup/cpu_limit_group
echo "50000 100000" | sudo tee /sys/fs/cgroup/cpu_limit_group/cpu.max

# Assign current shell's PID → cgroup runs under quota
echo $$ | sudo tee /sys/fs/cgroup/cpu_limit_group/cgroup.procs

sudo unshare --fork --uts --pid --mount-proc \
    chroot container_root ./container_prog subtask3
```

- `cpu.max = 50000 100000` → 50% of one CPU core
- The compute benchmark (subtask3) will take roughly 2× longer than without the limit
- Cgroup is removed after the run with `rmdir`

### Run

```bash
cd task2
sudo bash simple_container.sh
```

---

## Task 3 — Conductor: A Bash Container Runtime

**Files:** `task3/conductor.sh`, `task3/setup.sh`, `task3/Conductorfile`, `task3/Conductorfiles/`

### What it does

A self-contained container runtime written in Bash, inspired by Docker. Supports building layered images from Conductorfiles, running isolated containers, networking with veth pairs and iptables NAT, and inter-container communication.

### Architecture

```
conductor.sh
├── setup.sh          — env config: network interface, IP subnets, tool list
├── .images/          — built images (layer stack pointers)
├── .containers/      — running containers (overlay upper/work/merged)
└── .cache/
    ├── base/         — debootstrap'd base filesystems (debian/ubuntu)
    └── layers/       — cached RUN and COPY layers (SHA256-addressed)
```

### Conductorfile Syntax

```dockerfile
FROM debian:bookworm       # Base image (debootstrap)
RUN apt-get update && apt-get install -y htop
COPY ./Conductorfiles/ /   # Copy host path into image
```

Supported instructions: `FROM`, `RUN`, `COPY`

### Layered Filesystem (OverlayFS)

Each `RUN` or `COPY` instruction creates a new immutable layer:

```
base layer (debootstrap)
     │
     ▼  COPY layer  →  hash = sha256("COPY-<parent_hash>-<content_hash>")
     │
     ▼  RUN layer   →  hash = sha256("RUN-<parent_hash>-<cmd_hash>")
     │
     ▼  container upper (read-write, per container)
```

Layers are cached — rebuilding with the same instruction and parent skips re-execution.

### Container Isolation

When `run` is called, `unshare` creates a new container with:

| Namespace | Flag | Effect |
|-----------|------|--------|
| UTS | `--uts` | Isolated hostname |
| PID | `--pid` | Process PID 1 inside container |
| NET | `--net` | Own network stack |
| MOUNT | `--mount` | Isolated mount table |
| IPC | `--ipc` | Isolated shared memory/semaphores |

`/proc` and `/sys` are mounted inside the container; `/dev` is bind-mounted from the host.

### Networking (`addnetwork`)

```
Host                         Container
────────────────────────────────────────────
<name>-outside  ◄──veth──►  <name>-inside
  192.168.X.1                 192.168.X.2
       │
  iptables MASQUERADE (if --internet)
  iptables DNAT (if --expose)
```

- veth pair links host and container network namespaces
- `ip_forward` enabled on host
- Optional: internet access (`-i`) via NAT masquerade
- Optional: port forwarding (`-e inner-outer`) via iptables DNAT

### Commands

```bash
sudo ./conductor.sh build <image-name> [Conductorfile]
sudo ./conductor.sh images
sudo ./conductor.sh rmi <image-name>
sudo ./conductor.sh rmcache

sudo ./conductor.sh run <image> <container> [-- command args]
sudo ./conductor.sh run <image> <container> -d [-- command args]   # detached
sudo ./conductor.sh ps
sudo ./conductor.sh stop <container>

sudo ./conductor.sh exec <container> [-- command args]
sudo ./conductor.sh exec <container> -d [-- command args]          # detached

sudo ./conductor.sh addnetwork [-i] [-e inner-outer] <container>
sudo ./conductor.sh peer <container-a> <container-b>
```

### Setup (`setup.sh`)

Before using, configure `DEFAULT_IFC` in `setup.sh` to match your host's outward-facing network interface:

```bash
DEFAULT_IFC=enp0s3    # change to your interface (e.g. eth0, ens3)
```

### Example — Build and run a debian container

```bash
cd task3

# Build image from default Conductorfile
sudo ./conductor.sh build mydebian

# Run interactively
sudo ./conductor.sh run mydebian mycontainer -- /bin/bash

# In another terminal — exec into the running container
sudo ./conductor.sh exec mycontainer -- ps aux

# Stop and clean up
sudo ./conductor.sh stop mycontainer
```

### Example — Run with internet + port forwarding

```bash
sudo ./conductor.sh run mydebian webcontainer -- sleep infinity
sudo ./conductor.sh addnetwork -i -e 8080-3000 webcontainer
# Service on container:8080 is now reachable at host:3000
```

### Provided Conductorfiles

| File | Base | Extra |
|------|------|-------|
| `Conductorfile` | `debian:bookworm` | (base only) |
| `Conductorfiles/debian` | `debian:bookworm` | htop, copies `Conductorfiles/` into `/` |
| `Conductorfiles/ubuntu-1` | `ubuntu:jammy` | copies `Conductorfiles/`, then installs htop |
| `Conductorfiles/ubuntu-2` | `ubuntu:focal` | copies `Conductorfiles/`, then installs htop |

---

## Task 4 — Service Orchestration

**Files:** `task4/service-orchestrator.sh`, `task4/csfile`, `task4/esfile`

### What it does

Deploys two services in separate containers using the Conductor runtime from Task 3, wires them together over a peer network, and exposes the external-service to the host.

### Services

| Service | Language | Port | Container |
|---------|----------|------|-----------|
| `counter-service` | C (compiled with `make`) | 8080 | `cscont` |
| `external-service` | Python 3 / Flask | 8080 | `escont` |

### Conductorfiles

**`csfile`** (counter-service image):
```dockerfile
FROM debian:bookworm
COPY ./counter-service /
RUN apt update && apt install -y build-essential
RUN cd /counter-service && make
```

**`esfile`** (external-service image):
```dockerfile
FROM debian:bookworm
COPY ./external-service /
RUN apt update && apt install -y python3 python3-flask python3-requests
```

### Deployment Architecture

```
Host (enp0s3)
│
│  Port 3000 ◄── iptables DNAT ─────────────────────┐
│                                                    │
│  cscont-outside (192.168.A.1)                escont-outside (192.168.B.1)
│         │                                          │
│  ┌──────▼──────────────┐            ┌──────────────▼──────┐
│  │  cscont             │            │  escont             │
│  │  counter-service    │◄─── peer ──│  external-service   │
│  │  :8080              │            │  :8080              │
│  └─────────────────────┘            └─────────────────────┘
│       Internet ✓                         Internet ✓, Port 3000 exposed
```

### Orchestration Steps (automated by `service-orchestrator.sh`)

1. Build `cs-image` from `csfile` and `es-image` from `esfile`
2. Run both containers detached (`sleep infinity` as init to keep them alive)
3. Add internet-enabled networking to both containers
4. Expose `escont` port 8080 → host port 3000
5. Set up peer networking between `cscont` and `escont`
6. Exec `counter-service` inside `cscont` (detached)
7. Discover `cscont`'s IP from the host veth interface
8. Exec `external-service` inside `escont`, pointing it at `cscont`'s IP
9. `curl http://<host-ip>:3000/` to verify end-to-end

### Run

> **Prerequisites:** `counter-service/` and `external-service/` directories must be present inside `task4/` before running.

```bash
cd task4
sudo bash service-orchestrator.sh
```

### Verify

```bash
# From host
curl http://localhost:3000/

# From any machine that can reach the host
curl http://<host-ip>:3000/
```

---

## Prerequisites

```bash
sudo apt install -y \
    gcc make \
    debootstrap \
    iproute2 iptables \
    python3 python3-flask python3-requests
```

All tasks require **root** privileges.

---

## Notes

- `setup.sh` must be updated with the correct `DEFAULT_IFC` before using Task 3 or Task 4.
- The `.cache/` directory persists across builds for layer reuse; run `sudo ./conductor.sh rmcache` to clear it.
- All debug `echo` statements in `conductor.sh` are commented out and can be re-enabled for troubleshooting.
