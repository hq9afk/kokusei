# `kokusei` development critical knowledge

## Description

Hard-won rules from kokusei's development.
Can be updated if found new knowledge that supersedes old ones, or genuinely new ones.

## Rule

One statement + One explanation, ≤ 20 words each.
Drop an entry once newer knowledge fully supersedes it.

## 1. Subprocesses in a multithreaded process

- **The poll loop must never block, even briefly.** One poll() loop drives every surface; a single blocking call freezes the whole shell.
- **No code path on the shared poll thread may make a synchronous blocking D-Bus call — signal callbacks and click handlers alike.** A blocking Bluetooth `getProperty()` chain froze the shell 18s; a polkit-gated WiFi-toggle `setProperty` froze it 24s.
- **Building GPU textures or rasterizing text from a D-Bus callback blocks the poll loop, not just network I/O.** `notification_apply_content`'s eager Pango/Cairo/GL work on every Bluetooth connect stuttered the shell; build lazily in the paint path.
- **kokusei is multithreaded even though it spawned no threads itself.** Mesa, pipewire, and Pango each start their own background threads automatically.
- **The mpris player scan was the last synchronous D-Bus path on the shared poll thread.** A Bluetooth connect's `NameOwnerChanged` burst ran its blocking `ListNames`/`Get` chain, freezing the shell.
- **Never allocate memory in a forked child before exec().** fork() can copy a lock held by another thread, deadlocking the child forever.
- **Ignoring SIGCHLD and calling waitpid() cannot coexist.** Ignoring SIGCHLD is process-wide and lets the kernel auto-reap, breaking waitpid() everywhere.
- **Fire-and-forget processes should double-fork, not rely on global reaping.** The intermediate child exits immediately, reparenting the grandchild to init for automatic reaping.
- **A dedicated blocking-waitpid thread beats watching a child's fd in the poll loop.** waitpid() returns instantly on exit; poll() sometimes missed pipe readiness for tens of seconds.
- **A cancelled background process must be killed, not just detached.** Leaving it running lets retyped searches pile up competing processes that never finish.
- **SIGKILL stops future CPU use but doesn't guarantee immediate process death.** Confirm actual death before forking a replacement, or the two processes compete.
- **Reusing a handle across restarts needs a generation counter.** A cancelled worker can still wake later with a stale result unless generations are checked.
- **`async_process`'s worker thread resets `pid` to `-1` the instant it sets `done` and fills `buffer`.** Track "request in flight" with your own bool flag, not by reading `pid` back later.
- **Stopping N worker threads on shutdown must signal every stop flag before joining any.** Signal-then-join one at a time made `kokusei kill` block on each column's thread serially.
- **A module's owned background thread must be torn down in its destructor, not only its explicit-close path.** `kokusei kill` skipped the visualizer's shutdown; the joinable thread's destruction called `std::terminate()` mid-`eglSwapBuffers`.
- **`fork()` on the poll thread can stall it even with an async-signal-safe child.** glibc `fork()` blocks acquiring allocator locks held by decode/Mesa/Pango threads; `async_process` uses `posix_spawn`.
- **`request_frame` defers a mapped surface's repaint to `frame_done`, never paints inline.** Inline `eglSwapBuffers` from a D-Bus handler froze the shell; only the first unmapped paint stays synchronous.
- **The deferred `request_frame` commit needs a 1px `wl_surface_damage_buffer` to guarantee a `frame_done`.** Hyprland skips scheduling a frame for a bufferless, undamaged commit, so the callback never fires.
- **`frame_done` must arm the next frame callback before calling `draw()`.** Arming only outside halved animation rate; the paint's own commit must carry the next request.
- **A timer- or signal-driven service must gate its redraw callback on a real state diff.** `bluetooth_tick` repainted every surface once per second unconditionally; compare state and set a `dirty` flag.
- **Reacting to every NetworkManager `State`/`ActiveConnections` change with `nmcli --rescan yes` self-amplifies.** Forced rescans delay association and emit more state churn; debounce through `schedule_rescan`.

## 2. Debugging methodology

- **Three plausible root-cause theories were wrong before the real one was found.** Every theory was ruled out by measurement, not argument.
- **A /proc process watcher plus timestamped logs and gdb backtraces located the real bug.** Only correlating both sides' timestamps distinguished "child is slow" from "we missed it finishing."
- **A stopgap workaround left after its cause is fixed becomes a silent regression.** `fd --threads 4` stayed after the real fix, silently halving all search parallelism.
- **Fix the shared function, not just the caller that reported the bug.** The same defect usually routes through every sibling caller too.
- **Restore `kernel.yama.ptrace_scope` to 1 after live debugging.** It was lowered to 0 temporarily so gdb could attach without sudo.

## 3. Rendering

