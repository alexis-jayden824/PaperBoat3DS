# M12.1 Title Presentation Correction

M12.1 is a focused visual correction to the M12 checkpoint. It does not
replace PaperBoat game logic and does not promote the temporary file-select
panels into a final menu.

## New 3DS XL/LL display contract

Every retail Nintendo 3DS top LCD, including New 3DS XL and New 3DS LL, has a
logical resolution of 400x240. The XL/LL panel is physically larger but does
not expose more pixels. Folium may scale and surround that image differently
on an iPhone; that phone presentation must not be used as the layout canvas.

Paper Mario's fixed title art is authored for 320x240. M12.1 preserves it at
one source pixel per LCD pixel and centers it horizontally:

| Element | 400x240 rectangle (left, bottom, width, height) |
|---|---:|
| 4:3 safe canvas | 40, 0, 320, 240 |
| `title_bg` | 52, 20, 296, 200 |
| Logo | 100, 113, 200, 112 |
| PRESS START | 136, 71, 128, 32 |
| Copyright | 129, 17, 144, 32 |

The safe canvas therefore has equal 40-pixel pillars. The background begins
another 12 pixels inside the original canvas, which is why its LCD edge is at
x=52. No texture is stretched, fractionally scaled, or cropped. M13 may widen
the world camera to the full 400-pixel playfield; fixed 2D art remains in this
safe area unless upstream PaperBoat explicitly defines a widescreen anchor.

M12 used the renderer's navy bootstrap clear around the title. M12.1 clears to
black so the expected pillars blend with the physical black bezel rather than
looking like a smaller card floating on the screen.

## Prompt visibility

PaperBoat modulates the IA8 PRESS START texture by RGB `(248, 240, 152)` and a
blinking alpha. M12.1 applies that same pale-yellow tint. The bottom-screen
diagnostic prints `A:<value>` beside `Safe:320@x40`; a capture with `A:255`
must show the prompt, while `A:0` is an intentional off phase.

## Acceptance

Run:

```sh
make m12-layout-test
make m10-graphics-test
make m12-flow-test
make packages
```

The host layout suite must report the exact rectangles above and equal 40-pixel
pillars. Folium must then show black pillars, an uncropped upright title, and a
visible pale-yellow prompt when the diagnostic reports nonzero alpha. Renderer
rejects, frame failures, and stream overflows must remain zero. A real New 3DS
XL/LL remains authoritative for physical scaling, lifecycle, and controls.
