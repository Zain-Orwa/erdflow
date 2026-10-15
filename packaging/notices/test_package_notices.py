"""Regression checks for missing or misleading package licensing evidence."""
import contextlib
import io
import json
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

    def test_licence_reference_ending_a_sentence(self):
        text = ('On Debian systems the GPL is in /usr/share/common-licenses/GPL-2.\n'
                'See /usr/share/common-licenses/LGPL-2.1 and /usr/share/common-licenses/Apache-2.0.')
        self.assertEqual(notices.common_licence_references(text),
                         ['/usr/share/common-licenses/Apache-2.0', '/usr/share/common-licenses/GPL-2',
                          '/usr/share/common-licenses/LGPL-2.1'])

    def test_parenthesized_licence_alternatives(self):
        self.assertEqual(notices.split_or('(MIT OR BSD-2-Clause) OR (GPL-3.0-only)'),
                         ['(MIT OR BSD-2-Clause)', '(GPL-3.0-only)'])
        self.assertEqual(notices.elect('(GPL-3.0-only OR LGPL-3.0-only)', True),
                         'LGPL-3.0-only')


if __name__ == '__main__':
    unittest.main()
