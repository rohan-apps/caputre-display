# caputre-display

## Cross Compile

### setup SDK 
- The SDK must include libdrm library.
- You can use the SDK provided by buildroot or yocto.

### configuration
 - for ARM static  
 $ ./autogen.sh --target=arm-linux-gnueabihf --host=arm-linux-gnueabihf --prefix=/usr --exec-prefix=/usr --enable-static=yes --enable-arch=arm  
 - for ARM shared  
 $ ./autogen.sh --target=arm-linux-gnueabihf --host=arm-linux-gnueabihf --prefix=/usr --exec-prefix=/usr --enable-arch=arm 
   
 - for X86 shared   
 $ sudo apt install libdrm-dev  
 $ ./autogen.sh --includedir=/usr/include

### build
 $ make

### install
 $ make DESTDIR=<PATH> install
 or
 $ make DESTDIR=<PATH> install-strip
 - install stripped binary
