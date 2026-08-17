// The documentation theme: VitePress's own, plus the components the pages use.
//
// Registered globally rather than imported per page, because a Markdown page
// cannot import anything — a component used in Markdown has to already exist by
// the time the page is rendered.
import DefaultTheme from 'vitepress/theme'
import type { Theme } from 'vitepress'

import GbuiCode from './components/GbuiCode.vue'
import GbuiComponent from './components/GbuiComponent.vue'
import GbuiComponentIndex from './components/GbuiComponentIndex.vue'
import GbuiDemo from './components/GbuiDemo.vue'
import GbuiBand from './components/GbuiBand.vue'
import GbuiDownloads from './components/GbuiDownloads.vue'
import GbuiShowcase from './components/GbuiShowcase.vue'
import './custom.css'

export default {
  extends: DefaultTheme,
  enhanceApp({ app }) {
    app.component('GbuiDemo', GbuiDemo)
    app.component('GbuiCode', GbuiCode)
    app.component('GbuiComponent', GbuiComponent)
    app.component('GbuiComponentIndex', GbuiComponentIndex)
    app.component('GbuiDownloads', GbuiDownloads)
    app.component('GbuiShowcase', GbuiShowcase)
    app.component('GbuiBand', GbuiBand)
  },
} satisfies Theme
