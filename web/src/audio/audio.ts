/**
 * DOMINION // ASCENDANT — procedural audio engine.
 *
 * Everything here is synthesised at runtime with WebAudio: no asset files,
 * no dependencies. The engine is lazy (nothing is built until `unlock()` is
 * called from a user gesture) and defensive (every public entry point is a
 * no-op outside a browser and never throws).
 *
 * Signal graph
 *
 *   pad ─┐
 *   arp ─┤                                         ┌─► musicVerbSend ─┐
 *   drone┼─► musicTone(LP) ─► musicDuck ─► musicVol┤                  ├─► reverb ─► verbReturn ─┐
 *   pulse┘                                         └──────────────────┼───────────────────────► master ─► comp ─► limiter ─► out
 *   sfx voices ─► sfxVol ──────────────► sfxVerbSend ─────────────────┘                          ▲
 *                       └──────────────────────────────────────────────────────────────────────┘
 *
 * Music is a generative ambient score in D (dorian for acts 1–2, aeolian
 * with a raised leading tone for act 3), scheduled with a lookahead
 * scheduler on a ~100ms interval rather than per frame.
 */

export type Sfx =
  | 'click'
  | 'select'
  | 'place'
  | 'complete'
  | 'cycle'
  | 'quest'
  | 'event'
  | 'error'
  | 'raid'
  | 'draft'
  | 'tactic'
  | 'victory'
  | 'defeat'
  | 'coin'

export interface Volumes {
  master: number
  music: number
  sfx: number
  muted: boolean
}

export interface Mood {
  act: 1 | 2 | 3
  tension: number
  paused: boolean
  night: number
}

// ---------------------------------------------------------------------------
// Settings
// ---------------------------------------------------------------------------

const STORAGE_KEY = 'dominion-ascendant.settings.audio'
const DEFAULT_VOLUMES: Volumes = { master: 0.8, music: 0.55, sfx: 0.8, muted: false }

const clamp01 = (v: unknown, fallback: number): number =>
  typeof v === 'number' && Number.isFinite(v) ? Math.min(1, Math.max(0, v)) : fallback

function getStorage(): Storage | null {
  try {
    if (typeof window === 'undefined') return null
    return window.localStorage ?? null
  } catch {
    return null
  }
}

function loadVolumes(): Volumes {
  const v: Volumes = { ...DEFAULT_VOLUMES }
  try {
    const raw = getStorage()?.getItem(STORAGE_KEY)
    if (!raw) return v
    const parsed = JSON.parse(raw) as Partial<Record<keyof Volumes, unknown>> | null
    if (!parsed || typeof parsed !== 'object') return v
    v.master = clamp01(parsed.master, v.master)
    v.music = clamp01(parsed.music, v.music)
    v.sfx = clamp01(parsed.sfx, v.sfx)
    if (typeof parsed.muted === 'boolean') v.muted = parsed.muted
  } catch {
    /* corrupted or unavailable storage: defaults */
  }
  return v
}

function saveVolumes(v: Volumes): void {
  try {
    getStorage()?.setItem(STORAGE_KEY, JSON.stringify(v))
  } catch {
    /* quota / privacy mode: ignore */
  }
}

let volumes: Volumes = loadVolumes()

// ---------------------------------------------------------------------------
// Music material
// ---------------------------------------------------------------------------

interface Chord {
  pad: number[] // MIDI notes for the pad voicing
  bass: number // MIDI note for the sub voice
}

/** Act 1: calm, hopeful D dorian (the B natural gives the lift). */
const ACT1: Chord[] = [
  { pad: [50, 53, 57, 60, 64], bass: 38 }, // Dm9
  { pad: [53, 57, 60, 64], bass: 41 }, // Fmaj7
  { pad: [48, 55, 62, 64], bass: 36 }, // Cadd9
  { pad: [47, 55, 59, 64], bass: 35 }, // G6/B
]
/** Act 2: more motion, borrowed Bb for weight. */
const ACT2: Chord[] = [
  { pad: [50, 53, 57, 60], bass: 38 }, // Dm7
  { pad: [46, 53, 57, 62], bass: 34 }, // Bbmaj7
  { pad: [53, 57, 60, 67], bass: 41 }, // Fadd9
  { pad: [48, 55, 60, 62], bass: 36 }, // Csus2
  { pad: [50, 53, 57, 64], bass: 38 }, // Dm9
  { pad: [45, 52, 55, 60], bass: 33 }, // Am7
  { pad: [46, 50, 53, 57], bass: 34 }, // Bbmaj7
  { pad: [48, 52, 55, 62], bass: 36 }, // Cadd9
]
/** Act 3: darker D minor with a harmonic-minor dominant. */
const ACT3: Chord[] = [
  { pad: [50, 53, 57, 64], bass: 38 }, // Dm(add9)
  { pad: [46, 50, 53, 58], bass: 34 }, // Bb
  { pad: [43, 50, 55, 58], bass: 31 }, // Gm
  { pad: [45, 52, 57, 61], bass: 33 }, // A
]
const PROGRESSIONS: Record<1 | 2 | 3, Chord[]> = { 1: ACT1, 2: ACT2, 3: ACT3 }

