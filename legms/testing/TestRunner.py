#!/bin/env python3

import argparse
import itertools
import subprocess
import sys
import tempfile

def named_result(ln):
    end_state = ln.find(':')
    end_name = ln.rfind(':')
    if end_name == end_state:
        end_name = -1
    if end_name != -1:
        return (ln[end_state + 2:end_name], ln[0:end_state])
    else:
        return (ln[end_state + 2:], ln[0:end_state])

def run_test(compare_with, compare_by, test, *args):
    with tempfile.TemporaryFile('w+t') as out:
        subprocess.run(args=[test] + list(args), stdout=out.fileno())
        out.seek(0)
        log = [l for ln in out
               for l in [ln.strip()]
               if len(l) > 0
               and l.find('WARNING: field destructors ignored') == -1]
        if compare_with is None:
            fails = map(lambda lg: lg[0:lg.find(':')] == 'FAIL', log)
            sys.exit(1 if any(fails) else 0)
        elif compare_by == 'line':
            eq = map(lambda lns: lns[0].strip() == lns[1],
                     itertools.zip_longest(compare_with, log, fillvalue=''))
            sys.exit(0 if all(eq) else 1)
        else:
            results = dict(named_result(lg) for lg in log)
            expected = dict(named_result(l) for ln in compare_with
                            for l in [ln.strip()]
                            if len(l) > 0)
            eq_keys = set(results.keys()) == set(expected.keys())
            eq = map(lambda k: k in expected and expected[k] == results[k],
                     results.keys())
            sys.exit(0 if eq_keys and all(eq) else 1)
    return

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Run a TestLog-based test program')
    parser.add_argument(
        '--compare-with',
        nargs='?',
        type=argparse.FileType('r'),
        help='file with expected output',
        metavar='FILE',
        dest='compare_with')
    parser.add_argument(
        '--compare-by',
        choices=['line', 'name'],
        nargs='?',
        default='name',
        help='compare by line number or test name (default "%(default)s")',
        metavar='line|name',
        dest='compare_by')
    parser.add_argument(
        'test',
        nargs=1,
        type=argparse.FileType('r'),
        help='test executable name',
        metavar='TEST')
    parser.add_argument(
        'args',
        nargs=argparse.REMAINDER,
        help='test arguments',
        metavar='ARGS')
    args = parser.parse_args()
    if 'compare_with' in args:
        compare_with = args.compare_with
    else:
        compare_with = None
    test_name = args.test[0].name
    args.test[0].close()
    run_test(compare_with, args.compare_by, test_name, *args.args)