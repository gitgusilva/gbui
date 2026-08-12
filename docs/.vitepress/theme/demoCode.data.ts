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
import { readFileSync } from 'node:fs'
import { dirname, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

import { defineLoader } from 'vitepress'
import { createHighlighter } from 'shiki'

const here = dirname(fileURLToPath(import.meta.url))
const demosDir = resolve(here, '../../../demos')

const ORDER = ['analytics', 'weather', 'scada', 'production', 'grid', 'logistics'] as const

export declare const data: Record<string, string>

export default defineLoader({
  watch: ['../../../demos/src/*.cpp'],

  async load(): Promise<Record<string, string>> {
    const highlighter = await createHighlighter({
      themes: ['github-light', 'github-dark'],
      langs: ['cpp'],
    })

    const out: Record<string, string> = {}
    for (const id of ORDER) {
      const source = readFileSync(resolve(demosDir, `src/${id}.cpp`), 'utf-8')
      out[id] = highlighter.codeToHtml(source, {
        lang: 'cpp',
        themes: { light: 'github-light', dark: 'github-dark' },
        defaultColor: false,
      })
    }
    return out
  },
})
