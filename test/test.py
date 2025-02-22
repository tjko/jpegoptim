#!/usr/bin/env python3
#
# test.py -- Unit tests for jpegoptim
#
# Copyright (C) 2023-2025 Timo Kokkonen <tjko@iki.fi>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

"""jpegoptim unit tester"""

import os
import subprocess
import unittest


class JpegoptimTests(unittest.TestCase):
    """jpegoptim test cases"""

    program = '../jpegoptim'
    debug = False

    def setUp(self):
        if "JPEGOPTIM" in os.environ:
            self.program = os.environ["JPEGOPTIM"]
        if "DEBUG" in os.environ:
            self.debug = True

    def run_test(self, args, check=True, directory=None):
        """execute jpegoptim for a test"""
        command = [self.program] + args
        if directory:
            if not os.path.isdir(directory):
                os.makedirs(directory)
            command.extend(['-o', '-d', directory])
        if self.debug:
            print(f'\nRun command: {" ".join(command)}')
        res = subprocess.run(command, encoding="utf-8", check=check,
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        output = res.stdout
        if self.debug:
            print(f'Result: {res.returncode}')
            print(f'---\n{output}\n---\n')
        return output, res.returncode


    def test_version(self):
        """test version information output"""
        output, _ = self.run_test(['--version'])
        self.assertIn('GNU General Public License', output)
        self.assertRegex(output, r'jpegoptim v\d+\.\d+\.\d+')

    def test_noarguments(self):
        """test running withouth arguments"""
        output, res = self.run_test([], check=False)
        self.assertEqual(1, res)
        self.assertRegex(output, r'file argument\(?s\)? missing')

    def test_default(self):
        """test default optimization"""
        output, _ = self.run_test(['jpegoptim_test1.jpg'], directory='tmp/default')
        self.assertTrue(os.path.exists('tmp/default/jpegoptim_test1.jpg'))
        self.assertRegex(output, r'\s\[OK\]\s.*\soptimized\.\s*$')
        # check that output file is indeed smaller than the input file
        self.assertGreater(os.path.getsize('jpegoptim_test1.jpg'),
                           os.path.getsize('tmp/default/jpegoptim_test1.jpg'))

        # check that output file is valid and "optimized"
        output, _ = self.run_test(['-n', 'tmp/default/jpegoptim_test1.jpg'], check=False)
        self.assertRegex(output, r'\s\[OK\]\s.*\sskipped\.\s*$')

    def test_lossy(self):
        """test lossy optimization"""
        output, _ = self.run_test(['-m', '10', 'jpegoptim_test1.jpg'],
                                  directory='tmp/lossy')
        self.assertTrue(os.path.exists('tmp/lossy/jpegoptim_test1.jpg'))
        self.assertRegex(output, r'\s\[OK\]\s.*\soptimized\.\s*$')
        # check that output file is indeed smaller than the input file
        self.assertGreater(os.path.getsize('jpegoptim_test1.jpg'),
                           os.path.getsize('tmp/lossy/jpegoptim_test1.jpg'))

        # check that output file is valid and "optimized"
        output, _ = self.run_test(['-n', 'tmp/lossy/jpegoptim_test1.jpg'], check=False)
        self.assertRegex(output, r'\s\[OK\]\s.*\sskipped\.\s*$')

    def test_optimized(self):
        """test already optimized image"""
        output, _ = self.run_test(['jpegoptim_test2.jpg'],
                                  directory='tmp/optimized')
        self.assertRegex(output, r'\s\[OK\]\s.*\sskipped\.\s*$')

    def test_broken(self):
        """test broken image"""
        output, _ = self.run_test(['jpegoptim_test2-broken.jpg'],
                                    directory='tmp/broken', check=False)
        self.assertRegex(output, r'\s\[WARNING\]\s.*\sskipped\.\s*$')


if __name__ == '__main__':
    unittest.main()

# eof :-)
