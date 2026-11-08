# Calibrating Cybrid V1

Every key has three trimpots, one per detection point (see [How it works](how-it-works.md)):

- **Point 3:** the hammer halfway between its rest position and the stop rail. Above this point the
  damper counts as lifted: after a strike, note-off is sent when the hammer drops below it.
- **Point 2:** the hammer 1 mm from the stop rail, where it escapes from the jack. The flight-time
  measurement starts here.
- **Point 1:** the hammer at (or very close to) the stop rail. The measurement stops here and the
  velocity is computed.

> The calibration sketches call these "sensor 1/2/3". They are three *points* of the same sensor.

Calibration has two passes: a quick rough setting with simple tools, then fine-tuning by ear. The
rough pass takes under 10 minutes and the whole procedure 20–30 minutes. It's needed only once;
after that, only an occasional touch-up.

## 1. Rough setting with the calibration sketches

The three sketches in [`firmware/v1/src/calibration`](../../firmware/v1/src/calibration) each
watch one point on all keys and light the Teensy's LED while **any** hammer is at that point or
closer.

Tools:

- **Point 3:** a wooden block sized so that, placed against the stop rail, a hammer lifted to touch
  it is at half its blow distance.
- **Point 2:** a 1 mm thick plastic strip.
- **Point 1:** nothing; lift the hammer until it touches the stop rail.

For each point:

1. Flash the matching sketch (`calibrate_1`, `calibrate_2` or `calibrate_3`, see
   [firmware](../../firmware/v1/README.md)).
2. For every key, lift the hammer by hand until it touches the block, the strip or the rail.
3. Turn that key's trimpot for this point until the LED lights up.

Then flash the main firmware. The result is usually already close and the instrument is playable.

## 2. Fine-tuning by ear

With the main firmware running, set each key in turn:

1. **Point 1:** play loud staccato notes and turn the point-1 trimpot until the key stops being
   detected. The point is now so high that the rail stops the hammer before it. Then back off
   slowly until the note comes back. Back off a bit more: right at the edge, very soft notes can be
   missed (the rail felt isn't compressed enough), and very loud ones too (the hammer spends so
   little time at the point that the scan misses it). The hammer must be detected at its peak at
   every velocity.
2. **Point 2:** moving point 1 changes the distance between points 1 and 2, and with it how loud the
   key plays. Turn the point-2 trimpot until the key's velocity response feels right next to its
   neighbours. This sounds demanding, but it's easy to hear where the right position is. It feels
   like voicing a real piano, and I found it satisfying.
3. **Point 3** (optional): move it to where you want note-off to happen. In my experience it
   matters little.
