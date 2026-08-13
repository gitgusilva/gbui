// The six screens' source, highlighted at build time.
//
// Split from `demos.data.ts` — which carries the few hundred bytes of metadata
// — because this is the heavy half and it must not ride in the shared theme
// chunk. The component imports it with a dynamic `import()`, so Vite gives it a
// chunk of its own that no other page on the site pays for.
//
// One block per file rather than two, and the dark theme travels inside it as
// CSS custom properties: Shiki's `defaultColor: false` writes both palettes
// into the same spans, and `custom.css` picks between them off the class
// VitePress puts on <html>. Two full copies would have doubled this for a
// difference nobody can see at once.
//
// The palettes are Visual Studio Code's own — a C++ reader should not have to
// learn a second set of colours to read a page about C++.
import { readFileSync } from 'node:fs'
import { dirname, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

import { defineLoader } from 'vitepress'
import { createHighlighter } from 'shiki'

const here = dirname(fileURLToPath(import.meta.url))
const demosDir = resolve(here, '../../../demos')

// The same order the gallery uses, and it has to *be* the same: a screen
// missing from this list has no source in the viewer at all, which is how the
// trading desk shipped with a Run button and nothing to read beside it.
const ORDER = ['markets', 'analytics', 'weather', 'scada', 'production', 'grid',
               'logistics'] as const

export declare const data: Record<string, string>

export default defineLoader({
  watch: ['../../../demos/src/*.cpp'],

  async load(): Promise<Record<string, string>> {
    // `dark-plus` and `light-plus` are Visual Studio Code's own defaults, so a
    // C++ reader sees the colours they already have in their editor: types,
    // functions, macros and namespaces each distinct rather than a wash of
    // three. GitHub's themes colour far less of C++ — most of a header comes
    // out one shade of foreground.
    const highlighter = await createHighlighter({
      themes: ['light-plus', 'dark-plus'],
      langs: ['cpp'],
    })

    const out: Record<string, string> = {}
    for (const id of ORDER) {
      const source = readFileSync(resolve(demosDir, `src/${id}.cpp`), 'utf-8')
      out[id] = highlighter.codeToHtml(source, {
        lang: 'cpp',
        themes: { light: 'light-plus', dark: 'dark-plus' },
        defaultColor: false,
        // Every line wrapped, so the viewer can put a number beside it without
        // a second pass over the HTML.
        transformers: [
          {
            line(node, line) {
              node.properties['data-line'] = String(line)
            },
          },
        ],
      })
    }
    return out
  },
})
