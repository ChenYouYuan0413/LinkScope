**[简体中文](README.md) | English**

<div align=center>
	<img src="imgs/icon.svg" height="100px"/>
	<h1>LinkScope</h1>
</div>

<div align=center>
	<font size=3>
		<b>Non-intrusive Hardware Debugging Tool</b><br>
		<b>Variable Oscilloscope · Online Value Modification · Data Export · Log Output</b>
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

## Project Overview

This application directly interfaces hardware debuggers with microcontrollers, parses compiled symbol files to obtain a variable table, and enables non-intrusive debugging of target chips (no need to modify the chip’s firmware).

The program is developed with Qt, and utilizes OpenOCD to support a wide range of debuggers and MCUs.

Additionally, serial port connection is supported for any chip with a UART—simply port a provided firmware stub.

**Main Features**

* Real-time variable read and modification

* Real-time waveform plotting for variable values

* Data export

* Formatted log output

**Connection Methods**

The software provides two ways to connect to the target chip:

* Via debugger:

	* Non-intrusive: no changes required to the target’s firmware

	* In principle, supports all debuggers and MCUs compatible with OpenOCD, such as STLink, JLink, CMSIS-DAP, STM32 series, etc.

		> See official OpenOCD docs: [Supported Debug Adapters](https://openocd.org/doc/html/Debug-Adapter-Hardware.html), [Supported CPUs](https://openocd.org/doc/html/CPU-Configuration.html#Target-CPU-Types)

	* For chips that require dedicated OpenOCD drivers (e.g. ESP32C3), external OpenOCD processes can be started manually and connected to afterward

	* Maximum sampling rate: ~100Hz

* Via serial port:

	* Intrusive: requires porting a firmware stub to the target chip

	* Supports any chip with a UART

	* Maximum sampling rate: ~80Hz

![Demo Running](imgs/run-demo.png)

---

## Getting Started

1. If using a serial connection, or if you need logging, please refer to the [Serial Port Porting Instructions](lower/serial/README.md) and [Log Porting Instructions](lower/log/README.md) to port the corresponding firmware to your target device.

2. Download the latest release, extract it, and double-click `LinkScope.exe` to run.

3. Set your compiled symbol file (such as AXF or ELF), then specify variables to view.

	* Add variables:

		* Via the variable selection window

		* Or enter them manually in the last row of the main table, under the variable name column

	* Remove variables:

		* Right-click the variable name

		* Or select it and press Del

	> Note: The variable name can be any legal C expression supported by GDB. See [Advanced Usage](#advanced-usage). Compound types like structs can be viewed only, not edited or plotted.

4. Select a connection mode and connect; once connected, sampling begins.

	* In debugger mode, select your debugger and target chip from the dropdown, then click Connect. Or check “External Process” to link to an already running OpenOCD process.

	* In serial mode, click “Refresh Serial Ports” to list available ports, select the correct one, and click Connect.

5. Edit the `Modify Variable` column to change variable values. Double-click the `Plot Color` column to select a plot color.

6. Click on a `Variable Name` cell to highlight its plotted waveform; its current and selected values (mouse hover) are displayed in the lower left.

7. In the plot window, use the mouse wheel with `Ctrl`, `Shift`, or `Alt` for zooming and panning.

![Operation Demo](imgs/oper-sample.gif)

![Log Output](imgs/log-sample.gif)

---

## Main Menu Items

* `Refresh Connection Config`: Reload configuration files you’ve edited or created into the dropdown lists. See [Advanced Usage](#advanced-usage).

* `Save Config`: Save the configured connection mode, debugger type, MCU model, symbol file path, and variable setup to a config file.

* `Import Config`: Reload a previously saved config file.

* `Export Data`: Export all sampled variable data to a CSV file.

* `Advanced Settings`: Open the advanced settings dialog, with options for serial configuration, sample rate, GDB port, etc.

---

## Advanced Usage

* Variable expressions can be any C expressions supported by GDB.

	Assuming the following global variables in `main.c` of your target program:

	```c
	static int g_int = 0; // static global variable
	int g_arr[10] = {0}; // global array
	struct Pack {
		int var1, var2;
	} g_pack = {0}; // global struct variable
	```

	Example expressions:

	```c
	g_int // value of g_int
	&g_int // address of g_int
	g_pack.var1 // value of g_pack.var1
	g_pack.var1+g_pack.var2 // sum of two variables
	g_arr[0] or *g_arr // value of first element in g_arr
	g_pack // the whole struct (view only)
	g_arr[2]@3 // g_arr[2] through g_arr[4] (view only)
	'main.c'::g_int // specifically g_int from main.c
	*(int*)0x20005c5c // value at address 0x20005c5c (works even without a symbol file)
	```

* Custom OpenOCD configurations for debugger connection:

	* Files are located under `openocd/share/openocd/scripts/target` and `interface`; you can modify or add your configs there.

	* See [OpenOCD Config File Guidelines](https://openocd.org/doc/html/Config-File-Guidelines.html) for syntax.

	* After adding, load them into the software via `Refresh Connection Config`.

---

## Notes

* Without a symbol file, variable names are unavailable; only absolute addresses can be monitored.

* If you change the symbol file path, reconnect is required.

* This software does not perform flash programming—make sure your firmware has already been flashed before connecting. If you use a different debugger, even with the same MCU firmware, re-flashing is recommended.

* Serial and log firmware stubs should match the LinkScope version. Update the firmware after updating the PC software.

---

## Known Issues

* If the application crashes in debugger mode, it may not reconnect on next startup. If so, manually end any lingering `openocd.exe` processes.

* In serial mode, reading a single byte from address 0 always returns 0.

---

## Additional Information

**About Sampling Rate**

* Sampling rate depends on CPU load, number of variables, log output frequency, etc. The application samples as fast as possible.

* The rates in the introduction were measured on an `i5-8265U` CPU under nearly idle load, with a single variable and logging disabled.

* In debugger mode, fetching one log entry takes about 50ms; about 90ms in serial mode. Sampling is interrupted during log retrieval; many logs will reduce sampling rate.

**About Supported Hardware**

* While all OpenOCD-supported devices are theoretically compatible, real-world issues may prevent correct operation on some platforms.

* Verified/tested devices so far include:

	* Debuggers: STLINK-V2, CMSIS-DAP

	* Target MCUs: STM32F103RCT6, STM32F103C8T6, STM32F407ZGT6, STM32F407IGH6, ESP32C3 (built-in JTAG + [dedicated OpenOCD](https://github.com/espressif/openocd-esp32))

---

## Developer Notes

> Note: If you'd like to contribute, please read this section. If you only wish to use the software, you can ignore it.

**Development Environment**

* IDE: Qt Creator

* Qt Version: 5.9.9

**Runtime Architecture**

* In debugger mode, when connecting, the program launches an OpenOCD process in the background and has GDB connect to it. In serial mode, OpenOCD is replaced by a TCP server that interprets GDB’s RSP protocol and forwards it over UART.

	![Process Block Diagram](imgs/process-block-diagram.png)

* The main process communicates with GDB via stdin/stdout. When you add a variable, it sends a `display expr` command to GDB’s display list. It sends `display` every 10ms, parses output via regex, and updates the UI.

* A microsecond timer records timestamps for each received variable sample, tagging data for plotting; the graph window continuously renders historied data.

* GDB does not accept non-ASCII paths for symbol files. Therefore, before connecting, the symbol file is copied to the GDB directory with an ASCII name and loaded via a relative path; the temp file is deleted on disconnect.

* The variable selection window uses a separate GDB process to parse the symbol file, storing variable structures as a tree. When you expand a node, it queries GDB with `info variables`, `whatis`, or `ptype` as needed, parses results via regex, and builds the hierarchy.

* The logging firmware stub maintains a queue; logs are output into the buffer, and the main program routinely dequeues and displays them via a GDB print command.

**Repository File Structure**

* `mainwindow.cpp/h`: Main window

* `graphwindow.cpp/h`: Plot window

* `listwindow.cpp/h`: Variable selection window

* `logwindow.cpp/h`: Log window

* `aboutwindow.cpp/h`: About dialog

* `helpwindow.cpp/h`: Help dialog

* `openocd.cpp/h`: OpenOCD process control

* `serialocd.cpp/h`: Serial server logic (receives GDB TCP, communicates with firmware via serial port)

* `gdbprocess.cpp/h`: GDB process control and output parsing

* `vartype.h`: Variable type definitions

* `qss`: QSS style files

* `gdb`/`openocd`: GDB and OpenOCD binaries/configs (not built, must be manually placed beside the executable after build)

* `lower/log`/`lower/serial`: Firmware code for logging/serial connection

**TODO**

* Increase sampling rate

* Add support for native (host) debugging
