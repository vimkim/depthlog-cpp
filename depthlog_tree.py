#!/usr/bin/env python3
"""
depthlog_tree.py

Visualize call tree per thread from depthlog/spdlog logfmt-ish lines like:
ts="..." level=info depth=2 tid=123 file="x.cpp" line=10 func="foo" msg="..."

Assumptions:
- `depth` is an integer representing current call depth (0 at top-level).
- `func` is present and is the function name.
- Logs are in chronological order per thread (interleaving across threads is OK).

Output:
- Prints an ASCII tree per thread.
- Collapses consecutive identical nodes by default (optional).

Usage:
  python3 depthlog_tree.py app.log
  python3 depthlog_tree.py app.log --show-msg
  python3 depthlog_tree.py app.log --only-tid 3547698
  python3 depthlog_tree.py app.log --max-lines 2000
"""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple


KV_RE = re.compile(r"""([A-Za-z_][A-Za-z0-9_]*)=("(?:\\.|[^"])*"|[^\s]+)""")


def _unquote(s: str) -> str:
    if len(s) >= 2 and s[0] == '"' and s[-1] == '"':
        body = s[1:-1]
        # handle \" and \\ and \xNN
        out = []
        i = 0
        while i < len(body):
            c = body[i]
            if c == "\\" and i + 1 < len(body):
                n = body[i + 1]
                if n in ['\\', '"']:
                    out.append(n)
                    i += 2
                    continue
                if n == "x" and i + 3 < len(body):
                    hx = body[i + 2 : i + 4]
                    try:
                        out.append(chr(int(hx, 16)))
                        i += 4
                        continue
                    except ValueError:
                        pass
                # unknown escape, keep as-is
                out.append(c)
                i += 1
                continue
            out.append(c)
            i += 1
        return "".join(out)
    return s


def parse_logfmt_line(line: str) -> Dict[str, str]:
    kv: Dict[str, str] = {}
    for m in KV_RE.finditer(line):
        k = m.group(1)
        v = m.group(2)
        kv[k] = _unquote(v)
    return kv


@dataclass
class Event:
    ts: str
    level: str
    tid: str
    depth: int
    func: str
    file: str = ""
    line: str = ""
    msg: str = ""


@dataclass
class Node:
    label: str
    count: int = 1
    children: List["Node"] = field(default_factory=list)
    events: List[Event] = field(default_factory=list)


def node_label(ev: Event, show_msg: bool) -> str:
    base = f'{ev.func} ({ev.file}:{ev.line})'
    if show_msg and ev.msg:
        return f'{base} :: {ev.msg}'
    return base


def add_event_to_tree(
    root: Node,
    stack: List[Tuple[int, Node]],
    ev: Event,
    show_msg: bool,
    collapse: bool,
) -> None:
    """
    stack contains tuples of (depth, node) representing current path.
    root is depth -1 virtual root.
    """
    # Pop until parent depth == ev.depth - 1
    while stack and stack[-1][0] >= ev.depth:
        stack.pop()
    parent = stack[-1][1] if stack else root

    lbl = node_label(ev, show_msg)
    # Try to append to existing last child if same label and collapse is on
    if collapse and parent.children and parent.children[-1].label == lbl:
        parent.children[-1].count += 1
        parent.children[-1].events.append(ev)
        cur = parent.children[-1]
    else:
        cur = Node(label=lbl, events=[ev])
        parent.children.append(cur)

    stack.append((ev.depth, cur))


def render_tree(node: Node, prefix: str = "", is_last: bool = True) -> List[str]:
    lines: List[str] = []
    # Root is virtual; don't print it
    for idx, child in enumerate(node.children):
        last = idx == (len(node.children) - 1)
        branch = "└── " if last else "├── "
        suffix = f"  x{child.count}" if child.count > 1 else ""
        lines.append(prefix + branch + child.label + suffix)

        ext = "    " if last else "│   "
        lines.extend(render_tree(child, prefix + ext, last))
    return lines


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("logfile", help="path to app.log")
    ap.add_argument("--only-tid", default=None, help="show only this tid")
    ap.add_argument("--show-msg", action="store_true", help="include msg in node labels")
    ap.add_argument("--no-collapse", action="store_true", help="do not collapse identical consecutive nodes")
    ap.add_argument("--max-lines", type=int, default=0, help="process at most N lines (0 = all)")
    args = ap.parse_args()

    roots: Dict[str, Node] = {}
    stacks: Dict[str, List[Tuple[int, Node]]] = {}

    processed = 0
    with open(args.logfile, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            if args.max_lines and processed >= args.max_lines:
                break
            processed += 1

            kv = parse_logfmt_line(line)
            if not kv:
                continue
            if "tid" not in kv or "depth" not in kv or "func" not in kv:
                continue

            tid = kv.get("tid", "")
            if args.only_tid and tid != args.only_tid:
                continue

            try:
                depth = int(kv["depth"])
            except ValueError:
                continue

            ev = Event(
                ts=kv.get("ts", ""),
                level=kv.get("level", ""),
                tid=tid,
                depth=depth,
                func=kv.get("func", ""),
                file=kv.get("file", ""),
                line=kv.get("line", ""),
                msg=kv.get("msg", ""),
            )

            root = roots.get(tid)
            if root is None:
                root = Node(label=f"tid={tid}")
                roots[tid] = root
                stacks[tid] = []

            add_event_to_tree(
                root=root,
                stack=stacks[tid],
                ev=ev,
                show_msg=args.show_msg,
                collapse=not args.no_collapse,
            )

    # Print
    for tid in sorted(roots.keys(), key=lambda x: int(x) if x.isdigit() else x):
        root = roots[tid]
        print(f"\n=== thread tid={tid} ===")
        # If you want, you can show total events:
        # total = sum(len(n.events) for n in walk_nodes(root))
        print("\n".join(render_tree(root)))


if __name__ == "__main__":
    main()

