# Running

```console
g++ -o sampler main.cpp
sudo usb_modeswitch -v 0763 -p 103b --detach # Remove the driver
sudo ./sampler 
```