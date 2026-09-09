import { BoxGeometry, BufferGeometry, CylinderGeometry, ExtrudeGeometry, Matrix4, Shape, SphereGeometry } from 'three'
import { mergeGeometries } from 'three/examples/jsm/utils/BufferGeometryUtils.js'
import type { CardType } from '@/game/types'

export type ArchitectureFinish = 'shell' | 'stone' | 'metal' | 'glass' | 'signal' | 'garden'
export interface ArchitecturePart { name: string; finish: ArchitectureFinish; geometry: BufferGeometry }

/** Original civic-industrial architecture, authored in metres. Every component is real geometry. */
export function createArchitectureGeometry(type: CardType, width: number, depth: number): ArchitecturePart[] {
  const groups = new Map<string, { finish: ArchitectureFinish; geometries: BufferGeometry[] }>()
  function add(name: string, finish: ArchitectureFinish, geometry: BufferGeometry, x = 0, y = 0, z = 0) {
    const geo = geometry.index ? geometry.toNonIndexed() : geometry
    if (geo !== geometry) geometry.dispose()
    geo.deleteAttribute('uv')
    geo.applyMatrix4(new Matrix4().makeTranslation(x, y, z))
    const group = groups.get(name) ?? { finish, geometries: [] }
    group.geometries.push(geo)
    groups.set(name, group)
  }
  function box(name: string, finish: ArchitectureFinish, w: number, h: number, d: number, x = 0, y = 0, z = 0) {
    add(name, finish, new BoxGeometry(w, h, d), x, y, z)
  }
  function pavilion(name: string, w: number, h: number, d: number, x: number, bottom: number, z: number) {
    // Clipped stone corners and bevels catch natural light without an emissive outline.
    const r = Math.min(w, d) * 0.075
    const s = new Shape()
    s.moveTo(-w / 2 + r, -d / 2); s.lineTo(w / 2 - r, -d / 2)
    s.lineTo(w / 2, -d / 2 + r); s.lineTo(w / 2, d / 2 - r)
    s.lineTo(w / 2 - r, d / 2); s.lineTo(-w / 2 + r, d / 2)
    s.lineTo(-w / 2, d / 2 - r); s.lineTo(-w / 2, -d / 2 + r); s.closePath()
    const geometry = new ExtrudeGeometry(s, { depth: h, bevelEnabled: true, bevelSegments: 1, bevelSize: 0.06, bevelThickness: 0.06, steps: 1 })
    geometry.rotateX(-Math.PI / 2)
    add(name, 'shell', geometry, x, bottom, z)
  }
  const w = width * 0.82, d = depth * 0.82
  const base = 0.55
  box('foundation', 'stone', width * 0.97, 0.3, depth * 0.97, 0, 0.16)
  box('plinth', 'stone', w + 0.35, 0.25, d + 0.35, 0, 0.425)

  function wing(name: string, ww: number, hh: number, dd: number, x = 0, y = base, z = 0, floors = 2) {
    pavilion(name, ww, hh, dd, x, y, z)
    const columns = Math.max(2, Math.floor(ww / 1.3))
    for (let floor = 0; floor < floors; floor++) {
      const fy = y + (floor + .5) * hh / floors
      for (const side of [-1, 1]) {
        for (let c = 0; c < columns; c++) box('windows', 'glass', ww / columns * .54, hh / floors * .52, .05, x + ((c + .5) / columns - .5) * ww * .86, fy, z + side * (dd / 2 + .07))
        const colsZ = Math.max(2, Math.floor(dd / 1.3))
        for (let c = 0; c < colsZ; c++) box('windows', 'glass', .05, hh / floors * .52, dd / colsZ * .54, x + side * (ww / 2 + .07), fy, z + ((c + .5) / colsZ - .5) * dd * .86)
      }
    }
    box('roof-slabs', 'stone', ww + .14, .2, dd + .14, x, y + hh + .13, z)
  }
  function cylinder(name: string, finish: ArchitectureFinish, r: number, h: number, x: number, y: number, z: number, sides = 16) {
    add(name, finish, new CylinderGeometry(r, r, h, sides), x, y, z)
  }
  function pitchedRoof(name: string, ww: number, hh: number, dd: number, x: number, y: number, z: number, teeth = 1) {
    const shape = new Shape(); shape.moveTo(-ww / 2, 0)
    for (let t = 0; t < teeth; t++) {
      shape.lineTo(-ww / 2 + ww / teeth * (t + (teeth === 1 ? .5 : .68)), hh)
      shape.lineTo(-ww / 2 + ww / teeth * (t + 1), 0)
    }
    shape.lineTo(-ww / 2, 0); shape.closePath()
    add(name, 'metal', new ExtrudeGeometry(shape, { depth: dd, bevelEnabled: false }), x, y, z - dd / 2)
  }
  function dome(name: string, r: number, x: number, y: number, z: number) {
    add(name, 'metal', new SphereGeometry(r, 20, 10, 0, Math.PI * 2, 0, Math.PI / 2), x, y, z)
  }

  switch (type) {
    case 'Residential': {
      // A U-shaped habitat encloses a visibly open planted courtyard; front terraces
      // descend toward the street instead of sharing the office tower's massing.
      wing('courtyard-rear-wing', w, 8.4, d * .27, 0, base, d * .32, 4)
      for (const side of [-1, 1]) {
        wing('courtyard-side-wings', w * .26, 6.3, d * .54, side * w * .36, base, -d * .06, 3)
        wing('terraced-homes', w * .26, 3.6, d * .24, side * w * .36, base, -d * .35, 2)
        box('balconies', 'stone', w * .33, .16, d * .52, side * w * .34, 2.65, -d * .06)
        box('terrace-gardens', 'garden', w * .18, .22, d * .18, side * w * .36, 4.47, -d * .35)
      }
      box('courtyard-lawn', 'garden', w * .4, .06, d * .61, 0, base + .04, -d * .1)
      box('courtyard-path', 'stone', w * .1, .08, d * .63, 0, base + .1, -d * .1)
      break
    }
    case 'Retail': {
      // Two market arcade rows leave a central street; three separate peaked roofs.
      for (const side of [-1, 1]) {
        wing('market-arcades', w * .32, 2.7, d * .85, side * w * .33, base, 0, 1)
        pitchedRoof('market-pitched-roofs', w * .35, 1.8, d * .9, side * w * .33, 3.55, 0)
        for (let i = 0; i < 3; i++) box('market-awning', 'metal', w * .17, .14, d * .19, side * w * .15, 2.6, (i - 1) * d * .27)
      }
      box('market-street', 'stone', w * .24, .06, d, 0, base)
      break
    }
    case 'Office': {
      wing('office-podium', w, 3.5, d, 0, base, 0, 2)
      wing('office-tower', w * .7, 15.5, d * .67, 0, 4.3, d * .06, 7)
      for (let f = 1; f < 7; f++) box('office-cornices', 'stone', w * .72, .14, d * .69, 0, 4.3 + f * 15.5 / 7, d * .06)
      wing('roof-plant', w * .25, 1.2, d * .24, 0, 20.1, d * .06, 1)
      break
    }
    case 'Industrial': {
      wing('factory-hall', w * .82, 3.2, d * .7, -w * .07, base, -d * .1, 1)
      pitchedRoof('sawtooth-factory-roof', w * .85, 1.75, d * .74, -w * .07, 3.95, -d * .1, 4)
      wing('loading-annex', w * .42, 2, d * .2, -w * .22, base, d * .36, 1)
      for (const x of [w * .2, w * .37]) {
        cylinder('exhaust-stacks', 'metal', w * .048, 8.4, x, base + 4.2, d * .32, 10)
        cylinder('stack-collars', 'stone', w * .062, .28, x, base + 8.28, d * .32, 10)
      }
      break
    }
    case 'Infrastructure': {
      // Exposed process equipment occupies the lot; only the front control room is enclosed.
      wing('plant-control-room', w * .6, 2.1, d * .2, -w * .14, base, -d * .38, 1)
      for (const side of [-1, 1]) {
        const r = Math.min(w, d) * .205
        cylinder('water-tanks', 'metal', r, 4.2, side * w * .25, base + 2.1, d * .12, 20)
        cylinder('tank-caps', 'stone', r * 1.035, .22, side * w * .25, base + 4.26, d * .12, 20)
        cylinder('tank-bases', 'stone', r * 1.09, .25, side * w * .25, base + .15, d * .12, 20)
        box('service-pipes', 'metal', w * .025, .2, d * .62, side * w * .25, base + .36, 0)
      }
      box('pipe-manifold', 'signal', w * .57, .16, .16, 0, base + .4, -d * .2)
      break
    }
    case 'Civic': {
      // A broad forum, deep columned portico and triangular pediment.
      wing('civic-hall', w * .91, 4.5, d * .6, 0, base, d * .13, 2)
      for (let i = 0; i < 6; i++) {
        const x = ((i + .5) / 6 - .5) * w * .9
        cylinder('civic-colonnade', 'stone', w * .033, 4.3, x, base + 2.15, -d * .32, 12)
        box('column-capitals', 'stone', w * .092, .25, w * .09, x, 4.82, -d * .32)
      }
      box('portico-entablature', 'stone', w, .44, d * .34, 0, 5.13, -d * .29)
      pitchedRoof('civic-pediment', w * 1.015, 2.1, d * .93, 0, 5.35, 0)
      box('civic-steps', 'stone', w * .85, .23, d * .11, 0, .67, -d * .46)
      break
    }
    case 'Research': {
      const r = Math.min(w, d) * .33
      cylinder('observatory-drum', 'shell', r, 3.2, 0, base + 1.6, d * .08, 24)
      cylinder('observatory-cornice', 'stone', r * 1.05, .3, 0, 3.86, d * .08, 24)
      dome('observatory', r * 1.025, 0, 4.0, d * .08)
      for (const side of [-1, 1]) wing('research-laboratory-wings', w * .24, 2.5, d * .62, side * w * .36, base, -d * .06, 1)
      wing('research-entry', w * .38, 2.25, d * .26, 0, base, -d * .34, 1)
      box('observatory-shutter', 'glass', r * .18, r * .95, r * .62, 0, 4.05 + r * .48, d * .08 - r * .32)
      break
    }
    case 'Defense': {
      // Four bastions surround an open parade yard and two separated gatehouse wings.
      for (const side of [-1, 1]) {
        box('bastion-walls', 'stone', w * .13, 2.8, d * .74, side * w * .4, 1.95)
        box('bastion-rear-wall', 'stone', w * .8, 2.8, d * .13, 0, 1.95, d * .4)
        wing('gatehouses', w * .28, 3.2, d * .2, side * w * .31, base, -d * .38, 1)
        for (const front of [-1, 1]) {
          cylinder('watchtowers', 'shell', Math.min(w, d) * .125, 5.2, side * w * .36, base + 2.6, front * d * .36, 8)
          cylinder('tower-battlements', 'stone', Math.min(w, d) * .14, .42, side * w * .36, 5.88, front * d * .36, 8)
        }
      }
      box('parade-yard', 'metal', w * .56, .09, d * .56, 0, base)
      break
    }
    case 'Wonder': {
      // Four monumental terraces and a tall faceted obelisk make the landmark
      // identifiable without windows or office-floor repetition.
      for (let i = 0; i < 4; i++) {
        const scale = 1 - i * .19
        pavilion('monument-terraces', w * scale, 2.5, d * scale, 0, base + i * 2.7, 0)
        box('terrace-cornices', 'stone', w * scale + .12, .2, d * scale + .12, 0, base + i * 2.7 + 2.6)
        box('monument-friezes', 'metal', w * scale * .76, .24, .09, 0, base + i * 2.7 + 2.15, -d * scale / 2 - .075)
        for (const side of [-1, 1]) {
          box('monument-pilasters', 'stone', w * .04, 1.95, .18, side * w * scale * .32, base + i * 2.7 + 1.05, -d * scale / 2 - .04)
          box('monument-pilasters', 'stone', .18, 1.95, d * .04, side * (w * scale / 2 + .04), base + i * 2.7 + 1.05, -d * scale * .22)
        }
      }
      const obelisk = new CylinderGeometry(w * .025, w * .12, 11, 4); obelisk.rotateY(Math.PI / 4)
      add('crown', 'metal', obelisk, 0, 16.75, 0)
      box('monument-portal', 'glass', w * .23, 2, .1, 0, 1.6, -d * .5 - .08)
      for (let step = 0; step < 3; step++) box('entrance-stair', 'stone', w * .29, .18, .16, 0, .24 + step * .16, -d * .5 - .36 + step * .12)
      for (let i = 0; i < 8; i++) box('processional-stairs', 'stone', w * .23, .34, d * (.11 + i * .042), 0, base + (7 - i) * .32, -d * (.43 - i * .018))
      break
    }
    case 'Unit': {
      wing('motor-depot', w * .9, 2.8, d * .62, 0, base, d * .16, 1)
      pitchedRoof('hangar-roof', w * .95, 2.3, d * .68, 0, 3.5, d * .16)
      for (const side of [-1, 1]) {
        box('hangar-doors', 'metal', w * .3, 2.5, .1, side * w * .22, 1.85, -d * .16)
        box('vehicle-bays', 'metal', w * .2, .62, d * .2, side * w * .23, .93, -d * .35)
      }
      break
    }
    case 'Leader': {
      wing('assembly-hall', w * .76, 4.3, d * .76, -w * .07, base, d * .03, 2)
      pitchedRoof('assembly-roof', w * .8, 2.5, d * .8, -w * .07, 5.1, d * .03)
      wing('campanile', w * .23, 13.5, d * .24, w * .32, base, d * .3, 4)
      pitchedRoof('crown', w * .3, 2.5, d * .3, w * .32, 14.3, d * .3)
      break
    }
    case 'Special': {
      // Twin needle towers and two inhabited bridges leave a large central void.
      for (const side of [-1, 1]) {
        wing('twin-towers', w * .24, 18.5, d * .44, side * w * .35, base, 0, 7)
        pitchedRoof('crown', w * .27, 3.2, d * .47, side * w * .35, 19.25, 0)
      }
      wing('skybridges', w * .49, 2.2, d * .25, 0, 9, 0, 1)
      wing('skybridges', w * .49, 2.2, d * .25, 0, 15, 0, 1)
      break
    }
  }
  // Shared street furniture is subordinate to the family-specific architecture.
  box('entry', 'metal', Math.min(1.2, w * .18), .65, .07, 0, .92, -d / 2 - .08)
  box('entry-signal', 'signal', Math.min(.9, w * .14), .14, .09, 0, 1.38, -d / 2 - .1)
  for (const side of [-1, 1]) {
    box('planters', 'stone', w * .14, .3, .36, side * w * .35, .74, -d / 2 - .08)
    box('planting', 'garden', w * .11, .23, .29, side * w * .35, 1, -d / 2 - .08)
  }
  return [...groups].map(([name, { finish, geometries }]) => {
    const geometry = mergeGeometries(geometries, false)!
    geometries.forEach((part) => part.dispose())
    geometry.computeBoundingBox()
    return { name, finish, geometry }
  })
}
