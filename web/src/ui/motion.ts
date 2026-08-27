import { animate, stagger } from 'animejs'

const motionAllowed = () => !window.matchMedia('(prefers-reduced-motion: reduce)').matches

export function revealIntro() {
  if (!motionAllowed()) return
  animate('.intro-kicker, .intro-title, .intro-sub, .intro-body, .intro-rule, .intro-actions, .intro-controls', {
    opacity: { from: 0 },
    y: { from: 22 },
    filter: { from: 'blur(8px)', to: 'blur(0px)' },
    duration: 860,
    delay: stagger(85),
    ease: 'outExpo',
  })
}

export function revealCommandInterface() {
  if (!motionAllowed()) return
  animate('.hud-top, .rail-left, .rail-right, .hand-bar', {
    opacity: { from: 0 },
    y: { from: (_, index) => (index === 0 ? -28 : index === 3 ? 38 : 0) },
    x: { from: (_, index) => (index === 1 ? -28 : index === 2 ? 28 : 0) },
    duration: 780,
    delay: stagger(90),
    ease: 'outQuint',
  })
}

export function announceToast() {
  if (!motionAllowed()) return
  animate('.toast', {
    opacity: { from: 0 },
    y: { from: -16 },
    scale: { from: 0.96 },
    duration: 420,
    ease: 'outExpo',
  })
}
