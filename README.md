# esp32-cam-motion-detection
This repository contains the required code to use a wifi esp32 micro-controller with an attached camera
as a cheap motion detector, which alerts you via email with the captured image and a timestamp, when something was
detected. The pictures get also saved to micro sd, which should be inserted into the esp 32.




## Requirements
- esp32 with camera (for example esp-cam-ai-thinker module)
- ftdi programmer (or esp32-cam-mb) to uplaod compiled code/power it
- jumper cables, if you use ftdi programmer
- battery or usb-b micro cable as power source
- micro sd card (optional)
- arduino ide

![components](https://user-images.githubusercontent.com/51853225/131255739-bba17b88-59fb-4797-bfd3-2b843828fd53.jpg)


## Installation
Clone the repository to your local computer.
```
git clone https://github.com/MarkusTelser/esp32-cam-motion-detection.git
```
Start arduino-ide and connect esp32 to computer. Then open the local repository.
Open esp32-cam-motion-detection.ino and configure the following values:
- WiFi name/credentials
- mail sender/recipient 
- mail account password
- ntp server settings
- camera resolution (optional)

Click 'Upload', so the code gets compiled and flashed on the micro controller. 
Open Serial Montior at 115200 baud and check if the output shows any errors. If 
the setup was completed correctly the red built-in led on the backside should 
flash three times.

## Usage
Connect esp 32 to power source and check if red led flashes three times. Everything is setup 
correctly, wait and check your mails or the SD after some time. Hopefully you catch something😉.

![esp32-cam and esp32-cam-mb](https://user-images.githubusercontent.com/51853225/131255771-698c6e5d-c659-4f95-9e18-9548c349e045.jpg)
![size comparison esp 32](https://user-images.githubusercontent.com/51853225/131255779-21452079-d4c8-4d20-bffc-4bd75389669f.jpeg)
![ESP32-Camera-Dev-Boards-Review-and-Comparison-Best-ESP32-CAM](https://user-images.githubusercontent.com/51853225/131255901-4e5a005e-4a8c-41a7-9fa9-b6f1057272bf.jpg)
