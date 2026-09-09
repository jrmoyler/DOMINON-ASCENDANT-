import { animate, stagger } from 'animejs'
const motionAllowed = () => !window.matchMedia('(prefers-reduced-motion: reduce)').matches
export function revealIntro() {
  if (!motionAllowed()) return
  return animate('.intro-kicker, .intro-title, .intro-sub, .intro-body, .intro-supplies, .intro-actions', { opacity: { from: 0 }, y: { from: 14 }, duration: 600, delay: stagger(55), ease: 'outQuint' })
}
export function revealCommandInterface() {
  if (!motionAllowed()) return
  return animate('.hud-top, .rail-left, .rail-right, .hand-bar', { opacity: { from: 0 }, y: { from: (_, index) => index === 0 ? -12 : 12 }, duration: 460, delay: stagger(50), ease: 'outQuint' })
}
export function announceToast() {
  if (!motionAllowed()) return
  return animate('.toast', { opacity: { from: 0 }, y: { from: -8 }, duration: 280, ease: 'outCubic' })
}
export function settleCard() {
  if (!motionAllowed()) return
  return animate('.card.selected .card-name', { y: { from: 3 }, opacity: { from: .6 }, duration: 220, ease: 'outCubic' })
}
