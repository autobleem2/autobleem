#!/usr/bin/env python3
"""Writes THIRD_PARTY_NOTICES.md at the repository root: every third-party component AutoBleem's programs
and packages carry, with its licence text verbatim - taken from the vendored copy where there is one,
so the file cannot drift from what is actually in the tree.

  python tools/make_third_party_notices.py

Run it after adding, removing or upgrading anything under lib_ableem/third_party, tests/third_party,
the fonts or the shipped libraries, and commit the result. The package scripts copy the file next to the
launcher (the MIT/BSD/zlib licences require their notices to travel with the binaries).
"""
import os
import re
import sys

REPO = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
TP = os.path.join(REPO, 'autobleem-core', 'lib_ableem', 'third_party')  # the submodule


def read(rel):
    # lib_ableem/ and tests/third_party live in the autobleem-core submodule since 2026-09-23
    path = os.path.join(REPO, rel)
    if not os.path.exists(path):
        path = os.path.join(REPO, 'autobleem-core', rel)
    with open(path, encoding='utf-8', errors='replace') as f:
        return f.read().strip('\n')


def header_block(rel, first, last):
    """the licence text inside a source file's comment header: the lines between the ones matching
    `first` and `last`, with the comment prefix removed"""
    lines = read(rel).splitlines()
    out = []
    on = False
    for line in lines:
        if not on and re.search(first, line):
            on = True
        if on:
            out.append(re.sub(r'^\s*(/\*+|\*+/|\*\*|\*|//)\s?', '', line).rstrip())
            if re.search(last, line):
                break
    return '\n'.join(out).strip('\n')


ZLIB_LICENCE = '''This software is provided 'as-is', without any express or implied
warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.'''

BSD3_XIPH = '''Copyright (c) 2002-2020 Xiph.org Foundation

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

- Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.

- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.

- Neither the name of the Xiph.org Foundation nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION
OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.'''

