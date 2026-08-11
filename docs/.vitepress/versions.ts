// Which versions of the documentation exist, and which one this build is.
//
// VitePress has no built-in versioning, so the site follows the convention the
// rest of the Vite ecosystem uses: every released version is built once, with
// its own `base`, and deployed to its own directory. The newest is also copied
// to the root, so `/guide/layout` is always the current release and
// `/v0.2/guide/layout` is that release forever.
//
// `tools/build_docs.sh` does the building. Adding a release means adding one
// line to `archived` below, tagging, and running that script.

/** The version this checkout documents. Kept in step with CMakeLists.txt. */
export const current = '0.2'

/**
 * Older versions that are still deployed, newest first. A version leaves this
 * list when its directory is deleted, not before — a link that 404s is worse
 * than a page that says it is out of date.
 */
export const archived: string[] = []

/** True while building a version that is not the current one. */
export const isArchived = process.env.GBUI_DOCS_VERSION
  ? process.env.GBUI_DOCS_VERSION !== current
  : false

/** The nav dropdown: the current release, then everything archived. */
export function versionMenu() {
  return {
    text: `v${process.env.GBUI_DOCS_VERSION ?? current}`,
    items: [
      { text: `v${current} (latest)`, link: '/' },
      ...archived.map((version) => ({ text: `v${version}`, link: `/v${version}/` })),
      { text: 'Release notes', link: 'https://github.com/gitgusilva/gbui/releases' },
    ],
  }
}
