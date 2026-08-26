export const fmt = (n: number, digits = 0): string =>
  n.toLocaleString('en-US', { minimumFractionDigits: digits, maximumFractionDigits: digits })

export const signed = (n: number, digits = 1): string =>
  `${n >= 0 ? '+' : ''}${fmt(n, digits)}`

export const pct = (n: number): string => `${Math.round(n * 100)}%`
