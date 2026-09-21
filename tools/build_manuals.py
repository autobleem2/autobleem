#!/usr/bin/env python3
"""The user manuals: manuals/<lang>/*.md (a small Markdown) -> build_manuals/<lang>/*.html (+ the images) ->
the same as PDF, rendered by Chrome or Edge in headless mode. Nothing but the standard library on our side.

    python tools/build_manuals.py            # every manual, HTML and PDF
    python tools/build_manuals.py --html     # HTML only (no browser needed)
    python tools/build_manuals.py --lang pl

The Markdown understood: # / ## / ### headings (the table of contents is made from ## and ###), paragraphs,
"- " lists (nested by two spaces) and "1. " lists, **bold**, *italic*, `code`, [text](url), a line that is
only ![caption](path) becomes a figure with its caption, | tables | with a |---| line under the header,
"> " notes, ``` code blocks, --- a rule, and <!-- pagebreak --> a page break in the PDF. The title (the first
"# ") and manuals/style.css make the page; the footer carries the version from git.
"""
import html
import os
import re
import shutil
import subprocess
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
SRC = os.path.join(REPO, 'manuals')
OUT = os.path.join(REPO, 'build_manuals')
BROWSERS = [
    r'C:\Program Files\Google\Chrome\Application\chrome.exe',
    r'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe',
    r'C:\Program Files\Microsoft\Edge\Application\msedge.exe',
    '/usr/bin/google-chrome', '/usr/bin/chromium', '/usr/bin/chromium-browser', '/usr/bin/microsoft-edge',
]

TOC_TITLE = {'en': 'Contents', 'pl': 'Spis treści'}


def inline(text):
    text = html.escape(text, quote=False)
    text = re.sub(r'`([^`]+)`', r'<code>\1</code>', text)
    text = re.sub(r'\*\*([^*]+)\*\*', r'<strong>\1</strong>', text)
    text = re.sub(r'(?<![*\w])\*([^*]+)\*(?!\w)', r'<em>\1</em>', text)
    text = re.sub(r'\[([^\]]+)\]\(([^)]+)\)', r'<a href="\2">\1</a>', text)
    return text


def slug(text):
    s = re.sub(r'[^\w\s-]', '', text.lower(), flags=re.UNICODE).strip()
    return re.sub(r'[\s_]+', '-', s)


def convert(md):
    """the body's HTML, the title, and the headings [(level, text, id)]"""
    lines = md.split('\n')
    out, headings, title = [], [], ''
    i = 0
    para = []
    list_stack = []  # (indent, tag)

    def flush_para():
        if para:
            out.append('<p>' + inline(' '.join(para)) + '</p>')
            para.clear()

    def close_lists(to_indent=-1):
        while list_stack and list_stack[-1][0] > to_indent:
            out.append('</%s>' % list_stack.pop()[1])

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()
        if stripped.startswith('```'):
            flush_para()
            close_lists()
            block = []
            i += 1
            while i < len(lines) and not lines[i].strip().startswith('```'):
                block.append(lines[i])
                i += 1
            out.append('<pre><code>' + html.escape('\n'.join(block)) + '</code></pre>')
            i += 1
            continue
        m = re.match(r'^(#{1,4})\s+(.*)$', stripped)
        if m:
            flush_para()
            close_lists()
            level = len(m.group(1))
            text = m.group(2).strip()
            if level == 1 and not title:
                title = text
            hid = slug(text)
            headings.append((level, text, hid))
            out.append('<h%d id="%s">%s</h%d>' % (level, hid, inline(text), level))
            i += 1
            continue
        if stripped == '<!-- pagebreak -->':
            flush_para()
            close_lists()
            out.append('<div class="pagebreak"></div>')
            i += 1
            continue
        if stripped == '---':
            flush_para()
            close_lists()
            out.append('<hr>')
            i += 1
            continue
        m = re.match(r'^!\[([^\]]*)\]\(([^)]+)\)$', stripped)
        if m:
            flush_para()
            close_lists()
            caption = m.group(1)
            out.append('<figure><img src="%s" alt="%s">%s</figure>' %
                       (html.escape(m.group(2)), html.escape(caption),
                        '<figcaption>%s</figcaption>' % inline(caption) if caption else ''))
            i += 1
            continue
        if stripped.startswith('|') and i + 1 < len(lines) and re.match(r'^\|?\s*:?-{2,}', lines[i + 1].strip()):
            flush_para()
            close_lists()
            cells = [c.strip() for c in stripped.strip('|').split('|')]
            rows = []
            i += 2
            while i < len(lines) and lines[i].strip().startswith('|'):
                rows.append([c.strip() for c in lines[i].strip().strip('|').split('|')])
                i += 1
            out.append('<table><thead><tr>' + ''.join('<th>%s</th>' % inline(c) for c in cells) + '</tr></thead><tbody>')
            for row in rows:
                out.append('<tr>' + ''.join('<td>%s</td>' % inline(c) for c in row) + '</tr>')
            out.append('</tbody></table>')
            continue
        if stripped.startswith('> '):
            flush_para()
            close_lists()
            note = []
            while i < len(lines) and lines[i].strip().startswith('>'):
                note.append(lines[i].strip()[1:].strip())
                i += 1
            out.append('<div class="note">' + inline(' '.join(note)) + '</div>')
            continue
        m = re.match(r'^(\s*)([-*]|\d+\.)\s+(.*)$', line)
        if m:
            flush_para()
            indent = len(m.group(1))
            tag = 'ol' if m.group(2)[0].isdigit() else 'ul'
            close_lists(indent)
            if not list_stack or list_stack[-1][0] < indent:
                list_stack.append((indent, tag))
                out.append('<%s>' % tag)
            out.append('<li>' + inline(m.group(3)) + '</li>')
            i += 1
            continue
        if not stripped:
            flush_para()
            close_lists()
            i += 1
            continue
        if list_stack:
            # a continuation line of a list item
            out[-1] = out[-1][:-5] + ' ' + inline(stripped) + '</li>'
            i += 1
            continue
        para.append(stripped)
        i += 1
    flush_para()
    close_lists()
    return '\n'.join(out), title, headings


