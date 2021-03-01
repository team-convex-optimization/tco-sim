# Simulator

## Joystick
To get an Xbox360 controller working on Ubuntu:
1. Install ```sudo apt install dkms```.
2. Clone the driver ```sudo git clone https://github.com/paroj/xpad.git /usr/src/xpad-0.4```
3. Install driver ```sudo dkms install -m xpad -v 0.4```

## Dependencies
- godot3
