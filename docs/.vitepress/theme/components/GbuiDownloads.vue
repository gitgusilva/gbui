<script setup lang="ts">
// The download buttons, read from the newest release at run time.
//
// Not generated at build time on purpose: a released page is deployed once and
// then sits there, so a list baked into it would point at whatever was current
// the day it was built and go quietly stale the next time a tag is pushed. The
// GitHub API knows, and it costs one request.
//
// The visitor's own platform is listed first, because that is the button they
// came for and a grid of four makes them read all four to find it.
//
// Everything here degrades to a link. The unauthenticated API allows sixty
// requests an hour per address, and a rate-limited visitor who is shown a
// spinner forever has been told nothing — `/releases/latest` always resolves.
import { computed, onMounted, ref } from 'vue'

const REPO = 'gitgusilva/gbui'

const RELEASES_URL = `https://github.com/${REPO}/releases`
const LATEST_URL = `${RELEASES_URL}/latest`

interface Asset {
  name: string
  browser_download_url: string
  size: number
}

interface Platform {
  /** The substring the archive's name carries, which is what matches it. */
  id: string
  title: string
  note: string
}

const PLATFORMS: Platform[] = [
  { id: 'linux-x86_64', title: 'Linux', note: 'x86-64 · libgbui.so' },
  { id: 'windows-x86_64', title: 'Windows', note: 'x86-64 · gbui.dll' },
  { id: 'macos-arm64', title: 'macOS', note: 'Apple silicon · libgbui.dylib' },
  { id: 'macos-x86_64', title: 'macOS', note: 'Intel · libgbui.dylib' },
]

const version = ref<string | null>(null)
const assets = ref<Asset[]>([])
const loading = ref(true)
const failed = ref(false)

/** Best guess at the visitor's system, used to order the cards and label one. */
function detect(): string | null {
  if (typeof navigator === 'undefined') return null
  const agent = navigator.userAgent.toLowerCase()
  if (agent.includes('win')) return 'windows-x86_64'
  if (agent.includes('mac')) {
    // Apple silicon does not say so in the user agent. Every Mac made since
    // 2020 is one, and naming the wrong one only reorders two cards.
    return 'macos-arm64'
  }
  if (agent.includes('linux') || agent.includes('x11')) return 'linux-x86_64'
  return null
}

const mine = ref<string | null>(null)

function size(bytes: number): string {
  return bytes >= 1024 * 1024
    ? `${(bytes / 1024 / 1024).toFixed(1)} MB`
    : `${Math.round(bytes / 1024)} KB`
}

const cards = computed(() => {
  const found = PLATFORMS.map((platform) => ({
    ...platform,
    asset: assets.value.find(
      (asset) => asset.name.includes(platform.id) && !asset.name.endsWith('.sha256'),
    ),
  })).filter((card) => card.asset)

  const ours = found.filter((card) => card.id === mine.value)
  return [...ours, ...found.filter((card) => card.id !== mine.value)]
})

const checksums = computed(() =>
  assets.value.find((asset) => asset.name === 'SHA256SUMS'),
)

onMounted(async () => {
  mine.value = detect()
  // Bounded: a request that hangs would leave the grid on its skeleton rather
  // than on the fallback, which is the one state that tells the visitor nothing.
  const controller = new AbortController()
  const timer = setTimeout(() => controller.abort(), 6000)
  try {
    const response = await fetch(`https://api.github.com/repos/${REPO}/releases/latest`, {
      headers: { Accept: 'application/vnd.github+json' },
      signal: controller.signal,
    })
    if (!response.ok) throw new Error(`GitHub API ${response.status}`)
    const data = await response.json()
    version.value = data.tag_name
    assets.value = (data.assets ?? []) as Asset[]
    failed.value = cards.value.length === 0
  } catch {
    failed.value = true
  } finally {
    clearTimeout(timer)
    loading.value = false
  }
})
</script>

