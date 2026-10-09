# Roulette Manual - Wheel of Fortune

Guide for room owners, game masters and players.

---

## 1. Overview

### What is this device?

The **Roulette** is an electronic luminous roulette. The player presses a button to spin the ring of LEDs. The light runs around the ring and decelerates gradually until it stops at a position. If it stops on the winning sector (marked in green), the player wins the round. Three rounds won solve the puzzle.

### How does it work in the game?

- The players see a ring of LEDs with a sector illuminated in green
- Pressing the button, a white light starts running around the ring
- The light decelerates until it stops
- If it stops on the green sector, the player wins the round
- If it stops outside, they can try again
- Three hits solve the puzzle

---

## 2. Game Components

| Component | Description |
|------------|-----------|
| **NeoPixel LED ring** | The roulette itself |
| **SELECT button** | To spin the roulette |
| **Speaker** | Spin, victory, defeat and solving sounds |

---

## 3. Initial Configuration

### Roulette adjustment

Edit the `/roulette.json` file:

```json
{
  "spinSpeed": 8.0,
  "friction": 0.97,
  "minSpeed": 0.1,
  "winSectorCount": 1,
  "roundsToWin": 3
}
```

| Parameter | Effect | Easy | Hard |
|-----------|--------|-------|---------|
| `spinSpeed` | Initial speed | 6.0 | 12.0 |
| `friction` | Friction (0.99 = decelerates slowly) | 0.98 | 0.95 |
| `winSectorCount` | How many LEDs are winners | 3 | 1 |
| `roundsToWin` | Hits required to win | 1 | 5 |

### Expected behavior

- friction = 0.97: roulette spins ~12 laps before stopping
- friction = 0.95: roulette spins ~6 laps (faster)
- friction = 0.99: roulette spins ~50 laps (too slow to stop)

---

## 4. How to Play

### For the Game Master

1. Adjust the difficulty in the JSON
2. Place the ring in a visible location
3. Show the players that the green sector is the target

### For the players

1. **Look** at the winning sector (pulsing green)
2. **Press SELECT** to spin the roulette
3. **Follow** the light decelerating
4. **If you hit**: green! One more step to win
5. **If you miss**: red. Try again!
6. **After 3 hits**: puzzle solved!

### Tips

- The winning sector is always visible (pulsing green)
- The roulette always decelerates - there is no trick
- Watch the deceleration pattern to predict where it will stop
- Luck is the main factor, but persistence wins

---

## 5. What happens upon solving

1. The victory sound plays
2. The LEDs do a rainbow effect
3. The lock is released
4. After 8 seconds, a new winning sector is drawn and the puzzle returns to the start

---

## 6. Audio Checklist

| File | When It Plays | Mandatory? |
|---------|-------------|--------------|
| `spin.mp3` | When spinning the roulette | Yes |
| `win.mp3` | Hit the sector | Yes |
| `lose.mp3` | Missed the sector | Yes |
| `solved.mp3` | 3 hits | Yes |

Format: MP3, 128kbps, 44.1kHz, stereo, no ID3 tags.

---

## 7. Troubleshooting

### The roulette does not spin

1. Check the SELECT button (GPIO 4)
2. Check the NeoPixel ring

### The roulette stops too fast

1. Increase `friction` in the JSON (0.98-0.99)
2. Increase `spinSpeed` (10.0-15.0)

### The roulette takes too long to stop

1. Decrease `friction` (0.95-0.96)
2. Increase `minSpeed` (0.2-0.3)

### It never hits the sector

1. Increase `winSectorCount` (3-5)
2. Check whether `roundsToWin` is not too high

---

## Quick Summary

| Action | How to Do It |
|------|------------|
| Spin roulette | Press SELECT |
| Winning sector | Pulsing green LED |
| Hit | Solid green LED |
| Missed | Solid red LED |
| Win | 3 hits |

### LED States

| Color/Pattern | Meaning |
|------------|-------------|
| Pulsing green | Winning sector visible (idle) |
| White + trail | Roulette spinning |
| Solid green | Hit! |
| Solid red | Missed! |
| Rainbow | Puzzle solved! |

---

*Good luck! May the roulette be in your favor...*

*Roulette Manual - Version 1.0 for the PropMaker EscapeRoom library*