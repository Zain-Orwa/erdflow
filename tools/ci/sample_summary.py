#!/usr/bin/env python3
# Read this before changing anything: CLAUDE.md at the repository root.
#
# Summarises a macOS `sample` report of desktop_tests for the CI log
# (2026-10-12, temporary, for the macOS Intel window-test diagnosis). Counts are
# samples of the main thread at the interval sample was given -- wall-clock
# time the thread spent there, waiting or working, not CPU time.
import collections
import re
import sys

LINE = re.compile(r'^([ +!:|]*)(\d+) (.+?)  \(in ([^)]+)\)')
WAITS = ('settle_for', 'settle_motion', 'settle_until', 'anon::settle')


def name(symbol):
    symbol = symbol.replace('(anonymous namespace)', 'anon')
    return re.sub(r'\(.*', '', symbol)[:110]


def main(path, top=45):
    text = open(path, errors='replace').read()
    if 'Call graph:' not in text:
        print('No call graph in', path)
        return
    graph = text.split('Call graph:')[1].split('Total number in stack')[0]
    nodes = []
    stack = []
    for line in graph.split('\n'):
        match = LINE.match(line)
        if not match:
            continue
        depth, count = len(match.group(1)), int(match.group(2))
        symbol, library = name(match.group(3)), match.group(4)
        while stack and stack[-1][0] >= depth:
            stack.pop()
        nodes.append((depth, count, symbol, library, [s for _, s in stack]))
        stack.append((depth, symbol))

    inclusive = collections.Counter()
    for depth, count, symbol, library, above in nodes:
        if symbol not in above and library == 'desktop_tests':
            inclusive[symbol] += count
    main_samples = inclusive.get('main', 0)
    print(f'SAMPLE main thread samples under main: {main_samples}')
    print('SAMPLE inclusive, functions in desktop_tests (samples):')
    for symbol, count in inclusive.most_common(top):
        print(f'SAMPLE {count:7d}  {symbol}')

    # Time on the main thread's own frames, by library, split by whether a
    # test wait was on the stack.
    own = collections.Counter()
    for index, (depth, count, symbol, library, above) in enumerate(nodes):
        children = 0
        for later in nodes[index + 1:]:
            if later[0] <= depth:
                break
            if len(later[4]) == len(above) + 1:
                children += later[1]
        mine = count - children
        if mine <= 0 or not (symbol == 'main' or 'main' in above):
            continue
        waiting = any(any(w in s for w in WAITS) for s in above + [symbol])
        own[('in waits' if waiting else 'working ', library)] += mine
    print('SAMPLE own frames by library (samples):')
    for (where, library), count in own.most_common(25):
        print(f'SAMPLE {count:7d}  {where}  {library}')


if __name__ == '__main__':
    main(sys.argv[1])
