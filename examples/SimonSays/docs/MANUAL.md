# Simon Says Manual - Genius Game for Escape Room

A complete guide for room owners, game masters and players.

---

## 1. Overview

### What is this device?

The **Simon Says** is a version of the classic Genius game adapted for Escape Rooms. The device consists of a box with 4 colored buttons, each with its own ring of LEDs that light up in sequence. The players must memorize and repeat the color sequence to open the box.

### How does it work in the game?

- The players find the device in the room (usually a locked box)
- Pressing any button starts the game with an opening animation
- The device shows a color sequence on the LED rings
- The players must repeat the sequence by pressing the buttons in the same order
- If they get it right, they advance to the next level with a longer sequence
- If they get it wrong, the system emits an error sound and repeats the same sequence (no penalty!)
- Upon completing all levels, the box opens automatically

> **Differentiator**: This system does not punish mistakes! If the players make a mistake, or take longer than 5 seconds to press, the system simply repeats the current sequence until they get it right.

---

## 2. Game Components

These are the elements the players interact with during the game:

| Component | Description |
|------------|-----------|
| **Red Button** | Ring of 12 red LEDs on the upper/left edge |
| **Blue Button** | Ring of 12 blue LEDs on the upper/right edge |
| **Yellow Button** | Ring of 12 yellow LEDs on the lower/left edge |
| **Green Button** | Ring of 12 green LEDs on the lower/right edge |
| **Lock** | Opens automatically when the puzzle is solved |
| **Speaker** | Emits sounds for each color and hit/error feedback |

---

## 3. How to Play (for the players)

### Starting the game

1. The device will be in standby mode, with all the LEDs off
2. **Press any button** to start
3. Listen to the opening sound and watch all the rings light up in the sequence: red → blue → yellow → green

### During the game

1. **Watch the sequence** of colors that the device shows
2. **Repeat the sequence** by pressing the buttons in the same order
3. Each correctly pressed color emits its characteristic sound
4. If you get the whole sequence right, you hear a "correct" sound and the next level begins

### If you make a mistake or take too long

- Do not worry! This system **does not restart the game** nor apply penalties
- If you make a mistake or take longer than 5 seconds, you hear an error sound
- The device repeats the **same sequence** again
- Keep trying until you get it right!

### Winning the game

- Complete all 7 levels (increasingly longer sequences)
- Upon completing the last level, you hear a victory sound and see a rainbow animation
- The box opens automatically
- To reset the game (after removing the contents), press and hold any button for 2 seconds

---

## 4. Configuration and Maintenance

### For the room owner / game master

#### Progression rules

The game follows an automatic difficulty progression:

| Level | Speed | Description |
|-------|------------|-----------|
| 1-2 | 800ms | Slow - easy start |
| 3-4 | 600-700ms | Medium - moderate speed |
| 5-6 | 400-500ms | Fast - challenge increases |
| 7 | 300ms | Very fast - final level |

#### Resetting the game after use

1. After the puzzle is solved and the box is open
2. Press **any button for 2 seconds** (until you hear the "ready" sound)
3. The game will be reset and ready for the next match

> **Tip**: The game does not reset automatically. Always reset after the match ends to guarantee it starts from level 1.

#### Sound check

If necessary, check whether the audio files are correct:

1. On power-up, it should play the "ready" sound
2. On start, it should play the "begin" sound
3. Each button should play its respective sound: red, blue, yellow, green
4. On getting a whole sequence right: "correct" sound
5. On a mistake: "error" sound
6. On winning: "victory" sound

---

## 5. Troubleshooting

### The device does not turn on

- Check whether the power is connected correctly
- Check whether there is power at the power supply

### LEDs do not light up

- Check whether there is a "ready.mp3" file on the card (the presence of audio indicates it is working)
- If the audio works but the LEDs do not: there may be a problem with the NeoPixel connection

### Sound does not work

- Check whether there are MP3 files in the correct folder (LittleFS or SD)
- Check whether the files are in the correct format: 44.1kHz, stereo, 128kbps
- Try increasing the volume in the settings (if available in the menu).

### The box does not open upon winning

- Check the connection of the lock pin (PIN_EXTERNAL_POWER)
- Test by manually triggering the lock during the game

### The game is too difficult for the players

- Remember: this system **does not restart** on a mistake, it just repeats the same sequence
- This allows players to try as many times as they need at the same level
- If it is still difficult, adjust the maximum level in the code (default: 7 levels)

---

## 6. Technical Specifications

### Required hardware

| Component | Description |
|------------|-----------|
| Board | Adafruit PropMaker Feather RP2040 |
| Expander | PCF8575 (I2C address 0x20) |
| LEDs | 48 NeoPixel LEDs (4 rings of 12 LEDs each) |
| Buttons | 4 momentary buttons |
| Audio | I2S audio output integrated into the PropMaker |
| Lock | Solenoid or relay (control via PIN_EXTERNAL_POWER) |

### Required audio files

| File | Function |
|---------|--------|
| ready.mp3 | System boot |
| begin.mp3 | Game start |
| red.mp3 | Red button |
| blue.mp3 | Blue button |
| yellow.mp3 | Yellow button |
| green.mp3 | Green button |
| correct.mp3 | Correct sequence |
| error.mp3 | Sequence error |
| victory.mp3 | Puzzle solved |

**File format:**
- Bitrate: 128 Kbps
- Sample rate: 44.1 kHz
- Channels: Stereo (2)
- No ID3 metadata

To convert: `ffmpeg -i input.mp3 -acodec libmp3lame -ar 44100 -ac 2 -b:a 128k -map_metadata -1 output.mp3`

### Communication (optional)

- The device can communicate with other puzzles via RS485
- Network ID: 20 (configurable in the code)
- Supported commands:
  - `ForceSolve`: Forces immediate solving (opens the box)
  - `GlobalEvent`: Resets the game to its initial state

---

## 7. Tips for Game Masters

### Integration in the room

1. **Narrative**: The Simon Says can be presented as:
   - A security system that needs to be deactivated
   - A magical mechanism that requires memory and concentration
   - A scientific pattern-testing device

2. **Difficulty**: Since the system does not restart on a mistake:
   - The challenge lies in memorizing increasingly long sequences
   - The speed increases with each level
   - Consider giving visual hints if necessary

3. **Post-solving**:
   - The rainbow animation indicates success
   - The box opens automatically
   - Remember to reset the device before the next match

### Preventive maintenance

- Check the button connections periodically
- Test all the sounds before each day of operation
- Keep the system clean and protected from moisture

---

## 8. FAQ (Frequently Asked Questions)

**Q: Do the players need to start from level 1 every time?**
A: Yes, the game always starts from level 1 after a reset.

**Q: What happens if the 5-second time between buttons runs out?**
A: The system considers it a mistake, plays the error sound and repeats the same sequence.

**Q: Can I adjust the game speed?**
A: Yes, but it requires a modification in the code (calculateSpeed() function). The default values are optimized for players.

**Q: Can the system work without all the sounds?**
A: Yes, but the experience will be impaired. The game works without audio, but this is not recommended.

**Q: Can I change the number of levels?**
A: Yes, change the MAX_LEVELS constant in the code (default: 7 levels).

---

## Quick Reference

| Situation | Action |
|----------|------|
| Start game | Press any button |
| Play | Watch the sequence and repeat it |
| Mistake or took too long | System repeats automatically |
| Won | Box opens, rainbow animation |
| Reset | Hold any button for 2 seconds |
| Problems | Check power and connections |

---

Documentation generated automatically.
Last update: 2026-04-23