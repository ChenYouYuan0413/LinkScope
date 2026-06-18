**简体中文 | [English](README_en.md)**

<div align=center>
	<img src="imgs/icon.svg" height="100px"/>
	<h1>LinkScope</h1>
</div>

<div align=center>
	<font size=3>
		<b>非侵入式硬件调试工具</b><br>
		<b>变量示波 · 在线改值 · 数据导出 · 日志输出</b>
	</font>
</div>

<br>

<div align=center>
	<p>
		<a href="https://gitee.com/skythinker/link-scope">
			<img src="https://gitee.com/skythinker/link-scope/badge/star.svg"/>
			<img src="https://gitee.com/skythinker/link-scope/badge/fork.svg"/>
		</a>
		<a href="https://github.com/Skythinker616/LinkScope">
			<img src="https://img.shields.io/github/stars/skythinker616/LinkScope?logo=github&style=flat"/>
			<img src="https://img.shields.io/github/forks/skythinker616/LinkScope?logo=github&style=flat"/>
		</a>
		<img src="https://img.shields.io/badge/License-GPL3.0-red"/>
	</p>
</div>

---

## 项目简介

本程序可直接驱动硬件调试器与单片机连接，并通过解析编译后的符号文件得到变量表，实现对目标芯片的非侵入式调试（无需修改芯片程序）。

程序使用QT编写，基于OpenOCD广泛支持多种调试器和芯片。

此外，也支持通过串口连接任意带有串口的芯片（需移植下位机程序）。

**主要功能**

* 实时查看和修改变量值

* 实时绘制变量值波形

* 导出采样数据

* 格式化日志输出

**连接方式**

软件支持以下两种方式与目标芯片连接：

