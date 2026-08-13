// Puts a gbui demo on a canvas.
//
// The other half of demos/runtime/wasm.cpp, and the only file that knows how a
// browser event becomes toolkit input. Everything it does is a call into the C
// surface next door — there is no drawing here, because the frame arrives as a
// block of RGBA the canvas can take as it is.
//
//     import { mountDemo } from './gbui-embed.js'
//     const demo = await mountDemo(canvas, { id: 'scada' })
//     demo.setDark(false)
//     demo.destroy()
//
// It is framework-free on purpose: the documentation site drives it from a Vue
// component, and anything else can drive it from nothing at all.

/** Where the WebAssembly module and its data sit, relative to this file. */
const moduleUrl = new URL('./gbui-demos.js', import.meta.url)

/**
 * The module is loaded once per page and shared.
 *
 * Not per canvas: the download is the toolkit, six screens and three fonts, and
 * a page showing a gallery of six demos should pay for that once. Each canvas
 * still gets a `Host` of its own, which is where all the per-demo state lives.
 */
let modulePromise = null

function loadModule() {
  if (!modulePromise) {
    modulePromise = import(/* @vite-ignore */ moduleUrl.href)
      .then((namespace) => namespace.default({
        // The .wasm and the preloaded .data sit beside the .js, wherever the
        // site is served from — which is not the page's own directory.
        locateFile: (path) => new URL(path, moduleUrl).href,
      }))
      .catch((error) => {
        // A failed load must not be cached as a pending promise forever: the
        // next canvas to mount should be able to try again.
        modulePromise = null
        throw error
      })
  }
  return modulePromise
}

/** Runs `fn` with a C string, and frees it however `fn` ends. */
function withText(module, text, fn) {
  const pointer = module.stringToNewUTF8(text ?? '')
  try {
    return fn(pointer)
  } finally {
    module._free(pointer)
  }
}

/** Reads a `char*` the module allocated, and frees it. */
function takeText(module, pointer) {
  if (!pointer) return ''
  const text = module.UTF8ToString(pointer)
  module._free(pointer)
  return text
}

/** Every demo the module carries, without creating one. */
export async function catalogue() {
  const module = await loadModule()
  return JSON.parse(takeText(module, module._gbui_demos_catalogue()))
}

/**
 * Every component the library declares, with its group, documentation,
 * signature and options.
 *
 * Read out of `gbui::meta`, which is generated from the headers — so a page
 * built on this cannot describe a component the library does not have, or miss
 * an option it does.
 */
export async function components() {
  const module = await loadModule()
  return JSON.parse(takeText(module, module._gbui_demos_components()))
}

/** The palettes and shape rules a demo can be shown in. */
export async function skins() {
  const module = await loadModule()
  return JSON.parse(takeText(module, module._gbui_demos_skins()))
}

/**
 * Mounts a demo on a canvas and starts it.
 *
 * Options:
 *   id           which demo; the first in the catalogue by default
 *   component    show one component's example instead of a screen
 *   dark         start in the dark palette (true)
 *   skin         gitbox | material | cupertino | fluent
 *   fontSize     base UI size in logical pixels
 *   maxScale     ceiling on devicePixelRatio; 2 is already past what a reader
 *                can see and 3 costs 2.25 times the rasterising
 *   autoPause    stop the loop while the canvas is off screen (true)
 *   onReady      called once, with the controller
 */
