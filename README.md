# ESP32-Chime

How to program the ESP, see repository ESP32-Doorbell

The ESP32-chime is made, to send a 433mhz signal or modify a byron 23513 chime to a wifi chime.
Both need to be supported by rcswitch library (https://github.com/sui77/rc-switch).

Sending the command http://ip/ring will trigger the code in the esp32 chime. This will use the stored data, which is configurated in the menu.
Sending the custom command http://ip/ring?protocol=**&pulse=**&code=** (where ** need to be changed for your data values)
This will output the code, which can be send using a 433mhz transmitter, or build in the byron 23513 chime

The 433mhz transmitter which can be used

<img src="assets/Readme_home_pictures/433Mhz-RF-Wireless-transmitter.jpg" width="150" >

The Byron 23513 which can be modified (warranty is gone, your own responsibility when doing this)
Info how to change/modify the byron chime will folow later.

<img src="assets/Readme_home_pictures/byron_dby-23513.jpg" width="150" >
