#!/bin/bash
make -j24 && sudo make INSTALL_MOD_STRIP=1 modules_install -j24 && sudo make install -j24
