#KreyosFirmware
====

##Instructions:

Hello there, this is *NOT* the official Kreyos firmware, which written not by a nice guy.
This firmware mostly does is translate the UI into Simplified Chinese...
And fix some anonying bugs. If you find anything and have the ablity to code, then submit PR this PLZ.
*DO NOT SUBMIT ISSUES, I'M NOT GONNA READING IT.*

##简介

你好，这*不是*Kreyos官方的固件，并且现在的维护者仅仅只是靠爱发电（随性开发）。
这个固件大多数的维护工作就是把界面翻译成中文……
顺便修复一些在使用时遇到的烦人的BUG。如果你遇到任何问题（*并且你还必须会写代码*），请提交一个PR，或者fork仓库回去自己开发。
*不要提交Issues（不要提任何关于固件、编译之类的问题），我不会去看的。*


#Then...然后呢？
====

##What you need:
1. You *NEED* IAR For MSP-430(`NOT for ARM, etc.`) v5.50 or above installed.
2. You *NEED* a computer that runs Windows, with unix environment(DO NOT use WSL).
3. Absolutely patience.
4. A bunch of free time to wait for until its compiled.

##你需要……
1. 你安装的*必须*是IAR For MSP-430（`不能是for ARM或者其他的`）v5.50或以上版本。
2. 你的电脑*必须*是Windows系统，上面还要装Unix环境（不能用WSL）。
3. `绝对的耐心`。
4. 充足的等待编译的时间。

##Compiled it step by step:
1. Make sure you have what you need(see above).
2. Do `git clone`.
3. (Optional) Check out the branch "lang_ZH-CN".
4. Open unix terminal, then navigate to the path of `Watch`.
5. (Optional) Use whatever the package management software you have on unix environment to install `make`.
6. Edit the file `Makefile`, make sure the variable `IARPATH` exists(If you have IAR For MSP-430 v7.x.x, it should be fine).
7. Run command `make`, then wait a little while.
If everything works fine, there should be a `firmware.txt` under inside of `Watch/objs.msp430/`, this is the firmware we need.

##编译步骤：
1. 确保上面所需都已经正确配置。
2. `git clone`这个仓库。
3. （可选）签出“lang_ZH-CN”。
4. 打开Unix环境的终端，然后进入`Watch`这个文件夹。
5. （可选）用Unix环境提供的包管理器来安装`make`这个程序。
6. 编辑`Makefile`这个文件，确保IARPATH这个变量指向的文件夹是存在的（如果你用的IAR版本是7，那么应该不需要编辑）。
7. 运行`make`命令后稍等一会。
如果你做完了，在Watch文件夹里面的objs.msp430子文件夹中就会有firmware.txt这个文件，这就是我们需要的固件了

##How to flash firmware?
Copy the `firmware.txt` to *path/to/KreyosFirmware*, then run `run.bat`.
*DO NOT EDIT `PROG` or `RESET` UNLESS YOU KNOW WHAT YOU'RE DOING.*

##怎么刷固件？
复制`firmware.txt`到*KreyosFirmware*文件夹里面，然后运行`run.bat`。
*如果你不知道`PROG`或`RESET`是干什么的，就不要去编辑它们。*

#For Developers 开发者提示
====
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
