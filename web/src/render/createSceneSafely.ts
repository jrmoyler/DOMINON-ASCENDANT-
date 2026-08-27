export function createSceneSafely<T>(
  factory: () => T,
  reportFailure: (message: string) => void,
): T | null {
  try {
    return factory()
  } catch (error) {
    const detail = error instanceof Error ? error.message : String(error)
    reportFailure(
      /webgl|gpu|3d acceleration/i.test(detail)
        ? '3D acceleration is unavailable. Enable WebGL and hardware acceleration, then reload.'
        : 'The 3D command world could not initialize. Reload the game or try another browser.',
    )
    return null
  }
}
