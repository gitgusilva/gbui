// The documentation theme: VitePress's own, plus the components the pages use.
//
// Registered globally rather than imported per page, because a Markdown page
// cannot import anything — a component used in Markdown has to already exist by
// the time the page is rendered.
import DefaultTheme from 'vitepress/theme'
import type { Theme } from 'vitepress'

import GbuiDemo from './components/GbuiDemo.vue'
import './custom.css'

export default {
  extends: DefaultTheme,
  enhanceApp({ app }) {
    app.component('GbuiDemo', GbuiDemo)
  },
} satisfies Theme
