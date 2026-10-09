# User Manual - Vintage Shortwave Radio

## For Escape Room Owners

**Version:** 2.0 (PropMaker Edition)  
**Puzzle:** Vintage Radio with FM Tuning and Morse Code  
**Level:** Intermediate  
**Estimated Time:** 10-15 minutes

---

## Puzzle Overview

This is a **vintage shortwave radio** that the players need to "repair" to receive a secret message. Its operation imitates an old tube radio.

### Physical Components

1. **Transformer** (main part - connects first)
2. **3 Valves** (connect to the radio to activate the FM)
3. **1 Final Valve** (connects last to receive the message)
4. **Tuning Knob** (rotary encoder)
5. **Radio Case** with indicator LEDs

### Player Objective

1. Turn on the radio (connect the transformer)
2. Insert the 3 valves to activate the radio system
3. Explore the radio stations by turning the knob
4. Insert the final valve to receive the message in Morse code
5. Write down the message and use it to leave the room

---

## How It Works (For the Player)

### Step by Step of the Experience

**1. Turning on the Radio**
- The player connects the transformer to the case
- The white LED (Power) turns on
- The radio is "on" but does not work yet

**2. Inserting the Valves**
- As each valve is inserted, a blue LED turns on
- The system makes different sounds for each one:
  - **Valve 1:** Short sound (beep beep beep)
  - **Valve 2:** Long sound (beeeep beeeep beeeep)
  - **Valve 3:** The radio "comes alive" - now it makes radio noise!

**3. Tuning the Radio**
- With the 3rd valve inserted, the knob works
- Turning the knob, the player hears different radio stations
- Each station has a different interference/static sound
- The volume increases and decreases as the knob is turned (simulating tuning).

**4. The Solution**
- The player needs to insert the **4th valve (Final Valve)**
- This stops the radio noise and starts the message in Morse code
- The message is transmitted in beeps by the radio
- The players need to decipher the Morse code

**5. Deciphering**
- The message in Morse code is repeated 5 times
- The player writes down the translation (it comes written in the normal alphabet)
- The message contains the password or clue for the next step of the game

---

## Configuration For You (Operator)

### Before the Match

#### 1. The Secret Message (IMPORTANT!)

You can change the message that will be transmitted in Morse code.

**How to do it:**
1. Remove the SD card from the system
2. Open the file `mensagem.txt` on your computer.
3. Write your message (you can use accents and punctuation)
4. Save the file
5. Put the SD card back in the system

**Example messages:**
```
CONGRATULATIONS THE PASSWORD IS 4729

THE DOOR OPENS WITH THE RED CODE

LOOK FOR THE BOX IN THE CLOSET

THE NEXT DIGIT IS 7
```

**Rules:**
- Use short phrases (maximum 256 characters)
- You can use letters A-Z, numbers 0-9 and spaces
- Accents are accepted (they are converted automatically)
- The message is read from the SD card first, then from the internal memory

#### 2. The Radio Station Sounds

The radio has **9 stations** that the player can explore. Each one plays a different MP3 file.

**Required files:**
- `station1.mp3` through `station9.mp3`

**Where to put them:**
- **Option 1 (recommended):** SD card in the root (main folder)
- **Option 2:** Internal memory (LittleFS)

**Priority:** If the same file exists on the SD card and in the internal memory, the **SD card wins**.

**This allows:** You can have a default version in the internal memory, and place special versions on the SD for specific events (for example, a station with extra clues or custom messages for different groups).

**What kind of audio to use:**
- Radio interference sounds (static, hiss)
- Old radio broadcast sounds
- Any MP3 at 128kbps, 44.1kHz, stereo
- Duration: can be short (automatic loop)

**Suggestion:** Use different sounds for each station to create the sensation of "different channels". If you do not know where to get them, you can leave the default files that already come with the system.

### Test Before Use

**Test procedure:**

1. **Reset:** Disconnect all parts
2. **Turn on:** Connect the transformer
   - The Power LED should turn on (white)
3. **Valve 1:** Connect the first valve
   - Blue LED turns on + short beep sound
4. **Valve 2:** Connect the second valve
   - Blue LED turns on + long beep sound
5. **Valve 3:** Connect the third valve
   - Blue LED turns on + radio starts to "make noise"
   - Turn the knob - you should hear different stations
6. **Valve 4:** Connect the final valve
   - The radio stops making noise
   - It starts transmitting Morse code
   - The message should be clear and repeat 5 times

**If something does not work:**
- Check whether all connections are firm
- Restart by disconnecting and reconnecting the transformer
- Check whether the SD card is inserted correctly
- Swap the SD card if necessary

---

## Troubleshooting

### Problem: The radio does not turn on

**Possible causes:**
1. Transformer not connected correctly
2. Problem in the electrical connection
3. System froze

**Solutions:**
- Disconnect and reconnect the transformer firmly
- Check whether the Power LED turns on
- If it persists, turn the system off and on completely
- Check whether the USB cable is connected (if using USB power)

### Problem: The valves do not work

**Possible causes:**
1. The valves need to be connected in order
2. Loose connection
3. System was not reset

**Solutions:**
- Always connect in the order: Valve 1 → 2 → 3 → 4
- Press firmly until you hear/see the feedback
- Each valve should turn on its blue LED
- Disconnect the transformer and start over

### Problem: The tuning knob does not work

**Possible causes:**
1. The 3rd valve is not connected
2. Problem in the encoder

**Solutions:**
- Check whether the 3rd valve is connected (blue LED on)
- The knob only works AFTER the 3rd valve is inserted
- Turn the knob slowly to test
- Check whether the encoder is well connected

