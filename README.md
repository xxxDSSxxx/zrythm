[![translated](https://translate.zrythm.org/widgets/zrythm/-/zrythm/svg-badge.svg "Translated")](https://translate.zrythm.org/projects/zrythm/zrythm/)
[![pipeline](https://git.zrythm.org/zrythm/zrythm/badges/master/pipeline.svg "Pipeline")](https://git.zrythm.org/zrythm/zrythm/pipelines)
[![coverage](https://git.zrythm.org/zrythm/zrythm/badges/master/coverage.svg "Test Coverage")](https://git.zrythm.org/zrythm/zrythm)
[![chat](data/chat-matrix.svg "Chat on Matrix")](https://riot.im/app/#/room/#freenode_#zrythm:matrix.org?action=chat)
[![patrons](http://img.shields.io/liberapay/patrons/Zrythm.svg?logo=liberapay "LiberaPay Patrons")](https://liberapay.com/Zrythm)

Zrythm is a highly automated Digital Audio Workstation (DAW) designed to be featureful and intuitive to use. Zrythm sets itself apart from other DAWs by allowing extensive automation via built-in LFOs and envelopes and intuitive MIDI or audio editing and arranging via clips.

In the usual Composing -> Mixing -> Mastering workflow, Zrythm puts the most focus on the Composing part. It allows musicians to quickly lay down and process their musical ideas without taking too much time for unnecessary work.

It is written in C and uses the GTK+3 toolkit, with bits and pieces taken from other programs like Ardour and Jalv. Zrythm is free software licensed under the GPLv3+.

More info at https://www.zrythm.org

## Current state
![screenshot 1](https://www.zrythm.org/img/Screenshot%20from%202019-02-07%2018-19-51.png)
![screenshot 2](https://www.zrythm.org/img/Screenshot%20from%202019-02-07%2018-20-47.png)

## Currently supported protocols:
- LV2

## Installation
For easy package installation see [Installation](https://manual.zrythm.org/zrythm-configuration/installation/intro.html) in the Manual.

For manual installation,
```
./autogen.sh
./configure
make -j8
sudo make install
```

For more details see [INSTALL.md](INSTALL.md).

## Using
At the moment, Zrythm assumes you have Jack installed and will only run if Jack is running. For Jack setup instructions see https://libremusicproduction.com/articles/demystifying-jack-%E2%80%93-beginners-guide-getting-started-jack

Please refer to the [manual](https://manual.zrythm.org) for more information.

## Contributing
For detailed installation instructions see [INSTALL.md](INSTALL.md)

For contributing guidelines see [CONTRIBUTING.md](CONTRIBUTING.md)

For any bugs please raise an issue or join a chatroom below

## Chatrooms
### Matrix/IRC (Freenode)
`#zrythm` channel (for Matrix users `#freenode_#zrythm:matrix.org`)

## Mailing lists
zrythm-dev@nongnu.org for developers, zrythm-user@nongnu.org for users

## License
Copyright (C) 2018-2019  Alexandros Theodotou

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <https://www.gnu.org/licenses/>.

## Support
Zrythm and its up-to-date binaries and packages are provided at no cost.

Our aim is to work on Zrythm full time, so if you like the software we’d appreciate a donation below.

Thank you.

### Paypal
https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=LZWVK6228PQGE&source=url
### LiberaPay
https://liberapay.com/Zrythm/
### Bitcoin
bc1qjfyu2ruyfwv3r6u4hf2nvdh900djep2dlk746j
