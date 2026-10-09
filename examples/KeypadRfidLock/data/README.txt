Include the lock.json file in LittleFS, and place the following audio files in /sfx/:

Arquivos necessarios:
- /sfx/beep.mp3     (Bip de tecla pressionada)
- /sfx/error.mp3    (Som de erro/acesso negado)
- /sfx/magic.mp3    (Som de acesso liberado)
- /sfx/cancel.mp3   (Som ao resetar/limpar)

Note: If /lock.json does not exist in LittleFS, the code will use default values.
embutidos (code=1234, mode=KEYPAD_OR_RFID, lockPin=10).
