import type { WorldAsset } from './types'

/** Vertical Slice Production Spec v1.1 §8: 32x32 logical cells, 8 m each. */
export const GRID_SIZE = 32
export const CELL_METRES = 8

/** Spec §46: production acceptance target for persistent player-placed assets. */
export const BUILD_LIMIT = 50

export function rotatedFootprint(
  footprint: [number, number],
  rotation: 0 | 1 | 2 | 3,
): [number, number] {
  return rotation % 2 === 0 ? [footprint[0], footprint[1]] : [footprint[1], footprint[0]]
}

export function cellsOf(asset: Pick<WorldAsset, 'x' | 'y' | 'footprint' | 'rotation'>): number[] {
  const [w, d] = rotatedFootprint(asset.footprint, asset.rotation)
  const cells: number[] = []
  for (let dx = 0; dx < w; dx++) {
    for (let dy = 0; dy < d; dy++) {
      cells.push((asset.y + dy) * GRID_SIZE + (asset.x + dx))
    }
  }
  return cells
}

export function inBounds(x: number, y: number, w: number, d: number): boolean {
  return x >= 0 && y >= 0 && x + w <= GRID_SIZE && y + d <= GRID_SIZE
}

/** Set of every occupied cell index. */
export function occupancy(assets: WorldAsset[]): Set<number> {
  const set = new Set<number>()
  for (const asset of assets) for (const cell of cellsOf(asset)) set.add(cell)
  return set
}

export interface PlacementCheck {
  ok: boolean
  reason?: string
}

export function canPlace(
  assets: WorldAsset[],
  x: number,
  y: number,
  footprint: [number, number],
  rotation: 0 | 1 | 2 | 3,
): PlacementCheck {
  const [w, d] = rotatedFootprint(footprint, rotation)
  if (!inBounds(x, y, w, d)) return { ok: false, reason: 'Outside the build grid' }
  const occupied = occupancy(assets)
  for (const cell of cellsOf({ x, y, footprint, rotation })) {
    if (occupied.has(cell)) return { ok: false, reason: 'Cell already occupied' }
  }
  if (assets.length >= BUILD_LIMIT) {
    return { ok: false, reason: `Build limit reached (${BUILD_LIMIT} assets)` }
  }
  return { ok: true }
}

export function assetAt(assets: WorldAsset[], x: number, y: number): WorldAsset | undefined {
  const target = y * GRID_SIZE + x
  return assets.find((asset) => cellsOf(asset).includes(target))
}

/** Chebyshev distance between two assets' nearest cells, in grid cells. */
export function assetDistance(a: WorldAsset, b: WorldAsset): number {
  const [aw, ad] = rotatedFootprint(a.footprint, a.rotation)
  const [bw, bd] = rotatedFootprint(b.footprint, b.rotation)
  const dx = Math.max(0, Math.max(a.x - (b.x + bw - 1), b.x - (a.x + aw - 1)))
  const dy = Math.max(0, Math.max(a.y - (b.y + bd - 1), b.y - (a.y + ad - 1)))
  return Math.max(dx, dy)
}
