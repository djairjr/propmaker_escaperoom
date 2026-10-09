This phone example uses a hybrid storage system:

Files in LittleFS Memory (System effects):
- dialtone.mp3   (Dial tone)
- busy.mp3       (Busy signal)
- naoexiste.mp3  (Invalid number warning)

Files on the SD Card (Dynamic content):
- [number].mp3   (Audio files for each dialed number, e.g.: 123.mp3)

Tip: In LittleFS, the files must be in the root (without the sfx/ folder).