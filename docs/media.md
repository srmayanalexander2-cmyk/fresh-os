# Media, photos and file formats on Stratus

FreshOS has no audio card, network stack or binary file store. The PC speaker
is the only sound output, and Home holds at most 16 ASCII text documents of
3,071 bytes each. Media and Photos therefore use **text formats** that fit
that store. There is no MP3, MP4, WAV or JPEG support, and no way to copy
binary files onto Home from inside the guest.

## Music (.mus)

A `.mus` file is whitespace-separated `NOTE:BEATS` tokens:

```
C4:1 E4:1 G4:2 REST:1 C5:1
```

Notes are `A`–`G`, optional `#`, octave `2`–`7` (for example `F#3`), or
`REST` for silence. Beats are 1–16; one beat is 0.2 seconds. Media plays the
file on the PC speaker with a visualizer, volume, next/previous and loop.
Four built-in tunes are included. A fresh Home disk also starts with
`example.mus`; open it from Files or use `play example.mus` in Terminal. To
make your own, create a file in Writer and save it as `song.mus`, then open
it from Files (it opens in Media automatically) or right-click it in Files.

## Pictures (.pbm / .pgm / .ppm)

Photos opens Netpbm ASCII pictures: `P1` bitmap, `P2` grayscale and `P3`
RGB, up to 128×128 pixels, with `#` comments allowed in the header. Example:

```
P1
4 2
1010 0101
```

Sketch can save its canvas as a 64×36 `P1` file (`Save .pbm`); open the
result in Photos. A fresh Home disk includes `pattern.pbm` as a first example.
Four built-in scenes (Sunset, Mountains, Ocean, Pattern) are also available.
Use Prev/Next and Fit/Actual zoom.

## What this is not

No video playback, no MP3/WAV decoding, no microphone, no CD audio and no
internet radio. The speaker is a single square-wave channel: one note at a
time, no chords, no stereo. Binary formats cannot be stored in Home without
changing the on-disk format, which would break existing Home disks.
