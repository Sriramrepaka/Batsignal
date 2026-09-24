# Batsignal
A ESP32 based batsignal, that projects a signal onto a wall when the paired smartphone receives a phone call, also turns it on when a voice assistant on the phone is triggered.

This project involves making a helicoid like a camera lens, using a rim I can move the lens further or closer to the Batman emblem so that the sharper projection can be changed based on the projection wall distance.  

All the 3D modes for the Batsignal are in "3D models" folder and the basic code for an ESP32 bluetooth relay board with LED pin as 23 and RELAY pin as 16 is present in code folder, please change accordingly. I am using a 220V ESP32 Relay board.

When ever the connected phone receives a call the light is turned on and stays on till the call disconnects or the user disconnects it. If a user triggers a voice assistant then also the light turns on for 3 minutes or when a new voice assistant is triggered again which ever is earlier. 