/** Eighth-note rhythm masks for the bell motif (1 = probable hit). */
const ARP_RHYTHM: Record<1 | 2 | 3, number[]> = {
  1: [0.9, 0, 0, 0.55, 0, 0, 0.7, 0, 0.35, 0, 0, 0.5, 0, 0, 0.25, 0],
  2: [0.95, 0, 0.6, 0.8, 0, 0.7, 0.85, 0, 0.9, 0, 0.6, 0.75, 0, 0.55, 0.8, 0.3],
  3: [0.9, 0, 0, 0.7, 0, 0.5, 0, 0, 0.85, 0, 0, 0.65, 0, 0.45, 0, 0.3],
}
const ARP_RANGE: Record<1 | 2 | 3, [number, number]> = { 1: [62, 84], 2: [60, 81], 3: [55, 76] }
const BPM: Record<1 | 2 | 3, number> = { 1: 64, 2: 78, 3: 70 }
const STEPS_PER_CHORD = 16 // eighth notes
const LOOKAHEAD = 0.35 // seconds
const TICK_MS = 100
const MAX_MUSIC_VOICES = 40
const MAX_SFX_VOICES = 48

const mtof = (m: number): number => 440 * Math.pow(2, (m - 69) / 12)
const rand = (a: number, b: number): number => a + Math.random() * (b - a)

function arpPool(chord: Chord, act: 1 | 2 | 3): number[] {
  const [lo, hi] = ARP_RANGE[act]
  const out = new Set<number>()
  for (const p of chord.pad) {
    for (let o = -12; o <= 36; o += 12) {
      const n = p + o
      if (n >= lo && n <= hi) out.add(n)
    }
  }
  return [...out].sort((a, b) => a - b)
}

// ---------------------------------------------------------------------------
// Engine
// ---------------------------------------------------------------------------

interface Engine {
  ctx: AudioContext
  master: GainNode
  reverb: ConvolverNode
  musicVol: GainNode
  musicDuck: GainNode
  musicTone: BiquadFilterNode
  padBus: GainNode
  arpBus: GainNode
  pulseBus: GainNode
  droneGain: GainNode
  droneFilter: BiquadFilterNode
  sfxVol: GainNode
  sfxVerb: GainNode
  noise: AudioBuffer
  timer: ReturnType<typeof setInterval> | null
  // scheduler state
  nextTime: number
  step: number
  chordIdx: number
  progAct: 1 | 2 | 3
  motif: number[]
  musicVoices: number
  sfxVoices: number
}

let engine: Engine | null = null
let mood: Mood = { act: 1, tension: 0, paused: false, night: 0 }
const lastPlayed: Partial<Record<Sfx, number>> = {}

function getCtor(): typeof AudioContext | null {
  if (typeof window === 'undefined') return null
  const w = window as unknown as {
    AudioContext?: typeof AudioContext
    webkitAudioContext?: typeof AudioContext
  }
  return w.AudioContext ?? w.webkitAudioContext ?? null
}

function makeImpulse(ctx: AudioContext, seconds: number, decay: number): AudioBuffer {
  const rate = ctx.sampleRate
  const len = Math.max(1, Math.floor(rate * seconds))
  const pre = Math.floor(rate * 0.018)
  const buf = ctx.createBuffer(2, len, rate)
  for (let ch = 0; ch < 2; ch++) {
    const data = buf.getChannelData(ch)
    let lp = 0
    for (let i = pre; i < len; i++) {
      const x = (i - pre) / (len - pre)
      // Tail darkens over time: one-pole lowpass coefficient falls with x.
      const a = 0.85 - 0.7 * x
      lp += a * (Math.random() * 2 - 1 - lp)
      data[i] = lp * Math.pow(1 - x, decay)
    }
    // A few sparse early reflections for a sense of hall size.
    for (let r = 0; r < 6; r++) {
      const idx = pre + Math.floor(rate * rand(0.005, 0.08))
      if (idx < len) data[idx] += (Math.random() < 0.5 ? -1 : 1) * rand(0.2, 0.45)
    }
  }
  return buf
}

function makeNoise(ctx: AudioContext): AudioBuffer {
  const len = Math.floor(ctx.sampleRate * 2)
  const buf = ctx.createBuffer(1, len, ctx.sampleRate)
  const d = buf.getChannelData(0)
  for (let i = 0; i < len; i++) d[i] = Math.random() * 2 - 1
  return buf
}

