<script setup lang="ts">
// The contents page for /components: every group, every page, every name.
//
// The sidebar carries the same list — this is here so the landing page is a map
// rather than a paragraph pointing at one, and because a name is easier to find
// in a grid than in a column. Both are built from the metadata, so neither can
// name a component the library does not have.
import { computed, ref } from 'vue'
import { withBase } from 'vitepress'

import { all, groups } from '../../componentPages'

const query = ref('')

const needle = computed(() => query.value.trim().toLowerCase())

const shown = computed(() =>
  groups()
    .map((group) => ({
      text: group.text,
      pages: group.pages.filter(
        (page) =>
          !needle.value ||
          page.title.toLowerCase().includes(needle.value) ||
          page.group.toLowerCase().includes(needle.value) ||
          page.components.some(
            (component) =>
              component.name.toLowerCase().includes(needle.value) ||
              component.summary.toLowerCase().includes(needle.value),
          ),
      ),
    }))
    .filter((group) => group.pages.length),
)

const count = computed(() => shown.value.reduce((n, group) => n + group.pages.length, 0))
</script>

<template>
  <div class="gbui-index">
    <input
      v-model="query"
      class="gbui-index-search"
      type="search"
      placeholder="Filter — name, group, description"
      aria-label="Filter components"
    />
    <p class="gbui-index-count">
      {{ all.length }} components on {{ count }} pages
    </p>

    <section v-for="group in shown" :key="group.text">
      <h2>{{ group.text }}</h2>
      <ul class="gbui-index-list">
        <li v-for="page in group.pages" :key="page.slug">
          <a :href="withBase(`/components/${page.slug}`)">{{ page.title }}</a>
          <span class="gbui-index-summary">{{ page.components[0].summary }}</span>
          <span v-if="page.components.length > 1" class="gbui-index-also">
            {{ page.components.map((component) => component.name).join(' · ') }}
          </span>
        </li>
      </ul>
    </section>
  </div>
</template>

<style scoped>
.gbui-index {
  margin: 24px 0;
}

.gbui-index-search {
  width: 100%;
  max-width: 420px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 6px;
  padding: 7px 12px;
  font-size: 14px;
  color: var(--vp-c-text-1);
  background: var(--vp-c-bg-soft);
}

.gbui-index-count {
  margin: 8px 0 0;
  font-size: 12px;
  color: var(--vp-c-text-3);
}

.gbui-index h2 {
  margin: 30px 0 0;
  padding-top: 18px;
  font-size: 16px;
}

.gbui-index-list {
  list-style: none;
  margin: 12px 0 0;
  padding: 0;
  display: grid;
  gap: 12px;
  grid-template-columns: repeat(auto-fill, minmax(260px, 1fr));
}

.gbui-index-list li {
  margin: 0;
  padding: 12px 14px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 9px;
  background: var(--vp-c-bg-soft);
}

.gbui-index-list a {
  font-family: var(--vp-font-family-mono);
  font-size: 13.5px;
  font-weight: 500;
}

.gbui-index-summary {
  display: block;
  margin-top: 4px;
  font-size: 13px;
  line-height: 1.55;
  color: var(--vp-c-text-2);
}

.gbui-index-also {
  display: block;
  margin-top: 6px;
  font-family: var(--vp-font-family-mono);
  font-size: 11.5px;
  color: var(--vp-c-text-3);
}
</style>
