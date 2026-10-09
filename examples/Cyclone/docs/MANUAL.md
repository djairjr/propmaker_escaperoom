# Cyclone Manual - Reflex Game

Guide for room owners, game masters and players.

---

## 1. Overview

### What is this device?

The **Cyclone** is a reflex game where a light runs in a circle on a ring of LEDs and the player must press a button at the right moment to make the light stop inside a green target zone. With each hit, the speed increases. Three hits solve the puzzle.

### How does it work in the game?

- The players see a ring of lights with a white light running in a circle
- A green zone indicates where the light must stop
- The player presses the button to try to stop the light in the green zone
- If they hit, the light turns green and the speed increases
- If they miss, the light turns red and the player tries again
- After 3 hits, the puzzle is solved

---

## 2. Game Components

| Component | Description |
|------------|-----------|
| **NeoPixel LED ring** | Lights that show the running light and the target zone |
| **SELECT button** | Main button to stop the light |
| **DET button** | Alternative button (works the same as SELECT) |
| **Speaker** | Tick, hit, miss and victory sounds |

---

## 3. Initial Configuration

### Difficulty adjustment

Edit the `/cyclone.json` file:

```json
{
  "targetSize": 3,
  "startSpeed": 0.05,
  "speedIncrement": 0.02,
  "maxSpeed": 0.4,
  "roundsToWin": 3
}
```

| Parameter | Effect | Easy | Hard |
|-----------|--------|-------|---------|
| `targetSize` | Target zone size | 5 (large) | 1 (point) |
| `startSpeed` | Initial speed | 0.03 | 0.08 |
| `speedIncrement` | Increase per round | 0.01 | 0.04 |
| `maxSpeed` | Maximum speed | 0.2 | 0.6 |
| `roundsToWin` | Hits required | 2 | 5 |

---

## 4. How to Play

### For the Game Master

1. Adjust the difficulty in the JSON before starting
2. Place the LED ring in a visible location
3. Press SELECT to start the game

### For the players

1. **Watch** the light running on the ring
2. **Wait** until the light approaches the green zone
3. **Press SELECT** to stop the light
4. **If you hit**: the light turns green, the speed increases, go to the next round
5. **If you miss**: the light turns red, try again
6. **After 3 hits**: puzzle solved!

### Tips

- Anticipate the movement: the light always moves in the same direction
- Press a little before the light reaches the target (reaction time)
- The speed increases with each hit - be faster!
- Use both buttons (SELECT or DET) if available

---

## 5. What happens upon solving

1. The victory sound plays
2. The LEDs do a rainbow effect
3. The lock is released
4. After 8 seconds, the puzzle returns to its initial state

---

## 6. Audio Checklist

| File | When It Plays | Mandatory? |
|---------|-------------|--------------|
| `tick.mp3` | Each light step (optional) | No |
| `hit.mp3` | Hit the target | Yes |
| `miss.mp3` | Missed the target | Yes |
| `solved.mp3` | Puzzle solved | Yes |

Format: MP3, 128kbps, 44.1kHz, stereo, no ID3 tags.

---

## 7. Troubleshooting

### The light does not move

1. Check whether the NeoPixel ring is connected
2. Test with `hw.test()` via Serial
3. Check neoCount in the JSON

### The button does not work

1. Check the SELECT pin (GPIO 4)
2. Test with `switches` via Serial debug

### The light is too fast

1. Reduce `startSpeed` and `speedIncrement` in the JSON
2. Increase `targetSize` to make it easier

---

## Quick Summary

| Action | How to Do It |
|------|------------|
| Start game | Press SELECT |
| Stop the light | Press SELECT |
| Hit | Light stops in the green zone |
| Miss | Light stops outside the zone |
| Win | 3 hits |

### LED States

| Color/Pattern | Meaning |
|------------|-------------|
| Pulsing blue | Waiting for start |
| White + trail + green | Game in progress |
| Solid green | Hit! |
| Solid red | Missed! |
| Rainbow | Puzzle solved! |

---

*Cyclone Manual - Version 1.0 for the PropMaker EscapeRoom library*