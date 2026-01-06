# VeryImportant

- [x] Add box weight to db 
- [x] Add Update db function to update db box height and box weight
- [x] Add confirmation window for updating weight or height of box before saving to db.
- [x] Add Text next to weight input field if no weight been set by a user before noting that they should weigh 1 package and put the weight in the field. (if we save the weight we calculate maybe we should also have this message once the weight we load is the same as the one we calculate???)


# Important:

- [x] Undo new stuff until it is functioning without additional stuff
- [ ] Palettieren fortsetzen ja nein (wenn palette nicht frei und programm abbruch mitten in lage)
  - [ ] maybe automatically continue???
- [ ] Palette leeren wenn palettierung erfolgreich beendet wurde.
  - [x] Manual palette clearing implemented (`clear_palette`, `check_palette_clearing_status`)
  - [ ] Automatic clearing when palletization finishes successfully
- [ ] rename Invert label to something more fitting (label facing op _checked by default_)
  - Still named "Invert Label" in UI files

# Ideas

- [x] Zwischenlagen bestätigen popup
- [x] keyfile on usb for passwordless settings entry
- [x] invert label position
- [ ] Klemmung an aus via HMI
- [x] database
  - [x] saving palette data into db
  - [x] dimension search palettes
  - [x] loading through database
- [ ] fix [audio.py](./utils/audio/audio.py)
  - [ ] volume wont work (volume control exists but may have issues)
  - [ ] after changing volume from 100 to 0 to 100 it stops playing
  - [x] Safety status monitoring with audio signals implemented
  - [ ] add more signals (audio files) for certain alarm types
- [ ] generate report
- [ ] automatisch wechseln aus/an
- [ ] palette fertig simulieren (palette ist "fertig" auch wenn nicht fertig)
- [ ] Nur auf Palette1/2 palettieren

Maybe:

- [ ] i forgor
