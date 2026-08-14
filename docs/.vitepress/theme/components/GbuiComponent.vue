<script setup lang="ts">
// The running example at the top of a component's page.
//
// Everything else on these pages — the summary, the signatures, the options —
// is Markdown generated from the same metadata, so this is only the part
// Markdown cannot be. One WebAssembly module per page, started when asked and
// not before; a page covering more than one component switches which of them it
// is running rather than starting a second module.
import { computed, nextTick, onBeforeUnmount, ref, shallowRef, watch } from 'vue'
import { useData, withBase } from 'vitepress'

import { pageOf } from '../../componentPages'

const props = defineProps<{ page: string }>()

const { isDark } = useData()

const components = computed(() => pageOf(props.page)?.components ?? [])

const canvas = ref<HTMLCanvasElement | null>(null)
const demo = shallowRef<any>(null)
const current = ref(components.value[0]?.name ?? '')
const running = ref(false)
const status = ref<'idle' | 'loading' | 'ready' | 'missing' | 'error'>('idle')
const message = ref('')

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
  <div class="gbui-component">
    <div v-if="components.length > 1" class="gbui-component-picker">
      <button
        v-for="entry in components"
        :key="entry.name"
        class="gbui-component-tab"
        :class="{ active: entry.name === current }"
        @click="choose(entry.name)"
      >
        {{ entry.name }}
      </button>
    </div>

    <div class="gbui-component-stage">
      <canvas
        v-show="running"
        ref="canvas"
        class="gbui-component-canvas"
        :aria-label="`${current}, running`"
      />
      <div v-if="!running || status !== 'ready'" class="gbui-component-overlay">
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
          <button class="gbui-component-run" @click="run">▶ Run {{ current }}</button>
          <p>Nothing is downloaded until you press it.</p>
        </template>
      </div>
    </div>
  </div>
</template>

<style scoped>
.gbui-component {
  margin: 22px 0 28px;
}

.gbui-component-picker {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
  margin-bottom: 10px;
}

.gbui-component-tab {
  padding: 3px 10px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 999px;
  font-family: var(--vp-font-family-mono);
  font-size: 12.5px;
  color: var(--vp-c-text-2);
}

.gbui-component-tab:hover {
  color: var(--vp-c-text-1);
  background: var(--vp-c-default-soft);
}

.gbui-component-tab.active {
  border-color: var(--vp-c-brand-1);
  color: var(--vp-c-brand-1);
  background: var(--vp-c-brand-soft);
}

.gbui-component-stage {
  position: relative;
  height: 340px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 10px;
  overflow: hidden;
  background: var(--vp-c-bg-alt);
}

.gbui-component-canvas {
  display: block;
  width: 100%;
  height: 100%;
  outline: none;
}

.gbui-component-overlay {
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

.gbui-component-overlay p {
  margin: 0;
}

.gbui-component-run {
  border: 1px solid var(--vp-c-brand-1);
  border-radius: 8px;
  padding: 7px 18px;
  font-family: var(--vp-font-family-mono);
  font-size: 14px;
  color: var(--vp-c-brand-1);
  background: var(--vp-c-brand-soft);
}
</style>