function build(): Engine | null {
  const Ctor = getCtor()
  if (!Ctor) return null
  const ctx = new Ctor({ latencyHint: 'interactive' })

  const master = ctx.createGain()
  master.gain.value = volumes.muted ? 0 : volumes.master

  const comp = ctx.createDynamicsCompressor()
  comp.threshold.value = -18
  comp.knee.value = 12
  comp.ratio.value = 3
  comp.attack.value = 0.012
  comp.release.value = 0.25

  const limiter = ctx.createDynamicsCompressor()
  limiter.threshold.value = -2
  limiter.knee.value = 0
  limiter.ratio.value = 20
  limiter.attack.value = 0.002
  limiter.release.value = 0.1

  master.connect(comp)
  comp.connect(limiter)
  limiter.connect(ctx.destination)

  const reverb = ctx.createConvolver()
  reverb.buffer = makeImpulse(ctx, 3.4, 2.6)
  const verbReturn = ctx.createGain()
  verbReturn.gain.value = 0.9
  reverb.connect(verbReturn)
  verbReturn.connect(master)

  // Music chain
  const musicVol = ctx.createGain()
  musicVol.gain.value = volumes.music * 0.7
  musicVol.connect(master)
  const musicVerbSend = ctx.createGain()
  musicVerbSend.gain.value = 0.42
  musicVol.connect(musicVerbSend)
  musicVerbSend.connect(reverb)

  const musicDuck = ctx.createGain()
  musicDuck.gain.value = 0
  musicDuck.connect(musicVol)
  const musicTone = ctx.createBiquadFilter()
  musicTone.type = 'lowpass'
  musicTone.frequency.value = 9000
  musicTone.Q.value = 0.5
  musicTone.connect(musicDuck)

  const padBus = ctx.createGain()
  padBus.gain.value = 0.55
  padBus.connect(musicTone)
  const arpBus = ctx.createGain()
  arpBus.gain.value = 0.32
  arpBus.connect(musicTone)
  const pulseBus = ctx.createGain()
  pulseBus.gain.value = 0
  pulseBus.connect(musicTone)

  // Drone: persistent, very cheap (3 oscillators + 1 LFO for the whole session).
  const droneGain = ctx.createGain()
  droneGain.gain.value = 0
  droneGain.connect(musicTone)
  const droneFilter = ctx.createBiquadFilter()
  droneFilter.type = 'lowpass'
  droneFilter.frequency.value = 260
  droneFilter.Q.value = 2
  droneFilter.connect(droneGain)
  const d1 = ctx.createOscillator()
  d1.type = 'sine'
  d1.frequency.value = mtof(38)
  const d2 = ctx.createOscillator()
  d2.type = 'sawtooth'
  d2.frequency.value = mtof(38)
  d2.detune.value = -6
  const d3 = ctx.createOscillator()
  d3.type = 'sawtooth'
  d3.frequency.value = mtof(45)
  d3.detune.value = 5
  const d1g = ctx.createGain()
  d1g.gain.value = 0.7
  const d2g = ctx.createGain()
  d2g.gain.value = 0.22
  const d3g = ctx.createGain()
  d3g.gain.value = 0.1
  d1.connect(d1g).connect(droneFilter)
  d2.connect(d2g).connect(droneFilter)
  d3.connect(d3g).connect(droneFilter)
  const lfo = ctx.createOscillator()
  lfo.frequency.value = 0.045
  const lfoAmt = ctx.createGain()
  lfoAmt.gain.value = 120
  lfo.connect(lfoAmt).connect(droneFilter.frequency)
  const t0 = ctx.currentTime
  d1.start(t0)
  d2.start(t0)
  d3.start(t0)
  lfo.start(t0)

  // SFX chain
  const sfxVol = ctx.createGain()
  sfxVol.gain.value = volumes.sfx
  sfxVol.connect(master)
  const sfxVerb = ctx.createGain()
  sfxVerb.gain.value = 0.22
  sfxVol.connect(sfxVerb)
  sfxVerb.connect(reverb)

  return {
    ctx,
    master,
    reverb,
    musicVol,
    musicDuck,
    musicTone,
    padBus,
    arpBus,
    pulseBus,
    droneGain,
    droneFilter,
    sfxVol,
    sfxVerb,
    noise: makeNoise(ctx),
    timer: null,
    nextTime: ctx.currentTime + 0.2,
    step: 0,
    chordIdx: -1,
    progAct: mood.act,
    motif: [0, 2, 1, 3, 2, 4, 3, 1],
    musicVoices: 0,
    sfxVoices: 0,
  }
}

// ---------------------------------------------------------------------------
// Voice helpers
// ---------------------------------------------------------------------------

type Kind = 'music' | 'sfx'

function canVoice(e: Engine, kind: Kind, n = 1): boolean {
  return kind === 'music'
    ? e.musicVoices + n <= MAX_MUSIC_VOICES
    : e.sfxVoices + n <= MAX_SFX_VOICES
}

/** Start/stop a source and disconnect its private node chain when it ends. */
function run(e: Engine, kind: Kind, src: AudioScheduledSourceNode, nodes: AudioNode[], t: number, end: number): void {
  if (kind === 'music') e.musicVoices++
  else e.sfxVoices++
  src.onended = () => {
    if (kind === 'music') e.musicVoices = Math.max(0, e.musicVoices - 1)
    else e.sfxVoices = Math.max(0, e.sfxVoices - 1)
    try {
      src.disconnect()
    } catch {
      /* already gone */
    }
    for (const n of nodes) {
      try {
        n.disconnect()
      } catch {
        /* already gone */
      }
    }
  }
  src.start(t)
  src.stop(end)
}

/** Attack / hold / exponential-release envelope. Returns the end time. */
function envelope(p: AudioParam, t: number, peak: number, a: number, h: number, r: number): number {
  const pk = Math.max(0.0002, peak)
  p.setValueAtTime(0.0001, t)
  p.linearRampToValueAtTime(pk, t + a)
  if (h > 0) p.setValueAtTime(pk, t + a + h)
  p.exponentialRampToValueAtTime(0.0001, t + a + h + r)
  return t + a + h + r
}

function makePanner(e: Engine, pan: number, dest: AudioNode): AudioNode {
  if (pan === 0 || typeof e.ctx.createStereoPanner !== 'function') return dest
  const p = e.ctx.createStereoPanner()
  p.pan.value = Math.max(-1, Math.min(1, pan))
  p.connect(dest)
  return p
}

interface ToneOpts {
  f: number
  t: number
  g: number
  r: number // release (decay) time
  a?: number
  h?: number
  type?: OscillatorType
  f2?: number // glide target
  glide?: number // glide duration
  detune?: number
  pan?: number
  lp?: number // lowpass cutoff start
  lp2?: number // lowpass cutoff end
  q?: number
  kind?: Kind
  dest?: AudioNode
}

