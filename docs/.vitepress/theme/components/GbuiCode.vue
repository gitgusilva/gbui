<script setup lang="ts">
// A code block that reads like an editor: a path, a copy button, line numbers —
// and only the lines you can see.
//
// The HTML arrives already highlighted from a build-time loader, so this ships
// no highlighter. What it does do is refuse to put a thousand `<span>`s in the
// document to show forty of them: the markup is split per line once, and the
// window scrolls over it with a spacer above and below standing in for the rest.
// The largest screen here is a thousand lines, and a page that shows six of them
// is six thousand nodes the browser lays out to display two hundred.
//
// **Selecting still means the whole file.** That is the tax a virtualised
// viewer usually charges and the reason people avoid one for code: Ctrl+A can
// only select what is in the document, so it selects what is on screen and the
// reader pastes forty lines out of a thousand without noticing. So Ctrl+A is
// intercepted here, and `copy` and `cut` answer with the *file* rather than with
// the fragment — which is also what the Copy button has always done.
import { computed, nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue'

const props = withDefaults(
  defineProps<{
    /** Pre-highlighted markup. Trusted: it comes from our own build. */
    html?: string
    /** Repository-relative path, shown in the bar and used for the label. */
    path?: string
    /** Ceiling before the block scrolls. */
    maxHeight?: number
    /** Number every line down the left, the way an editor does. */
    numbers?: boolean
  }>(),
  { html: '', path: '', maxHeight: 620, numbers: true },
)

/** Lines built above and below the window. Enough that a flick of the wheel
 *  never shows an edge, few enough that the point of this is not lost. */
const OVERSCAN = 14

const body = ref<HTMLElement | null>(null)
const copied = ref(false)
const allSelected = ref(false)
const scrollTop = ref(0)
const viewport = ref(0)
const lineHeight = ref(20)

/**
 * The markup, taken apart once.
 *
 * Shiki writes `<pre …><code …>` and then one `<span class="line">` per line.
 * Splitting on that boundary keeps every attribute the loader stamped — the
 * `data-line` the gutter reads is on the span itself, so a window of lines
 * numbers itself correctly without anything being counted at runtime.
 */
const parsed = computed(() => {
  const open = props.html.indexOf('<span class="line"')
  const close = props.html.lastIndexOf('</code>')
  if (open < 0 || close < 0) return { head: '', lines: [] as string[], tail: '' }
  const body = props.html.slice(open, close)
  return {
    head: props.html.slice(0, open),
    // The split point is the start of each line's span; the trailing newline
    // Shiki leaves between them is whitespace the grid drops anyway.
    lines: body.split(/(?=<span class="line")/).filter((one) => one.trim().length > 0),
    tail: props.html.slice(close),
  }
})

const lines = computed(() => parsed.value.lines.length)

const first = computed(() =>
  Math.max(0, Math.floor(scrollTop.value / lineHeight.value) - OVERSCAN),
)
const last = computed(() =>
  Math.min(
    lines.value,
    Math.ceil((scrollTop.value + viewport.value) / lineHeight.value) + OVERSCAN,
  ),
)
const shown = computed(() => parsed.value.lines.slice(first.value, last.value))
const above = computed(() => first.value * lineHeight.value)
const below = computed(() => Math.max(0, (lines.value - last.value) * lineHeight.value))

/**
 * The file as text, built once and kept.
 *
 * Through the DOM rather than by stripping tags with a regular expression:
 * the markup is full of `&lt;` and `&amp;` — it is C++ — and a regular
 * expression would hand back a file that no longer compiles.
 */
let plainCache = ''
function plain(): string {
  if (plainCache) return plainCache
  const scratch = document.createElement('div')
  plainCache = parsed.value.lines
    .map((one) => {
      scratch.innerHTML = one
      return scratch.textContent ?? ''
    })
    .join('\n')
  return plainCache
}
watch(() => props.html, () => (plainCache = ''))

let observer: ResizeObserver | null = null

function measure() {
  const host = body.value
  if (!host) return
  // `|| maxHeight`, and this is the whole of a bug worth naming: the markup
  // arrives after the mount — it is a hundred kilobytes in a chunk of its own —
  // so at the mount there is no element to measure, `v-if` having kept it out
  // of the document. A viewport of zero renders `OVERSCAN` lines and a spring
  // where the rest of the file should be, which is a source viewer showing
  // fourteen lines of a thousand and nothing to say why.
  viewport.value = host.clientHeight || props.maxHeight
  // A line's height is what every offset here is counted in, so it is measured
  // rather than assumed: the page's font size is the reader's to change.
  //
  // Measured on the row, not on Shiki's `.line` inside it. That span is inline,
  // and an inline box is as tall as its glyphs — twelve pixels against the
  // row's twenty-four. Counting in half the real height makes the springs half
  // the file's length, and the window then races the scrollbar: right at the
  // top, and a screen of blank by the bottom.
  //
  // `getBoundingClientRect`, not `offsetHeight`, because the row is 23.8 pixels
  // and rounding it to 24 is two hundred pixels of drift over a thousand lines.
  const row = host.querySelector<HTMLElement>('.gbui-code-row')
  const height = row?.getBoundingClientRect().height ?? 0
  if (height > 0) lineHeight.value = height
}

/** Measures now, and again whenever the block changes size. */
function watchSize() {
  observer?.disconnect()
  measure()
  if (!body.value || typeof ResizeObserver === 'undefined') return
  // The window resize listener is not enough: this block grows when its source
  // lands and when the reader switches demos, and neither is a resize.
  observer = new ResizeObserver(() => measure())
  observer.observe(body.value)
}

onMounted(async () => {
  await nextTick()
  watchSize()
  window.addEventListener('resize', measure)
})

onBeforeUnmount(() => {
  observer?.disconnect()
  window.removeEventListener('resize', measure)
})

// The element only exists once there are lines to put in it, so this is where
// the first real measurement happens for every asynchronously loaded file.
watch(lines, async () => {
  await nextTick()
  watchSize()
})

function onScroll(event: Event) {
  scrollTop.value = (event.target as HTMLElement).scrollTop
  // Any scroll is the reader moving on; a selection that covered the file no
  // longer describes what they are looking at.
  allSelected.value = false
}

async function write(text: string) {
  if (!text) return
  try {
    await navigator.clipboard.writeText(text)
    copied.value = true
    window.setTimeout(() => (copied.value = false), 1400)
  } catch {
    // A denied clipboard permission is not worth a dialog; the reader can
    // still select the text, which is what they would have done anyway.
  }
}

function onKeydown(event: KeyboardEvent) {
  if (!(event.ctrlKey || event.metaKey) || event.key.toLowerCase() !== 'a') return
  // What the browser would do is select the rendered window, which is a
  // fraction of the file and looks like the whole of it.
  event.preventDefault()
  allSelected.value = true
  const host = body.value
  if (!host) return
  const range = document.createRange()
  range.selectNodeContents(host)
  const selection = window.getSelection()
  selection?.removeAllRanges()
  selection?.addRange(range)
}

function onCopy(event: ClipboardEvent) {
  if (!allSelected.value) return
  event.preventDefault()
  event.clipboardData?.setData('text/plain', plain())
}
</script>

<template>
  <div class="gbui-code" :class="{ numbered: numbers }">
    <div v-if="path" class="gbui-code-bar">
      <span class="gbui-code-path">{{ path }}</span>
      <span v-if="lines" class="gbui-code-lines">{{ lines }} lines</span>
      <span class="gbui-code-spacer" />
      <button class="gbui-code-copy" :aria-label="`Copy ${path}`" @click="write(plain())">
        {{ copied ? 'Copied' : 'Copy' }}
      </button>
    </div>

    <div
      v-if="lines"
      ref="body"
      class="gbui-code-body"
      tabindex="0"
      :style="{ maxHeight: `${maxHeight}px` }"
      @scroll="onScroll"
      @keydown="onKeydown"
      @copy="onCopy"
      @cut="onCopy"
      @mousedown="allSelected = false"
    >
      <!-- The head and tail are Shiki's own `<pre>` and `<code>`, kept so the
           theme's background, colours and grid apply exactly as they did when
           this was one string. -->
      <div class="gbui-code-shell" v-html="parsed.head" />
      <div class="gbui-code-window">
        <div class="gbui-code-spring" :style="{ height: `${above}px` }" />
        <div
          v-for="(line, index) in shown"
          :key="first + index"
          class="gbui-code-row"
          v-html="line"
        />
        <div class="gbui-code-spring" :style="{ height: `${below}px` }" />
      </div>
    </div>
    <div v-else class="gbui-code-body gbui-code-waiting">
      <p>Loading {{ path || 'the source' }}…</p>
    </div>
  </div>
</template>

<style scoped>
.gbui-code {
  border: 1px solid var(--vp-c-divider);
  border-radius: 10px;
  overflow: hidden;
  background: var(--vp-c-bg-alt);
}

.gbui-code-bar {
  display: flex;
  align-items: center;
  gap: 12px;
  padding: 6px 10px 6px 14px;
  border-bottom: 1px solid var(--vp-c-divider);
  font-size: 12px;
  color: var(--vp-c-text-2);
}

.gbui-code-path {
  font-family: var(--vp-font-family-mono);
}

.gbui-code-lines {
  opacity: 0.7;
}

.gbui-code-spacer {
  flex: 1;
}

.gbui-code-copy {
  border: 1px solid var(--vp-c-divider);
  border-radius: 6px;
  padding: 2px 10px;
  font-size: 12px;
  color: var(--vp-c-text-2);
  background: var(--vp-c-bg-soft);
  transition: color 0.2s, border-color 0.2s;
}

.gbui-code-copy:hover {
  color: var(--vp-c-brand-1);
  border-color: var(--vp-c-brand-1);
}

.gbui-code-body {
  overflow: auto;
  position: relative;
}

.gbui-code-body:focus-visible {
  outline: 2px solid var(--vp-c-brand-1);
  outline-offset: -2px;
}

/* Shiki's `<pre>` carries the theme's background and its CSS variables, and it
 * is empty: it is here to paint and to be inherited from, and the lines are in
 * the window stacked over it. */
.gbui-code-shell {
  position: absolute;
  inset: 0;
}

.gbui-code-shell :deep(pre) {
  margin: 0;
  height: 100%;
  border-radius: 0;
}

.gbui-code-window {
  position: relative;
  display: grid;
  padding: 14px 0;
  font-family: var(--vp-font-family-mono);
}

.gbui-code-waiting {
  padding: 16px 18px;
  font-size: 12.5px;
  color: var(--vp-c-text-3);
}

/* VitePress styles `code` and `pre` inside `.vp-doc` for its own blocks —
 * padding, a background, a line height, a rounded box. Shiki's markup walks
 * straight into all of it, which is why the first render of this had forty
 * pixels between every line. The reset is explicit rather than a guess at
 * which rule won. */
.gbui-code-window :deep(pre),
.gbui-code-window :deep(code),
.gbui-code-window :deep(span) {
  padding: 0;
  border-radius: 0;
  background: transparent;
  font-size: 12.5px;
  line-height: 1.6;
}

.gbui-code-window :deep(.line) {
  padding: 0 18px;
  /* An empty line is still a line: without a height it collapses and the
   * gutter numbers stop lining up with the code beside them. */
  min-height: 1.6em;
  white-space: pre;
}

/* Numbers from the attribute the loader stamps on every line, so nothing is
 * counted at runtime and a window of lines cannot desynchronise the gutter —
 * the failure of every implementation that renders the numbers as a second
 * column, and the one that virtualising would otherwise guarantee. */
.numbered .gbui-code-window :deep(.line) {
  padding-left: 62px;
  position: relative;
}

.numbered .gbui-code-window :deep(.line)::before {
  content: attr(data-line);
  position: absolute;
  left: 0;
  width: 44px;
  text-align: right;
  color: var(--vp-c-text-3);
  opacity: 0.55;
  user-select: none;
}

.gbui-code-window :deep(.line:hover) {
  background: var(--vp-c-default-soft);
}

.gbui-code-spring {
  /* Nothing to see: it is the height of the lines that are not here. */
  pointer-events: none;
}
</style>
