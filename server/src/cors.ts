/**
 * Allow the local web app plus browser extension popup origins.
 */

export function isAllowedCorsOrigin(
  origin: string | undefined,
  allowedOrigins: ReadonlySet<string>,
): origin is string {
  if (!origin) {
    return false;
  }
  if (allowedOrigins.has(origin)) {
    return true;
  }
  return (
    origin.startsWith("moz-extension://") ||
    origin.startsWith("chrome-extension://")
  );
}
