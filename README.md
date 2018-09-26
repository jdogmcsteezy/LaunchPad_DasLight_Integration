# Original Repo
This DasLight integration was created using https://github.com/dvhdr/launchpad-pro. The repo by dvhdr gives access to a HAL that allows anyone to create a plug in for the Launchpad-pro device.

# Uploading to a Launchpad Pro
Directly from dvhdr with small edits marked by js - ''.

You'll need a sysex tool for your host platform. I recommend [Sysex Librarian](http://www.snoize.com/SysExLibrarian/) on macOS, and [MIDI OX](http://www.midiox.com/) on Windows.  On Linux, js - 'AMIDI is probably already installed'.

I won't describe how to use these tools, I'm sure you already know - and if you don't, their documentation is superior to mine!  Here's what you need to do:

1. Unplug your Launchpad Pro
2. Hold the "Setup" button down while connecting it to your host via USB (ensure it's connected to the host, and not to a virtual machine!)
3. The unit will start up in "bootloader" mode
4. Send your js - 'launchpad_pro_daslight.syx' file to the device MIDI port - it will briefly scroll "upgrading..." across the grid.
5. Wait for the update to complete, and for the device to reboot!

Tip - set the delay between sysex messages to as low a value as possible, so you're not waiting about for ages while the firmware uploads!

      

