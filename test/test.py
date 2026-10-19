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
import struct
import subprocess
import unittest


def jpeg_segment(marker, data):
    """build a JPEG marker segment"""
    return struct.pack('>BBH', 0xFF, marker, len(data) + 2) + data


def xmp_segment(properties, attributes=''):
    """build an XMP (APP1) segment"""
    packet = ('<?xpacket begin="" id="W5M0MpCehiHzreSzNTczkc9d"?>'
              '<x:xmpmeta xmlns:x="adobe:ns:meta/">'
              '<rdf:RDF xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#">'
              '<rdf:Description rdf:about=""'
              ' xmlns:dc="http://purl.org/dc/elements/1.1/"'
              ' xmlns:dcterms="http://purl.org/dc/terms/"'
              ' xmlns:Iptc4xmpExt="http://iptc.org/std/Iptc4xmpExt/2008-02-29/"'
              f'{attributes}>{properties}</rdf:Description>'
              '</rdf:RDF></x:xmpmeta><?xpacket end="w"?>')
    return jpeg_segment(0xE1, b'http://ns.adobe.com/xap/1.0/\0' + packet.encode())


def exif_segment(user_comment):
    """build an Exif (APP1) segment with Orientation and UserComment tags"""
    comment = b'ASCII\0\0\0' + user_comment.encode()
    tiff = b'MM\0\x2a' + struct.pack('>I', 8)
    # IFD0 at offset 8: Orientation and Exif IFD pointer
    tiff += struct.pack('>H', 2)
    tiff += struct.pack('>HHIHH', 0x0112, 3, 1, 6, 0)
    tiff += struct.pack('>HHII', 0x8769, 4, 1, 8 + 2 + 2 * 12 + 4)
    tiff += struct.pack('>I', 0)
    # Exif IFD: UserComment, with its value right after
    tiff += struct.pack('>H', 1)
    tiff += struct.pack('>HHII', 0x9286, 7, len(comment), len(tiff) + 12 + 4)
    tiff += struct.pack('>I', 0)
    tiff += comment
    return jpeg_segment(0xE1, b'Exif\0\0' + tiff)


AI_TYPE = 'http://cv.iptc.org/newscodes/digitalsourcetype/trainedAlgorithmicMedia'
CAMERA_TYPE = 'http://cv.iptc.org/newscodes/digitalsourcetype/digitalCapture'
C2PA_REF = 'self#jumbf=/c2pa/urn:uuid:5f5bba8c-9184-44c6-b9a7-62cfda37b343/c2pa.claim'
A1111_PARAMS = ('a cat\nNegative prompt: blurry\nSteps: 20, Sampler: Euler a, '
                'CFG scale: 7, Seed: 1234, Model hash: abcdef1234')


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

    def make_image(self, name, *segments):
        """create an already optimized test image with the given markers"""
        with open('jpegoptim_test2.jpg', 'rb') as f:
            data = f.read()
        os.makedirs('tmp', exist_ok=True)
        path = os.path.join('tmp', name)
        with open(path, 'wb') as f:
            f.write(data[:2] + b''.join(segments) + data[2:])
        return path

    def run_strip_ai(self, path, args, directory):
        """optimize an image with --strip-ai, returning the output and the result file"""
        output, _ = self.run_test(['--strip-ai'] + args + [path], directory=directory)
        with open(os.path.join(directory, os.path.basename(path)), 'rb') as f:
            return output, f.read()

    def test_strip_ai_c2pa(self):
        """test removal of C2PA manifests"""
        self.run_test(['--strip-none', 'jpegoptim_test1.jpg'], directory='tmp/c2pa_keep')
        with open('tmp/c2pa_keep/jpegoptim_test1.jpg', 'rb') as f:
            self.assertIn(b'jumdc2pa', f.read())
        _, data = self.run_strip_ai('jpegoptim_test1.jpg', ['--strip-none'],
                                    'tmp/c2pa_strip')
        self.assertNotIn(b'jumdc2pa', data)
        self.assertIn(b'http://ns.adobe.com/xap/1.0/', data)

    def test_strip_ai_xmp(self):
        """test removal of AI digital source type and C2PA references from XMP"""
        path = self.make_image('xmp_ai.jpg', xmp_segment(
            f'<dc:title>Holiday</dc:title><dcterms:provenance>{C2PA_REF}</dcterms:provenance>',
            f' Iptc4xmpExt:DigitalSourceType="{AI_TYPE}"'))
        output, data = self.run_strip_ai(path, [], 'tmp/xmp_ai')
        self.assertRegex(output, r'\soptimized\.\s*$')
        self.assertNotIn(b'trainedAlgorithmicMedia', data)
        self.assertNotIn(b'c2pa', data)
        self.assertIn(b'<dc:title>Holiday</dc:title>', data)
        self.assertIn(b'xmlns:Iptc4xmpExt="http://iptc.org/std/Iptc4xmpExt/2008-02-29/">', data)

        path = self.make_image('xmp_ai_element.jpg', xmp_segment(
            f'<Iptc4xmpExt:DigitalSourceType>{AI_TYPE}</Iptc4xmpExt:DigitalSourceType>'
            '<dc:title>Holiday</dc:title>'))
        _, data = self.run_strip_ai(path, [], 'tmp/xmp_ai')
        self.assertNotIn(b'DigitalSourceType', data)
        self.assertIn(b'"><dc:title>Holiday</dc:title></rdf:Description>', data)

    def test_strip_ai_exif(self):
        """test removal of generation parameters from Exif"""
        path = self.make_image('exif_ai.jpg', exif_segment(A1111_PARAMS))
        _, data = self.run_strip_ai(path, [], 'tmp/exif_ai')
        self.assertNotIn(b'Negative prompt', data)
        # the comment is blanked in place, the rest of Exif is untouched
        exif = exif_segment(A1111_PARAMS)
        blank = exif.replace(A1111_PARAMS.encode(), bytes(len(A1111_PARAMS)))
        self.assertIn(blank, data)

    def test_strip_ai_none(self):
        """test that --strip-ai does not touch images without AI metadata"""
        segments = (exif_segment('Holiday'),
                    xmp_segment('<dc:title>Holiday</dc:title>',
                                f' Iptc4xmpExt:DigitalSourceType="{CAMERA_TYPE}"'))
        path = self.make_image('camera.jpg', *segments)
        output, _ = self.run_test(['--strip-ai', path], directory='tmp/camera')
        self.assertRegex(output, r'\sskipped\.\s*$')
        output, data = self.run_strip_ai(path, ['-f'], 'tmp/camera')
        self.assertIn(b''.join(segments), data)

    def test_strip_ai_threshold(self):
        """test that images with AI metadata are written regardless of threshold"""
        path = self.make_image('threshold.jpg', exif_segment(A1111_PARAMS))
        output, _ = self.run_test(['-T', '99', path], directory='tmp/threshold')
        self.assertRegex(output, r'\sskipped\.\s*$')
        output, data = self.run_strip_ai(path, ['-T', '99'], 'tmp/threshold')
        self.assertRegex(output, r'\soptimized\.\s*$')
        self.assertNotIn(b'Negative prompt', data)


if __name__ == '__main__':
    unittest.main()

# eof :-)
