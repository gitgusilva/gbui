import { defineConfig } from 'vitepress'
import { current, isArchived, versionLink, versionMenu } from './versions'

// Each version is built with its own base path and deployed to its own
// directory; the newest is copied to the root. See versions.ts.
const version = process.env.GBUI_DOCS_VERSION ?? current
const base = process.env.GBUI_DOCS_BASE ?? '/'

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
    nav: [
      { text: 'Guide', link: '/guide/introduction' },
      { text: 'Reference', link: '/reference/overview' },
      { text: 'Components', link: '/components' },
      { text: 'Demos', link: '/demos' },
      versionMenu(),
    ],

    sidebar: {
      '/components': [
        {
          text: 'Components',
          items: [
            { text: 'All of them', link: '/components' },
            { text: 'Writing a component', link: '/guide/writing-a-component' },
            { text: 'The six demo screens', link: '/demos' },
          ],
        },
      ],
      '/demos': [
        {
          text: 'Demos',
          items: [
            { text: 'The six screens', link: '/demos' },
            { text: 'Components', link: '/components' },
            { text: 'Your first window', link: '/guide/first-window' },
            { text: 'Writing a component', link: '/guide/writing-a-component' },
          ],
        },
      ],
      '/guide/': [
        {
          text: 'Getting started',
          items: [
            { text: 'Introduction', link: '/guide/introduction' },
            { text: 'Installation', link: '/guide/installation' },
            { text: 'Your first window', link: '/guide/first-window' },
          ],
        },
        {
          text: 'Concepts',
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
          items: [
            { text: 'Writing a component', link: '/guide/writing-a-component' },
            { text: 'Icons', link: '/guide/icons' },
            { text: 'Writing a backend', link: '/guide/writing-a-backend' },
            { text: 'Testing', link: '/guide/testing' },
          ],
        },
      ],
      '/reference/': [
        {
          text: 'Reference',
          items: [
            { text: 'Overview', link: '/reference/overview' },
            { text: 'core', link: '/reference/core' },
            { text: 'style', link: '/reference/style' },
            { text: 'scene', link: '/reference/scene' },
            { text: 'layout', link: '/reference/layout' },
            { text: 'input', link: '/reference/input' },
            { text: 'anim', link: '/reference/anim' },
            { text: 'overlay', link: '/reference/overlay' },
            { text: 'paint', link: '/reference/paint' },
            { text: 'widgets', link: '/reference/widgets' },
            { text: 'charts', link: '/reference/charts' },
            { text: 'platform', link: '/reference/platform' },
          ],
        },
      ],
    },

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