* 通过调试器连接：

	* 非侵入式连接，无需修改目标芯片程序

	* 理论上支持OpenOCD所支持的各种调试器及硬件芯片，如STLink、JLink、CMSIS-DAP等以及STM32全系列等

		> 附OpenOCD官方文档： [支持的调试器](https://openocd.org/doc/html/Debug-Adapter-Hardware.html)、[支持的芯片](https://openocd.org/doc/html/CPU-Configuration.html#Target-CPU-Types)

	* 对于需要专用OpenOCD驱动的芯片（如ESP32C3等），允许在外部手动开启OpenOCD进程后进行连接

	* 最高采样速度约100Hz

* 通过串口连接：

	* 侵入式连接，需将一段下位机程序移植至目标芯片中

	* 支持任意带有串口的芯片

	* 最高采样速度约80Hz

![运行演示](imgs/run-demo.png)

---

## 使用方法

1. 若使用串口连接，或需使用日志功能，请参考[串口移植说明](lower/serial/README.md)和[日志移植说明](lower/log/README.md)将对应下位机程序移植到目标芯片中

2. 下载最新发行版，解压后双击`LinkScope.exe`运行程序

3. 点击设置编译好的符号文件（如AXF或ELF文件），然后设置要查看的变量

	* 添加变量：

		* 在变量选择窗口添加

		* 或在主窗口表格最后一行的变量名处手动填写

	* 删除变量：

		* 在变量名上单击右键

		* 或单击选中变量名后按Del键

	> 注：添加的变量名可以是任何合法的C语言表达式，可参考[进阶使用说明](#进阶使用说明)；结构体等复合类型只能查看，不能修改和绘图

4. 选择连接模式，连接芯片，连接成功后程序开始采样

	* 调试器模式下，在下拉框中选择调试器和芯片类型，点击连接目标；或勾选“外部进程”后直接连接到正在运行的OpenOCD进程

	* 串口模式下，点击“刷新串口”加载串口列表，选中所连接的串口，点击连接目标

5. 编辑`修改变量`列可以修改变量值，双击`图线颜色`列可以选择绘图颜色

6. 单击`变量名`列选中对应的变量，绘图窗口会加粗绘制波形，左下角会显示当前值和查看值（拖动鼠标进行查看）

7. 绘图界面中滚轮配合`Ctrl`、`Shift`、`Alt`可以实现画面的缩放和移动

![操作演示](imgs/oper-sample.gif)

![日志输出](imgs/log-sample.gif)

---

## 主要菜单项说明

* `刷新连接配置`：若自行编写了配置文件，可以通过该菜单项将配置文件加载到下拉选框中，可参考[进阶使用说明](#进阶使用说明)

* `保存配置`：软件中所配置的连接模式、调试器型号、芯片型号、符号文件路径和各变量的配置都可以通过该菜单项保存到一个配置文件中

* `导入配置`：将上述保存的配置文件重新载入软件中

* `导出数据`：将获取到的各变量采样数据导出到CSV表格文件

* `高级设置`：打开高级设置窗口，可以设置串口参数配置、采样频率、GDB端口等

---

## 进阶使用说明

* 在变量名处可以填写任意GDB支持的C语言表达式

	假设目标程序的`main.c`中含有如下全局变量

	```c
	static int g_int = 0; //静态全局变量
	int g_arr[10] = {0}; //全局数组
	struct Pack {
		int var1,var2;
	} g_pack = {0}; //全局结构体变量
	```

	则可以通过变量名实现下列查看

	```c
	g_int //查看变量g_int的值
	&g_int //查看g_int的地址
	g_pack.var1 //查看g_pack中var1成员变量的值
	g_pack.var1+g_pack.var2 //对两个变量求和
	p_arr[0] 或 *g_arr //查看g_arr数组的第一个元素
	g_pack //查看整个结构体(不能绘图)
	g_arr[2]@3 //查看g_arr[2]至g_arr[4](不能绘图)
	'main.c'::g_int //指定查看main.c文件中的g_int
	*(int*)0x20005c5c //查看0x20005c5c地址上的一个int数据(此方式无需设置符号文件也能用)
	```

* 可以通过修改OpenOCD配置文件自定义调试器模式下的连接

	* 配置文件位于`openocd/share/openocd/scripts`下的`target`和`interface`文件夹中，可以在其中进行修改或添加

	* 配置语法可以参考[OpenOCD官方文档](https://openocd.org/doc/html/Config-File-Guidelines.html)

	* 若新增了配置文件，可以通过`刷新连接配置`菜单项将其载入到软件下拉选框中

---

## 使用注意事项

* 若不指定符号文件，无法使用变量名，只能通过绝对地址进行查看

* 修改符号文件路径后需要重新连接

* 本程序不带下载功能，连接目标前请确认已为目标芯片下载过指定程序；若更换为不同类型的调试器，即使芯片程序没有变动，也建议使用更换后的调试器再次下载程序

* 串口和日志下位机程序应使用与上位机同时发行的版本，更新上位机软件后应同时更新下位机程序

---

## 已知问题

* 若程序在调试器连接模式下发生错误闪退，可能在下次运行时无法成功连接目标，可以尝试手动查找`openocd.exe`进程并强制结束

* 串口模式下从地址0读取单个字节结果恒为0

---

## 其他说明

**关于采样速度**

* 采样速度与CPU占用率、添加的变量数量、日志输出频率等因素相关，程序会以尽可能高的速度进行采样

* 简介中介绍的采样速度是在`i5-8265U`CPU接近空载时，添加单个变量并关闭日志的情况下测试得到的

* 调试器模式下获取单条日志用时约50ms，串口模式约90ms，获取过程中无法进行采样，若日志数量较多则会对采样速度造成较大影响

**关于实际支持的设备**

* 虽然理论上支持内置OpenOCD所支持的所有设备，但受各种因素影响，实际使用时仍可能在部分设备上无法工作

* 目前已测试过的设备如下：

	* 调试器：STLINK-V2、CMSIS-DAP

	* 目标芯片：STM32F103RCT6、STM32F103C8T6、STM32F407ZGT6、STM32F407IGH6、ESP32C3(内置JTAG+[专用OpenOCD](https://github.com/espressif/openocd-esp32))

---

## 开发说明

> 注：若您希望参与贡献，可以阅读本段说明，若仅需使用软件则无需阅读

**开发环境**

* IDE：Qt Creator

* QT版本：5.9.9

**运行过程简介**

* 调试器模式下，连接目标时，程序会在后台启动OpenOCD进程进行连接，并命令GDB进程连接到OpenOCD；串口模式下，OpenOCD被替换为一个Tcp服务器用于解析GDB的RSP指令并收发串口数据

	![程序结构框图](imgs/process-block-diagram.png)

* 运行过程中主进程会通过标准输入输出与GDB进程进行交互，在用户添加变量时使用`display expr`指令将变量添加到GDB的查看表中，同时定时10ms发送`display`指令并进行正则解析，更新用户界面

* 程序开有一个微秒级定时器，每收到一个变量采样数据时，会从该定时器获取当前的时间戳并与数据一起记录下来，同时绘图窗口会不断对历史数据进行更新绘图

* GDB不支持符号文件路径中含有非ASCII字符，因此在连接GDB前会将指定符号文件复制到GDB目录下并用相对路径读取，断开连接时删除临时文件

* 变量选择窗口另开有一个GDB进程用于解析符号文件，内部使用树形结构存储各级变量信息，在用户每展开一级树形图时使用`info variables`、`whatis`、`ptype`指令从GDB获取下一级变量类型信息并用正则表达式解析

* 日志下位机程序会创建一个缓冲区队列，下位机会将日志输出到缓冲区中，程序会定时使用GDB的print指令将日志出队并解析显示出来

**仓库文件说明**

* `mainwindow.cpp/h`：主窗口

* `graphwindow.cpp/h`：绘图窗口

* `listwindow.cpp/h`：变量选择窗口

* `logwindow.cpp/h`：日志窗口

* `aboutwindow.cpp/h`：关于窗口

* `helpwindow.cpp/h`：帮助窗口

* `openocd.cpp/h`：openocd进程控制

* `serialocd.cpp/h`：串口服务器相关逻辑，负责接收GDB的TCP连接并通过串口与下位机连接

* `gdbprocess.cpp/h`：gdb进程控制和输出解析

* `vartype.h`：变量相关数据类型定义

* `qss`：QSS样式文件

* `gdb`/`openocd`：gdb和openocd的执行和配置文件，不参与编译，需在编译后手动置于可执行文件同一目录下

* `lower/log`/`lower/serial`：下位机代码

**TODO**

* 增加对本地程序的支持

---

## 🚀 Enhanced Edition（macOS 适配 + VSCode 集成 + 高速模式）

以下为在原版基础上新增的功能和适配修改。

### 环境要求

| 依赖 | 版本 | 安装方式 |
|------|------|----------|
| Qt 5 | 5.15+ | `brew install qt@5` |
| arm-none-eabi-gdb | 17.2 | `brew install arm-none-eabi-gcc` |
| sqlite3 | 系统自带 | - |
| VSCode Cortex-Debug | 1.12.1+ | 扩展 `marus25.cortex-debug` |

### 新增功能

#### 1. VSCode Cortex-Debug Live Watch 同步

- 连接时自动读取 VSCode workspace state 中的 Watch 变量列表
- 每 2 秒轮询检测增删，自动同步到 LinkScope
- 支持自动展开结构体（`p` → `p.x`, `p.y`, `p.color.r` 等），支持多级嵌套
- 通过「高级设置」配置项目目录，自动扫描 `workspaceStorage` 找到对应 state.vscdb

**配置路径**：
```
~/Library/Application Support/Code/User/workspaceStorage/<id>/state.vscdb
→ key "marus25.cortex-debug" → livewatch.watchTree
```

#### 2. 高速批量采样模式（~3kHz）

- **批量内存读取**：一次 GDB `x/Nwx` 命令读取变量所在整块内存区域，本地按偏移拆值
- **绕过 GDB 变量查找**：连接时一次性解析所有变量地址（nm -S + GDB print &），运行时用裸地址 `x/1wx` / `x/1hx` / `x/1bx` 读取
- **类型感知**：自动检测 uint8_t/int8_t/uint16_t/int16_t/float 等类型，进行符号扩展和 IEEE 754 转换
- **解除 Hz 限制**：高速模式下 `watchTimer.setInterval(0)` 以 Qt 事件循环最大速度触发
- **实时 FPS 显示**：日志窗口每 500 帧输出实际采样率

**速度对比**：

| 模式 | 采样方式 | 典型速度 |
|------|---------|---------|
| 标准模式（GDB display） | GDB 变量名查找 + 逐个读取 | ~100Hz |
| 高速模式（批量 x/Nwx） | 一次读整块内存 | ~3kHz |

#### 3. 窗口停靠系统

- 图形窗口、日志窗口、变量选择器从独立 `QDialog` 改为 `QDockWidget` 嵌入主窗口
- 支持拖拽合并、Tab 分页、拖拽调整大小、拖出变为浮动窗口
- 布局状态自动保存，重启恢复

#### 4. UI 精简

- 调试模式选择、接口/目标下拉、外部 OpenOCD、Cortex 同步、高速模式 → 移到「高级设置」
- ELF 符号文件选择 → 移到「高级设置」
- 主窗口只保留变量表格 + 操作栏（连接/暂停/复位/日志）
- 新增「暂停」按钮，发送 GDB `monitor halt`/`monitor resume` 暂停/恢复采样

#### 5. 多 GDB 连接共享

- OpenOCD 配置 `gdb-max-connections 4`，允许 Cortex-Debug（2 连接）和 LinkScope（1-2 连接）同时接入
- LinkScope 勾选「外部 OpenOCD」模式，连接到已运行的 OpenOCD 实例

---

### macOS 适配修改

原版为 Windows 开发，以下为 macOS 编译和运行所需的修改：

| 问题 | 文件 | 修复 |
|------|------|------|
| `#include <windows.h>` | `openocd.h`, `mainwindow.h` | `#ifdef Q_OS_WIN32` 宏保护 |
| `setNativeArguments()` | `gdbprocess.cpp`, `openocd.cpp` | Windows 专用 API，macOS 改用 `setArguments()` + `setProgram()` |
| `taskkill` 杀进程 | `openocd.cpp`, `mainwindow.cpp` | macOS 改用 `QProcess::terminate()` |
| GDB 输出 `\r\n` → `\n` | `gdbprocess.cpp`, `listwindow.cpp` | 所有正则 `\r\n` → `\r?\n`，GDB 提示符检测改用 `trimmed().endsWith("(gdb)")` |
| `QDialog → QWidget` | `graphwindow.h/cpp`, `listwindow.h/cpp`, `logwindow.h/cpp` | 改为 QWidget 以支持 QDockWidget 嵌入 |
| `.ui` 文件基类 | `graphwindow.ui`, `listwindow.ui`, `logwindow.ui` | `QDialog` → `QWidget` |
| GDB 启动参数 | `gdbprocess.cpp` | 添加 `-q -nx -ex "set confirm off"` 参数 |
| 临时文件路径 | `gdbprocess.cpp` | App bundle 无写权限，改用 `QDir::tempPath()` |
| 配置文件路径 | `mainwindow.cpp` | `conf.ini` 相对路径 → `~/.linkscope/conf.ini` 绝对路径 |
| `1u << 32` 未定义行为 | `mainwindow.cpp` | ARM64 shift mask 导致 4 字节变量归零，特判 `size >= 4` 用 `0xFFFFFFFF` |
| GDB 管道分批送达 | `gdbprocess.cpp` | `trimmed().endsWith("(gdb)")` 健壮检测提示符 |
| UI 布局插入 | `mainwindow.cpp` | 通过 `findChild` 找到 `groupBox_4` 插入暂停按钮 |

---

### Windows 端迁移和二次开发注意事项

1. **环境准备**：安装 Qt 5.15+（MSVC 或 MinGW）、ARM GCC 工具链
2. **编译**：`qmake LinkScope.pro && make`（或 `nmake`）
3. **运行依赖**：`openocd.exe` 和 `gdb.exe` 需放在可执行文件同目录的 `openocd/bin/` 和 `gdb/` 下
4. **路径分隔符**：代码中 `applicationDirPath()` + `"/"` 在 Windows 下也可用（Qt 会转换）；原版用的 `\\` 无需改动
5. **进程管理**：Windows 保留 `taskkill` / `tasklist` 相关代码（已用 `#ifdef Q_OS_WIN32` 保护）
6. **配置存储**：Windows 下 `QSettings("conf.ini", ...)` 相对路径写到 exe 同目录，如需集中管理可改为 `QStandardPaths`
7. **GDB 二进制**：macOS 版用 `/opt/homebrew/bin/arm-none-eabi-gdb`，Windows 版用 `gdb/gdb.exe`（保持原版逻辑）
8. **跨平台共用**：所有平台相关代码均用 `#ifdef Q_OS_WIN32` / `#else` 分支，mac 分支可直接在 Linux 上使用（需调整 GDB 路径）
9. **新增文件清单**：
   - `mainwindow.h/cpp` — 大量新增函数（syncWatchVars、resolveVarAddresses、valueToString、struct 展开等）
   - `configwindow.h/cpp` — 扩展 ConfigWindowParam + 动态 UI
   - `gdbprocess.cpp` — runCmd 修复、启动参数、跨平台 GDB 路径
   - `vartype.h` — VarInfo 增加 address/size/isSigned/isFloat 字段
   - `.gitignore` — 新增构建产物排除
10. **恢复 Windows 原版**：切到 `master` 分支即可；`mac` 分支包含所有 macOS 适配和功能增强