function tone(e: Engine, o: ToneOpts): void {
  const kind = o.kind ?? 'sfx'
  if (!canVoice(e, kind)) return
  const { ctx } = e
  const a = o.a ?? 0.005
  const h = o.h ?? 0
  const osc = ctx.createOscillator()
  osc.type = o.type ?? 'sine'
  osc.frequency.setValueAtTime(o.f, o.t)
  if (o.f2 !== undefined) osc.frequency.exponentialRampToValueAtTime(Math.max(1, o.f2), o.t + (o.glide ?? a + h + o.r))
  if (o.detune) osc.detune.value = o.detune
  const g = ctx.createGain()
  const end = envelope(g.gain, o.t, o.g, a, h, o.r)
  const nodes: AudioNode[] = [g]
  const head: AudioNode = g
  if (o.lp !== undefined) {
    const f = ctx.createBiquadFilter()
    f.type = 'lowpass'
    f.Q.value = o.q ?? 0.7
    f.frequency.setValueAtTime(o.lp, o.t)
    if (o.lp2 !== undefined) f.frequency.exponentialRampToValueAtTime(Math.max(20, o.lp2), end)
    osc.connect(f)
    f.connect(g)
    nodes.push(f)
  } else {
    osc.connect(g)
  }
  const out = makePanner(e, o.pan ?? 0, o.dest ?? e.sfxVol)
  if (out !== (o.dest ?? e.sfxVol)) nodes.push(out)
  head.connect(out)
  run(e, kind, osc, nodes, o.t, end + 0.05)
}

interface NoiseOpts {
  t: number
  g: number
  r: number
  a?: number
  h?: number
  type?: BiquadFilterType
  f: number
  f2?: number
  q?: number
  pan?: number
  kind?: Kind
  dest?: AudioNode
}

function noise(e: Engine, o: NoiseOpts): void {
  const kind = o.kind ?? 'sfx'
  if (!canVoice(e, kind)) return
  const { ctx } = e
  const src = ctx.createBufferSource()
  src.buffer = e.noise
  src.loop = true
  const offset = Math.random() * 1.5
  const f = ctx.createBiquadFilter()
  f.type = o.type ?? 'bandpass'
  f.Q.value = o.q ?? 1
  f.frequency.setValueAtTime(o.f, o.t)
  const g = ctx.createGain()
  const end = envelope(g.gain, o.t, o.g, o.a ?? 0.003, o.h ?? 0, o.r)
  if (o.f2 !== undefined) f.frequency.exponentialRampToValueAtTime(Math.max(20, o.f2), end)
  src.connect(f)
  f.connect(g)
  const dest = o.dest ?? e.sfxVol
  const out = makePanner(e, o.pan ?? 0, dest)
  g.connect(out)
  const nodes: AudioNode[] = [f, g]
  if (out !== dest) nodes.push(out)
  if (kind === 'music') e.musicVoices++
  else e.sfxVoices++
  src.onended = () => {
    if (kind === 'music') e.musicVoices = Math.max(0, e.musicVoices - 1)
    else e.sfxVoices = Math.max(0, e.sfxVoices - 1)
    try {
      src.disconnect()
    } catch {
      /* noop */
    }
    for (const n of nodes) {
      try {
        n.disconnect()
      } catch {
        /* noop */
      }
    }
  }
  src.start(o.t, offset)
  src.stop(end + 0.05)
}

/** Two-operator FM bell: warm, glassy, decays naturally. */
function bell(
  e: Engine,
  f: number,
  t: number,
  g: number,
  dur: number,
  opts: { ratio?: number; index?: number; pan?: number; kind?: Kind; dest?: AudioNode } = {},
): void {
  const kind = opts.kind ?? 'sfx'
  if (!canVoice(e, kind, 2)) return
  const { ctx } = e
  const car = ctx.createOscillator()
  car.type = 'sine'
  car.frequency.value = f
  const mod = ctx.createOscillator()
  mod.type = 'sine'
  mod.frequency.value = f * (opts.ratio ?? 3.5)
  const modGain = ctx.createGain()
  const idx = f * (opts.index ?? 1.6)
  modGain.gain.setValueAtTime(idx, t)
  modGain.gain.exponentialRampToValueAtTime(Math.max(0.01, idx * 0.04), t + dur * 0.6)
  mod.connect(modGain)
  modGain.connect(car.frequency)
  const amp = ctx.createGain()
  const end = envelope(amp.gain, t, g, 0.004, 0, dur)
  car.connect(amp)
  const dest = opts.dest ?? e.sfxVol
  const out = makePanner(e, opts.pan ?? 0, dest)
  amp.connect(out)
  const nodes: AudioNode[] = [amp]
  if (out !== dest) nodes.push(out)
  run(e, kind, car, nodes, t, end + 0.05)
  run(e, kind, mod, [modGain], t, end + 0.05)
}

/** Soft brass-like ensemble voice: detuned saws through an opening lowpass. */
function brass(e: Engine, m: number, t: number, g: number, a: number, h: number, r: number, bright = 2400, pan = 0): void {
  const f = mtof(m)
  tone(e, { f, t, g, a, h, r, type: 'sawtooth', detune: -7, lp: 300, lp2: bright, q: 0.9, pan: pan - 0.15 })
  tone(e, { f, t, g: g * 0.8, a, h, r, type: 'sawtooth', detune: 7, lp: 300, lp2: bright, q: 0.9, pan: pan + 0.15 })
}

/** Low drum: pitched sine drop plus a short noise skin. */
function drum(e: Engine, t: number, g: number, f0 = 110, f1 = 42, decay = 0.45, kind: Kind = 'sfx', dest?: AudioNode): void {
  tone(e, { f: f0, f2: f1, glide: 0.12, t, g, a: 0.003, r: decay, kind, dest })
  noise(e, { t, g: g * 0.25, r: 0.06, type: 'lowpass', f: 900, kind, dest })
}

// ---------------------------------------------------------------------------
// Music scheduler
// ---------------------------------------------------------------------------