export async function mountDemo(canvas, options = {}) {
  const module = await loadModule()
  const context = canvas.getContext('2d', { alpha: false })
  if (!context) throw new Error('gbui: this canvas has no 2d context')

  const maxScale = options.maxScale ?? 2
  const settings = {
    dark: options.dark ?? true,
    skin: options.skin ?? 'gitbox',
    autoPause: options.autoPause ?? true,
  }

  // Logical size comes from the element's box; device size from that times the
  // display's scale. The toolkit works in logical pixels and is handed the
  // scale, which is the whole of "support a HiDPI display".
  const box = () => {
    const rect = canvas.getBoundingClientRect()
    return {
      width: Math.max(320, Math.round(rect.width)) || 960,
      height: Math.max(240, Math.round(rect.height)) || 600,
      scale: Math.min(maxScale, window.devicePixelRatio || 1),
    }
  }

  const first = box()
  const handle = withText(module, options.id ?? '', (id) =>
    module._gbui_demos_create(id, first.width, first.height, first.scale, settings.dark ? 1 : 0),
  )
  if (!handle) throw new Error('gbui: the demo host could not be created')

  // A page that wants one component rather than a screen says so at mount, so
  // the first frame is already the right thing and nothing flashes.
  if (options.component) {
    withText(module, options.component, (name) =>
      module._gbui_demos_select_component(handle, name),
    )
  }

  if (options.skin) withText(module, settings.skin, (id) => module._gbui_demos_set_skin(handle, id))
  if (options.fontSize) module._gbui_demos_set_font_size(handle, options.fontSize)

  let destroyed = false
  let paused = false
  let visible = true
  let frameRequest = 0
  let lastFrameAt = 0
  let lastCursor = ''

  // ---- the loop -----------------------------------------------------------

  const present = () => {
    const width = module._gbui_demos_pixel_width(handle)
    const height = module._gbui_demos_pixel_height(handle)
    if (width <= 0 || height <= 0) return
    if (canvas.width !== width || canvas.height !== height) {
      canvas.width = width
      canvas.height = height
    }
    // A view onto the wasm heap rather than a copy — but built fresh every
    // frame, because growing the heap detaches every view onto the old one and
    // a cached ImageData would quietly stop updating.
    const pointer = module._gbui_demos_pixels(handle)
    const pixels = new Uint8ClampedArray(module.HEAPU8.buffer, pointer, width * height * 4)
    context.putImageData(new ImageData(pixels, width, height), 0, 0)

    const cursor = module.UTF8ToString(module._gbui_demos_cursor(handle))
    if (cursor !== lastCursor) {
      canvas.style.cursor = cursor
      lastCursor = cursor
    }
  }

  const tick = (now) => {
    frameRequest = 0
    if (destroyed) return
    // Clamped, and this is not paranoia: a tab left in the background hands
    // back a delta of several seconds on its first frame, which would advance
    // every rolling series past its own buffer in one step.
    const delta = lastFrameAt ? Math.min(0.1, (now - lastFrameAt) / 1000) : 1 / 60
    lastFrameAt = now
    module._gbui_demos_frame(handle, delta)
    present()
    schedule()
  }

  function schedule() {
    if (destroyed || paused || !visible || frameRequest) return
    frameRequest = requestAnimationFrame(tick)
  }

  // ---- size ---------------------------------------------------------------

  const applySize = () => {
    if (destroyed) return
    const next = box()
    module._gbui_demos_resize(handle, next.width, next.height, next.scale)
    schedule()
  }

  const resizeObserver = new ResizeObserver(applySize)
  resizeObserver.observe(canvas)
  window.addEventListener('resize', applySize)

  let intersectionObserver = null
  if (settings.autoPause && 'IntersectionObserver' in window) {
    intersectionObserver = new IntersectionObserver(
      (entries) => {
        visible = entries.some((entry) => entry.isIntersecting)
        // The clock restarts rather than jumping: a demo scrolled back into
        // view should carry on, not fast-forward through the minutes it spent
        // off screen.
        if (visible) {
          lastFrameAt = 0
          schedule()
        }
      },
      { rootMargin: '120px' },
    )
    intersectionObserver.observe(canvas)
  }

  // ---- input --------------------------------------------------------------

  const pointerAt = (event) => {
    const rect = canvas.getBoundingClientRect()
    return { x: event.clientX - rect.left, y: event.clientY - rect.top }
  }

  const sendModifiers = (event) => {
    module._gbui_demos_modifiers(
      handle,
      event.shiftKey ? 1 : 0,
      event.ctrlKey ? 1 : 0,
      event.altKey ? 1 : 0,
      event.metaKey ? 1 : 0,
    )
  }

  const onPointerMove = (event) => {
    const at = pointerAt(event)
    sendModifiers(event)
    module._gbui_demos_pointer(handle, at.x, at.y, event.buttons & 1 ? 1 : 0)
    schedule()
  }

  const onPointerDown = (event) => {
    if (event.button !== 0) return
    const at = pointerAt(event)
    // The capture is what makes a drag survive leaving the canvas — a slider
    // followed off its own track, which is exactly what `Interaction::dragging`
    // is for on the other side.
    canvas.setPointerCapture?.(event.pointerId)
    canvas.focus({ preventScroll: true })
    sendModifiers(event)
    module._gbui_demos_pointer(handle, at.x, at.y, 1)
    schedule()
  }

  const onPointerUp = (event) => {
    const at = pointerAt(event)
    canvas.releasePointerCapture?.(event.pointerId)
    module._gbui_demos_pointer(handle, at.x, at.y, 0)
    schedule()
  }

  const onPointerLeave = () => {
    module._gbui_demos_pointer_leave(handle)
    schedule()
  }

  const onWheel = (event) => {
    // Only once the reader has clicked into the demo. A canvas that eats the
    // wheel on hover puts a hole in the middle of the page, and the reader has
    // to steer around it to get past — see the same reasoning on
    // `ChartZoom::wheel` in the toolkit.
    if (document.activeElement !== canvas) return
    event.preventDefault()
    sendModifiers(event)
    const lines = event.deltaMode === 1 ? event.deltaY : event.deltaY / 40
    module._gbui_demos_wheel(handle, lines)
    schedule()
  }

  const onKeyDown = (event) => {
    if (event.ctrlKey || event.metaKey) return
    sendModifiers(event)

    // Escape hands the page back. Without it the reader who tabbed in has no
    // way out that does not involve the mouse, which is the standard failing
    // of an embedded canvas that takes Tab.
    if (event.key === 'Escape' && !module._gbui_demos_has_focus(handle)) {
      canvas.blur()
      return
    }

    const known = withText(module, event.key, (name) =>
      module._gbui_demos_key(handle, name, event.repeat ? 1 : 0),
    )
    // A single character that is not a shortcut is text, which is what a field
    // in the demo is waiting for.
    if (event.key.length === 1 && !event.altKey) {
      withText(module, event.key, (text) => module._gbui_demos_text(handle, text))
    }
    if (known) event.preventDefault()
    schedule()
  }

  canvas.addEventListener('pointermove', onPointerMove)
  canvas.addEventListener('pointerdown', onPointerDown)
  canvas.addEventListener('pointerup', onPointerUp)
  canvas.addEventListener('pointerleave', onPointerLeave)
  canvas.addEventListener('pointercancel', onPointerLeave)
  canvas.addEventListener('wheel', onWheel, { passive: false })
  canvas.addEventListener('keydown', onKeyDown)
  if (!canvas.hasAttribute('tabindex')) canvas.tabIndex = 0

  // The first frame is drawn immediately rather than waiting for a rAF, so a
  // demo that mounts off screen still has a picture in it.
  module._gbui_demos_frame(handle, 1 / 60)
  present()
  schedule()

  const controller = {
    get element() {
      return canvas
    },
    select(id) {
      const ok = withText(module, id, (name) => module._gbui_demos_select(handle, name))
      lastFrameAt = 0
      schedule()
      return ok === 1
    },
    /** Shows one component's live example instead of an application screen. */
    selectComponent(name) {
      const ok = withText(module, name, (text) =>
        module._gbui_demos_select_component(handle, text),
      )
      lastFrameAt = 0
      schedule()
      return ok === 1
    },
    setDark(dark) {
      settings.dark = !!dark
      module._gbui_demos_set_dark(handle, dark ? 1 : 0)
      schedule()
    },
    setSkin(id) {
      settings.skin = id
      withText(module, id, (name) => module._gbui_demos_set_skin(handle, name))
      schedule()
    },
    setFontSize(size) {
      module._gbui_demos_set_font_size(handle, size)
      schedule()
    },
    restart() {
      module._gbui_demos_restart(handle)
      lastFrameAt = 0
      schedule()
    },
    resize: applySize,
    pause() {
      paused = true
      if (frameRequest) cancelAnimationFrame(frameRequest)
      frameRequest = 0
    },
    resume() {
      paused = false
      lastFrameAt = 0
      schedule()
    },
    get paused() {
      return paused
    },
    destroy() {
      if (destroyed) return
      destroyed = true
      if (frameRequest) cancelAnimationFrame(frameRequest)
      resizeObserver.disconnect()
      intersectionObserver?.disconnect()
      window.removeEventListener('resize', applySize)
      canvas.removeEventListener('pointermove', onPointerMove)
      canvas.removeEventListener('pointerdown', onPointerDown)
      canvas.removeEventListener('pointerup', onPointerUp)
      canvas.removeEventListener('pointerleave', onPointerLeave)
      canvas.removeEventListener('pointercancel', onPointerLeave)
      canvas.removeEventListener('wheel', onWheel)
      canvas.removeEventListener('keydown', onKeyDown)
      module._gbui_demos_destroy(handle)
    },
  }

  options.onReady?.(controller)
  return controller
}
