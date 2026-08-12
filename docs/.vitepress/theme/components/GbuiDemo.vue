<script setup lang="ts">
// A demo screen: its source, and — when the reader asks for it — the screen
// itself, running.
//
// **Nothing runs until it is asked to.** The WebAssembly module is 1.7 MB and
// then rasterises every frame on the CPU, which is a real cost on a phone and
// an unreasonable one to charge someone who came to read. So the page opens on
// the code, the reader presses Run, and only then is anything downloaded. That
// is also the more useful default for a documentation page: the source is what
// the page is teaching, and the running screen is the evidence.
//
// The frame around it is here; the screen itself is `demos/web/gbui-embed.js`,
// which is framework-free so anything else can drive it too. The catalogue and
// the highlighted source come from `demos.data.ts`, which reads both out of the
// C++ at build time — so neither can drift from the code it describes.
import { computed, nextTick, onBeforeUnmount, onMounted, ref, shallowRef, watch } from 'vue'
import { useData, withBase } from 'vitepress'

import { data as demos } from '../demos.data'

const props = withDefaults(
  defineProps<{
    /** Which screen to open on. Empty takes the first in the catalogue. */
    id?: string
    /** Height of the stage, in CSS pixels. */
    height?: number
    /** Start on the source or on the running screen. */
    mode?: 'code' | 'run'
  }>(),
  { id: '', height: 620, mode: 'code' },
)

const { isDark } = useData()

const canvas = ref<HTMLCanvasElement | null>(null)
const demo = shallowRef<any>(null)
const skins = ref<{ id: string; name: string }[]>([])
const current = ref(props.id || demos[0].id)
const skin = ref('gitbox')
const mode = ref<'code' | 'run'>(props.mode)
const status = ref<'idle' | 'loading' | 'ready' | 'missing' | 'error'>('idle')
const message = ref('')

const selected = computed(() => demos.find((entry) => entry.id === current.value) ?? demos[0])
const sourceUrl = computed(
  () => `https://github.com/gitgusilva/gbui/blob/main/${selected.value.path}`,
)

// The highlighted source is a hundred kilobytes and lives in a chunk of its
// own, fetched once the component is on the page rather than bundled into the
// theme every other page loads. Until it arrives the block shows the plain
// file, which is the honest placeholder: it is the same text, uncoloured.
const highlighted = shallowRef<Record<string, string> | null>(null)
const code = computed(() => highlighted.value?.[current.value] ?? '')

onMounted(async () => {
  const module = await import('../demoCode.data')
  highlighted.value = module.data
})

/** Downloads the module and mounts the selected screen. Only ever from Run. */
async function start() {
  mode.value = 'run'
  if (demo.value) {
    demo.value.select(current.value)
    return
  }
  if (status.value === 'loading') return

  status.value = 'loading'
  try {
    const module = await import(/* @vite-ignore */ withBase('/demo/gbui-embed.js'))
    skins.value = await module.skins()
    // The canvas is `v-show`n, so it exists — but it has just been unhidden and
    // its box is measured on mount. A tick, and it is the size it will be.
    await nextTick()
    demo.value = await module.mountDemo(canvas.value, {
      id: current.value,
      dark: isDark.value,
      skin: skin.value,
    })
    status.value = 'ready'
  } catch (error) {
    // A 404 on the module is the ordinary case — the bundle was never built in
    // this checkout — and deserves a different message from one that loaded and
    // then threw.
    const text = String((error as Error)?.message ?? error)
    status.value = /Failed to fetch|Importing a module script failed|404/.test(text)
      ? 'missing'
      : 'error'
    message.value = text
  }
}

function showCode() {
  mode.value = 'code'
  demo.value?.pause()
}

function choose(id: string) {
  current.value = id
  if (mode.value === 'run') demo.value?.select(id)
}

function chooseSkin(event: Event) {
  skin.value = (event.target as HTMLSelectElement).value
  demo.value?.setSkin(skin.value)
}

watch(isDark, (dark) => demo.value?.setDark(dark))
watch(mode, (next) => {
  if (next === 'run') demo.value?.resume()
})

onBeforeUnmount(() => demo.value?.destroy())
</script>