function stepDur(): number {
  const bpm = BPM[mood.act] + (mood.act === 3 ? mood.tension * 18 : mood.tension * 6)
  return 30 / bpm // eighth note
}

function musicAudible(): boolean {
  return !volumes.muted && volumes.master > 0.001 && volumes.music > 0.001
}

function schedulePad(e: Engine, chord: Chord, t: number, dur: number): void {
  const { ctx } = e
  const voices = chord.pad.length * 2 + 1
  if (!canVoice(e, 'music', voices)) return
  const a = Math.min(2.8, dur * 0.4)
  const r = 3.2
  const h = Math.max(0.1, dur - a)
  const end = t + a + h + r

  // One shared breathing filter + gain per chord keeps the node count low.
  const filt = ctx.createBiquadFilter()
  filt.type = 'lowpass'
  filt.Q.value = 0.8
  const base = mood.act === 3 ? 700 : mood.act === 2 ? 1300 : 1000
  const dark = 1 - mood.night * 0.4
  filt.frequency.setValueAtTime(base * 0.55 * dark, t)
  filt.frequency.linearRampToValueAtTime(base * 1.5 * dark, t + dur * 0.55)
  filt.frequency.linearRampToValueAtTime(base * 0.8 * dark, end)
  const amp = ctx.createGain()
  const level = 0.16 / Math.sqrt(chord.pad.length)
  amp.gain.setValueAtTime(0.0001, t)
  amp.gain.linearRampToValueAtTime(level, t + a)
  amp.gain.setValueAtTime(level, t + a + h)
  amp.gain.exponentialRampToValueAtTime(0.0001, end)
  filt.connect(amp)
  amp.connect(e.padBus)

  const shared: AudioNode[] = [filt, amp]
  let remaining = chord.pad.length * 2 + 1
  const release = () => {
    remaining--
    if (remaining <= 0) {
      for (const n of shared) {
        try {
          n.disconnect()
        } catch {
          /* noop */
        }
      }
    }
  }
  const startOsc = (freq: number, type: OscillatorType, detune: number, gain: number, pan: number) => {
    const o = ctx.createOscillator()
    o.type = type
    o.frequency.value = freq
    o.detune.value = detune
    const g = ctx.createGain()
    g.gain.value = gain
    o.connect(g)
    const out = makePanner(e, pan, filt)
    g.connect(out)
    const own: AudioNode[] = [g]
    if (out !== filt) own.push(out)
    e.musicVoices++
    o.onended = () => {
      e.musicVoices = Math.max(0, e.musicVoices - 1)
      try {
        o.disconnect()
      } catch {
        /* noop */
      }
      for (const n of own) {
        try {
          n.disconnect()
        } catch {
          /* noop */
        }
      }
      release()
    }
    o.start(t)
    o.stop(end + 0.05)
  }
  chord.pad.forEach((m, i) => {
    const f = mtof(m)
    const spread = (i / Math.max(1, chord.pad.length - 1)) * 0.7 - 0.35
    startOsc(f, 'sawtooth', rand(-9, -4), 0.5, spread - 0.2)
    startOsc(f, 'sawtooth', rand(4, 9), 0.5, spread + 0.2)
  })
  // Sub voice anchors the harmony under the drone.
  startOsc(mtof(chord.bass), 'triangle', 0, mood.act === 3 ? 1.3 : 1.0, 0)
}

function mutateMotif(e: Engine, poolLen: number): void {
  const m = e.motif
  const r = Math.random()
  if (r < 0.35) {
    const i = Math.floor(Math.random() * m.length)
    const j = Math.floor(Math.random() * m.length)
    ;[m[i], m[j]] = [m[j], m[i]]
  } else if (r < 0.7) {
    const i = Math.floor(Math.random() * m.length)
    m[i] = Math.max(0, m[i] + (Math.random() < 0.5 ? -1 : 1))
  } else if (r < 0.85) {
    m.reverse()
  }
  for (let i = 0; i < m.length; i++) m[i] = Math.min(Math.max(0, m[i]), Math.max(0, poolLen - 1))
}

function heartbeat(e: Engine, t: number, level: number): void {
  drum(e, t, 0.55 * level, 78, 38, 0.5, 'music', e.pulseBus)
  drum(e, t + 0.19, 0.36 * level, 70, 36, 0.42, 'music', e.pulseBus)
}

