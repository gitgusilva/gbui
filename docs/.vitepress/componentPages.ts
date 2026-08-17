// One page per component, worked out from the generated metadata.
//
// The gallery used to be a single route with its own list down the left, which
// meant the site had a navigation tree and the components had another one. This
// turns the list into real routes: the sidebar is the list, every component has
// an address, and search finds it because there is a page to find.
//
// Components that are the same idea share a page, and the rule for "the same
// idea" is the header they are declared in — `colorField` and `colorPicker` are
// one file because they are one thing with and without its input, and a reader
// who lands on either wants both. The exception is `chart.hpp`, which is nine
// unrelated charts under one include; those get a page each.
import table from './theme/components.json'

export interface Property {
  name: string
  type: string
  default: string
  doc: string
  optional: boolean
  choices: string[]
}

export interface Component {
  name: string
  group: string
  header: string
  summary: string
  signatures: string[]
  notes: string[]
  optionsType: string
  container: boolean
  interactive: boolean
  properties: Property[]
}

export interface ComponentPage {
  /** The URL segment: `/components/color-picker`. */
  slug: string
  /** The identifier the page is named after: `colorPicker`, `pickers`. */
  title: string
  /** The same thing capitalised, for anywhere it is a label rather than code. */
  label: string
  group: string
  components: Component[]
}

export const all = table as Component[]

/** Headers whose components are a group rather than a component. */
const SPLIT = new Set(['chart.hpp'])

/**
 * Headers that are one idea across several files, and the page they share.
 *
 * The header rule below gets the common case right and this is where it does
 * not. A date, a time and a date-and-time are the same control with different
 * amounts of it, and a reader who lands on one wants to see the other two under
 * it rather than to find them in the sidebar — the same reasoning that already
 * puts `colorField` beside `colorPicker`, except that these three could not
 * share a file without breaking "one component, one header".
 *
 * The value is the page's slug *and* its title, so adding an entry is one line
 * and no other list has to learn about it.
 */
const TOGETHER: Record<string, string> = {
  'datePicker.hpp': 'pickers',
  'timePicker.hpp': 'pickers',
  'dateTimePicker.hpp': 'pickers',
}

function stem(header: string) {
  return header.slice(header.lastIndexOf('/') + 1)
}

/** `colorPicker` -> `color-picker`, which is what a URL should look like. */
function kebab(name: string) {
  return name.replace(/([a-z0-9])([A-Z])/g, '$1-$2').toLowerCase()
}

/**
 * `colorPicker` -> `ColorPicker`, which is what a *label* should look like.
 *
 * The identifier is `colorPicker` and stays that way everywhere it is code — in
 * the signatures, in the option tables, in the mono line above each running
 * example. This is only for the places the name is being *named*: the sidebar,
 * the index cards, the page heading. A column of sixty lowercase words is hard
 * to scan and reads like a list of variables rather than a set of components,
 * which is why every toolkit's gallery capitalises them.
 */
export function label(name: string) {
  return name.charAt(0).toUpperCase() + name.slice(1)
}

function build(): ComponentPage[] {
  const byKey = new Map<string, Component[]>()
  for (const component of all) {
    const file = stem(component.header)
    const key = SPLIT.has(file) ? component.name : TOGETHER[file] ?? file
    const bucket = byKey.get(key)
    if (bucket) bucket.push(component)
    else byKey.set(key, [component])
  }

  const named = new Set(Object.values(TOGETHER))
  const pages: ComponentPage[] = []
  for (const [key, components] of byKey) {
    // A page holding one component is named after it — `progressBar`, not
    // `progress`. A page holding several takes the header's name, which is the
    // only name the several of them share — unless `TOGETHER` gave the page a
    // name of its own, which is the point of naming it there.
    const title = named.has(key)
      ? key
      : components.length === 1
        ? components[0].name
        : key.replace(/\.hpp$/, '')
    // The one the page is named after comes first; the rest keep the
    // metadata's order, which is alphabetical.
    components.sort((a, b) => Number(b.name === title) - Number(a.name === title))
    pages.push({
      slug: kebab(title),
      title,
      label: label(title),
      group: components[0].group,
      components,
    })
  }
  return pages
}

export const pages: ComponentPage[] = build()

/** The groups in the order the metadata lists them, each with its pages. */
export function groups(): { text: string; pages: ComponentPage[] }[] {
  const order: string[] = []
  for (const page of pages) if (!order.includes(page.group)) order.push(page.group)
  return order.map((text) => ({ text, pages: pages.filter((page) => page.group === text) }))
}

/** The `/components` half of the site's sidebar. */
export function sidebar() {
  return [
    {
      text: 'Components',
      items: [
        { text: 'All of them', link: '/components' },
        { text: 'Writing a component', link: '/guide/writing-a-component' },
        { text: 'Demo screens', link: '/demos' },
      ],
    },
    ...groups().map((group) => ({
      text: group.text,
      collapsed: false,
      items: group.pages.map((page) => ({
        text: page.label,
        link: `/components/${page.slug}`,
      })),
    })),
  ]
}
