# KreyosFirmware

## Instructions:

Hello there, this is *NOT* the official Kreyos firmware, which written not by a nice guy.

This firmware mostly does is translate the UI into Simplified Chinese...

And fix some anonying bugs. If you find anything and have the ablity to code, then submit PR this PLZ.

*DO NOT SUBMIT ISSUES, I'M NOT GONNA READING IT.*

# 简介

你好，这*不是*Kreyos官方的固件，并且现在的维护者仅仅只是靠爱发电（随性开发）。

这个固件大多数的维护工作就是把界面翻译成中文……

顺便修复一些在使用时遇到的烦人的BUG。如果你遇到任何问题（*并且你还必须会写代码*），请提交一个PR，或者fork仓库回去自己开发。

*不要提交Issues（不要提任何关于固件、编译之类的问题），我不会去看的。*

----
# Then...然后呢？

## What you need:
1. You *NEED* IAR For MSP-430(`NOT for ARM, etc.`) v5.50 or above installed.
   
2. You *NEED* a computer that runs Windows, with unix environment(DO NOT use WSL).
   
3. Absolutely patience.
   
4. A bunch of free time to wait for until its compiled.

## 你需要……
1. 你安装的*必须*是IAR For MSP-430（`不能是for ARM或者其他的`）v5.50或以上版本。
   
2. 你的电脑*必须*是Windows系统，上面还要装Unix环境（不能用WSL）。
   
3. `绝对的耐心`。
   
4. 充足的等待编译的时间。

## Compiled it step by step:
1. Make sure you have what you need(see above).
   
2. Do `git clone`.
   
3. (Optional) Check out the branch "lang_ZH-CN".
   
4. Open unix terminal, then navigate to the path of `Watch`.
   
5. (Optional) Use whatever the package management software you have on unix environment to install `make`.
   
6. Edit the file `Makefile`, make sure the variable `IARPATH` exists(If you have IAR For MSP-430 v7.x.x, it should be fine).
   
7. Run command `make`, then wait a little while.
   
If everything works fine, there should be a `firmware.txt` under inside of `Watch/objs.msp430/`, this is the firmware we need.

## 编译步骤：
1. 确保上面所需都已经正确配置。
   
2. `git clone`这个仓库。
   
3. （可选）签出“lang_ZH-CN”。
   
4. 打开Unix环境的终端，然后进入`Watch`这个文件夹。
   
5. （可选）用Unix环境提供的包管理器来安装`make`这个程序。
   
6. 编辑`Makefile`这个文件，确保IARPATH这个变量指向的文件夹是存在的（如果你用的IAR版本是7，那么应该不需要编辑）。
   
7. 运行`make`命令后稍等一会。
如果你做完了，在Watch文件夹里面的objs.msp430子文件夹中就会有firmware.txt这个文件，这就是我们需要的固件了。

## How to flash firmware?
Copy the `firmware.txt` under folder named *obj.msp430* to *path/to/KreyosFirmware*, then run `run.bat`.

`See, this is why you need to do compiling and flashing your watch on a Windows machine.`
*DO NOT EDIT `PROG` or `RESET` UNLESS YOU KNOW WHAT YOU'RE DOING.*

## 怎么刷固件？
复制*obj.msp430*文件夹里面的`firmware.txt`到*KreyosFirmware*文件夹里面，然后运行`run.bat`。

`这就是为什么需要在Windows系统上编译和刷固件的原因。`
*如果你不知道`PROG`或`RESET`是干什么的，就不要去编辑它们。*

## Having Problem when flashing device?
1. Q: Device not found or Windows found a unknown device.
   
   You need to install the driver for Windows 7/8 or lower version manually, Windows 10 might not having this problem.

   Check `Driver/CP210x_VCP_Windows` for driver installer.

2. Q: Device not found but driver is installed.

   Everytime you flash firmware to the watch require a Hard-reset(ie. LONG-PRESS ALL KEYS to force reboot), then it'll show a boot screen, connect to your computer ASAP, then open `flash.bat`.

   *If your watch stuck at any screen you might do a Hard-reset as well. It happens, a lot.* :(

3. Q: Can I do this under *nix or macOS using CrossOver?
   
   I don't know.

   But you can try to compile this tools by yourself, the source code is under `Watch/tools/BSL_Script`. Good luck(?).

## 刷固件时遇到问题了？
1. Q: 未找到设备，Windows提示发现一个未知设备。
   
   在Windows 7/8或者的更低版本中，你需要手工安装驱动程序。Windows 10就大概不需要了。

   在`Driver/CP210x_VCP_Windows`下载驱动程序，然后安装。

2. Q: 未找到设备，驱动程序也已经安装上了。
   
   每次你刷固件的时候，都要把手表重启一遍，方法就是长按手表上面的所有按键。当你的手表显示启动LOGO的时候，马上连接到电脑，然后打开`flash.bat`。

   *如果你的手表不幸卡住了，你也可以用这种方法强制重启。这毛病经常出现。* :(

3. Q: 我能在类unix或macOS系统上刷固件吗？
   
   我不知道。
   
   当然，如果你愿意的话，在`Watch/tools/BSL_Script`下有源码，你可以自己试着编译一个刷机工具出来。祝你好运(?)。
  
----

# For Developers 开发者提示

```
+-Watch				It's a mess.
|--ant				ANT stuff...
|--btstack			Header files of btstack, the implements already compiled in `btstack.r43`.
|--build			A map file required by IAR compiler.
|--core				Contiki system file.
|--flash			Flashing tool.
|--gesture			Meaning guesture.(Yes, it has the sensor.)
|--grlib			Fonts and drawing.
|--pawnscript		*Duplicated*, seems like they wanted user to upload their own applications.
|--pedometer		Well, it's "pedometer"
|--platform			Is's a mess.
|	|--common		Containing drivers and marco...
|	|--iwatch		Containing drivers, marcos and `main.c`.
|	\--native		Not related to project.
|--tools
|	|--BSL_Script	Source code of that flashing tool, but not compile-able.
|	|--convertbin	Convert *.txt* to *.bin*.
|	|--falshspi		That guy spell "flash" wrong, but I don't know what this does.
|	|--Fonts		This generates bin file of ordinary fonts(alphabet-based, or numbers, not Unicode).
|	|--grlib		Use `ftraterize` to generates Unicode fonts' bin file.
|	|--pawncc		*Duplicated*.
|	|--Transparent	*Duplicated*.
|--unittest			Unit test.
\--watch			Appliactions. If you like to programs you might place it here.
```