function scheduleStep(e: Engine, t: number): void {
  const act = mood.act
  const sd = stepDur()
  const stepInChord = e.step % STEPS_PER_CHORD
  const audible = musicAudible()

  if (stepInChord === 0) {
    if (e.progAct !== act) {
      e.progAct = act
      e.chordIdx = -1
    }
    const prog = PROGRESSIONS[e.progAct]
    e.chordIdx = (e.chordIdx + 1) % prog.length
    const chord = prog[e.chordIdx]
    mutateMotif(e, arpPool(chord, e.progAct).length)
    if (audible) schedulePad(e, chord, t, sd * STEPS_PER_CHORD)
  }

  if (audible) {
    const prog = PROGRESSIONS[e.progAct]
    const chord = prog[Math.max(0, e.chordIdx)]
    const pool = arpPool(chord, e.progAct)

    // Bell motif.
    const p = ARP_RHYTHM[act][stepInChord] * (mood.paused ? 0.5 : 1)
    if (p > 0 && pool.length > 0 && Math.random() < p) {
      const idx = e.motif[e.step % e.motif.length] % pool.length
      const note = pool[idx]
      const accent = stepInChord % 4 === 0 ? 1 : 0.7
      const swing = stepInChord % 2 === 1 ? sd * 0.08 : 0
      bell(e, mtof(note), t + swing, 0.22 * accent, act === 2 ? 1.4 : 2.2, {
        ratio: act === 3 ? 2.76 : 3.5,
        index: act === 3 ? 1.2 : 0.9,
        pan: rand(-0.45, 0.45),
        kind: 'music',
        dest: e.arpBus,
      })
      // Occasional soft octave echo for shimmer.
      if (act !== 3 && Math.random() < 0.18) {
        bell(e, mtof(note + 12), t + sd * 1.5, 0.06, 1.6, { kind: 'music', dest: e.arpBus, pan: rand(-0.6, 0.6) })
      }
    }

    // Motion: soft offbeat ticks in acts 2–3.
    if (act >= 2 && stepInChord % 2 === 1 && !mood.paused) {
      const lvl = act === 2 ? 0.05 : 0.025 + mood.tension * 0.05
      if (Math.random() < 0.85) {
        noise(e, { t, g: lvl, r: 0.05, type: 'highpass', f: 7000, q: 0.7, pan: rand(-0.3, 0.3), kind: 'music', dest: e.arpBus })
      }
    }

    // Heartbeat pulse scales with tension (always in act 3, late tension in act 2).
    const pulse = act === 3 ? 0.3 + 0.7 * mood.tension : act === 2 ? Math.max(0, mood.tension - 0.55) * 1.4 : 0
    if (pulse > 0.02) {
      const every = act === 3 && mood.tension > 0.6 ? 2 : 4
      if (stepInChord % every === 0) heartbeat(e, t, pulse)
    }
    // Low swell at the top of each chord in act 3.
    if (act === 3 && stepInChord === 0 && mood.tension > 0.4) {
      drum(e, t, 0.3 * mood.tension, 60, 32, 1.4, 'music', e.pulseBus)
    }
  }

  e.step++
  e.nextTime += sd
}

function tick(): void {
  const e = engine
  if (!e) return
  try {
    if (e.ctx.state !== 'running') return
    const now = e.ctx.currentTime
    // Tab was throttled / context was suspended: resync instead of bursting.
    if (e.nextTime < now - 0.2) e.nextTime = now + 0.05
    let guard = 0
    while (e.nextTime < now + LOOKAHEAD && guard++ < 16) scheduleStep(e, e.nextTime)
  } catch {
    /* never let the scheduler throw */
  }
}

function applyMood(e: Engine, immediate = false): void {
  const { ctx } = e
  const t = ctx.currentTime
  const tc = immediate ? 0.05 : 1.2
  const act = mood.act
  const baseCut = act === 3 ? 6000 : act === 2 ? 11000 : 8500
  const cut = mood.paused ? 480 : baseCut * (1 - 0.45 * mood.night)
  e.musicTone.frequency.setTargetAtTime(cut, t, mood.paused ? 0.25 : tc)
  e.musicDuck.gain.setTargetAtTime(mood.paused ? 0.45 : 1, t, mood.paused ? 0.2 : 0.8)
  const drone = (act === 3 ? 0.2 + mood.tension * 0.08 : act === 2 ? 0.14 : 0.11) * (1 + mood.night * 0.2)
  e.droneGain.gain.setTargetAtTime(drone, t, tc * 1.5)
  e.droneFilter.frequency.setTargetAtTime((act === 3 ? 200 : 300) * (1 - 0.3 * mood.night), t, tc * 2)
  e.padBus.gain.setTargetAtTime(act === 2 ? 0.5 : 0.55, t, tc)
  e.arpBus.gain.setTargetAtTime((act === 2 ? 0.36 : act === 3 ? 0.26 : 0.3) * (1 - 0.25 * mood.night), t, tc)
  e.pulseBus.gain.setTargetAtTime(act === 3 ? 0.9 : act === 2 ? 0.6 : 0, t, tc)
}

function applyVolumes(e: Engine): void {
  const t = e.ctx.currentTime
  e.master.gain.setTargetAtTime(volumes.muted ? 0 : volumes.master, t, 0.04)
  e.musicVol.gain.setTargetAtTime(volumes.music * 0.7, t, 0.08)
  e.sfxVol.gain.setTargetAtTime(volumes.sfx, t, 0.04)
}

// ---------------------------------------------------------------------------
// Sound effects
// ---------------------------------------------------------------------------

const D = (m: number) => mtof(m)

