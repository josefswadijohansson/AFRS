# AFRS

[![CC BY-NC-SA 4.0][cc-by-nc-sa-shield]][cc-by-nc-sa]

Just a fun side electronic project, an Automatic Fishing Reel Spooler(not for when you are on the lake, but before when you are putting line on the fishing reel).

## Demostration

[Showcasing Video](https://www.youtube.com/shorts/x-qOwM7sNGY)

## Software Used

- [FreeCAD 1.0](https://www.freecad.org/)
- [OrcaSlicer](https://orcaslicer.com/)
- [Blender](https://www.blender.org/)

## Library Used
- LittleFS
- AccelStepper
- ArduinoJson
- Wifi
- Webserver

## Concept Picture
![alt text](https://github.com/josefswadijohansson/AFRS/blob/main/Concept%20pictures/v3/AFRS_concept_v3.png)


## End Result
<p float="left">
  <img src="https://github.com/josefswadijohansson/AFRS/blob/main/Pictures/pic1.jpg" width="400" height="400" />
  <img src="https://github.com/josefswadijohansson/AFRS/blob/main/Pictures/pic2.jpg" width="400" height="400" />
  <img src="https://github.com/josefswadijohansson/AFRS/blob/main/Pictures/screenshot_webserver.png" width="400" height="400" />
</p>

## Components used

- 2x ESP32-C3 Mini development boards
- 2x Small "Pancake" variants of Nema 17
- 2x TMC2208 Driver module
- 150mm T8 Leadscrew
- 100mm Linear rail
- 3.3v Small laser guide
- 2x Push buttons acting as limit switches

## Improvements and fixes

- For the fishing line distance measuring, i was planning on having a measuring wheel with magnets glued to them, and when fishing line was reeled on to the new fishing reel it would spin and activate a hall sensor and for each pulse a distance would be added to the counting, issue is when i had printed the measuring wheel and then glued magnets, i realised that the hall sensor only activated on one polarity, which i didn't know. To not have this issue blocking the development of the webserver i instead removed and put that part of the project in pause for later testing and adding in later. So in the future i want the user to be able to set a X amount of meters of line to be only feed to the reel via the webserver interface.
- For now it uses TCP communication, issue is that since status update is wanted on the webserver to know the device position and state, TCP is a bit blocking on the ESP32, meaning i would prefer to maybe use UDP or some other communcation type to not block the device but to know the status of the device in the future.
- Could make the webserver interface better looking and user friendly, for now it's in the most basic form, issue i wanted to reduce file size since i had limited 400kb memory on the ESP32-C3
- Instead of hosting a webserver i could develop a desktop .NET program that talks to the ESP32-C3 devices

## License

This work is licensed under a
[Creative Commons Attribution-NonCommercial-ShareAlike 4.0 International License][cc-by-nc-sa].

[![CC BY-NC-SA 4.0][cc-by-nc-sa-image]][cc-by-nc-sa]

[cc-by-nc-sa]: http://creativecommons.org/licenses/by-nc-sa/4.0/
[cc-by-nc-sa-image]: https://licensebuttons.net/l/by-nc-sa/4.0/88x31.png
[cc-by-nc-sa-shield]: https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg
