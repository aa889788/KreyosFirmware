KreyosFirmware
==============

Build Instruction:
1. The build tool is IAR MSP-430 5.50 or higher. Suggest 8.0
2. You need a workable unix tool in path, like `make`. Suggest to install cygwin
3. run `make` in "Watch" folder, a watch.txt will be generated @objs.msp430.
4. use tool/convertbin to convert txt file to a bin file that is able to upload through phone application

简介：
1.  需要IAR MSP430 Workbench 5.50或者更高的版本，建议8.0
2.  需要一个在Windows下可用的Unix的环境，或者反过来，因为要使用`make`这类工具。建议使用cygwin
3.  在Watch目录下运行`make`，将会在 objs.msp430 文件夹中生成一个名为`watch.txt`的文件，这就是固件了
4.  使用 tools/convertbin 下面的工具将txt转换成bin文件，这样可以在未来的配套手机app上推送更新