const SFX: Record<Sfx, (e: Engine, t: number) => void> = {
  click(e, t) {
    noise(e, { t, g: 0.12, r: 0.018, type: 'highpass', f: 3500, q: 0.8 })
    tone(e, { f: 1850, f2: 1500, glide: 0.03, t, g: 0.06, r: 0.035 })
  },

  cycle(e, t) {
    tone(e, { f: 2600, t, g: 0.025, r: 0.02 })
    noise(e, { t, g: 0.03, r: 0.012, type: 'highpass', f: 6000 })
  },

  select(e, t) {
    noise(e, { t, g: 0.1, a: 0.04, r: 0.16, type: 'bandpass', f: 500, f2: 2600, q: 1.4 })
    tone(e, { f: D(74), f2: D(81), glide: 0.12, t: t + 0.02, g: 0.07, a: 0.02, r: 0.22, type: 'triangle' })
    bell(e, D(86), t + 0.06, 0.035, 0.4)
  },

  place(e, t) {
    drum(e, t, 0.7, 140, 48, 0.32)
    noise(e, { t, g: 0.18, r: 0.09, type: 'lowpass', f: 600 })
    // Metallic settle: inharmonic partials, slightly delayed.
    const partials = [1240, 1873, 2711, 3390]
    partials.forEach((f, i) => tone(e, { f: f * rand(0.98, 1.02), t: t + 0.035 + i * 0.008, g: 0.03 / (i + 1), r: 0.45 - i * 0.06, pan: rand(-0.3, 0.3) }))
    noise(e, { t: t + 0.04, g: 0.03, r: 0.12, type: 'bandpass', f: 4200, q: 6 })
  },

  complete(e, t) {
    ;[69, 74, 78, 81, 86].forEach((m, i) => bell(e, D(m), t + i * 0.065, 0.12 - i * 0.012, 1.1, { pan: -0.3 + i * 0.15 }))
    tone(e, { f: D(62), t, g: 0.05, a: 0.03, r: 0.6, type: 'triangle' })
  },

  coin(e, t) {
    bell(e, D(83), t, 0.08, 0.45, { ratio: 2, index: 0.8, pan: -0.15 })
    bell(e, D(88), t + 0.055, 0.07, 0.6, { ratio: 2, index: 0.8, pan: 0.15 })
    noise(e, { t, g: 0.025, r: 0.15, type: 'highpass', f: 8000 })
  },

  quest(e, t) {
    // D major fanfare arpeggio resolving onto a held chord.
    const steps = [62, 66, 69, 74]
    steps.forEach((m, i) => brass(e, m, t + i * 0.12, 0.07, 0.02, 0.08, 0.25, 2800, -0.2 + i * 0.13))
    const hit = t + 0.48
    ;[50, 57, 62, 66, 69, 74].forEach((m, i) => brass(e, m, hit, 0.045 - i * 0.003, 0.05, 0.35, 0.4, 3200, -0.3 + i * 0.12))
    ;[78, 81, 86].forEach((m, i) => bell(e, D(m), hit + 0.05 + i * 0.08, 0.06, 1.0, { pan: 0.25 - i * 0.2 }))
    drum(e, hit, 0.35, 100, 45, 0.6)
  },

  event(e, t) {
    // Ominous two-tone alert: E4 then Bb3 (tritone), over a low swell.
    const two = [64, 58]
    two.forEach((m, i) => {
      const ti = t + i * 0.32
      tone(e, { f: D(m), t: ti, g: 0.09, a: 0.02, h: 0.12, r: 0.3, type: 'square', lp: 1600, lp2: 700, q: 1.2 })
      tone(e, { f: D(m), t: ti, g: 0.05, a: 0.02, h: 0.12, r: 0.35, type: 'triangle', detune: 8 })
    })
    tone(e, { f: D(38), t, g: 0.12, a: 0.25, h: 0.3, r: 0.6, type: 'sawtooth', lp: 180, lp2: 120 })
  },

  error(e, t) {
    tone(e, { f: 110, t, g: 0.09, a: 0.005, h: 0.1, r: 0.12, type: 'sawtooth', lp: 520, lp2: 300 })
    tone(e, { f: 116.5, t, g: 0.07, a: 0.005, h: 0.1, r: 0.12, type: 'square', lp: 420, lp2: 260 })
    noise(e, { t, g: 0.04, r: 0.08, type: 'lowpass', f: 400 })
  },

  raid(e, t) {
    // Alarm swell: two detuned siren saws gliding up through a widening band.
    for (const det of [-10, 10]) {
      tone(e, { f: 330, f2: 520, glide: 0.75, t, g: 0.07, a: 0.55, h: 0.2, r: 0.25, type: 'sawtooth', detune: det, lp: 600, lp2: 2600, q: 3, pan: det / 30 })
    }
    noise(e, { t, g: 0.06, a: 0.7, r: 0.1, type: 'bandpass', f: 400, f2: 3000, q: 0.8 })
    // Impact.
    const hit = t + 0.82
    drum(e, hit, 0.9, 90, 30, 1.1)
    noise(e, { t: hit, g: 0.3, r: 0.6, type: 'lowpass', f: 2400, f2: 200 })
    tone(e, { f: D(38), t: hit, g: 0.12, a: 0.01, h: 0.2, r: 1.0, type: 'sawtooth', lp: 400, lp2: 90 })
  },

  draft(e, t) {
    // Card shuffle: a ragged run of short paper-like bursts.
    let ti = t
    for (let i = 0; i < 7; i++) {
      noise(e, { t: ti, g: 0.06 + Math.random() * 0.03, r: 0.035, type: 'bandpass', f: rand(2200, 4200), q: 1.2, pan: rand(-0.4, 0.4) })
      ti += rand(0.026, 0.05)
    }
    // Sparkle.
    const pent = [81, 83, 86, 88, 90, 93]
    for (let i = 0; i < 3; i++) {
      const m = pent[Math.floor(Math.random() * pent.length)]
      bell(e, D(m), ti + 0.02 + i * 0.06, 0.04, 0.6, { ratio: 2.5, index: 0.6, pan: rand(-0.5, 0.5) })
    }
  },

  tactic(e, t) {
    // Resonant power-up sweep into a confirming chime.
    tone(e, { f: 110, f2: 220, glide: 0.5, t, g: 0.07, a: 0.35, h: 0.1, r: 0.2, type: 'sawtooth', lp: 220, lp2: 5500, q: 7 })
    tone(e, { f: 165, f2: 330, glide: 0.5, t, g: 0.05, a: 0.35, h: 0.1, r: 0.2, type: 'sawtooth', detune: 6, lp: 220, lp2: 5000, q: 6 })
    noise(e, { t, g: 0.05, a: 0.45, r: 0.08, type: 'bandpass', f: 600, f2: 6000, q: 1.5 })
    bell(e, D(81), t + 0.5, 0.09, 0.9)
    bell(e, D(88), t + 0.56, 0.06, 1.0)
  },

  victory(e, t) {
    // Triumphant swell: D major, rising into a sustained tutti with timpani.
    drum(e, t, 0.5, 95, 45, 0.9)
    ;[50, 57, 62, 66, 69, 74].forEach((m, i) => brass(e, m, t + i * 0.03, 0.05 - i * 0.003, 0.6, 1.4, 1.2, 3400, -0.35 + i * 0.14))
    tone(e, { f: D(38), t, g: 0.12, a: 0.5, h: 1.5, r: 1.2, type: 'triangle' })
    ;[74, 78, 81, 86, 90].forEach((m, i) => bell(e, D(m), t + 0.3 + i * 0.11, 0.07, 1.8, { pan: -0.4 + i * 0.2 }))
    drum(e, t + 0.9, 0.35, 95, 45, 0.6)
    drum(e, t + 1.05, 0.45, 95, 45, 1.2)
    ;[62, 69, 74].forEach((m) => brass(e, m + 12, t + 1.05, 0.025, 0.2, 0.9, 1.0, 4200))
  },

  defeat(e, t) {
    // Somber descent: Dm → Bb → Gm(add9) in a low register, filters closing.
    const chords = [
      [50, 53, 57, 62],
      [46, 50, 53, 58],
      [43, 50, 55, 57],
    ]
    chords.forEach((c, ci) => {
      const ti = t + ci * 0.9
      const last = ci === chords.length - 1
      c.forEach((m, i) => {
        const f = D(m)
        tone(e, { f, t: ti, g: 0.04, a: 0.25, h: last ? 0.9 : 0.55, r: last ? 1.1 : 0.5, type: 'sawtooth', detune: -6, lp: 1400 - ci * 350, lp2: 250, pan: -0.3 + i * 0.2 })
        tone(e, { f, t: ti, g: 0.035, a: 0.25, h: last ? 0.9 : 0.55, r: last ? 1.1 : 0.5, type: 'triangle', detune: 6, pan: 0.3 - i * 0.2 })
      })
    })
    tone(e, { f: D(38), f2: D(31), glide: 2.6, t, g: 0.14, a: 0.3, h: 1.6, r: 1.1, type: 'sine' })
    drum(e, t, 0.45, 70, 32, 1.4)
    drum(e, t + 1.8, 0.35, 60, 28, 1.6)
  },
}

