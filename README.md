This repository contains a modified fork of the `talon` telescope control software for the Warwick One Metre telescope.

```
sudo yum install cmake3 motif-devel libXpm-devel
mkdir core_build
cd core_build
cmake3 ..
make
make package
```

Manually copy generaed rpm to the centos-packages repository and then trigger a travis build from another package to update the repository metadata

Install on the TCS machine using `sudo yum install --nogpgcheck onemetre-talon`.
Also need to `sudo yum install xorg-x11-fonts-*` for the xobs GUI to work.

TODO: Integrate the above steps into the toolchain and rpm deps.
