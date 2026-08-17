<script setup lang="ts">
// The component set, on the landing page, as a count and a wall of names.
//
// A list of features tells a visitor what the library believes; this tells them
// whether the thing they need is in it, which is the question they actually
// arrived with. Built from the generated metadata, so it cannot name a
// component the library does not have or miss one it does.
import { computed } from 'vue'
import { withBase } from 'vitepress'

import { all, groups, label } from '../../componentPages'

const sections = computed(() =>
  groups().map((group) => ({
    text: group.text,
    pages: group.pages,
  })),
)
</script>

<template>
  <div class="gbui-band">
    <p class="gbui-band-count">
      <strong>{{ all.length }} components</strong>, every one with a live example
      on its own page — and a name, a role and a state for a screen reader.
    </p>

    <div v-for="section in sections" :key="section.text" class="gbui-band-group">
      <h3>{{ section.text }}</h3>
      <p class="gbui-band-names">
        <a
          v-for="page in section.pages"
          :key="page.slug"
          :href="withBase(`/components/${page.slug}`)"
        >{{ page.label }}</a>
      </p>
    </div>
  </div>
</template>

<style scoped>
.gbui-band {
  margin: 8px 0 32px;
}

.gbui-band-count {
  margin: 0 0 20px;
  font-size: 15px;
  color: var(--vp-c-text-2);
}

.gbui-band-group {
  margin-top: 18px;
}

.gbui-band-group h3 {
  margin: 0 0 8px;
  padding: 0;
  border: 0;
  font-size: 11.5px;
  font-weight: 600;
  letter-spacing: 0.08em;
  text-transform: uppercase;
  color: var(--vp-c-text-3);
}

.gbui-band-names {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
  margin: 0;
}

.gbui-band-names a {
  border: 1px solid var(--vp-c-divider);
  border-radius: 7px;
  padding: 4px 10px;
  font-size: 12.5px;
  font-weight: 500;
  color: var(--vp-c-text-1);
  text-decoration: none;
  background: var(--vp-c-bg-soft);
  transition: color 0.2s, border-color 0.2s, background-color 0.2s;
}

.gbui-band-names a:hover {
  color: var(--vp-c-brand-1);
  border-color: var(--vp-c-brand-1);
  background: var(--vp-c-brand-soft);
}
</style>