# (title, where in the tree / what ships, licence name, the text)
SECTIONS = [
    ('unecm', 'lib_ableem/src/engine/unecm.c - the ECM decoder in every launcher binary',
     'GPL-2.0-or-later',
     'Copyright (C) 2002 Neill Corlett\n\n' + header_block('lib_ableem/src/engine/unecm.c',
                                                            r'This program is free software', r'02111-1307')),
    ('SQLite', 'lib_ableem/third_party/sqlite - compiled into the engine', 'Public domain',
     'SQLite is in the public domain (https://www.sqlite.org/copyright.html).'),
    ('nlohmann/json and fifo_map', 'lib_ableem/third_party/nlohmann', 'MIT',
     'Copyright (c) 2013-2019 Niels Lohmann <http://nlohmann.me>\n'
     'fifo_map: Copyright (c) 2015-2017 Niels Lohmann\n\n' + header_block('lib_ableem/third_party/nlohmann/fifo_map.h',
                                                                            r'Permission is hereby granted', r'^\s*SOFTWARE\.')),
    ('miniz', 'lib_ableem/third_party/miniz - zip reading and writing, CRC-32', 'MIT',
     read('lib_ableem/third_party/miniz/LICENSE')),
    ('plog', 'lib_ableem/third_party/plog - logging', 'MIT', read('lib_ableem/third_party/plog/LICENSE')),
    ('libchdr', 'lib_ableem/third_party/libchdr - CHD disc images', 'BSD-3-Clause',
     read('lib_ableem/third_party/libchdr/LICENSE.txt')),
    ('zlib', 'lib_ableem/third_party/libchdr/deps/zlib-1.3.1', 'zlib',
     read('lib_ableem/third_party/libchdr/deps/zlib-1.3.1/LICENSE')),
    ('Zstandard', 'lib_ableem/third_party/libchdr/deps/zstd-1.5.6', 'BSD-3-Clause',
     read('lib_ableem/third_party/libchdr/deps/zstd-1.5.6/LICENSE')),
    ('LZMA SDK', 'lib_ableem/third_party/libchdr/deps/lzma-24.05 and lib_ableem/third_party/lzma-7z (the 7z reader)',
     'Public domain', read('lib_ableem/third_party/libchdr/deps/lzma-24.05/LICENSE')),
    ('Mbed TLS', 'third_party/mbedtls (3.6.7) - TLS in abfetch, the console\'s update downloader', 'Apache-2.0',
     'Copyright The Mbed TLS Contributors. Mbed TLS is offered under Apache-2.0 OR GPL-2.0-or-later; AutoBleem '
     'takes it under Apache-2.0 (compatible with GPL-3.0). Only a subset of library/ is vendored, unmodified, '
     'with a configuration of our own (third_party/mbedtls/autobleem_config.h).\n\n' +
     read('third_party/mbedtls/LICENSE')),
    ('Mozilla CA certificate bundle', 'src/tools/abfetch/cacert.pem - shipped next to abfetch on the console',
     'MPL-2.0',
     'The root certificates of Mozilla\'s NSS (certdata.txt), as extracted by the curl project '
     '(https://curl.se/docs/caextract.html), unmodified. This Source Code Form is subject to the terms of the '
     'Mozilla Public License, v. 2.0; a copy is at https://mozilla.org/MPL/2.0/.'),
    ('SDL_FontCache','lib_ableem/src/ui/SDL_FontCache.c/.h - text rendering', 'MIT',
     'Copyright (c) 2019 Jonathan Dearborn\n\n' + header_block('lib_ableem/src/ui/SDL_FontCache.h',
                                                                r'Permission is hereby granted', r'THE SOFTWARE\.')),
    ('SDL2, SDL2_image, SDL2_mixer, SDL2_ttf',
     'linked by every program; shipped as shared libraries in the console package (Autobleem/lib/libs.tar.gz) '
     'and the Windows package', 'zlib',
     'Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>\n\n' + ZLIB_LICENCE),
    ('SDL_GameControllerDB', 'src/resources/gamecontrollerdb.txt - the pad mappings every launcher build loads '
     '(tools/update_gamecontrollerdb.py)', 'zlib', read('src/resources/gamecontrollerdb.LICENSE')),
    ('libogg, libvorbis, libvorbisfile', 'shipped in the console package (Autobleem/lib/libs.tar.gz) for SDL2_mixer',
     'BSD-3-Clause', BSD3_XIPH),
    ('GNU libiconv', 'shipped in the console package (Autobleem/lib/libs.tar.gz) as a shared library, unmodified',
     'LGPL-2.1-or-later',
     'Copyright (C) 1999-2022 Free Software Foundation, Inc. Distributed as an unmodified shared library; the '
     'source is at https://www.gnu.org/software/libiconv/ and the licence text at '
     'https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.'),
    ('doctest', 'tests/third_party/doctest - the unit tests only, in no package', 'MIT',
     'Copyright (c) 2016-2023 Viktor Kirilov\n\nhttps://opensource.org/licenses/MIT'),
    ('Open Sans', 'src/resources/fonts/OpenSans-Medium.ttf, OpenSans-Bold.ttf - the launcher\'s fonts',
     'SIL Open Font License 1.1',
     'Copyright 2020 The Open Sans Project Authors (https://github.com/googlefonts/opensans). The licence text is '
     'in src/resources/fonts/OFL.txt.'),
    ('Noto Sans CJK SC', 'src/resources/fonts/NotoSansSC-Regular.otf - Chinese, and Japanese save titles',
     'SIL Open Font License 1.1',
     'Copyright 2014-2021 Adobe (http://www.adobe.com/), with Reserved Font Name \'Source\'. The licence text is in '
     'src/resources/fonts/OFL.txt.'),
    ('Saira', 'autobleem-themes submodule, Themes/default/saira-semicondensed-medium.ttf - the classic screens\' font',
     'SIL Open Font License 1.1',
     'Copyright 2016 The Saira Project Authors (omnibus.type@gmail.com), with Reserved Font Name "Saira". The licence '
     'text is in the submodule\'s Themes/default/OFL.txt.'),
    ('Selawik', 'autobleem-themes submodule, Themes/ab2/selawik-light.ttf - the ab2 theme\'s font',
     'SIL Open Font License 1.1',
     'Copyright 2015 Microsoft Corporation (https://github.com/microsoft/Selawik). See the submodule\'s '
     'Themes/ab2/OFL.txt.'),
    ('Space Shooter Redux (Kenney) and "Venus" (SketchyLogic)', 'src/resources/surprise_game - the About screen\'s game',
     'CC0 1.0', read('src/resources/surprise_game/license.txt')),
    ('libretro-database, libretro-thumbnails', 'fetched at run time or by the installers, never in the repository',
     'see the libretro projects',
     'Game metadata (.rdb) and box art come from https://github.com/libretro/libretro-database and '
     'https://github.com/libretro/libretro-thumbnails under their own terms.'),
    ('pcsx-ab, pcsx-abnxt, RetroArch', 'separate programs the launcher starts; their binaries ship in the packages',
     'GPL-2.0-or-later (the emulators), GPL-3.0-or-later (RetroArch)',
     'Sources: https://github.com/autobleem/pcsx-ab2, https://github.com/autobleem/pcsx-abnxt, '
     'https://github.com/autobleem/retroarch-psc (our console build of https://github.com/libretro/RetroArch). '
     'Each carries its own COPYING.'),
]


def main():
    out = ['# Third-party notices',
           '',
           'AutoBleem is licensed under the GNU General Public License, version 3 or later (see `LICENSE`).',
           'It contains, links or ships the following third-party work under the licences below. This file is',
           'generated by `tools/make_third_party_notices.py` from the licence files in the tree; it is copied next',
           'to the launcher in every package.',
           '']
    for title, where, licence, text in SECTIONS:
        out += [f'## {title}', '', f'*{where}* - **{licence}**', '', '```', text, '```', '']
    path = os.path.join(REPO, 'THIRD_PARTY_NOTICES.md')
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(out))
    print(path, len(SECTIONS), 'sections')
    return 0


if __name__ == '__main__':
    sys.exit(main())
