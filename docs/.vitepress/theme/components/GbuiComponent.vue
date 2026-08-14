<script setup lang="ts">
// One component's running example, under its own heading.
//
// Everything else on these pages — the summary, the signatures, the options —
// is Markdown generated from the same metadata, so this is only the part
// Markdown cannot be. One of these per component, not per page: a page covering
// `text`, `strong` and `emphasis` shows three, because a reader comparing them
// wants to see them, not to be told which tab they are behind.
//
// Stacking them costs almost nothing. `gbui-embed.js` loads the WebAssembly
// module once for the whole page and gives each canvas a `Host` of its own, and
// a canvas that is off screen stops its loop — so the second example on a page
// is a few hundred kilobytes of state, not a second download.
import { nextTick, onBeforeUnmount, ref, shallowRef, watch } from 'vue'
import { useData, withBase } from 'vitepress'

const props = defineProps<{ name: string }>()

const { isDark } = useData()

const canvas = ref<HTMLCanvasElement | null>(null)
const demo = shallowRef<any>(null)
const running = ref(false)
const status = ref<'idle' | 'loading' | 'ready' | 'missing' | 'error'>('idle')
const message = ref('')

async function run() {
  if (status.value === 'loading') return
  running.value = true
  status.value = 'loading'
  try {
    const module = await import(/* @vite-ignore */ withBase('/demo/gbui-embed.js'))
    await nextTick()
    demo.value = await module.mountDemo(canvas.value, {
      component: props.name,
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

watch(isDark, (dark) => demo.value?.setDark(dark))
onBeforeUnmount(() => demo.value?.destroy())
</script>

<template>
  <div class="gbui-component">
    <!-- Shown from the moment Run is pressed, not from the moment it is ready:
         `mountDemo` sizes itself from the element's box, and a canvas still
         behind `display: none` measures zero by zero. The overlay covers it
         while it starts. -->
    <canvas
      v-show="running"
      ref="canvas"
      class="gbui-component-canvas"
      :aria-label="`${name}, running`"
    />
    <div v-if="status !== 'ready'" class="gbui-component-overlay">
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
        <button class="gbui-component-run" @click="run">▶ Run {{ name }}</button>
        <p>Nothing is downloaded until you press it.</p>
      </template>
    </div>
  </div>
</template>

<style scoped>
.gbui-component {
  position: relative;
  height: 320px;
  margin: 18px 0 24px;
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
