The plymouth boot theme `install.sh` puts under `/usr/share/plymouth/themes/autobleem/`.

`splash.png` is the picture: 1280x720 on a black background (the script paints black around it and scales
it down if it is larger - it never scales up). The installer skips the boot splash, with a warning, if the
file is missing from the package.
