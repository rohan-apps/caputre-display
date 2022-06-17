# caputre-display

## Cross Compile

### setup SDK 
- The SDK must include libdrm library.
- You can use the SDK provided by buildroot or yocto.

### configuration

 $ ./autogen.sh --target=arm-linux-gnueabihf --host=arm-linux-gnueabihf --prefix=/usr --exec-prefix=/usr

### build
 $ make

### install
 $ make DESTDIR=<PATH> install
 or
 $ make DESTDIR=<PATH> install-strip
 - install stripped binary
