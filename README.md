# ESP32-Chime

How to program the ESP, see repository ESP32-Doorbell

The ESP32-chime is made, to send a 433mhz signal or modify a byron 23513 chime to a wifi chime.
Both need to be supported by rcswitch library (https://github.com/sui77/rc-switch).

Sending the command http://ip/ring will trigger the code in the esp32 chime.
This will output the code, which can be send using a 433mhz transmitter, or build in the byron 23513 chime

The 433mhz transmitter which can be used

<img src="assets/Readme_home_pictures/433Mhz-RF-Wireless-transmitter.jpg" width="150" >

The Byron 23513 which can be modified (warranty is gone, your own responsibility when doing this)

<img src="assets/Readme_home_pictures/byron_dby-23513.jpg" width="150" >

How to modify Byron 23513, your own responsibility. <br>
Don't blame if you mess it up.
- Remove the screws on the backside <br>
- Open carefully the backside, just open a little<br>
- The powerplug part is held in place with 2 small hooks <br>
- Carefully open the hooks and take out the powerplug part. <br>
- Remove the screws, to take out the pcb <br>
- See the picture, where the 3,5Vdc, RF-signal and GND can be found.

The 3,5Vdc is on the 3,3Vdc pin. It is a little higher, but my ESP can handle it. <br>
Between the RF-signal and the GPIO i have a photomos relay, this takes out the enable signal of the ESP <br>
By doing this, you still can use the original push-button, otherwise not <br>

<img src="assets/Readme_home_pictures/20220205_193155_resized.jpg" width="250" > <br>
<img src="assets/Readme_home_pictures/20220205_193233_resized.jpg" width="250" >

