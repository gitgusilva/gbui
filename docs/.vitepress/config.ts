import { defineConfig } from 'vitepress'
import { sidebar as componentSidebar } from './componentPages'
import { current, isArchived, versionLink, versionMenu } from './versions'

// Each version is built with its own base path and deployed to its own
// directory; the newest is copied to the root. See versions.ts.
const version = process.env.GBUI_DOCS_VERSION ?? current
const base = process.env.GBUI_DOCS_BASE ?? '/'

/**
 * The same sidebar on every route.
 *
 * VitePress keys sidebars by path prefix and falls back to no sidebar at all
 * for a path no key matches, so "one sidebar everywhere" is spelt as one entry
 * per top-level path rather than as a bare array — a bare array works, but the
 * landing page then gets a sidebar too, which is the one page that should be
 * the hero and nothing else.
 */
function everywhere(items: unknown[]) {
  return {
    '/guide/': items,
    '/reference/': items,
    '/components': items,
    '/demos': items,
  } as never
}

// The documentation site. Content lives in Markdown next to this file; the
// sidebar is written by hand rather than generated, because the order a reader
// needs is not the order a directory listing gives.
export default defineConfig({
  title: 'gbui',
  description:
    'A retained-mode C++ UI toolkit: flexbox layout, themeable components, and a painter you can implement.',
  lang: 'en-US',
  cleanUrls: true,
  lastUpdated: true,
  base,

  head: [['meta', { name: 'theme-color', content: '#2563eb' }]],

  themeConfig: {
    // ---- one bar, one tree -------------------------------------------------
    //
    // The navbar used to carry Guide, Reference, Components and Demos, and each
    // of them swapped the sidebar underneath for a different one. That makes
    // the left column mean something different depending on where you already
    // are, so a reader who wants a component page from the middle of the guide
    // has to go up to the navbar, across, and back down — and the sidebar they
    // were reading disappears on the way.
    //
    // Everything is one tree now, the way PrimeVue and most large component
    // sites do it: the whole map is on screen wherever you are, and the navbar
    // keeps only what is not navigation — the version, the search, the source.
    // Not navigation, which now lives in the sidebar — a destination. The
    // download page is a screen of its own with no sidebar at all, so this is
    // the only way to it that is always on screen.
    nav: [{ text: 'Download', link: '/download' }, versionMenu()],

    sidebar: everywhere([
      {
        text: 'Getting started',
        items: [
          { text: 'Introduction', link: '/guide/introduction' },
          { text: 'Installation', link: '/guide/installation' },
          { text: 'Download a build', link: '/download' },
          { text: 'Your first window', link: '/guide/first-window' },
        ],
      },
      {
        text: 'Concepts',
        collapsed: false,
        items: [
          { text: 'Architecture', link: '/guide/architecture' },
          { text: 'Building a tree', link: '/guide/building-a-tree' },
          { text: 'Layout', link: '/guide/layout' },
          { text: 'Theming', link: '/guide/theming' },
          { text: 'Input and focus', link: '/guide/input' },
          { text: 'Motion', link: '/guide/motion' },
          { text: 'Memory', link: '/guide/memory' },
        ],
      },
      {
        text: 'Going further',
        collapsed: true,
        items: [
          { text: 'Writing a component', link: '/guide/writing-a-component' },
          { text: 'Icons', link: '/guide/icons' },
          { text: 'Writing a backend', link: '/guide/writing-a-backend' },
          { text: 'Testing', link: '/guide/testing' },
        ],
      },
      {
        text: 'Demos',
        collapsed: true,
        items: [{ text: 'The screens', link: '/demos' }],
      },
      // Every component is a route and the sidebar is the list of them; both
      // come from the generated metadata. See .vitepress/componentPages.ts.
      ...componentSidebar(),
      {
        text: 'Reference',
        collapsed: true,
        items: [
          { text: 'Overview', link: '/reference/overview' },
          { text: 'core', link: '/reference/core' },
          { text: 'style', link: '/reference/style' },
          { text: 'scene', link: '/reference/scene' },
          { text: 'layout', link: '/reference/layout' },
          { text: 'input', link: '/reference/input' },
          { text: 'a11y', link: '/reference/accessibility' },
          { text: 'anim', link: '/reference/anim' },
          { text: 'overlay', link: '/reference/overlay' },
          { text: 'paint', link: '/reference/paint' },
          { text: 'widgets', link: '/reference/widgets' },
          { text: 'charts', link: '/reference/charts' },
          { text: 'platform', link: '/reference/platform' },
        ],
      },
    ]),

    socialLinks: [{ icon: 'github', link: 'https://github.com/gitgusilva/gbui' }],

    search: { provider: 'local' },

    editLink: {
      pattern: 'https://github.com/gitgusilva/gbui/edit/main/docs/:path',
      text: 'Edit this page',
    },

    footer: {
      // Raw HTML, so this link is not rewritten with the base the way a nav
      // link is — which is exactly why it goes through versionLink().
      message: isArchived
        ? `Documentation for v${version} — <a href="${versionLink()}">the latest release is v${current}</a>.`
        : 'Released under the LGPL-3.0-or-later licence.',
      copyright: 'Icons by Lucide (ISC).',
    },
  },
})
