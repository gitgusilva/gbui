<script setup lang="ts">
// A live demo screen, running in the page.
//
// The component owns the frame around it — the picker, the skin menu, the
// state while the module downloads — and nothing about the demo itself: that is
// `demos/web/gbui-embed.js`, which knows how to put one on a canvas and is
// framework-free so that anything else can use it too.
//
// The WebAssembly module is a build artefact and is not committed, so the
// component has to behave when it is absent: `npm run docs:dev` on a machine
// with no Emscripten shows the placeholder below and every other page works.
import { computed, onBeforeUnmount, onMounted, ref, shallowRef, watch } from 'vue'
import { useData, withBase } from 'vitepress'

const props = withDefaults(
  defineProps<{
    /** Which screen to open on. Empty takes the first in the catalogue. */
    id?: string
    /** Height of the canvas, in CSS pixels. */
    height?: number
    /** Show the strip of screens along the top. */
    picker?: boolean
    /** Show the skin menu and the restart button. */
    controls?: boolean
  }>(),
  { id: '', height: 620, picker: false, controls: true },
)

const { isDark } = useData()

type Entry = { id: string; title: string; sector: string; summary: string; palette: string }
type Skin = { id: string; name: string }

const canvas = ref<HTMLCanvasElement | null>(null)
const demo = shallowRef<any>(null)
const entries = ref<Entry[]>([])
const skins = ref<Skin[]>([])
const current = ref(props.id)
const skin = ref('gitbox')
const status = ref<'loading' | 'ready' | 'missing' | 'error'>('loading')
const message = ref('')

const selected = computed(() => entries.value.find((entry) => entry.id === current.value))

onMounted(async () => {
  try {
    const module = await import(/* @vite-ignore */ withBase('/demo/gbui-embed.js'))
    const [catalogue, available] = await Promise.all([module.catalogue(), module.skins()])
    entries.value = catalogue
    skins.value = available
    if (!current.value && catalogue.length) current.value = catalogue[0].id

    demo.value = await module.mountDemo(canvas.value, {
      id: current.value,
      dark: isDark.value,
      skin: skin.value,
    })
    status.value = 'ready'
  } catch (error) {
    // A 404 on the module is the ordinary case — the artefact was never built
    // here — and is worth a different message from a module that loaded and
    // then threw.
    const text = String((error as Error)?.message ?? error)
    status.value = /Failed to fetch|Importing a module script failed|404/.test(text)
      ? 'missing'
      : 'error'
    message.value = text
  }
})

onBeforeUnmount(() => demo.value?.destroy())

watch(isDark, (dark) => demo.value?.setDark(dark))

function choose(id: string) {
  if (id === current.value) return
  current.value = id
  demo.value?.select(id)
}

function chooseSkin(event: Event) {
  skin.value = (event.target as HTMLSelectElement).value
  demo.value?.setSkin(skin.value)
}
</script>

<template>
  <div class="gbui-demo" :class="`is-${status}`">
    <div v-if="picker && entries.length" class="gbui-demo-tabs" role="tablist">
      <button
        v-for="entry in entries"
        :key="entry.id"
        class="gbui-demo-tab"
        :class="{ active: entry.id === current }"
        role="tab"
        :aria-selected="entry.id === current"
        @click="choose(entry.id)"
      >
        {{ entry.title }}
      </button>
    </div>

    <div v-if="controls && status === 'ready'" class="gbui-demo-bar">
      <span class="gbui-demo-sector">{{ selected?.sector }}</span>
      <span class="gbui-demo-spacer" />
      <label class="gbui-demo-field">
        <span>Design</span>
        <select :value="skin" @change="chooseSkin">
          <option v-for="option in skins" :key="option.id" :value="option.id">
            {{ option.name }}
          </option>
        </select>
      </label>
      <button class="gbui-demo-action" @click="demo?.restart()">Restart</button>
    </div>

    <div class="gbui-demo-stage" :style="{ height: `${height}px` }">
      <canvas
        ref="canvas"
        class="gbui-demo-canvas"
        :aria-label="selected ? `${selected.title} — a live gbui demo` : 'a live gbui demo'"
      />

      <div v-if="status !== 'ready'" class="gbui-demo-overlay">
        <template v-if="status === 'loading'">
          <strong>Starting the toolkit…</strong>
          <p>Downloading the WebAssembly module and its fonts.</p>
        </template>
        <template v-else-if="status === 'missing'">
          <strong>The demo module is not built in this checkout.</strong>
          <p>
            It is a build artefact rather than a committed file. Build it with the Emscripten
            SDK on the path:
          </p>
          <pre><code>tools/build_wasm.sh</code></pre>
          <p class="gbui-demo-quiet">
            The published documentation builds it in CI, so this only ever appears locally.
          </p>
        </template>
        <template v-else>
          <strong>The demo failed to start.</strong>
          <pre><code>{{ message }}</code></pre>
        </template>
      </div>
    </div>

    <p v-if="status === 'ready'" class="gbui-demo-hint">
      This is C++ compiled to WebAssembly, rasterised on the CPU and copied into a canvas —
      no DOM, no WebGL. Click a row, drag a slider, pan a chart. Click into it first to give it
      the wheel and the keyboard; <kbd>Esc</kbd> hands them back to the page.
    </p>
  </div>
