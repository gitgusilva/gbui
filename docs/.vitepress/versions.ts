// Which versions of the documentation exist, and which one this build is.
//
// VitePress has no built-in versioning, so the site follows the convention the
// rest of the Vite ecosystem uses: every released version is built once, with
// its own `base`, and deployed to its own directory. The newest is also copied
// to the root, so `/guide/layout` is always the current release and
// `/v0.2/guide/layout` is that release forever.
//
// `tools/build_docs.sh` does the building and the Pages workflow runs it.
// Cutting a release is three edits and a tag:
//
//   1. move the outgoing version into `archived` below;
//   2. set `current` to the new one, in step with CMakeLists.txt;
//   3. tag it — `git tag v0.3 && git push --tags`.
//
// The archived build comes from the tag, in a worktree, so it says what that
// release actually said rather than today's text wearing an old label.

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

/**
 * Absolute URL of the site root, when the build knows one — CI takes it from
 * the Pages configuration.
 *
 * Cross-version links have to be absolute, and that is not a preference. Every
 * internal link goes through VitePress's `withBase`, which prepends *this
 * build's* base: inside `/gbui/v0.2/`, a link written `/` comes out as
 * `/gbui/v0.2/` — the page it is already on. An absolute URL is left alone, so
 * it is the only form that can point from one version to another.
 *
 * Empty falls back to base-relative links, which is right for a local build
 * where the root is the only version there is.
 *
 * One consequence worth knowing rather than rediscovering: an archived build
 * uses **its own tag's** config, so a release tagged before this existed links
 * back to the current docs with whatever it knew at the time. Nothing here can
 * fix a build that already happened; every tag from now on is correct.
 */
export const siteUrl = (process.env.GBUI_DOCS_SITE_URL ?? '').replace(/\/*$/, '/')

/** A link to another version of this site, from whichever one is building. */
export function versionLink(version?: string): string {
  const path = version ? `v${version}/` : ''
  return siteUrl ? `${siteUrl}${path}` : `/${path}`
}

/** The nav dropdown: the current release, then everything archived. */
export function versionMenu() {
  return {
    text: `v${process.env.GBUI_DOCS_VERSION ?? current}`,
    items: [
      { text: `v${current} (latest)`, link: versionLink() },
      ...archived.map((version) => ({ text: `v${version}`, link: versionLink(version) })),
      { text: 'Release notes', link: 'https://github.com/gitgusilva/gbui/releases' },
    ],
  }
}
