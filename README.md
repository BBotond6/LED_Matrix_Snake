# LED Matrix (8x32) Snake with JoyStick
8x32 user controlable LED Matrix Snake
## Used components
- Potentiometer joystick
- Phototransistor
- Remote Controller
- 8x32 LED Matrix (FC16_HW)
- Arduino uno R3
## Working principle - Event loop
1. Analog read potentiometer values or read Remote Controller's pressed buttons with phototransistor
    - "OK" button in Remote Controller can change between the modes
3. Timing calculation
4. Snake shape calculation
5. Drawing - Digital out
## Demo video

Operation in 2022 Apr 3 without remote control:
https://user-images.githubusercontent.com/102668658/161448445-8409773e-3b0c-4cc6-9525-ff79f1c00a23.mp4

Operation in 2022 Maj 20 with remote control:
https://user-images.githubusercontent.com/102668658/169431234-602a0046-24a9-438c-be9f-dfba376a838d.mp4