<template>
  <div class="gbui-demo">
    <div class="gbui-demo-bar">
      <label class="gbui-demo-field">
        <span class="gbui-demo-caption">Screen</span>
        <select
          :value="current"
          aria-label="Which demo screen"
          @change="choose(($event.target as HTMLSelectElement).value)"
        >
          <option v-for="entry in demos" :key="entry.id" :value="entry.id">
            {{ entry.title }} — {{ entry.sector }}
          </option>
        </select>
      </label>

      <span class="gbui-demo-spacer" />

      <template v-if="mode === 'run' && status === 'ready'">
        <label class="gbui-demo-field">
          <span class="gbui-demo-caption">Design</span>
          <select :value="skin" aria-label="Which design system" @change="chooseSkin">
            <option v-for="option in skins" :key="option.id" :value="option.id">
              {{ option.name }}
            </option>
          </select>
        </label>
        <button class="gbui-demo-action" @click="demo?.restart()">Restart</button>
      </template>

      <div class="gbui-demo-modes" role="group" aria-label="View">
        <button
          class="gbui-demo-mode"
          :class="{ active: mode === 'code' }"
          :aria-pressed="mode === 'code'"
          @click="showCode"
        >
          Code
        </button>
        <button
          class="gbui-demo-mode"
          :class="{ active: mode === 'run' }"
          :aria-pressed="mode === 'run'"
          @click="start"
        >
          ▶ Run
        </button>
      </div>
    </div>

    <!-- The source. Highlighted during the build, so this ships no highlighter. -->
    <div v-show="mode === 'code'" class="gbui-demo-code" :style="{ maxHeight: `${height}px` }">
      <div v-if="code" v-html="code" />
      <p v-else class="gbui-demo-quiet gbui-demo-waiting">Loading {{ selected.path }}…</p>
    </div>

    <div v-show="mode === 'run'" class="gbui-demo-stage" :style="{ height: `${height}px` }">
      <canvas ref="canvas" class="gbui-demo-canvas" :aria-label="`${selected.title}, running`" />

      <div v-if="status !== 'ready'" class="gbui-demo-overlay">
        <template v-if="status === 'loading'">
          <strong>Starting the toolkit…</strong>
          <p>Downloading the WebAssembly module and its fonts, about 1.7 MB.</p>
        </template>
        <template v-else-if="status === 'missing'">
          <strong>The demo bundle is not built in this checkout.</strong>
          <p>It is a build artefact rather than a committed file:</p>
          <pre><code>tools/build_wasm.sh</code></pre>
          <p class="gbui-demo-quiet">
            The published documentation builds it in CI, so this only appears locally.
          </p>
        </template>
        <template v-else-if="status === 'error'">
          <strong>The demo failed to start.</strong>
          <pre><code>{{ message }}</code></pre>
        </template>
      </div>
    </div>

    <!-- What the reader is looking at. Written beside the screen it describes,
         in demos/src/*.cpp, and read out of there at build time. -->
    <div class="gbui-demo-about">
      <div class="gbui-demo-about-main">
        <p class="gbui-demo-summary">{{ selected.summary }}</p>
        <p class="gbui-demo-try"><strong>Try:</strong> {{ selected.tryThis }}</p>
      </div>
      <div class="gbui-demo-about-side">
        <ul class="gbui-demo-chips">
          <li v-for="point in selected.highlights" :key="point">{{ point }}</li>
        </ul>
        <a class="gbui-demo-source" :href="sourceUrl" target="_blank" rel="noreferrer">
          {{ selected.path }} · {{ selected.lines }} lines
        </a>
      </div>
    </div>

    <p v-if="mode === 'run'" class="gbui-demo-hint">
      C++ compiled to WebAssembly, rasterised on the CPU and copied into a canvas — no DOM, no
      WebGL. Click into it first to give it the wheel and the keyboard;
      <kbd>Esc</kbd> hands them back to the page.
    </p>
  </div>
</template>

<style scoped>
.gbui-demo {
  margin: 24px 0;
}

/* ---- the bar ---------------------------------------------------------- */

.gbui-demo-bar {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 10px;
  margin-bottom: 10px;
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

.gbui-demo-caption {
  white-space: nowrap;
}

.gbui-demo-field select,
.gbui-demo-action {
  border: 1px solid var(--vp-c-divider);
  border-radius: 6px;
  padding: 5px 9px;
  font-size: 13px;
  color: var(--vp-c-text-1);
  background: var(--vp-c-bg-soft);
  max-width: 46ch;
}

.gbui-demo-action:hover {
  border-color: var(--vp-c-brand-1);
  color: var(--vp-c-brand-1);
}

.gbui-demo-modes {
  display: inline-flex;
  border: 1px solid var(--vp-c-divider);
  border-radius: 6px;
  overflow: hidden;
}

.gbui-demo-mode {
  padding: 5px 14px;
  font-size: 13px;
  color: var(--vp-c-text-2);
  background: var(--vp-c-bg-soft);
  transition: color 0.2s, background-color 0.2s;
}

.gbui-demo-mode + .gbui-demo-mode {
  border-left: 1px solid var(--vp-c-divider);
}

.gbui-demo-mode:hover {
  color: var(--vp-c-text-1);
}

.gbui-demo-mode.active {
  color: var(--vp-c-brand-1);
  background: var(--vp-c-brand-soft);
}

/* ---- the code --------------------------------------------------------- */

.gbui-demo-code {
  border: 1px solid var(--vp-c-divider);
  border-radius: 10px;
  overflow: auto;
  background: var(--vp-c-bg-alt);
}

.gbui-demo-code :deep(pre) {
  margin: 0;
  padding: 16px 18px;
  font-size: 12.5px;
  line-height: 1.6;
  background: transparent !important;
}

/* ---- the stage -------------------------------------------------------- */

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

/* ---- what it is ------------------------------------------------------- */

.gbui-demo-about {
  display: flex;
  flex-wrap: wrap;
  align-items: flex-start;
  gap: 16px 32px;
  margin-top: 14px;
}

.gbui-demo-about-main {
  flex: 1 1 30ch;
  min-width: 0;
  max-width: 78ch;
}

.gbui-demo-summary {
  margin: 0;
  line-height: 1.65;
  color: var(--vp-c-text-1);
}

.gbui-demo-try {
  margin: 8px 0 0;
  font-size: 13.5px;
  line-height: 1.65;
  color: var(--vp-c-text-2);
}

.gbui-demo-about-side {
  flex: 0 1 auto;
}

.gbui-demo-chips {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
  margin: 2px 0 0;
  padding: 0;
  list-style: none;
}

.gbui-demo-chips li {
  border: 1px solid var(--vp-c-divider);
  border-radius: 999px;
  padding: 2px 10px;
  font-size: 12px;
  color: var(--vp-c-text-2);
}

.gbui-demo-source {
  display: inline-block;
  margin-top: 10px;
  font-family: var(--vp-font-family-mono);
  font-size: 12px;
  color: var(--vp-c-text-2);
}

.gbui-demo-source:hover {
  color: var(--vp-c-brand-1);
}

.gbui-demo-hint {
  max-width: 78ch;
  margin: 12px 0 0;
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
