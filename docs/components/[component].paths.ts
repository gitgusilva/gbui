// One route per component page, with its prose written from the metadata.
//
// The page bodies are Markdown rather than a Vue component reading the same
// JSON at runtime, because a signature nobody can grep for is a signature
// nobody finds: this way the summaries, signatures and option tables are in the
// built HTML and in the search index. The Vue component on each page is only
// the thing Markdown cannot be — a running example.
//
// ---- the shape of a page ----------------------------------------------------
//
// Import, then Example, then API, in that order and under those headings. It is
// the order a reader works in — *what do I include, what does it look like, what
// can I set* — and it is the order every component-library site converged on, so
// a reader who has used one of the others already knows where to look. The
// headings also give the "on this page" rail something to be: before them the
// rail had one entry per options struct and nothing else.
import { label, pages, type Component } from '../.vitepress/componentPages'

/** Markdown tables end a cell at `|`, and a doc comment is allowed to use one. */
function cell(text: string) {
  return text.replace(/\|/g, '\\|').trim()
}

function facts(component: Component) {
  const flags = [component.group]
  if (component.container) flags.push('container')
  if (component.interactive) flags.push('interactive')
  return flags.join(' · ')
}

function options(component: Component, level: string) {
  if (!component.properties.length) return []
  const lines = [
    `${level} ${component.optionsType}`,
    '',
    '| Option | Type | Default | What it does |',
    '| --- | --- | --- | --- |',
  ]
  for (const property of component.properties) {
    const name = `\`${property.name}\`` + (property.optional ? ' *optional*' : '')
    const choices = property.choices.length ? `<br>${cell(property.choices.join(' · '))}` : ''
    const fallback = property.default ? `\`${cell(property.default)}\`` : ''
    lines.push(`| ${name} | \`${cell(property.type)}\`${choices} | ${fallback} | ${cell(property.doc)} |`)
  }
  lines.push('')
  return lines
}

/** The running example, its signatures and whatever the header says about it. */
function example(component: Component, alone: boolean) {
  const lines: string[] = []
  if (!alone) {
    // Capitalised as a heading and spelt as code beneath it: the reader is
    // looking for the component here and for the call site there.
    lines.push(
      `### ${label(component.name)}`,
      '',
      `<span class="gbui-facts"><code>${component.name}</code> · ${facts(component)}</span>`,
      '',
    )
  }
  // Every component runs under its own heading, including the only one on a
  // page of its own — a page that shows two of three has decided for the reader
  // which two are worth looking at.
  lines.push(`<GbuiComponent name="${component.name}" />`, '')
  for (const signature of component.signatures) {
    lines.push('```cpp', signature, '```', '')
  }
  for (const note of component.notes) lines.push(note, '')
  return lines
}

export default {
  paths() {
    return pages.map((page) => {
      const alone = page.components.length === 1
      const first = page.components[0]
      // Usually one, but a page can gather components from more than one
      // header — the pickers do — and naming only the first would send a
      // reader to include a file that does not declare what they came for.
      const headers = [...new Set(page.components.map((component) => component.header))]

      const lines = [
        `# ${page.label}`,
        '',
        `<span class="gbui-facts">${alone ? `<code>${first.name}</code> · ` : ''}${facts(first)}</span>`,
        '',
        first.summary,
        '',
        '## Import',
        '',
        '```cpp',
        ...headers.map((header) => `#include "${header}"`),
        '```',
        '',
        '## Example',
        '',
      ]
      for (const component of page.components) lines.push(...example(component, alone))

      // One API section for the page, whatever it holds: a reader looking for
      // an option knows it is at the bottom, and a page carrying five
      // components does not make them hunt through five of these.
      const api: string[] = []
      for (const component of page.components) {
        api.push(...options(component, '###'))
      }
      if (api.length) lines.push('## API', '', ...api)

      return {
        params: {
          component: page.slug,
          title: page.label,
          description: first.summary,
        },
        content: lines.join('\n'),
      }
    })
  },
}
