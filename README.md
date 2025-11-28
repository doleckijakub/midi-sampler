# Building

Simply run

```console
make
```

in the project root

# Running on Linux

Most Linux distributions ship with MIDI drivers, thus for the program to work we need to disable them to be able to read the raw USB data

```console
sudo usb_modeswitch -v 0763 -p 103b --detach
```

Then, because on Linux the USB devices are owned by root and you need root access to access them you need to either

```console
sudo chmod 666 /dev/bus/usb/XXX/YYY
bin/sampler /dev/bus/usb/XXX/YYY
# Where XXX is the bus and YYY the device of your M-Audio Oxygen Pro Mini
```

or (not recommended, but does not expose your USB to non-root users)

```console
sudo bin/sampler /dev/bus/usb/XXX/YYY
# Where XXX is the bus and YYY the device of your M-Audio Oxygen Pro Mini
```