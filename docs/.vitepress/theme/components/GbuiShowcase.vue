<script setup lang="ts">
// The landing page's running screen: no Run button, no code tab, no waiting.
//
// Everywhere else on this site a demo opens on its source and downloads nothing
// until the reader asks — the right default for a page whose job is to teach,
// and the wrong one for the page whose job is to show that any of this works.
// A visitor who has to press a button to find out what a UI toolkit draws has
// been asked to take it on trust.
//
// It still does not download on load. The module starts when this scrolls into
// view, which on a landing page is immediately and on a phone that never gets
// there is never — and `mountDemo`'s own `autoPause` stops the loop again the
// moment it leaves. The cost is paid by the people looking at it.
import { onBeforeUnmount, onMounted, ref, shallowRef, watch } from 'vue'
import { useData, withBase } from 'vitepress'

import { data as catalogue } from '../demos.data'

const props = withDefaults(defineProps<{ height?: number }>(), { height: 560 })

const { isDark } = useData()

/** Four screens rather than seven: a strip that wraps is a strip nobody reads,
 *  and the rest are one link away. */
const screens = catalogue.demos.slice(0, 4)

const SKINS = [
  { id: 'gitbox', name: 'GitBox' },
  { id: 'material', name: 'Material' },
  { id: 'cupertino', name: 'Cupertino' },
  { id: 'fluent', name: 'Fluent' },
]

const host = ref<HTMLElement | null>(null)
const canvas = ref<HTMLCanvasElement | null>(null)
const demo = shallowRef<any>(null)
const current = ref(screens[0].id)
const skin = ref('gitbox')
const status = ref<'idle' | 'loading' | 'ready' | 'missing' | 'error'>('idle')
const message = ref('')

async function start() {
  if (status.value !== 'idle') return
  status.value = 'loading'
  try {
    const module = await import(/* @vite-ignore */ withBase('/demo/gbui-embed.js'))
    demo.value = await module.mountDemo(canvas.value, {
      id: current.value,
      dark: isDark.value,
      skin: skin.value,
    })
    status.value = 'ready'
  } catch (error) {
    const text = String((error as Error)?.message ?? error)
    status.value = /Failed to fetch|Importing a module script failed|404/.test(text)
      ? 'missing'
      : 'error'
    message.value = text
  }
}

function show(id: string) {
  current.value = id
  demo.value?.select(id)
}

function wear(id: string) {
  skin.value = id
  demo.value?.setSkin(id)
}

let observer: IntersectionObserver | null = null

onMounted(() => {
  if (typeof IntersectionObserver === 'undefined') {
    void start()
    return
  }
  observer = new IntersectionObserver(
    (entries) => {
      if (entries.some((entry) => entry.isIntersecting)) {
        observer?.disconnect()
        observer = null
        void start()
      }
    },
    { rootMargin: '200px' },
  )
  if (host.value) observer.observe(host.value)
})

watch(isDark, (dark) => demo.value?.setDark(dark))
onBeforeUnmount(() => {
  observer?.disconnect()
  demo.value?.destroy()
})
</script>

<template>
  <div ref="host" class="gbui-showcase">
    <div class="gbui-showcase-bar">
      <div class="gbui-showcase-tabs" role="tablist" aria-label="Demo screens">
        <button
          v-for="screen in screens"
          :key="screen.id"
          role="tab"
          :aria-selected="current === screen.id"
          :class="['gbui-showcase-tab', { on: current === screen.id }]"
          @click="show(screen.id)"
        >
          {{ screen.title }}
        </button>
      </div>
      <div class="gbui-showcase-skins" role="group" aria-label="Design system">
        <button
          v-for="entry in SKINS"
          :key="entry.id"
          :class="['gbui-showcase-skin', { on: skin === entry.id }]"
          @click="wear(entry.id)"
        >
          {{ entry.name }}
        </button>
      </div>
    </div>

    <div class="gbui-showcase-stage" :style="{ height: `${props.height}px` }">
      <canvas ref="canvas" class="gbui-showcase-canvas" :aria-label="`${current}, running`" />
      <div v-if="status !== 'ready'" class="gbui-showcase-veil">
        <template v-if="status === 'missing'">
          <strong>The demo bundle is not built in this checkout.</strong>
          <pre><code>tools/build_wasm.sh</code></pre>
        </template>
        <template v-else-if="status === 'error'">
          <strong>It failed to start.</strong>
          <pre><code>{{ message }}</code></pre>
        </template>
        <template v-else>
          <span class="gbui-showcase-spinner" aria-hidden="true" />
          <p>Compiling nothing — this is C++, already compiled.</p>
        </template>
      </div>
    </div>

    <p class="gbui-showcase-caption">
      A real application screen, drawn by the same source that builds a desktop
      binary. There is no DOM inside that rectangle, no elements and no CSS —
      what you are pointing at is a display list, rasterised on the CPU.
      Switch the design system above and watch it re-shape without a rebuild.
    </p>
  </div>
</template>

<style scoped>
.gbui-showcase {
  margin: 8px 0 40px;
}

.gbui-showcase-bar {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  justify-content: space-between;
  gap: 10px;
  margin-bottom: 10px;
}

.gbui-showcase-tabs,
.gbui-showcase-skins {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
}

.gbui-showcase-tab,
.gbui-showcase-skin {
  border: 1px solid var(--vp-c-divider);
  border-radius: 999px;
  padding: 5px 13px;
  font-size: 12.5px;
  font-weight: 500;
  color: var(--vp-c-text-2);
  background: var(--vp-c-bg-soft);
  cursor: pointer;
  transition: color 0.2s, border-color 0.2s, background-color 0.2s;
}

.gbui-showcase-tab:hover,
.gbui-showcase-skin:hover {
  color: var(--vp-c-text-1);
  border-color: var(--vp-c-brand-1);
}

.gbui-showcase-tab.on,
.gbui-showcase-skin.on {
  color: var(--vp-c-brand-1);
  border-color: var(--vp-c-brand-1);
  background: var(--vp-c-brand-soft);
}

.gbui-showcase-skin {
  font-size: 11.5px;
  padding: 4px 10px;
}

.gbui-showcase-stage {
  position: relative;
  border: 1px solid var(--vp-c-divider);
  border-radius: 12px;
  overflow: hidden;
  background: var(--vp-c-bg-alt);
}

.gbui-showcase-canvas {
  display: block;
  width: 100%;
  height: 100%;
  outline: none;
}

.gbui-showcase-veil {
  position: absolute;
  inset: 0;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 12px;
  padding: 20px;
  text-align: center;
  font-size: 13px;
  color: var(--vp-c-text-2);
  background: var(--vp-c-bg-alt);
}

.gbui-showcase-veil p {
  margin: 0;
}

.gbui-showcase-spinner {
  width: 26px;
  height: 26px;
  border: 2px solid var(--vp-c-divider);
  border-top-color: var(--vp-c-brand-1);
  border-radius: 50%;
  animation: gbui-showcase-spin 0.8s linear infinite;
}

@keyframes gbui-showcase-spin {
  to { transform: rotate(360deg); }
}

/* WCAG 2.2.2 — a reader who has asked for no motion gets a dot, not a wheel. */
@media (prefers-reduced-motion: reduce) {
  .gbui-showcase-spinner {
    animation: none;
    border-top-color: var(--vp-c-brand-1);
  }
}

.gbui-showcase-caption {
  margin: 12px 0 0;
  font-size: 13.5px;
  line-height: 1.65;
  color: var(--vp-c-text-2);
}
</style>