const MIN_GAP: Partial<Record<Sfx, number>> = { cycle: 0.07, coin: 0.05 }

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

function unlock(): void {
  try {
    if (typeof window === 'undefined') return
    if (!engine) {
      engine = build()
      if (!engine) return
      applyMood(engine, true)
      // Fade the score in gently on first start.
      engine.musicDuck.gain.cancelScheduledValues(engine.ctx.currentTime)
      engine.musicDuck.gain.setValueAtTime(0, engine.ctx.currentTime)
      engine.musicDuck.gain.setTargetAtTime(mood.paused ? 0.45 : 1, engine.ctx.currentTime + 0.1, 1.5)
    }
    const e = engine
    if (e.ctx.state === 'suspended' || (e.ctx.state as string) === 'interrupted') {
      void e.ctx.resume().catch(() => undefined)
    }
    if (e.timer === null) {
      e.nextTime = Math.max(e.nextTime, e.ctx.currentTime + 0.1)
      e.timer = setInterval(tick, TICK_MS)
      tick()
    }
  } catch {
    /* audio unavailable: stay silent */
  }
}

function play(sfx: Sfx): void {
  try {
    const e = engine
    if (!e || volumes.muted || volumes.master <= 0.001 || volumes.sfx <= 0.001) return
    if (e.ctx.state !== 'running') return
    const fn = SFX[sfx]
    if (!fn) return
    const now = e.ctx.currentTime
    const last = lastPlayed[sfx]
    if (last !== undefined && now - last < (MIN_GAP[sfx] ?? 0.04)) return
    lastPlayed[sfx] = now
    fn(e, now + 0.005)
  } catch {
    /* never throw from a sound effect */
  }
}

function setMood(next: Mood): void {
  try {
    if (!next || typeof next !== 'object') return
    const act: 1 | 2 | 3 = next.act === 2 || next.act === 3 ? next.act : 1
    // Quantise continuous inputs so per-frame jitter doesn't retrigger ramps.
    const tension = Math.round(clamp01(next.tension, 0) * 50) / 50
    const night = Math.round(clamp01(next.night, 0) * 50) / 50
    const paused = !!next.paused
    if (act === mood.act && tension === mood.tension && night === mood.night && paused === mood.paused) return
    mood = { act, tension, paused, night }
    if (engine) applyMood(engine)
  } catch {
    /* ignore */
  }
}

function getVolumes(): Volumes {
  return { ...volumes }
}

function setVolumes(v: Partial<Volumes>): void {
  try {
    if (!v || typeof v !== 'object') return
    volumes = {
      master: clamp01(v.master, volumes.master),
      music: clamp01(v.music, volumes.music),
      sfx: clamp01(v.sfx, volumes.sfx),
      muted: typeof v.muted === 'boolean' ? v.muted : volumes.muted,
    }
    saveVolumes(volumes)
    if (engine) applyVolumes(engine)
  } catch {
    /* ignore */
  }
}

export const audio: {
  unlock(): void
  play(sfx: Sfx): void
  setMood(mood: Mood): void
  getVolumes(): Volumes
  setVolumes(v: Partial<Volumes>): void
} = { unlock, play, setMood, getVolumes, setVolumes }

