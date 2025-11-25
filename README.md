# Running on Linux

```console
./build.sh
sudo usb_modeswitch -v 0763 -p 103b --detach # Remove the driver
sudo ./sampler /dev/bus/usb/XXX/YYY # Where XXX is the bus and YYY the device of your M-Audio Oxygen Pro Mini
```