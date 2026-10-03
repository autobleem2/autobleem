# Translating AutoBleem

The strings are in `src/resources/lang/<Language>.txt`, one per line:

```
# AutoBleem Polski Translation
# Format: English Text=Translated Text
# Untranslated: 2 / Total: 196

About=O programie
Apps=Aplikacje
Core:=
```

The English text is the key - exactly as it appears in the source, `_("...")` - and the translation follows
the first `=`. An empty value (`Core:=`) means "not translated yet": the app shows the English. A line
starting with `#` is a comment; the header's "Untranslated" count is rewritten by the tool. Keys sort
alphabetically. `|@lang|` is a key like any other - its value is the language code pcsx-ab is started with.
Markers such as `|@X|` or `|@Start|` in a string are replaced by button pictures and must be kept as they are.

`tools/lang_tools.py` keeps the files in step with the source:

```
python tools/lang_tools.py extract          # English.txt from every _("...") in src/code and autobleem-core
python tools/lang_tools.py update           # gives every language English's keys (new ones empty)
python tools/lang_tools.py compare Polski   # lists what Polski still lacks
python tools/lang_tools.py validate         # what make_win.sh runs: format, duplicates, keys not in English
```

To add a language, copy `English.txt` to `<Language>.txt`, translate the values, and it appears in Options
under that name. The file is UTF-8; a font that lacks the language's glyphs shows boxes (the theme's font is
the one drawing the classic screens - `classic.font` in its `theme.json`). Chinese is the exception: for a
language whose name contains `Chinese`, every screen is drawn with the bundled `fonts/NotoSansSC-Regular.otf`
instead of the theme's fonts, since no theme font carries the CJK glyphs.

Before 2026-09 the files were pairs of lines (the English, then the translation); such a file is still read.
`python tools/lang_tools.py convert FILE` turns it into the layout above.
