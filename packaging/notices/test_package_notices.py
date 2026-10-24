"""Regression checks for missing or misleading package licensing evidence."""
import contextlib
import io
import json
import struct
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import package_notices as notices


class NoticeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.qt = self.root / 'qt'
        (self.qt / 'sbom').mkdir(parents=True)
        self.package = self.root / 'package'
        (self.package / 'licenses').mkdir(parents=True)
        (self.package / 'erdflow').write_bytes(b'\x7fELFtest')
        (self.package / 'libQt6Core.so.6').write_bytes(b'\x7fELFtest')
        for name in notices.SEPARATE_TEXTS.values():
            (self.package / 'licenses' / name).write_bytes((notices.HERE / name).read_bytes())
        (self.qt / 'sbom/qtbase.spdx.json').write_text(json.dumps({
            'packages': [{'SPDXID': 'SPDXRef-qt-module-Core', 'name': 'Core',
                          'versionInfo': '6.11.2', 'licenseConcluded':
                          'LicenseRef-Qt-Commercial OR LGPL-3.0-only OR GPL-3.0-only'}],
            'files': [{'SPDXID': 'SPDXRef-file', 'fileName': 'lib/libQt6Core.so.6.11.2'}],
            'relationships': [{'relationshipType': 'CONTAINS',
                               'spdxElementId': 'SPDXRef-qt-module-Core',
                               'relatedSpdxElement': 'SPDXRef-file'}]}))
        self.output = self.package / 'licenses/THIRD-PARTY-LICENSES.txt'
        self.inventory = self.root / 'inventory.json'

    def run_scan(self, *extra):
        args = ['notices', '--root', str(self.package), '--qt-prefix', str(self.qt),
                '--application', 'erdflow', '--output', str(self.output),
                '--inventory', str(self.inventory), *extra]
        with patch('sys.argv', args), contextlib.redirect_stderr(io.StringIO()):
            status = notices.main()
        return status, json.loads(self.inventory.read_text())

    def test_inventory_is_not_release_approval(self):
        status, data = self.run_scan('--release-blocker', 'Source offer pending')
        self.assertEqual(status, 0)
        self.assertTrue(data['inventory_complete'])
        self.assertFalse(data['redistribution_approved'])
        self.assertIn('Source offer pending', data['release_blockers'])
        self.assertEqual(len(data['binary_sha256']['libQt6Core.so.6']), 64)

    def test_unknown_binary_blocks_packaging(self):
        (self.package / 'unknown.dll').write_bytes(b'MZunknown')
        status, data = self.run_scan()
        self.assertEqual(status, 1)
        self.assertFalse(data['inventory_complete'])
        self.assertIn('unknown.dll', '\n'.join(data['problems']))

    def test_missing_referenced_gpl_text_blocks_packaging(self):
        (self.package / 'licenses/GPL-3.0.txt').unlink()
        status, data = self.run_scan()
        self.assertEqual(status, 1)
        self.assertIn('GPL-3.0.txt', '\n'.join(data['problems']))

    def test_no_qt_binary_blocks_packaging(self):
        (self.package / 'libQt6Core.so.6').unlink()
        self.assertEqual(self.run_scan()[0], 1)

    def test_explicit_test_override_retains_incomplete_status(self):
        (self.package / 'unknown.dll').write_bytes(b'MZunknown')
        status, data = self.run_scan('--allow-incomplete')
        self.assertEqual(status, 0)
        self.assertFalse(data['inventory_complete'])
        self.assertIn('INCOMPLETE', self.output.read_text())

    def test_approval_is_written_only_for_a_clean_package(self):
        status, data = self.run_scan('--redistribution-approved')
        self.assertEqual(status, 0)
        self.assertTrue(data['redistribution_approved'])
        self.assertNotIn('TEST CANDIDATE', self.output.read_text())

    def test_approval_is_refused_while_a_blocker_remains(self):
        status, data = self.run_scan('--redistribution-approved', '--release-blocker', 'Source offer pending')
        self.assertEqual(status, 1)
        self.assertFalse(data['redistribution_approved'])
        self.assertIn('TEST CANDIDATE', self.output.read_text())

    def test_approval_is_refused_for_an_unknown_binary(self):
        (self.package / 'unknown.dll').write_bytes(b'MZunknown')
        status, data = self.run_scan('--redistribution-approved')
        self.assertEqual(status, 1)
        self.assertFalse(data['redistribution_approved'])

    def test_declared_library_records_its_file_version(self):
        info = struct.pack('<IIIIII', 0xFEEF04BD, 0x10000, (14 << 16) | 44, (35211 << 16) | 0, 0, 0)
        (self.package / 'msvcp140.dll').write_bytes(b'MZ' + b'\0' * 64 + info + b'\0' * 16)
        status, data = self.run_scan('--declared', 'msvcp140*.dll|Microsoft Visual C++ runtime|-|')
        self.assertEqual(status, 0)
        self.assertEqual(data['declared'][0]['versions'], {'msvcp140.dll': '14.44.35211.0'})
        self.assertIn('msvcp140.dll  (version 14.44.35211.0)', self.output.read_text())

    def test_licence_reference_ending_a_sentence(self):
        text = ('On Debian systems the GPL is in /usr/share/common-licenses/GPL-2.\n'
                'See /usr/share/common-licenses/LGPL-2.1 and /usr/share/common-licenses/Apache-2.0.')
        self.assertEqual(notices.common_licence_references(text),
                         ['/usr/share/common-licenses/Apache-2.0', '/usr/share/common-licenses/GPL-2',
                          '/usr/share/common-licenses/LGPL-2.1'])

    def test_spdx_bill_of_materials_from_the_inventory(self):
        import inventory_to_spdx
        status, _ = self.run_scan()
        self.assertEqual(status, 0)
        package = self.root / 'ERDFlow-0.1.0-test.dmg'
        package.write_bytes(b'package')
        sbom = self.root / 'sbom.spdx.json'
        with patch('sys.argv', ['spdx', '--inventory', str(self.inventory), '--package', str(package),
                                '--version', '0.1.0', '--output', str(sbom)]):
            self.assertEqual(inventory_to_spdx.main(), 0)
        document = json.loads(sbom.read_text())
        self.assertEqual(document['spdxVersion'], 'SPDX-2.3')
        erdflow = document['packages'][0]
        self.assertEqual((erdflow['name'], erdflow['licenseDeclared']), ('ERDFlow', 'MIT'))
        self.assertEqual(erdflow['checksums'][0]['checksumValue'],
                         'bc4a71180870f7945155fbb02f4b0a2e3faa2a62d6d31b7039013055ed19869a')
        names = [p['name'] for p in document['packages']]
        self.assertIn('Qt Core', names)
        refs = {e['licenseId'] for e in document.get('hasExtractedLicensingInfos', [])}
        self.assertIn('LicenseRef-Qt-Commercial', refs)

    def test_parenthesized_licence_alternatives(self):
        self.assertEqual(notices.split_or('(MIT OR BSD-2-Clause) OR (GPL-3.0-only)'),
                         ['(MIT OR BSD-2-Clause)', '(GPL-3.0-only)'])
        self.assertEqual(notices.elect('(GPL-3.0-only OR LGPL-3.0-only)', True),
                         'LGPL-3.0-only')


if __name__ == '__main__':
    unittest.main()