- **Don't render the Tabler icon font via fontconfig plus Pango.** Late-registered app fonts aren't reliably picked up by Pango's font map; use FreeType+Cairo directly.
- **kokusei clips using scissor rects plus a corner inset, not a stencil buffer.** Correct as long as nothing needs to visually touch a rounded edge.
- **A scissor clip helper needs a stack, not one slot, once clips nest.** An unconditional glDisable on destruction wiped the outer clip when clips nested.
- **A container animating its own size must clip children to the current size.** Fading opacity alone doesn't stop oversized content rendering outside the container mid-tween.
- **Gate that reveal clip on the height tween, not `animations.hasActive()`.** A looping marquee kept `hasActive()` true forever, clipping the network panel's sub-dialog off-screen permanently.
- **A cached geometry value must derive from the same variable used for drawing.** Using the animation's target width instead of the current frame stored a wrong position.
- **Scissor rects should floor/ceil each edge independently, not truncate uniformly.** Flooring the origin then rounding the size can drop the last row or column.
- **A narrower anti-aliasing band makes rounded-rect edges look crisper.** kokusei's 2px smoothstep band softened straight edges; matching noctalia's 1px band fixed it.
- **A filled widget drawn at the same origin as an earlier label silently paints over it.** Two settings-tab tiles once started at the label's own `(x,y)`, hiding it under the first tile.
- **Drawing textures at fractional pixel positions blurs every glyph and icon.** GL_LINEAR sampling blends edge texels 50/50 at .5px offsets; round positions before drawing.
- **`show_layout`'s current point is the top-left corner, not the baseline.** Adding ascent on top of that doubled the offset, rendering text clipped near the bottom.
- **`set_opacity()` is a single global value per frame, not per-node.** Simultaneous different opacities need baking alpha into each element's own color instead.
- **A bounded `stat()` search beats a subprocess search over a handful of candidates.** Icon resolution only needs existence checks against known paths, not an open-ended directory search.
- **Writing literal Unicode escapes through tool calls is unsafe.** The JSON layer can silently replace them with actual glyph bytes; verify with `od -c`.
- **Verify icon names against the widget's actual default-state property, not a plausible name.** Several icons were wrong because they were guessed from a config property list.
- **A glyphless icon codepoint fails silently as an empty texture, caught only by `rasterize_icon`'s `klog`.** `volume_empty` shipped as `U+0001`; check icon constants against the actual font.
- **Size text/icon textures from fixed font metrics, not per-string ink extents.** Ink-based sizing made baseline position jitter as string content changed between renders.
- **A per-glyph text-field draw must advance each cell by the font's fixed `Pango` advance, not ink width.** Summing ink widths drops side bearings, collapsing narrow glyphs so input reads shorter than normal.
- **A text input's whole-run slide belongs on its origin `x`, `keqing-shell`-style `Behavior on x`, not every length change.** Left-aligned fields keep a fixed origin; only centered dot rows whose origin moves should animate.
- **Cairo output is premultiplied alpha, but kokusei's blend convention is straight alpha.** Uploading one as the other silently squares alpha at edges, washing out antialiased pixels.
- **`draw_rounded_rect` always reads its border-color argument, even at zero border width.** Passing nullptr for "no border" is a null-pointer read, not a no-op.
- **A rebuilt-every-frame node tree should pool and reuse nodes, not reallocate.** Reallocating at animation frame rate causes unbounded heap high-water-mark growth over time.
- **A refactor changing a shared function's contract must migrate every call site.** Leaving old `add_child()` around let stragglers silently skip rendering after the pooling refactor.
- **An animated per-frame layout value needs a snap-on-first-value sentinel before tweening.** Without one, the first frame animates from a stale or zero default.
- **A tweened value must not be computed from another value that's itself mid-tween.** Otherwise it re-targets every frame instead of converging on a moving anchor.
- **AnimationManager has no delay primitive, so chain a no-op tween to get one.** `animate()`/`animateTimer()` both start immediately; a dummy tween's `on_complete` triggers the real one.
- **A module with its own entrance animation should skip the generic overlay-panel fade.** Layering both fades produces a visible double-fade the reference lacks.
- **A pre-upload downsample must derive its target size from the source's aspect ratio.** Squashing to the destination box's raw dimensions bakes in a stretch a later crop can't undo.
- **A per-element opacity tween must not run while a container-level fade is active.** Two independently-timed alpha ramps multiply into a visibly different, non-obvious result.
- **Node color pointers are read at draw time, not when stored, so must outlive the frame.** Passing a temporary `Color`'s address renders garbage once the stack slot is reused.
- **`node_add_texture` draws at native pixel size, not scaled to its container.** An aspect-preserving decode in a fixed cell without cropping spills into neighbors.
- **A `smoothstep(edge0, edge1, x)` call needs `edge0 < edge1`; reversed or equal edges are spec-undefined.** Invert with `1.0 - smoothstep(lo, hi, x)` and floor a computed edge that can reach `0`. Mesa silently tolerates reversed/equal edges; NVIDIA may not.
- **`precision mediump float` in a fragment shader is real on NVIDIA (range only `±2^14 = 16384`), fake on Mesa (promoted to highp).** `starward`'s thunder shaders rendered as a filled rectangle on NVIDIA because `length(ab)` / `dot(ab,ab)` on a ~900 px bolt vector overflowed mediump to `+inf`, so `dir = ab/len` collapsed to `0` and every fragment computed the same value. This still holds under ES 3 - `rrect` / `rounded_tex` were bumped to `highp` for the same reason. Declare `precision highp float` unconditionally for any fragment shader that touches pixel-scale magnitudes (`length`, `dot`, `d*d`, hashes); `#ifdef GL_FRAGMENT_PRECISION_HIGH` is NOT reliably defined on NVIDIA GLES.
- **A `float(x == x)` NaN guard must run before `clamp`, not after.** `clamp` launders `NaN` into an endpoint, so a post-clamp self-compare sees a finite value and is a no-op; guard the raw value (`f *= float(f == f)`) then clamp.
- **The rasterize-once-cache-by-key pattern generalizes beyond text/icons to any procedural drawing.** Cache keys must bucket continuous inputs like percentages, or entries grow unbounded.
- **A custom multi-pass GPU shader effect bypasses the Node/Scene graph entirely.** Give it its own programs/FBOs, called directly from the module's paint function.
- **The shell runs one OpenGL ES 3.2 context everywhere, shaders are `#version 320 es`.** EGL config asks `EGL_OPENGL_ES3_BIT`; bootstrap aborts loudly when 3.2 is unavailable.
- **Non-constant `for` loop bounds are legal at `#version 300 es`+.** GLSL ES 1.00's compile-time-constant-bound rule no longer applies; a tunable count can be a uniform.
- **`GL_UNPACK_ROW_LENGTH` is core in ES 3.** The `GL_EXT_unpack_subimage` probe is gone; `texture_row_length_supported()` is a constant `true`.
- **A per-frame multi-tap fullscreen shader pass at native resolution can stall the shared poll loop.** A since-removed 96-tap glow blur froze the shell until moved to a downsampled FBO.
- **A per-frame `glGenBuffers`/`glDeleteBuffers` for a static quad is driver churn the rest of the codebase avoids.** Create it once alongside the program/VBO it belongs to, like `Renderer::quad_vbo_`.
- **A single-pass module shader can ride `Renderer::draw_custom` instead of owning FBOs.** `starward`'s `thunder_burst`/`thunder_shock` compile lazily, reuse `kRendererQuadVs` and the shared `quad_vbo_`, have no uniform-bounded loop, and each draw covers only its effect's bounding box, not the output.
- **A GPU-heavy overlay that still stalls the poll loop after tuning must move off it entirely.** The visualizer window runs on its own thread with its own share-context `EGLContext`.
- **A render thread sharing an `EGLSurface` with the main thread must never be current on it simultaneously.** The render thread owns the surface while open; the main thread hands off a per-frame struct under a mutex.
- **`toplevel_window_init_egl` leaves its `EGLSurface` current on the main thread; a render-thread module must drop it with `app_detail::rest_egl_current` right after, or the toggle intermittently fails.** `resonance`'s open path skipped this, so the render thread's `eglMakeCurrent` raced the main thread and lost with `EGL_BAD_ACCESS` (`0x3002`), leaving the window unmapped. `gl_make_current` consumes the real error, so the render thread's own `eglGetError` misleadingly logs `EGL_SUCCESS`.
- **The default EGL swap interval (`1`) makes `eglSwapBuffers` block the shared poll thread indefinitely on NVIDIA when the compositor holds the buffer (surface occluded by a fullscreen overlay, or hidden under `ext-session-lock`); Mesa tolerates it.** `main()` calls `eglSwapInterval(egl_display, 0)` once after the first `eglMakeCurrent`; `wl_surface.frame` callbacks already pace every surface. Every `eglMakeCurrent`/`eglSwapBuffers` also checks its return and skips the frame on `EGL_FALSE` rather than blocking or crashing (noctalia pattern).
- **Never drive GL for a surface the compositor has stopped compositing.** `expanse_paint` no-ops while its output is covered or the session is locked; an animated wallpaper behind the lock keeps decoding and is composited into the visible `penance` lock surface, never swapped on its own hidden background surface.
- **Two hand-rolled opacity pipelines for one window's variants drift apart silently.** Special-casing let one path's opacity ignore fade-in; unifying onto one shared Renderer/Scene path fixed it.
- **A single shared `Renderer` needs its own reset at frame start, not caller discipline.** Callers leaked stale opacity into the next paint; `Renderer::begin_frame()` now resets it itself.
- **A freshly created share-context starts with `GL_BLEND` disabled, even sharing a namespace with an enabled context.** The visualizer render thread's raw GL path must call `glEnable(GL_BLEND)` itself once per context.
- **`glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` squares alpha when a translucent rect draws over a transparent-cleared surface.** Use `glBlendFuncSeparate` with `GL_ONE` for alpha so it lands exactly where set.
- **Renaming a persisted config value needs a load-time compatibility mapping, or users silently lose the setting.** Map old strings to new right after `value_or()` in `config.cpp`'s loader.
- **Control center's font/border/glow constants already match `keqing-shell` numerically; the visual diff wasn't a config gap.** It was a Pango/Cairo rasterization-path difference — root-caused as icon/text font hinting, see below.
- **An on-demand panel's own geometry should lock at open, not track live content.** Recomputing height every frame let content changes desync the close animation's start value.
- **A `border_width` ported 1:1 from a QML `Rectangle` looks thinner here than in Qt.** The fixed 1px SDF antialiasing band eats more of its opacity; match `metrics::border_thin` (`2.0f`).
- **Font hinting must differ between icon glyphs and text, not share one `cairo_font_options_t`.** `HINT_STYLE_NONE`+`HINT_METRICS_OFF` preserves Tabler stroke thickness while hinted text stays crisp; resolves the Control center diff above.
- **`keqing-shell`'s launcher menu fill uses its own `menuBgAlpha 0.8`, not the shared `overlay` token.** A module's background opacity can diverge from the process-wide `overlay` (`0.92`); trace it to the QML.
- **`keqing-shell`'s launcher row subtitle is the entry's own full home-relative `path`, for every entry carrying one.** Not the parent directory, not gated on kind — apps simply have no `path`.
- **A `Texture`'s `width`/`height` are device pixels (`logical * scale`); laying out against logical coords breaks at HiDPI.** `launcher`'s row centring used raw `tex->height`; divide by `tex->scale` — the block was 2x oversized on scale 2.
- **Draw CPU core cells with a synthesized sequential `Core N`, not the raw hwmon `tempN_label`.** Intel hybrid parts label cores sparsely (`0,4,8,…,25`), reading as junk; `label` stays for sort order only.
- **`Node`/`Scene` has no `z`; child claim order is paint order.** An overlay highlight must be claimed after every node it should sit above, not earlier.
- **Skip the CPU BGRA→RGBA swizzle for shm capture buffers; `GL_EXT_texture_format_BGRA8888` uploads them directly.** A per-pixel swizzle per window per frame is a poll-loop stall; the GL path keeps recapture cheap.
- **`Node` rotation/scale compose as Qt's `T(o)·S·R·T(-o)` about the centre, uploaded as a 2x3 `Affine2D`.** `node_draw` skips the scissor clip while a transform is active; only the centred lock card uses it.
- **A settings tab's master toggle must early-`return` from its paint fn when off, not just render the switch.** `idle_tab_paint` kept drawing rows with idle disabled; click regions are paint-time, so gating paint gates input.
- **A new settings tab must be appended to `SettingsTab`, `kSettingsTabs` and `kSettingsTabCount` in the same order.** `draw_nav_rail` maps rail row `i` to `SettingsTab(i)`, so a reordered list mis-routes tab clicks.
- **An animated image's frame textures must be freed when its surface is off-screen, not held for the module's lifetime.** `AnimatedImage::hide` clears the textures and decode job; a later `show` re-uploads from the `.rgba` cache.
- **One `dlopen`'d `libav` plugin (`media_plugin`) now decodes every animated surface, not only wallpaper.** The per-feature `ffmpeg` subprocess is gone; `animate_job_start` fills a `.rgba` frame cache.
- **`media_service` exposes two engine entry points on one decode core: `media_decode_stream` and `media_decode_frames`.** The frame-set path is software-only and one-shot; a 200px avatar gains nothing from `VAAPI` or pacing.
- **Hide the animated image after the close fade, not at close-start.** Clearing frames while `opacity` still tweens pops the image out; gate `hide` on fully closed.
- **`animated_image_draw`'s `alpha` fades the whole image, border ring and ring-fill included, not just the frame texture.** It once skipped the ring, so `starward`'s logo frame lingered a beat after everything else faded.
- **Every ad-hoc media-to-cache path is the same four steps: XDG-cache-dir, mtime hash key, decode, existence check.** `media_service` owns them; no per-feature copy.
- **A bounded UI-gif decode still needs `fps * seconds` frames, not a fixed count.** A `36`-frame cap once showed only the first `2.4s` of a `7s` logo; keep a generous ceiling.
- **Decode frames into a `.tmp` dir and rename on success, never straight into the cache dir.** A killed decode left a partial frame set that every later run reused as complete.
- **`fs::rename` of the `.tmp` frame dir must `remove_all` the target first; `rename(2)` into a non-empty dir fails.** A stale `.png`-format cache dir made every gif promotion fail silently.
- **A "cache already built" check counting one file extension doesn't prove the dir is absent.** `count_rgba_frames` saw zero and left the old `.png` dir in place, blocking the rename.
- **An `AnimatedImage` decode resolution must be device px (`logical * scale`), not logical px.** Decoding the lock avatar at its `200` logical size upscaled it blurrily on scale 2.
- **An animated wallpaper needs no pre-copy into the cache.** `media_plugin` reads the source path directly and libav probes by content; the copy only added a thread.
- **A closed full-screen overlay/panel must no-op its own `request_frame`, gated on `open`.** Blanket frame fan-outs otherwise repaint every idle surface, spiking the GPU on any toggle.
- **`herald_paint` re-arms only while a tween is active, so every time-varying element must be tween-driven.** The wall-clock timeout bar froze between the entrance tween and expiry; a per-entry linear tween fixes it.
- **`animated_image_animating` must exclude single-frame stills (`frame_count > 1 && fps > 0`).** A static logo otherwise reports as animating, so overlays repaint every frame for the panel's open life.
- **A live-preview overlay paces its re-arm to its capture interval, not the display refresh.** `liyue` re-armed every `frame_done`; a `tick` at `kLiyueCaptureIntervalMs` cuts idle GPU with no visible change.
- **Variable-count node colours come from a frame-scoped `static thread_local std::deque<Color>` cleared at build start.** `deque` keeps element pointers valid across `push_back`; `static` outlives the build call so pointers survive to `draw`.
- **On-demand qixing panels track live content height through one animated value (`PanelHeightReveal`), not a frozen height.** A `closing` latch keeps a live recompute from re-targeting the value the close collapse animates to zero.
- **Reusing `arc_gauge` at a larger diameter must scale the stroke by the reference's stroke/diameter ratio.** `penance`'s ~3x gauges kept `yuheng`'s 6px stroke and rendered as a hairline; `kPenanceResGaugeStrokeRatio` (`6/68`) fixes it.
- **`resonance`'s ported GLava audio chain renders into `GL_R16` targets for the per-write `[0,1]` clamp.** `GL_EXT_texture_norm16` gives R16; probe it for colour-renderability at init and degrade to a cleared window if absent.
- **A GLava-style temporal average needs an N-FBO ring, not one accumulator.** `resonance` copies each frame's gravity-decayed spectrum into `ring[i++%5]`, then `average` Hann-weights the five slots.
- **`ncs`'s blob bypasses `Node`/`Scene` entirely: an `r32ui` image texture is the real output, not the fragment colour.** `ncs-1` accumulates particle depth via `imageAtomicAdd`; `ncs-2` reads+clears it via `imageAtomicExchange`; a `glMemoryBarrier` between them is required on NVIDIA where Mesa is lenient.
- **`ncs`'s `sphere.radius` is an exclusion-disc radius, not the blob's size.** Particles inside it evacuate onto a shell; particles outside it stay as the raw grid, so a too-small radius shows a rectangle around a punched hole.
- **Every `ncs.glsl` / `ncs` default constant must be kept verbatim; the shader is a tuned whole and "roughly similar" tweaks all read wrong.** Reverted divergences: `sphere.radius = 0.5 * min(...)` (rectangle-with-hole) and a `baseForm.scale` override (should stay `ncs`'s `2.0`). The `0.7` black backdrop and the `accent`-hued blob/glow are deliberate kokusei overrides, fed as `u_backdrop` / `u_accent` uniforms so every shader-literal constant stays verbatim. The per-device knobs (`particleThin`, `particle.size`, `fractalField.complexity`, `glow.directions`, `glow.quality`, render `fps`) are also uniforms now, defaulted in `ResonanceParams` to the exact `ncs` constants — "keep verbatim" means the defaults equal the verbatim values.
- **`resonance`'s blob canvas is a square `resonance_canvas_size(surfaceW, surfaceH)` (`min/2`), not a fixed size, rebuilt (atomic texture + FBOs) whenever the surface resizes, then `glBlitFramebuffer`d centered onto the window.** This ports `ncs`'s `applyResize`. The blob geometry (`sphere.radius = .7236 * resolution.x`, `bassMultiplier`, `displacements`) is pixel-absolute against the canvas via the `resolution` uniform, which now carries the canvas size; the canvas must stay **square** or `sphereCoords()` yields an ellipsoid — squareness is the invariant, not a fixed number. A textured-quad present pass over the whole surface instead of the blit measurably stuttered the visualizer; the blit stays.
- **`ncs`'s grid-step divisor is `0` whenever `baseForm.numParticles.z == 1` (the `resonance` default), and only Intel turns that into a GPU hang.** `int(Inf)` clamps high on Mesa/NVIDIA (loop exits at once) but is `0` on Intel, then `-1` after `spaces -= 1`, so the `for i += spaces.z` loop runs backwards forever. The flattened `kNcs1Main` must carry `ncs`'s guard: `max(denom, vec3(0.0001))`, `max(spaces.x/y, 1)`, and the `numParticles.z <= 1.0 ? int(zSize)+1 : max(spaces.z, 1)` branch.
- **`ncs` never `glClear`s and its glow pass is premultiplied, transparent where the blob is absent; `resonance` gives the whole window a `0.7` black backdrop `ncs` does not.** The `glow` fragment shader composites `u_backdrop` source-over (`FragColor += (1 - FragColor.w) * u_backdrop`) so the blitted canvas region carries it, and the default framebuffer is `glClear`ed to the same `kResonanceWindowBackground` so any surface margin outside the canvas matches. `u_fade` fades blob and backdrop in together on open — so `u_fade == 0` makes the whole canvas region (backdrop included) transparent, and the `glClear`ed margin is the only thing left.
- **`resonance`'s render thread must present every frame and start its fade on the first presented frame, never gate either on a frame-time budget.** An earlier build dropped the `eglSwapBuffers` for any frame over `60 ms` and only started the fade once a frame came in under `30 ms`. On Intel the untuned `ncs` blob is ~110 ms/frame, so the `xdg_toplevel` never committed a buffer (no window at all); tuning the knobs under `60 ms` mapped the window but frames stayed over `30 ms`, so `fade` was pinned at `0` and the canvas stayed transparent. The blob being heavy is what the per-device knobs are for; the render loop just runs at whatever rate the hardware gives.
- **`ncs`'s `time` uniform is a frame counter paced to `fps` (default 60), and its GLava decay constants are per-frame at that rate.** `ncs` `nanosleep`s its render loop to `fps`; on a high-refresh display an un-paced port scrolls the `flows` noise and decays the spectrum too fast and the blob reads as over-reactive. `fps` is a `ResonanceParams` knob; the render thread's frame step AND the gravity-decay divisor (`ResonanceAudioStages::run`'s `fps` arg) both use the live value so lowering it stays wall-clock-consistent like `ncs`.
- **`resonance`'s render thread self-paces and owns every `wl_surface` request; the poll thread never drives its frames.** The thread loops on `cv.wait_until` at `1 / params.fps` (snapshotted under the mutex each iteration), does its own `capture.take` / FFT / `stages->run` / `blob->render` / `eglSwapBuffers`, and reads the fade from a `fade_start` timestamp. Driving it from the poll-thread `FrameClock` deadlocked the frame pump on Mesa (see section 4). `resonance_toggle` clears `base.frame_clock.surface` so nothing arms a `wl_surface_frame` behind the thread's back.

## 4. Wayland protocol

- **Wayland gives no way to query live which output the pointer is over.** Track it as a best-effort hint from your own surfaces' enter/motion events instead.
- **Optional protocol events need sane fallback defaults, not zero.** Some compositors never send `repeat_info`; defaulting to 0/0 silently disables key repeat.
- **Never live-test input-grabbing or lock-screen protocol code carelessly.** A prior live test hung the keyboard and forced a reboot.
- **A coordinate from one layer-shell surface isn't valid on another without translating margins.** Different surfaces can have different margins, so origins don't automatically align.
- **A click-through popup needs a small anchored surface, not a full-output one.** A full-output surface would swallow every pointer event across the whole screen while open.
- **A fading, resizing surface should tween opacity only and snap geometry at the endpoints.** Driving both from one tween made content visibly rescale instead of cleanly fading.
- **Hover-driven surface changes must only touch size, never margin or exclusive_zone.** Changing margin on hover repositioned the surface mid-hover, causing an infinite flicker loop.
- **`exclusive_zone` must not include the same-edge margin value.** The compositor already adds that margin automatically, so including it reserves the space twice.
- **An output's initial event burst isn't guaranteed by the same roundtrip that bound it.** A bind's reply is a second round-trip; do one extra roundtrip before reading output state.
- **A live-update IPC event may lack a field only a full snapshot query provides.** Check whether an adjacent event in the same stream already carries and can cache that field.
- **Don't start the polkit agent before keyboard input and a GLib main loop both exist.** An agent that registers but can't prompt intercepts and fails every real `pkexec` system-wide.
- **A struct member can't share a name with a Wayland protocol type used in the same header.** `xdg_surface *xdg_surface` compiles but breaks name lookup in including translation units; rename the field.
- **A real `xdg_toplevel` window is created on open and destroyed on close, not kept mapped-but-transparent.** A zero-opacity mapped toplevel would still show in switchers, unlike a layer-shell fade-in-place.
- **`ToplevelWindowBase` has no generic resize callback, only `on_close_request`.** A module keeping a persistent per-size buffer must compare against live width/height each paint.
- **An animation's `on_complete` that destroys the animated surface can fire mid-frame, inside paint's own `tick()`.** Re-check surface validity right after `tick()`, or defer the destroy to the next poll iteration.
- **A layer-shell overlay now stays mapped across same-output toggles, not destroying its surface on close.** Destroy-then-recreate left the compositor's layer stack stale until an unrelated event forced a recompute.
- **Destroying and recreating a layer surface with the same namespace can leave it uncomposited.** Hyprland kept the reopened panel invisible until an unrelated workspace switch or exclusive-zone change.
- **A recreated same-namespace layer surface can still paint yet be dropped from pointer-input routing.** `tray_menu`'s clicks fell through to the panel below, which closed it; keep popups mapped.
- **A panel-owned menu is a real `xdg_popup` grabbed to the panel's layer surface via `get_popup`, not a sibling surface.** Two layer surfaces on one layer have undefined z-order and no grab; the popup grab guarantees both and delivers `popup_done`.
- **Destroy an `xdg_popup` from the next `tick()` after `popup_done`, never inside the callback.** The callback runs mid `wl_display_dispatch`; freeing there frees the surface being iterated.
- **`xdg_popup` submenu navigation reuses the single-surface `menu_path` model, resized via `xdg_popup_reposition`.** `popup_window_reposition` clears `configured`; paint skips a frame until the fresh `configure` arrives, then draws.
- **`wl_seat` lives on `WaylandState`, not `BlinkState`.** `xdg_popup::grab` needs it plus `last_button_serial`/`PointerClick.serial` so a click-opened menu has its grab serial.
- **Tearing down a `ToplevelWindowBase` must also reset its `FrameClock` fields, not just Wayland/EGL handles.** A stale non-null `frame_clock.callback` makes `request_frame` silently no-op forever afterward.
- **A pending `wl_callback` must be released with `wl_callback_destroy`, never just nulled.** `wl_surface_destroy` doesn't free it; a late `done` then fires against a reused, reset struct.
- **Release an `EGLSurface` from the current context before `wl_egl_window_destroy`, not after.** `eglDestroySurface` on a current surface defers teardown; the freed `wl_egl_window` is then read, corrupting EGL state.
- **Matrix and visualizer are meant to tile as regular windows, not float.** Do not add a `float = true` window rule; that's a rejected direction, not an oversight.
- **A destroy-on-close `xdg_toplevel` can hand its next window handle the exact address a prior instance had.** Per-window state keyed by address must clear when the window goes away, or it inherits stale state.
- **In `hl`'s Lua event API, a window's fields are only safe to read on `window.close`, not `window.destroy`.** By `window.destroy` fields read back `nil`; assigning to a nil table key crashes Lua.
- **A best-effort pointer-hint into a per-output struct must be cleared on that output's removal.** `wl_output` removal frees its `MonitorOutput`; a stale `last_pointer_monitor` caused a use-after-free crash.
- **A mostly-click-through layer surface can still take clicks on sub-rects via a `wl_region` union of them.** The notification surface rebuilds its region from visible close-button rects each paint; the rest stays click-through.
- **A `PerMonitorModule::handle_pointer_move` fires on every monitor's instance with the same shared coords.** Guard hover state on `pointer.focused_surface == own surface`, or every monitor highlights at once.
- **The pointing-hand cursor needs `wants_pointing_hand_cursor()` on both `Module` and `PerMonitorModule`.** The poll loop only asked overlays, so `bar` and every unimplemented module kept the arrow.
- **`ext-session-lock` withholds the `locked` event until every output presented a non-null lock-surface buffer.** Create and paint all lock surfaces before flushing; a null-buffer commit is a protocol error.
- **`unlock_and_destroy` needs a `wl_display_roundtrip` before the lock surfaces are torn down.** The protocol warns the server may kill the client with a protocol error otherwise.
- **A lock-surface `configure` must be `ack`ed before any commit and re-acked on every resend.** Defer the `wl_egl_window` resize past the handler; resizing inside it causes reconfigure errors.
- **An unlock animation's `on_complete` must defer surface teardown via `DeferredCall`, never free inline.** It fires inside `AnimationManager::tick()`, mid-iteration over the very manager the teardown destroys.
- **The startup overlay `init_egl` loop must not gate on `surface()`.** The lock owns no surface until locked; gating skipped its `init_egl`, leaving draw state unset.
- **A `pam_start_confdir` service needs an `account` rule, not just `auth`.** With none, `pam_acct_mgmt` returns `PAM_PERM_DENIED` and a correct password still fails.
- **A `ToplevelWindowBase` surface with a dedicated render thread must not also be wired into the poll-thread `FrameClock`.** The `FrameClock` arms `wl_surface_frame` on the poll thread but the render thread's `eglSwapBuffers` is what commits; that split makes frame requests land on the wrong commit. On Mesa `wl_egl` (which drives the app's own surface proxy) the pump dies after frame one, so `resonance` never advanced its fade and showed only Hyprland's border, plus the mismatched traffic hitched the poll loop. nvidia's EGL platform services the window on a private `wl_event_queue`, so it was unaffected. The render thread now owns every `wl_surface` request and self-paces via `cv.wait_until` at `params.fps`; the poll thread only creates and tears down the surface. `resonance_toggle` clears `base.frame_clock.surface` after `init_egl` so `output_scale.on_change` cannot arm a frame.

## 5. Async state correctness

- **Never score an async operation's result against a live mutable field.** Freeze the input into its own field at start time and score against that instead.
- **Capture a value synchronously at the action site rather than deferring to the next repaint.** Relying on an async dispatch path left panels opening at position zero on first use.
- **Click coordinates must be captured atomically with the click event itself.** Reading the live shared pointer position later can return a different monitor's coordinates.
- **A value read right after triggering a state change can still be one frame stale.** Force the downstream paint/tick to run before reading its side effect.
- **Two `animate()` calls sharing an owner id cancel each other, even if unrelated.** Give each simultaneously-animated property of an item its own distinct owner id.
- **AnimationManager only advances when something calls `tick()` every frame.** A panel that forgets to tick freezes forever, including keyboard-release `on_complete` callbacks.
- **A reactive `*_changed` flag must be set at every code path that changes the value.** One mutator forgetting the flag silently breaks reactivity for just that path.
- **Binding a PipeWire node listener alone doesn't deliver live param-value updates.** An explicit `pw_node_subscribe_params()` call is required to receive future value changes.
- **Every panel requesting exclusive keyboard interactivity needs its own key-dispatch arm.** The two are declared separately, so nothing enforces they stay in sync as panels are added.
- **An optimistic local write can suppress the `*_changed` flag it's supposed to trigger.** The confirmation compares against the already-updated value and finds no change; raise the flag at the write.
- **A client's own callback confirming a write isn't proof the real state changed.** Device-backed PipeWire nodes need writes routed through the parent Device's Route, not the node.
- **A registry's initial announcement and an object's own info event carry different properties.** A property missing from one may only appear in the other's later event.
- **A generic "click missed" guard excluding a sibling surface pushes the decision onto it.** That surface's own handler must then know about every overlay stacked above it.
- **Calling a shared AnimationManager's `tick()` multiple times per instant is safe if absolute-time-based.** It recomputes from wall-clock time, not accumulated delta, so repeats don't double-advance.
- **A hover-driven highlight must clear on lost surface focus, not just recompute on motion.** Another surface stealing pointer focus mid-hover leaves a stale hovered index otherwise.
- **A mutex must cover the read side of a shared buffer, not just the write side.** Per-channel splitting moved ring-buffer reads outside `ring_mutex_` while the PipeWire thread still wrote under it.
- **A local sdbus proxy destroyed right after firing an async call corrupts the shared connection it borrowed.** Cache one proxy per object path on the owning state instead of a throwaway per call.
- **Adding a second writer to shared per-bin state should prompt auditing every existing writer.** `onParamChanged` recomputed FFT bin ranges off-mutex; only a problem once `setBarCount` made it frequent.
- **A sync-from-config function uploading only on non-empty paths must also clear the texture when the path empties.** The fix resets the column's `Texture` and bumps its generation counter to reject stale decodes.
- **Clearing a texture in memory doesn't repaint the surface, only uploading one does.** Both branches must call `wallpaper_request_frame`; the empty-path clear skipped it, so the old wallpaper stayed.
- **A widget/IPC-opened panel must prime its polled telemetry at the open site, not the timer tick.** The dashboard's cards popped in one-by-one over seconds; the CPU pill already primes on click.
- **An async result polled on a slow throttle lands a throttle-period late, not a request late.** `gpu_temp_poll` starts `nvidia-smi` on one call, reads it next; poll every tick while running.
- **Re-enabling idle management, or lowering a timeout mid-idle, must reset the per-monitor activity clock.** A stale `last_activity` otherwise fires the screensaver instantly; `apply_config_update` calls `idle_reset` on any idle-config change.
- **A panel's staged dismissal must be coded identically in every dismiss path.** `Escape` collapsed the subpanel while outside-click closed the whole panel, because the branches were written separately.

## 6. Architecture and scale discipline

- **noctalia is a reference for ideas, not a template to copy wholesale.** It's roughly 45x kokusei's size; every adopted idea must be resized to kokusei's scale.
- **Several noctalia subsystems were deliberately skipped, not overlooked.** A retained scene graph, backend abstraction, and scripting engine solve problems kokusei doesn't have.
- **Config hot-reload was built, but schema-validated multi-file config remains rejected.** Only when the single file's contents change is it reloaded, not its structure.
- **A config field lacking settings UI may need deletion, not a new control.** Check the reference project first; it may hardcode the same value with no UI either.
- **noctalia's testing philosophy and logic/UI separation already matched kokusei's convention.** Naming what was already right matters as much as naming what needs to change.
- **Check a reference technique against the full target hardware range, not one profiling machine.** An integrated-GPU-only measurement wrongly justified diverging from a technique discrete GPUs need.
- **Animated wallpaper decode runs in-process via `libavcodec`/`libavfilter`, not a spawned `ffmpeg` per column.** Replaced three duplicated `ffmpeg` processes plus a raw-rgba pipe with one shared decode loop.
- **Hardware decoder selection stays portable by trying a preference list of `AVHWDeviceType`s, not branching on hardware.** Tries `CUDA` then `VAAPI`, falling through to software.
- **A GPU zero-copy texture import mechanism is vendor-specific, unlike hardware decode selection.** `VAAPI`'s `DMA-BUF`/`EGLImage` trick needs Mesa; `CUDA`-GL interop is a separate NVIDIA mechanism.
- **`AVCodecContext::get_format` can't capture lambda state, only a plain function pointer.** `media_plugin.cpp` passes the wanted hardware pixel format through `codec_ctx->opaque` instead.
- **A decode filter graph can't be built before the first frame decodes.** `CUDA`/`VAAPI` transfer format varies by driver; the graph builds from the first decoded frame.
- **Looping in-process decoded video needs a seek-and-flush, not a process restart.** `av_seek_frame` plus `avcodec_flush_buffers` on EOF replaces the `ffmpeg` CLI's loop flag.
- **A decoder can hold a frame back internally, released only by the next `send_packet` or a flush.** Flushing before draining drops it; send a nullptr flush packet and drain first.
- **A reference's per-output design choice can follow from threading, not correctness.** kokusei is single-threaded, needing one shared EGLContext; the visualizer window is a scoped exception.
- **A glyph missing from the primary font shifts an entire line's baseline, not just that glyph.** `U+00B7` isn't in kokusei's font; Pango's fallback inflates line ascent. Use the em dash.
- **noctalia's render architecture is one GL/scene thread, every style an ordinary `Node` on one opacity pipeline.** kokusei's earlier per-visual special-casing caused divergence; one shared Renderer/Scene path now matches it.
- **An abstraction earns its place only by removing duplication that exists today.** A planned compositor-backend interface was dropped once its motivating duplication was already merged away.
- **Build a generic primitive only once a second real caller is visible, not imaginable.** `DeferredCall` ended up with no caller and stays documented rather than wired in.
- **`~` in a path is a display convention, never a real path.** `std::filesystem` never expands it, so `core/path_home.h` collapses `$HOME` only at UI/JSON edges and every stored or typed path passes `path_expand_home` before use.
- **A default-plus-override config value must be cached on the consumer's own per-monitor state.** Re-resolving the tier chain on every hot-path read would turn 15 reads into map lookups.
- **A resolved-with-fallback accessor and a raw-override accessor answer different questions.** A "remove override" control needs the raw override only; the fallback resolver makes it no-op wrongly.
- **Porting a singleton overlay to per-monitor rendering must split process-wide from per-monitor state.** A D-Bus connection is one per process; the render surface is one per monitor.
- **A shared notification is still dismissed per-monitor by keeping dismissed-state in the per-monitor view.** Each `NotificationView` fades locally-closed ids via its own `AnimationManager`; expire/D-Bus-close stay global.
- **A cross-cutting service with no surface of its own needs a `Service`, not a `Module`.** `Module`/`PerMonitorModule` own a surface; cross-cutting services get the generic-loop treatment via `service.h`.
- **Extending a fallback-inclusive resolver to a second mode needs its own raw-override accessor too.** Adding an animated-column fallback required a matching `_override` accessor to avoid the same pitfall.
- **A fallback gated on `column_index == 0` isn't global, it's whichever monitor resolves column 0 first.** A truly global toggle needs the same check on every column, everywhere.
- **A single-instance overlay bound to one output must not have its open-state read per-monitor.** Every monitor's `bar_paint` saw `starward`/`dashboard` open and expanded everywhere; gate on `bound_output() == mon.output.wl`.
- **Animated mode's empty-column default-wallpaper fallback reuses the static `wallpaper_path` image.** No separate default-animated-wallpaper asset exists; resolve any future one the same way.
- **A per-monitor module's `create_surface()` must not gate on config state.** It runs once with no re-entry; wallpaper gated surface creation on a startup check, breaking toggle-on.
- **A "prepare" step that pre-scales/fps-caps a video duplicates work the live decode filter graph already does.** The software `libx264` transcode caused the CPU spike it aimed to avoid; removed.
- **A zero-copy DRM delivery path bypasses the filter graph entirely, so an `fps=` filter there fixes nothing.** `media_plugin.cpp`'s VAAPI zero-copy branch delivers straight from the decoder, skipping `ensure_filter_graph`.
- **A fixed per-decoded-frame sleep throttles decode rate, not display rate, ignoring source timestamps.** Pace playback via each frame's pts against a wall-clock anchor; drop early frames, re-anchor on seek.
- **One detached decode thread per wallpaper-picker tile permanently bloats the shell's RSS.** ~35 concurrent frame-threaded first-frame decodes made glibc spawn ~8 never-freed 64 MB arenas.
- **A thumbnail decode must scale inside the filter graph, not decode native then downsample.** `decode_first_frame` takes a target box and emits `scale=W:H,format=rgba`, so `MediaFrame.rgba` is KB not MB.
- **First-frame/frame-set decodes pin `avcodec` `thread_count = 1`; frame-threading buys nothing and explodes arenas.** Only `decode_loop` (streaming, VAAPI) stays frame-threaded.
- **`main()` sets `mallopt(M_ARENA_MAX, 2)`.** kokusei's steady-state allocation is main-thread-dominated; capping arenas stops any decode burst permanently inflating RSS.
- **A backend two modules both need lives in `service/`, not one module's header.** The brightness backend moved to `service/brightness_service.*` so `yuheng` could set brightness without a module include.
- **Brightness is set through `brightnessctl`, never a direct `sysfs` write.** `/sys/class/backlight/*/brightness` is root-only without a `uaccess` udev rule; `brightnessctl` routes through `logind`.
- **The notification D-Bus server and `NotificationRecord` store moved to `service/notification_service` once `penance` became its second consumer.** `herald` keeps only its render model, rebuilt by `herald_sync` from the shared records by `id`.
- **`yuheng`'s circular arc gauge moved to `render/arc_gauge` whole, geometry and colors as caller params, once `penance` needed it.** Callers pass their own diameter/stroke/colors; only the arc is cached inside, not caller textures.
- **The toggle switch, device-row connect/forget actions, and centered status text were copy-pasted across the panels before `render/panel_chrome`.** Only the row action's measure step had to split from its draw step, since measured width sizes the text clip.
- **The same flat track+fill progress bar was reimplemented three times, with two byte-identical `kWarnColor` copies.** Moved to `render/progress_bar` with `min_fill_w` a caller param, since `spark`'s OSD has no width floor and panels do.
- **Every shell text input shares one caret blink and one per-character type-in pop, in `render/text_field`.** `overseer`/`penance` each had their own copy; `TextFieldTypeAnim` now serves all four with caller-passed manager and owner-id.
- **Every `penance` card goes through one local `draw_card` chrome: `overlay` fill, 2px `accent` border, radius, optional bold title.** It mirrors `yuheng`'s `card_chrome_draw` but can't share it — that's bound to `yuheng`'s `TextureCache`, and cross-module include is banned.

## 7. Build and workflow

- **A file writer must create its own target directory, not assume something else did.** `write_file_atomic` silently failed `save_config()` on fresh installs; other writers already `mkdir()` first.
- **Only the user runs `dist/install` and `dist/run`.** Both do real sudo actions or launch a live session; `dist/test` is safe to run freely.
- **Batch edits and build once, not after every small change.** Reformat with clang-format after each edit, keeping comments short enough not to wrap.
- **Bare `clang-format -i` reformats the whole file to 2-space LLVM default, not the project's 4-space style.** No `.clang-format` file exists; use `convention.md`'s `--style="{IndentWidth: 4}"` command.
- **Tests are plain `main()` plus `<cassert>`, no framework.** Run timing-sensitive tests repeatedly before trusting them; races can pass once and fail later.
- **Merging a module's pure logic and EGL/GL code into one file forces graphics deps onto the test binary.** Keep the `*_test_sources`/`*_main_only_sources` split so the test binary stays free of EGL/GL.
- **kokusei has no runtime shader preprocessor; flatten ported multi-file shaders at authoring time.** `resonance` inlines every `#include` and hand-expands `#expand` into string fragments assembled at runtime.
- **`glslangValidator` on the flattened shader text catches ES `int`/`float` and undeclared-identifier errors offline.** Dump each stage to a file and validate before a hardware run; it will not prove GL-runtime format support.
- **`meson test` names are the registered test names, not executable file names.** Use `async_process`, not `test_async_process`, when invoking a specific test.
- **Every bundled asset needs the installed-path-plus-dev-tree-fallback loading pattern.** A bare relative path resolves against the daemon's cwd, silently failing outside the source tree.
- **A connect()-to-socket liveness probe is unreliable against a leftover socket file.** Prefer a flock()-guarded lock file, which the kernel releases automatically on process death.
- **Grep the whole tree before hiding a `_detail::` helper in an anonymous namespace.** Some "internal-looking" helpers are actually called directly from other modules or tests.
- **keqing-shell uses a separate `accentAlt` token for tile/chip selection borders, not `accent`.** `accent` is reserved for other UI like the nav rail and toggle track.
- **A ported config header's constants must trace 1:1 to the QML source's actual values.** Invented "roughly similar" numbers drifted from real properties and missed computed geometry.
- **A generically-named `constexpr` constant can collide with an identical name in an unrelated header.** Two modules that never include each other can still land in the same translation unit transitively.
- **An include-path migration script must exclude generated protocol-header includes from rewriting.** They resolve as if under `src/` but are build-directory outputs, breaking only at compile time.
- **Overseer is split: `modules/overseer.cpp` is main-executable-only; pure logic in `src/modules/overseer/*` compiles into both binaries.** A `src/modules/overseer/*` file can't gain a `WaylandState`-typed function; the file boundary enforces it.
- **A module can't include another module's header, and `kokusei.cpp` can't name a module's function directly.** Cross-module orchestration — IPC verb table, key-dispatch table — lives in `src/app/` instead.
- **One module can still trigger another by name through the generic `Module` interface.** Find it in `app.overlays` by `name()`, then call its `ipc_handlers()` and invoke the matching verb.
- **A shared helper that drifted into one feature module's directory pulls every later caller across the boundary.** `spawn_detached`/`resolve_app_icon_path` had no launcher-specific logic; moving to `core/`/`service/` fixed every caller.
- **A generic dispatcher needing another module's `open` flag should take a `bool`, not the full state struct.** `panel_pill()` took full state structs to read two fields; it now resolves bools itself.
- **An enum shared between a module and its infrastructure-layer consumer belongs in `config/`, not the module's header.** `SettingsFieldId` lived in `settings.h`, forcing the service to include the whole module; moved to `config/`.
- **A pure-logic function needed by a second feature module should move to `service/`.** `wallpaper_decode_scaled` lived in `wallpaper.cpp` until the settings tab needed it too, forcing the move.
- **A `grep -rln '#include "modules/'` sweep across the whole tree catches violations a per-file review misses.** A reasoning-based pass found 4 violations; a full-tree grep found 2 more.
- **A feature dir's private draw helper can be a verbatim copy of an existing `render/` primitive.** `bar/widget` had its own `add_rrect_node`/`add_texture_node`, byte-identical to `node_add_*`; grep `render/` first.
- **A bar pill's widget file must be named for the pill, not bundled into a sibling.** `cpu_pill`/`tray_pill` lived in `dashboard_widget.cpp`, so no `system_monitor_widget`/`tray_widget` source existed.
- **Removing a UI feature's draw code but leaving its click-kinds, state field, and handlers reads as live.** The settings dropdown kept `open_dropdown_id`, two `PanelClickKind`s, and handler cases after its last caller went.
- **Every `src/service/` file now ends in `_service`, bare protocol wrappers included.** The old subsystem-vs-wrapper split is gone; `convention.md`'s file-naming note is stale.
- **Renaming a `service/` header needs a tree-wide `grep 'service/<old>\.h'` plus both `meson.build` source arrays.** A per-consumer guess misses transitive includers and the build lists.
- **Modules are named after Keqing, not their function; IPC verbs and code identifiers moved too.** See `index.md`'s `src/modules` for the name↔function map; `starward` is the only kept name.
- **`config.cpp` reads new JSON keys with a legacy fallback.** `section()`/`pick()` try new names (`qixing`/`expanse`/`blink`/...) then old (`bar`/`wallpaper`/`idle`/...); the next save rewrites keys.
- **A blanket identifier rename must protect external-API tokens from word-boundary regex.** `lock_guard`, `pw_thread_loop_lock`, `cairo_matrix`, `ext_session_lock_*`, `icon::lock`/`icon::wallpaper` all get mangled otherwise.

## 8. Hyprland IPC

- **This user's Hyprland build has the classic `dispatch <dispatcher> <args>` string protocol deprecated for Lua.** `hyprctl dispatch <X>` is shorthand for `hl.dispatch(X)`; `X` must be a `hl.dsp.*` call.
- **`hypr_dispatch`'s transport (`"dispatch " + command` over the request socket) is correct; only the argument shape was wrong.** It had zero callers until `hypr_tile_*` proved a `hl.dsp.*` Lua-expression string works.
- **`hypr_refresh` must also run on `activewindowv2`, not just structural events.** Focus changes bump every client's `focusHistoryID`; UI ordered by it goes stale without a re-read.
- **An overlay reading `hypr` state on open should `hypr_refresh` first.** Event-driven state can be arbitrarily stale by the time the user opens the panel.
- **`redraw_all_monitors` pokes only per-monitor modules, never `app.overlays`.** An open app overlay reacting live to an event needs its own `request_frame()` loop over `app.overlays`.
- **The bar's per-monitor workspace pills carry Hyprland's absolute workspace id.** Switching from a pill must call `hypr_tile_focus_workspace` with `global=true`, or `resolve_workspace` remaps it.
- **A bar widget that only emits scene nodes isn't clickable until its hit rects are recorded and routed.** `dispatch_pill_click` scans only the fixed `PillId` array; the workspace row stores and checks its own rects.