</template>

<style scoped>
.gbui-demo {
  margin: 24px 0;
}

.gbui-demo-tabs {
  display: flex;
  flex-wrap: wrap;
  gap: 4px;
  margin-bottom: 10px;
}

.gbui-demo-tab {
  border: 1px solid var(--vp-c-divider);
  border-radius: 6px;
  padding: 5px 11px;
  font-size: 13px;
  line-height: 1.4;
  color: var(--vp-c-text-2);
  background: transparent;
  transition: color 0.2s, border-color 0.2s, background-color 0.2s;
}

.gbui-demo-tab:hover {
  color: var(--vp-c-text-1);
  border-color: var(--vp-c-brand-1);
}

.gbui-demo-tab.active {
  color: var(--vp-c-brand-1);
  border-color: var(--vp-c-brand-1);
  background: var(--vp-c-brand-soft);
}

.gbui-demo-bar {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 0 2px 8px;
  font-size: 12px;
  color: var(--vp-c-text-2);
}

.gbui-demo-spacer {
  flex: 1;
}

.gbui-demo-field {
  display: inline-flex;
  align-items: center;
  gap: 6px;
}

.gbui-demo-field select,
.gbui-demo-action {
  border: 1px solid var(--vp-c-divider);
  border-radius: 6px;
  padding: 3px 8px;
  font-size: 12px;
  color: var(--vp-c-text-1);
  background: var(--vp-c-bg-soft);
}

.gbui-demo-action:hover {
  border-color: var(--vp-c-brand-1);
  color: var(--vp-c-brand-1);
}

.gbui-demo-stage {
  position: relative;
  border: 1px solid var(--vp-c-divider);
  border-radius: 10px;
  /* Sideways rather than squashed: these are dense screens designed at desk
     widths, and a phone showing a scrollable slice of one is a truer picture
     than a phone showing all of it at a quarter size. */
  overflow-x: auto;
  overflow-y: hidden;
  background: var(--vp-c-bg-alt);
}

.gbui-demo-canvas {
  display: block;
  width: 100%;
  min-width: 900px;
  height: 100%;
  outline: none;
}

.gbui-demo-canvas:focus-visible {
  outline: 2px solid var(--vp-c-brand-1);
  outline-offset: -2px;
}

.gbui-demo-overlay {
  position: absolute;
  inset: 0;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 4px;
  padding: 24px;
  text-align: center;
  background: var(--vp-c-bg-alt);
  color: var(--vp-c-text-2);
  font-size: 14px;
}

.gbui-demo-overlay strong {
  color: var(--vp-c-text-1);
}

.gbui-demo-overlay p {
  margin: 0;
  max-width: 46ch;
}

.gbui-demo-overlay pre {
  margin: 8px 0 0;
  padding: 8px 12px;
  border-radius: 6px;
  background: var(--vp-c-bg-soft);
  font-size: 12px;
  max-width: 100%;
  overflow-x: auto;
}

.gbui-demo-quiet {
  font-size: 12px;
  opacity: 0.75;
}

.gbui-demo-hint {
  max-width: 760px;
  margin: 10px 2px 0;
  font-size: 12.5px;
  line-height: 1.6;
  color: var(--vp-c-text-2);
}

.gbui-demo-hint kbd {
  border: 1px solid var(--vp-c-divider);
  border-radius: 4px;
  padding: 0 4px;
  font-size: 11px;
}
</style>
