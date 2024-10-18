# The Guard on Raspberry Pi 4b

A simple integrated system for alerting of an unauthorized physical intrusion.

Features:
[ ] Switch to the calm mode by entering a password
[ ] Report to another device through network

## Required items

[ ] Raspberry Pi 4b
[ ] Motion detector
[ ] Buzzer
[ ] 4 diodes
[ ] 4 buttons
[ ] A PC with Linux system that can run Buildroot
*TODO: details, blueprints*

## Installation

0. (recommended) Delete <kbd>.git</kbd> directory to prevent git warnings
1. Run <kbd>./rpisecsys.sh</kbd>
2. Format RPi's storage with <kbd>buildroot/output/images/sdcard.img</kbd> *TODO: how?*
3. Copy <kbd>buildroot/output/images/Image</kbd> to RPi's storage *TODO: where?*
4. Run <kbd>rpi_security_system</kbd> on the RPi
