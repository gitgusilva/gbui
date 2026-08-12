// What each demo screen *is*, read out of the C++ at build time.
//
// The `DemoInfo` each `demos/src/*.cpp` returns is already the catalogue —
// title, sector, summary, highlights, and the sentence telling a reader what to
// try — so none of it is copied into the documentation by hand. A summary
// edited in one place and shown in another is a summary that will be wrong
// within two releases.
//
// The *source* of each screen is next door in `demoCode.data.ts`, and split
// from this on purpose: this payload is a few hundred bytes and rides in the
// shared theme chunk, where it costs every page on the site. Six highlighted
// C++ files are a hundred kilobytes and must not.
//
// This is a VitePress data loader: it runs in Node during the build, and the
// client gets a plain JSON payload.
import { readFileSync } from 'node:fs'
import { dirname, resolve } from 'node:path'
import { fileURLToPath } from 'node:url'

import { defineLoader } from 'vitepress'

const here = dirname(fileURLToPath(import.meta.url))
const demosDir = resolve(here, '../../../demos')

/** The order the gallery shows them in — the same one `registry.cpp` uses. */
const ORDER = ['analytics', 'weather', 'scada', 'production', 'grid', 'logistics'] as const

export interface DemoEntry {
  id: string
  title: string
  sector: string
  summary: string
  highlights: string[]
  tryThis: string
  /** Repository-relative, for the "read the source" link. */
  path: string
  lines: number
}

export declare const data: DemoEntry[]

/**
 * Joins a run of adjacent C++ string literals into one JavaScript string.
 *
 * `"a " "b"` is one literal to a compiler and two tokens to a regular
 * expression, and every `summary` in these files is wrapped across lines that
 * way. Escapes are left as the file wrote them apart from the two that appear:
 * a quote and a backslash.
 */
function joinLiterals(raw: string): string {
  const parts = raw.match(/"(?:[^"\\]|\\.)*"/g) ?? []
  return parts
    .map((part) => part.slice(1, -1).replace(/\\(["\\])/g, '$1'))
    .join('')
}

/** The value of `.field = "…"` inside one `DemoInfo` literal. */
function stringField(block: string, field: string): string {
  const match = block.match(new RegExp(`\\.${field}\\s*=\\s*((?:\\s*"(?:[^"\\\\]|\\\\.)*")+)`))
  if (!match) throw new Error(`demos.data: no .${field} in the DemoInfo block`)
  return joinLiterals(match[1])
}

/** The value of `.highlights = {"a", "b"}`. */
function listField(block: string, field: string): string[] {
  const match = block.match(new RegExp(`\\.${field}\\s*=\\s*\\{([^}]*)\\}`))
  if (!match) return []
  return (match[1].match(/"(?:[^"\\]|\\.)*"/g) ?? []).map((part) =>
    part.slice(1, -1).replace(/\\(["\\])/g, '$1'),
  )
}

export default defineLoader({
  // Rebuild the page whenever a screen changes, which is what makes the code
  // view impossible to leave stale.
  watch: ['../../../demos/src/*.cpp', '../../../demos/include/gbui_demos/*.hpp'],

  async load(): Promise<DemoEntry[]> {
    return ORDER.map((id) => {
      const path = `demos/src/${id}.cpp`
      const source = readFileSync(resolve(demosDir, `src/${id}.cpp`), 'utf-8')

      // The `DemoInfo <name>() { return {...}; }` at the bottom of each file.
      const start = source.indexOf('DemoInfo ')
      if (start < 0) throw new Error(`demos.data: ${path} publishes no DemoInfo`)
      const block = source.slice(start)

      return {
        id,
        title: stringField(block, 'title'),
        sector: stringField(block, 'sector'),
        summary: stringField(block, 'summary'),
        highlights: listField(block, 'highlights'),
        tryThis: stringField(block, 'tryThis'),
        path,
        lines: source.split('\n').length,
      }
    })
  },
})