### Problem: I do not hear the radio stations

**Possible causes:**
1. MP3 files missing or with the wrong format
2. Volume is too low.
3. Problem on the SD card
4. Problem in the speaker

**Solutions:**
- Check whether the files `station1.mp3` to `station9.mp3` exist on the SD
- The format must be MP3, 128kbps, 44.1kHz
- Try replacing the SD card
- Check whether the speaker is connected

### Problem: The Morse message does not play

**Possible causes:**
1. The file `mensagem.txt` is missing.
2. The file `mensagem.txt` is empty.
3. SD card is not being read

**Solutions:**
- Check whether `mensagem.txt` exists in the SD root directory.
- Open the file and confirm that it has text inside
- If the file is missing, the system uses "SOS" as default
- Try another SD card

### Problem: Sound with noise or distorted

**Possible causes:**
1. MP3 files with an inadequate format
2. ID3 tags in the MP3s causing problems
3. Slow or corrupted SD card

**Solutions:**
- Re-encode the MP3s using the ffmpeg command:
  ```
  ffmpeg -i input.mp3 -acodec libmp3lame -ar 44100 -ac 2 -b:a 128k -map_metadata -1 output.mp3
  ```
- This removes ID3 tags and standardizes the format
- Format the SD card as FAT32

### Problem: The radio freezes or restarts by itself

**Possible causes:**
1. Weak power supply
2. Transformer was removed (intentional HARD RESET)
3. System needs a reset

**Solutions:**
- Use a 5V power supply with at least 2A
- The HARD RESET is normal when you remove the transformer - reconnect to continue
- If it freezes, disconnect everything and reconnect the transformer

---

## Care and Maintenance

### Cleaning
- Clean the valve connections with a dry cloth periodically
- Avoid moisture in the system
- Keep the tuning knob clean
- Do not use chemical products directly on the connectors

### Storage
- Store the valves in a dry place when not in use
- Avoid leaving the transformer connected for long periods without use
- Protect the SD card from dust and moisture

### File Backup
- Keep a copy of the files `station*.mp3` and `mensagem.txt` on your computer.
- If the SD card corrupts, you can recreate them easily
- Test the system weekly to guarantee operation

---

## Tips For Your Game

### Difficulty

**Easy:**
- Place the Morse message written somewhere in the room
- The players only need to identify the pattern
- The 4th valve can be visible and accessible

**Medium:**
- Provide a Morse code table in another puzzle
- The players need to decipher the message
- The 4th valve can be hidden or in another location

**Hard:**
- Without external help - the players need to know or discover Morse code
- The message can be an encrypted clue
- The 4th valve only appears after solving another puzzle

### Time

- The Morse transmission repeats **5 times**
- Each cycle takes a few seconds
- Total time: about 1-2 minutes to hear everything
- Leave enough time for the players to write it down
- The FM system can stay on indefinitely - there is no rush

### Combine With Other Puzzles

The Morse code can be:
- A numeric password for a padlock
- Coordinates to find a hidden object
- The first part of a longer password
- A clue about another puzzle in the room
- The code for an electronic safe

---

## Pre-Game Checklist

Use this list before each group of players:

- [ ] Transformer disconnected (initial position)
- [ ] All valves disconnected
- [ ] SD card inserted correctly
- [ ] Quick test: connect the transformer → Power LED turns on?
- [ ] Test: valve 1 → blue LED + short sound?
- [ ] Test: valve 2 → blue LED + long sound?
- [ ] Test: valve 3 → blue LED + radio noise?
- [ ] Test: turn the knob → changes station?
- [ ] Test: valve 4 → Morse message plays?
- [ ] Is the message configured in the `mensagem.txt` file correct?
- [ ] Audio files available (station1-9.mp3)
- [ ] Adequate volume for the room

---

## Basic Technical Information

### Specifications

| Item | Specification |
|------|---------------|
| Power | 5V DC (via USB or power supply) |
| Consumption | ~500mA typical, 1.5A peak |
| SD card | MicroSD, FAT32 format |
| Audio | MP3, 128kbps, 44.1kHz, stereo |
| Connections | 4mm banana plugs |

### PCF8575 Pins (for technical reference)

| Pin | Function |
|------|--------|
| P0 | Transformer |
| P1 | Valve 1 |
| P2 | Valve 2 |
| P3 | Valve 3 |
| P4 | Encoder A |
| P5 | Encoder B |
| P6 | Encoder Button |
| P7 | Valve 4 |

---

## Technical Support

If you find problems that you cannot solve:

1. Check whether all cables are connected correctly
2. Test with a different SD card
3. Check the logs in the serial monitor (if you have technical access)
4. Refer to the `ALGORITMO.MD` file for complete technical details.
5. Contact the manufacturer

### Contact

For technical support or questions about the puzzle:

**GitHub:** https://github.com/djairhector/propmaker_escaperoom

---

## Quick Summary

| Component | Function | When to Use |
|------------|--------|-------------|
| Transformer | Turns on the system | Always first |
| Valve 1 | Activates short sound | After the transformer |
| Valve 2 | Activates long sound | After valve 1 |
| Valve 3 | Activates the FM radio | After valve 2 |
| Knob | Changes stations | After valve 3 |
| Valve 4 | Receives Morse message | Last - activates the solution |

**Important files on the SD:**
- `mensagem.txt` – The message in Morse code
- `station1.mp3` to `station9.mp3` - Station sounds

**File priority:** SD Card > Internal memory

---

*Manual for Escape Room Owners*  
*PropMaker EscapeRoom System - Vintage Radio*  
*Version 2.0 - Updated with the 4-valve system and SD Card priority*