<template>
  <div class="gbui-downloads">
    <p v-if="version" class="gbui-downloads-version">
      Latest release <strong>{{ version }}</strong>
    </p>

    <div v-if="loading" class="gbui-downloads-grid" aria-hidden="true">
      <div v-for="n in 4" :key="n" class="gbui-downloads-card gbui-downloads-skeleton">
        <span class="gbui-downloads-bar" />
        <span class="gbui-downloads-bar gbui-downloads-bar-short" />
      </div>
    </div>

    <p v-else-if="failed" class="gbui-downloads-fallback">
      The release list could not be read from here — GitHub allows sixty
      unauthenticated requests an hour per address.
      <a :href="LATEST_URL">Open the latest release</a> for the same files.
    </p>

    <div v-else class="gbui-downloads-grid">
      <a
        v-for="card in cards"
        :key="card.id"
        class="gbui-downloads-card"
        :href="card.asset!.browser_download_url"
      >
        <span class="gbui-downloads-title">
          {{ card.title }}
          <span v-if="card.id === mine" class="gbui-downloads-mine">your system</span>
        </span>
        <span class="gbui-downloads-note">{{ card.note }}</span>
        <span class="gbui-downloads-size">{{ size(card.asset!.size) }}</span>
      </a>
    </div>

    <p class="gbui-downloads-foot">
      <template v-if="checksums">
        <a :href="checksums.browser_download_url">SHA256SUMS</a> —
        <code>sha256sum -c SHA256SUMS</code>.
      </template>
      Older versions are on the <a :href="RELEASES_URL">releases page</a>.
    </p>
  </div>
</template>

<style scoped>
.gbui-downloads {
  margin: 24px 0;
}

.gbui-downloads-version {
  margin: 0 0 12px;
  font-size: 13px;
  color: var(--vp-c-text-2);
}

.gbui-downloads-grid {
  display: grid;
  gap: 12px;
  grid-template-columns: repeat(auto-fill, minmax(220px, 1fr));
}

.gbui-downloads-card {
  display: block;
  padding: 14px 16px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 9px;
  background: var(--vp-c-bg-soft);
  font-weight: 400;
  text-decoration: none;
  transition: border-color 0.2s, background-color 0.2s;
}

.gbui-downloads-card:hover {
  border-color: var(--vp-c-brand-1);
  background: var(--vp-c-bg-elv);
}

.gbui-downloads-title {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 15px;
  font-weight: 600;
  color: var(--vp-c-text-1);
}

.gbui-downloads-mine {
  border-radius: 999px;
  padding: 1px 8px;
  font-size: 10.5px;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.04em;
  color: var(--vp-c-brand-1);
  background: var(--vp-c-brand-soft);
}

.gbui-downloads-note {
  display: block;
  margin-top: 4px;
  font-family: var(--vp-font-family-mono);
  font-size: 12px;
  color: var(--vp-c-text-2);
}

.gbui-downloads-size {
  display: block;
  margin-top: 6px;
  font-size: 11.5px;
  color: var(--vp-c-text-3);
}

.gbui-downloads-skeleton {
  pointer-events: none;
}

.gbui-downloads-bar {
  display: block;
  height: 11px;
  border-radius: 4px;
  background: var(--vp-c-divider);
  animation: gbui-downloads-pulse 1.4s ease-in-out infinite;
}

.gbui-downloads-bar-short {
  margin-top: 8px;
  width: 60%;
}

@keyframes gbui-downloads-pulse {
  50% { opacity: 0.45; }
}

/* WCAG 2.2.2: a decorative pulse is still motion, and a reader who has asked
   for none should get a plain box rather than a quieter animation. */
@media (prefers-reduced-motion: reduce) {
  .gbui-downloads-bar {
    animation: none;
  }
}

.gbui-downloads-fallback,
.gbui-downloads-foot {
  margin: 14px 0 0;
  font-size: 13px;
  line-height: 1.6;
  color: var(--vp-c-text-2);
}
</style>