def version():
    try:
        return subprocess.run(['git', 'describe', '--tags', '--always'], cwd=REPO, capture_output=True,
                              text=True).stdout.strip()
    except OSError:
        return ''


def page(body, title, headings, lang, css):
    toc = ['<nav class="toc"><h2>%s</h2><ul>' % TOC_TITLE.get(lang, 'Contents')]
    for level, text, hid in headings:
        if level in (2, 3):
            toc.append('<li class="l%d"><a href="#%s">%s</a></li>' % (level, hid, inline(text)))
    toc.append('</ul></nav>')
    return ('<!DOCTYPE html><html lang="%s"><head><meta charset="utf-8"><title>%s</title>'
            '<style>%s</style></head><body><div class="cover"><h1>%s</h1><p class="version">AutoBleem 2 %s</p></div>'
            '%s\n%s\n<footer>AutoBleem 2 %s</footer></body></html>'
            % (lang, html.escape(title), css, inline(title), version(), '\n'.join(toc), body, version()))


def to_pdf(html_path, pdf_path):
    browser = next((b for b in BROWSERS if os.path.exists(b)), None)
    if not browser:
        print('  no Chrome/Edge found - PDF skipped')
        return False
    url = 'file:///' + html_path.replace('\\', '/')
    r = subprocess.run([browser, '--headless=new', '--disable-gpu', '--no-pdf-header-footer',
                        '--print-to-pdf=' + pdf_path, url], capture_output=True, text=True, timeout=180)
    return r.returncode == 0 and os.path.exists(pdf_path)


def main(argv):
    html_only = '--html' in argv
    only = argv[argv.index('--lang') + 1] if '--lang' in argv else None
    css = open(os.path.join(SRC, 'style.css'), encoding='utf-8').read()
    for lang in sorted(os.listdir(SRC)):
        src_dir = os.path.join(SRC, lang)
        if not os.path.isdir(src_dir) or lang == 'images' or (only and lang != only):
            continue
        out_dir = os.path.join(OUT, lang)
        os.makedirs(out_dir, exist_ok=True)
        # the images next to the manual, as the Markdown references them (../images/...)
        images_out = os.path.join(OUT, 'images')
        if os.path.isdir(os.path.join(SRC, 'images')):
            shutil.copytree(os.path.join(SRC, 'images'), images_out, dirs_exist_ok=True)
        for name in sorted(os.listdir(src_dir)):
            if not name.endswith('.md'):
                continue
            md = open(os.path.join(src_dir, name), encoding='utf-8').read()
            body, title, headings = convert(md)
            stem = name[:-3]
            html_path = os.path.join(out_dir, stem + '.html')
            open(html_path, 'w', encoding='utf-8').write(page(body, title, headings, lang, css))
            print('%s/%s.html' % (lang, stem))
            if not html_only:
                pdf_path = os.path.join(out_dir, stem + '.pdf')
                if to_pdf(html_path, pdf_path):
                    print('%s/%s.pdf (%d KB)' % (lang, stem, os.path.getsize(pdf_path) // 1024))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
