# Lib data channel api

## Setup in home directory
```bash
  rm -rf libdatachannel
  git clone --recursive https://github.com/paullouisageneau/libdatachannel.git
  cd libdatachannel
  mkdir build
  cd build
  cmake ..
  make -j$(nproc)
  sudo make install
  ```