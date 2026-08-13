<script setup lang="ts">
// Every component the library declares, with a live example behind a Run
// button and its options read from the generated metadata.
//
// Nothing here is written twice. The list, the summaries, the signatures and
// the options table come from `components.json`, which `tools/generate_meta.py`
// writes from the headers alongside the C++ table — so this page cannot
// describe a component the library does not have, or miss an option it does.
// The running preview comes from the same WebAssembly module the demos use,
// and like there it downloads nothing until asked.
import { computed, nextTick, onBeforeUnmount, ref, shallowRef, watch } from 'vue'
import { useData, withBase } from 'vitepress'

import table from '../components.json'
import GbuiCode from './GbuiCode.vue'

interface Property {
  name: string
  type: string
  default: string
  doc: string
  optional: boolean
  choices: string[]
}

interface Component {
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

const all = table as Component[]
const { isDark } = useData()

const canvas = ref<HTMLCanvasElement | null>(null)
const demo = shallowRef<any>(null)
const current = ref(all[0].name)
const query = ref('')
const running = ref(false)
const status = ref<'idle' | 'loading' | 'ready' | 'missing' | 'error'>('idle')
const message = ref('')

const groups = computed(() => [...new Set(all.map((entry) => entry.group))])

const shown = computed(() => {
  const needle = query.value.trim().toLowerCase()
  if (!needle) return all
  return all.filter(
    (entry) =>
      entry.name.toLowerCase().includes(needle) ||
      entry.summary.toLowerCase().includes(needle) ||
      entry.group.toLowerCase().includes(needle),
  )
})

const selected = computed(() => all.find((entry) => entry.name === current.value) ?? all[0])

function inGroup(group: string) {
  return shown.value.filter((entry) => entry.group === group)
}

async function run() {
  running.value = true
  if (demo.value) {
    demo.value.selectComponent(current.value)
    demo.value.resume()
    return
  }
  if (status.value === 'loading') return

  status.value = 'loading'
  try {
    const module = await import(/* @vite-ignore */ withBase('/demo/gbui-embed.js'))
    await nextTick()
    demo.value = await module.mountDemo(canvas.value, {
      component: current.value,
      dark: isDark.value,
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

function choose(name: string) {
  current.value = name
  if (running.value) demo.value?.selectComponent(name)
}

watch(isDark, (dark) => demo.value?.setDark(dark))
onBeforeUnmount(() => demo.value?.destroy())
</script>

<template>
  <div class="gbui-components">
    <aside class="gbui-components-list">
      <input
        v-model="query"
        class="gbui-components-search"
        type="search"
        placeholder="Filter — name, group, description"
        aria-label="Filter components"
      />
      <p class="gbui-components-count">{{ shown.length }} of {{ all.length }}</p>

      <template v-for="group in groups" :key="group">
        <section v-if="inGroup(group).length" class="gbui-components-group">
          <h3>{{ group }}</h3>
          <button
            v-for="entry in inGroup(group)"
            :key="entry.name"
            class="gbui-components-item"
            :class="{ active: entry.name === current }"
            @click="choose(entry.name)"
          >
            {{ entry.name }}
            <span v-if="entry.container" class="gbui-components-flag" title="a container">{ }</span>
          </button>
        </section>
      </template>
    </aside>

    <div class="gbui-components-detail">
      <header class="gbui-components-head">
        <h2>{{ selected.name }}</h2>
        <p>{{ selected.summary }}</p>
        <p class="gbui-components-facts">
          <span>{{ selected.group }}</span>
          <span v-if="selected.container">container</span>
          <span v-if="selected.interactive">interactive</span>
          <code>{{ selected.header }}</code>
        </p>
      </header>

      <div class="gbui-components-stage">
        <canvas
          v-show="running"
          ref="canvas"
          class="gbui-components-canvas"
          :aria-label="`${selected.name}, running`"
        />
        <div v-if="!running || status !== 'ready'" class="gbui-components-overlay">
          <template v-if="status === 'loading'">
            <strong>Starting the toolkit…</strong>
            <p>About 1.9 MB, once for the whole page.</p>
          </template>
          <template v-else-if="status === 'missing'">
            <strong>The demo bundle is not built in this checkout.</strong>
            <pre><code>tools/build_wasm.sh</code></pre>
          </template>
          <template v-else-if="status === 'error'">
            <strong>It failed to start.</strong>
            <pre><code>{{ message }}</code></pre>
          </template>
          <template v-else>
            <button class="gbui-components-run" @click="run">▶ Run this component</button>
            <p>Nothing is downloaded until you press it.</p>
          </template>
        </div>
      </div>

      <h4>Signature</h4>
      <pre
        v-for="signature in selected.signatures"
        :key="signature"
        class="gbui-components-signature"
      ><code>{{ signature }}</code></pre>
      <p v-for="note in selected.notes" :key="note" class="gbui-components-note">{{ note }}</p>

      <template v-if="selected.properties.length">
        <h4>{{ selected.optionsType }}</h4>
        <div class="gbui-components-table">
          <table>
            <thead>
              <tr>
                <th>Option</th>
                <th>Type</th>
                <th>Default</th>
                <th>What it does</th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="property in selected.properties" :key="property.name">
                <td>
                  <code>{{ property.name }}</code>
                  <span v-if="property.optional" class="gbui-components-flag">optional</span>
                </td>
                <td>
                  <code>{{ property.type }}</code>
                  <span v-if="property.choices.length" class="gbui-components-choices">
                    {{ property.choices.join(' · ') }}
                  </span>
                </td>
                <td><code v-if="property.default">{{ property.default }}</code></td>
                <td>{{ property.doc }}</td>
              </tr>
            </tbody>
          </table>
        </div>
      </template>
    </div>
  </div>
</template>

<style scoped>
.gbui-components {
  display: flex;
  gap: 28px;
  align-items: flex-start;
  margin: 24px 0;
}

/* ---- the list --------------------------------------------------------- */

.gbui-components-list {
  flex: 0 0 220px;
  position: sticky;
  top: 84px;
  max-height: calc(100vh - 120px);
  overflow-y: auto;
}

.gbui-components-search {
  width: 100%;
  border: 1px solid var(--vp-c-divider);
  border-radius: 6px;
  padding: 6px 10px;
  font-size: 13px;
  color: var(--vp-c-text-1);
  background: var(--vp-c-bg-soft);
}

.gbui-components-count {
  margin: 6px 0 12px;
  font-size: 11.5px;
  color: var(--vp-c-text-3);
}

.gbui-components-group h3 {
  margin: 14px 0 4px;
  font-size: 11px;
  letter-spacing: 0.06em;
  text-transform: uppercase;
  color: var(--vp-c-text-3);
  border: 0;
  padding: 0;
}

.gbui-components-item {
  display: flex;
  align-items: center;
  gap: 6px;
  width: 100%;
  padding: 3px 8px;
  border-radius: 5px;
  font-family: var(--vp-font-family-mono);
  font-size: 12.5px;
  text-align: left;
  color: var(--vp-c-text-2);
}

.gbui-components-item:hover {
  color: var(--vp-c-text-1);
  background: var(--vp-c-default-soft);
}

.gbui-components-item.active {
  color: var(--vp-c-brand-1);
  background: var(--vp-c-brand-soft);
}

.gbui-components-flag {
  font-size: 10.5px;
  font-family: var(--vp-font-family-base);
  color: var(--vp-c-text-3);
}

/* ---- the detail ------------------------------------------------------- */

.gbui-components-detail {
  flex: 1 1 auto;
  min-width: 0;
}

.gbui-components-head h2 {
  margin: 0;
  padding: 0;
  border: 0;
  font-family: var(--vp-font-family-mono);
  font-size: 20px;
}

.gbui-components-head p {
  margin: 6px 0 0;
  line-height: 1.6;
}

.gbui-components-facts {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
  font-size: 12px;
  color: var(--vp-c-text-3);
}

.gbui-components-stage {
  position: relative;
  height: 320px;
  margin: 16px 0 8px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 10px;
  overflow: hidden;
  background: var(--vp-c-bg-alt);
}

.gbui-components-canvas {
  display: block;
  width: 100%;
  height: 100%;
  outline: none;
}

.gbui-components-overlay {
  position: absolute;
  inset: 0;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 8px;
  text-align: center;
  font-size: 13px;
  color: var(--vp-c-text-3);
  background: var(--vp-c-bg-alt);
}

.gbui-components-overlay p {
  margin: 0;
}

.gbui-components-run {
  border: 1px solid var(--vp-c-brand-1);
  border-radius: 8px;
  padding: 7px 18px;
  font-size: 14px;
  color: var(--vp-c-brand-1);
  background: var(--vp-c-brand-soft);
}

.gbui-components-detail h4 {
  margin: 22px 0 8px;
  font-size: 12px;
  letter-spacing: 0.06em;
  text-transform: uppercase;
  color: var(--vp-c-text-3);
}

.gbui-components-signature {
  margin: 0 0 6px;
  padding: 10px 14px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  background: var(--vp-c-bg-alt);
  font-size: 12.5px;
  line-height: 1.6;
  overflow-x: auto;
}

.gbui-components-note {
  margin: 4px 0 0;
  font-size: 13px;
  line-height: 1.6;
  color: var(--vp-c-text-2);
}

.gbui-components-table {
  overflow-x: auto;
}

.gbui-components-table table {
  width: 100%;
  border-collapse: collapse;
  font-size: 13px;
}

.gbui-components-table th {
  text-align: left;
  font-size: 11.5px;
  letter-spacing: 0.04em;
  text-transform: uppercase;
  color: var(--vp-c-text-3);
  padding: 6px 12px 6px 0;
  border-bottom: 1px solid var(--vp-c-divider);
}

.gbui-components-table td {
  vertical-align: top;
  padding: 8px 12px 8px 0;
  border-bottom: 1px solid var(--vp-c-divider);
  line-height: 1.55;
}

.gbui-components-table code {
  font-size: 12px;
  white-space: nowrap;
}

.gbui-components-choices {
  display: block;
  margin-top: 3px;
  font-size: 11px;
  color: var(--vp-c-text-3);
}

@media (max-width: 860px) {
  .gbui-components {
    flex-direction: column;
  }

  .gbui-components-list {
    position: static;
    flex: 1 1 auto;
    width: 100%;
    max-height: none;
  }
}
</style>
