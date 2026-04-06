#!/usr/bin/env python3
#
# Copyright (C) 2026 Collabora, Ltd.
# SPDX-License-Identifier: MIT

import argparse
import os
import subprocess

NPROC = len(os.sched_getaffinity(0))

class TestResults:
    def __init__(self):
        self.count = 0
        self.fail = 0

    def add_ret(self, ret):
        self.count += 1
        if ret != 0:
            self.fail += 1

    def print(self):
        print("Passed: {}".format(self.count - self.fail))
        print("FAILED: {}".format(self.fail))

def find_test_files(tests):
    for test in tests:
        for root, dirs, files in os.walk(test):
            for file in files:
                if file.endswith('.amber'):
                    yield os.path.join(root, file)

def main():
    parser = argparse.ArgumentParser(
        prog='RUN-TESTS',
        description='run panfrost amber tests')
    parser.add_argument('--amber', help='Amber executable to use',
                        default='amber')
    parser.add_argument('-j', '--jobs', help='Number of concurrent jobs',
                        type=int, default=NPROC)
    parser.add_argument('test', nargs='*', default=['.'])
    args = parser.parse_args()

    results = TestResults()

    jobs = [None] * args.jobs
    i = 0
    for test in find_test_files(args.test):
        if jobs[i] is not None:
            results.add_ret(jobs[i].wait())

        jobs[i] = subprocess.Popen(
            args=[args.amber, '-v', '1.1' ,'-t', 'spv1.3', test],
            stdout=subprocess.DEVNULL)

        i = (i + 1) % args.jobs

    for i in range(args.jobs):
        if jobs[i] is not None:
            results.add_ret(jobs[i].wait())

    results.print()

if __name__ == '__main__':
    main()
