// One route per component page, with its prose written from the metadata.
//
// The page bodies are Markdown rather than a Vue component reading the same
// JSON at runtime, because a signature nobody can grep for is a signature
// nobody finds: this way the summaries, signatures and option tables are in the
// built HTML and in the search index. The Vue component on each page is only
// the thing Markdown cannot be — a running example.
import { pages, type Component } from '../.vitepress/componentPages'

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

function body(component: Component, alone: boolean) {
  const lines: string[] = []
  if (!alone) {
    lines.push(`## ${component.name}`, '', `<span class="gbui-facts">${facts(component)}</span>`, '')
  }
  // Every component runs under its own heading, including the only one on a
  // page of its own — a page that shows two of three is a page that has
  // decided for the reader which two are worth looking at.
  lines.push(`<GbuiComponent name="${component.name}" />`, '')
  for (const signature of component.signatures) {
    lines.push('```cpp', signature, '```', '')
  }
  for (const note of component.notes) lines.push(note, '')
  lines.push(...options(component, alone ? '##' : '###'))
  return lines
}

export default {
  paths() {
    return pages.map((page) => {
      const alone = page.components.length === 1
      const first = page.components[0]
      const lines = [
        `# ${page.title}`,
        '',
        `<span class="gbui-facts">\`#include "${first.header}"\`${alone ? ` · ${facts(first)}` : ` · ${first.group}`}</span>`,
        '',
        first.summary,
        '',
      ]
      for (const component of page.components) lines.push(...body(component, alone))

      return {
        params: {
          component: page.slug,
          title: page.title,
          description: first.summary,
        },
        content: lines.join('\n'),
      }
    })
  },
}
