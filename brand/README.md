# EarBlaster official marks

Locked 2026-09-08. Third-party app *for* LCOS, not LCOS house art.

## Palette

| Token | Hex | Use |
|---|---|---|
| Navy | `#0B1D38` | wells, icon field, lockup ground |
| Ice | `#E8F2FF` | rings, bolt, pill outline, wordmark |
| Ice glow | `#7EC8E3` | neon fringe only |
| Client gray | `#E6E6E1` | window client area |
| Selection | muted steel blue | playlist row |

## Files

| File | Role |
|---|---|
| `mark-ring-bolt.png` | **Primary mark.** Single ring + bolt. Spins in the navy well. Window icon fallback. |
| `mark-ring-bolt-256.png` | Same, 256px. |
| `mark-ring-bolt-alt.png` | Alternate render of the same mark. Do not ship both. |
| `icon-tile.png` | **Desktop / `.desktop` icon.** Rounded navy tile, speaker rings + bolt. |
| `icon-tile-128.png` `icon-tile-48.png` `icon-tile-32.png` | Menu sizes. |
| `icon-tile-alt.png` | Alternate tile. |
| `lockup-pill.png` | **Product lockup.** Bolt-circle + EARBLASTER pill. About box, splash. |
| `lockup-pill-alt.png` | Alternate lockup. |
| `ui-reference.svg` | **Locked window.** Implement this layout. Shown on the project README. |
| `ui-reference.png` / `ui-reference.jpg` | Raster exports of the same window. Local preview; not required on Git. |
| `ui-reference-alt.png` | Earlier pass of the same layout. |

Primary ship set: `mark-ring-bolt.svg`, `icon-tile.svg`, `lockup-pill.svg`, `ui-reference.svg`.

Vector masters (what Git tracks cleanly): `mark-ring-bolt.svg`, `icon-tile.svg`, `lockup-pill.svg`, `ui-reference.svg`. Rasters are preview exports of the same marks.

## Grammar

- One ring. Never the LCOS double-ring seal.
- Pill sits *under* the spinning circle, or beside it in the lockup. Never across the disc.
- No “LUNDUKE” / “COMPUTER OPERATING SYSTEM” lettering in our chrome.
- About footer only: “EarBlaster — a media player for The Lunduke Computer Operating System.”
- Animation: rotate the ring; keep the bolt upright. Cover art overlays the circle; hide the pill while art is up.

These PNGs are reference. Runtime well should draw ring + bolt in Cairo so the spin is clean at any size.
