<script setup lang="ts">
// A code block that reads like an editor: a path, a copy button, line numbers.
//
// The HTML arrives already highlighted from a build-time loader, so this ships
// no highlighter and does no work on the client beyond counting lines in CSS.
// Copying reads `innerText` off the rendered block rather than carrying a
// second plain-text copy of every file — the text on screen is by definition
// the text to copy, and it cannot fall out of step with itself.
import { computed, ref } from 'vue'

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

const block = ref<HTMLElement | null>(null)
const copied = ref(false)
const lines = computed(() => (props.html.match(/class="line"/g) ?? []).length)

async function copy() {
  const text = block.value?.innerText ?? ''
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
</script>

<template>
  <div class="gbui-code" :class="{ numbered: numbers }">
    <div v-if="path" class="gbui-code-bar">
      <span class="gbui-code-path">{{ path }}</span>
      <span v-if="lines" class="gbui-code-lines">{{ lines }} lines</span>
      <span class="gbui-code-spacer" />
      <button class="gbui-code-copy" :aria-label="`Copy ${path}`" @click="copy">
        {{ copied ? 'Copied' : 'Copy' }}
      </button>
    </div>

    <div
      v-if="html"
      ref="block"
      class="gbui-code-body"
      :style="{ maxHeight: `${maxHeight}px` }"
      v-html="html"
    />
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
.gbui-code-body :deep(pre) {
  margin: 0;
  padding: 14px 0;
  background: transparent !important;
  border-radius: 0;
}

/* Everything except the colour: that is Shiki's, and overriding it here is
 * how the first version of this reset turned a syntax-highlighted file back
 * into one shade of grey. */
.gbui-code-body :deep(code),
.gbui-code-body :deep(span) {
  padding: 0;
  border-radius: 0;
  background: transparent;
  font-size: 12.5px;
  line-height: 1.6;
}

/* A grid, and this is the whole trick.
 *
 * Shiki writes `<span class="line">…</span>` followed by a literal newline, and
 * relies on that newline for the break. Making `.line` a block *as well* meant
 * every line was followed by a newline that `white-space: pre` still rendered
 * — two lines of height for one line of code, which is exactly what the first
 * render of this looked like. Grid drops whitespace-only children, so each
 * line becomes one row and the newlines stop counting. */
.gbui-code-body :deep(code) {
  display: grid;
  font-family: var(--vp-font-family-mono);
}

.gbui-code-body :deep(.line) {
  padding: 0 18px;
  /* An empty line is still a line: without a height it collapses and the
   * gutter numbers stop lining up with the code beside them. */
  min-height: 1.6em;
}

/* Numbers from the attribute the loader stamps on every line, so nothing is
 * counted at runtime and a wrapped line cannot desynchronise the gutter — the
 * failure of every implementation that renders the numbers as a second
 * column. */
.numbered .gbui-code-body :deep(.line) {
  padding-left: 62px;
  position: relative;
}

.numbered .gbui-code-body :deep(.line)::before {
  content: attr(data-line);
  position: absolute;
  left: 0;
  width: 44px;
  text-align: right;
  color: var(--vp-c-text-3);
  opacity: 0.55;
  user-select: none;
}

.gbui-code-body :deep(.line:hover) {
  background: var(--vp-c-default-soft);
}
</style>
