import * as THREE from 'three'
import type { CardDefinition, WorldAsset } from '@/game/types'
import { CELL_METRES, rotatedFootprint } from '@/game/grid'
import { FACTION_COLOR, TYPE_STYLE } from './palette'

/** Deterministic pseudo-random in [0,1) seeded by a string. */
function hashUnit(seed: string): number {
  let h = 2166136261
  for (let i = 0; i < seed.length; i++) {
    h ^= seed.charCodeAt(i)
    h = Math.imul(h, 16777619)
  }
  return ((h >>> 0) % 10000) / 10000
}

const boxGeometry = new THREE.BoxGeometry(1, 1, 1)
boxGeometry.translate(0, 0.5, 0)

/**
 * Build a graybox-styled mesh group for one placed asset.
 *
 * Silhouettes are massed from a few boxes rather than a single block so the
 * skyline reads at a glance: a base plinth, a main body, a setback tower for
 * tall types, and a faction-coloured accent that doubles as the status light.
 */
export function createBuildingMesh(asset: WorldAsset, def: CardDefinition): THREE.Group {
  const group = new THREE.Group()
  const [fw, fd] = rotatedFootprint(asset.footprint, asset.rotation)
  const style = TYPE_STYLE[def.cardType] ?? TYPE_STYLE.Civic
  const seed = hashUnit(asset.id + def.id)

  const width = fw * CELL_METRES
  const depth = fd * CELL_METRES
  const inset = 0.16 * CELL_METRES
  const bodyW = Math.max(2, width - inset * 2)
  const bodyD = Math.max(2, depth - inset * 2)

  const scaleByArea = Math.min(1.6, 0.75 + Math.sqrt(fw * fd) * 0.28)
  const height = style.height * scaleByArea * (0.85 + seed * 0.3)

  const bodyMaterial = new THREE.MeshStandardMaterial({
    color: style.color,
    roughness: 0.82,
    metalness: 0.12,
  })

  // Plinth — grounds the mass and reads as the construction pad.
  const plinth = new THREE.Mesh(boxGeometry, bodyMaterial)
  plinth.scale.set(width - inset, Math.max(0.8, height * 0.08), depth - inset)
  plinth.castShadow = true
  plinth.receiveShadow = true
  group.add(plinth)

  // Main body.
  const bodyHeight = height * (style.height > 14 ? 0.62 : 0.9)
  const body = new THREE.Mesh(boxGeometry, bodyMaterial)
  body.scale.set(bodyW, bodyHeight, bodyD)
  body.position.y = Math.max(0.8, height * 0.08)
  body.castShadow = true
  body.receiveShadow = true
  group.add(body)

  // Setback tower for the tall types.
  if (style.height > 14) {
    const tower = new THREE.Mesh(boxGeometry, bodyMaterial)
    const tw = bodyW * (0.5 + seed * 0.16)
    const td = bodyD * (0.5 + seed * 0.16)
    tower.scale.set(tw, height * 0.42, td)
    tower.position.y = body.position.y + bodyHeight
    tower.castShadow = true
    group.add(tower)
  }

  // Faction accent band. Emissive so it doubles as the operational status light.
  const accentMaterial = new THREE.MeshStandardMaterial({
    color: FACTION_COLOR[def.faction] ?? 0x8ea3c0,
    emissive: new THREE.Color(FACTION_COLOR[def.faction] ?? 0x8ea3c0),
    emissiveIntensity: 0.9,
    roughness: 0.4,
  })
  const accent = new THREE.Mesh(boxGeometry, accentMaterial)
  accent.scale.set(bodyW * 1.02, Math.max(0.35, height * 0.035), bodyD * 1.02)
  accent.position.y = body.position.y + bodyHeight * 0.72
  accent.name = 'accent'
  group.add(accent)

  // Utility producers get a visible stack so infrastructure is findable.
  if (def.powerProduced > 0 || def.waterProduced > 0) {
    const stack = new THREE.Mesh(
      new THREE.CylinderGeometry(CELL_METRES * 0.14, CELL_METRES * 0.18, height * 0.9, 10),
      bodyMaterial,
    )
    stack.position.set(bodyW * 0.28, body.position.y + height * 0.45, bodyD * 0.28)
    stack.castShadow = true
    group.add(stack)
  }

  group.userData.assetId = asset.id
  group.userData.accent = accent
  return group
}

/** Translucent ghost used for the placement preview. */
export function createGhostMesh(): THREE.Mesh {
  const material = new THREE.MeshBasicMaterial({
    color: 0x4ade80,
    transparent: true,
    opacity: 0.35,
    depthWrite: false,
  })
  const mesh = new THREE.Mesh(boxGeometry, material)
  mesh.renderOrder = 5
  return mesh
}
