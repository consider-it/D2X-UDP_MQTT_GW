# CityATM/ UDVeo: UDP MQTT Gateway

This application will forward raw data received on a UDP port to the specified MQTT topic.

Only the POSIX(-style) socket API is supported and unfortunately the application will also only bind to the IPv4 socket.


## Installation and Requirements

### Prerequisites: Eclipse Paho MQTT Client Library
Assuming, that you have a C build toolchain ready (see below), go to a directory of your choice and do:
```shell
git clone --depth 1 --recursive -b v1.3.6 https://github.com/eclipse/paho.mqtt.c.git ./paho-mqtt-c
cd paho-mqtt-c

make
sudo make install
```


### Prepatation of Deployment Target (Linux)
This chapter describes the steps needed to compile on the deployment target.
For the full development toolchain, with all tools helping you develop the application, see the next chapter.

The following commands assume, that Debian Buster (of a linux distribution based on it) is used.

To setup the build environment, do:
- `sudo apt install build-essential gcc make cmake clang libssl-dev`
- Install the Eclipse Paho MQTT library (see above)
- Git-clone this repository somewhere you like and `cd` to that directory
- See the [Installation](#installation) chapter how to compile and install


### Prepatation of Development Environment
This chapter, in contrast to the previous one, contains a more generic description, how to install a development toolchain.
To make development of this software a bit easier and safer, some additional tools are also installed.

#### Toolchain
For building everything, some tools are needed:
- Install [CMake](https://cmake.org/) (via the package manager of your choice)
- Install GNU Make and GCC or even better Clang
- Install `clang-format` for auto-formatting of the code (via the package manager of your choice)
  * If you are using Visual Studio Code, you might have to set the path to the formatter in the global setting (not the workspace settings!) at `C_Cpp.clang_format_path`
  * A suitable configuration file (`.clang-format`) is present at the root of this repository and should be detected automatically.
  * Before every commit, the code **must** be auto-formatted using clang-format.
- If you are using [VS Code](https://code.visualstudio.com/) or [VSCodium](https://vscodium.com/):
  * Install the `ms-vscode.cpptools`, `ms-vscode.cmake-tools` and `twxs.cmake` extensions


#### (Optional) Static Code Analyzer: Clang-Tidy
This project uses [clang-tidy](https://clang.llvm.org/extra/clang-tidy/index.html) as static analyzer/ linter.
It is a very powerful tool, which comes with checks for many common coding style flaws, that might lead to bugs or even security issues.

Install on macOS with homebrew:
```shell
brew install llvm
ln -s "$(brew --prefix llvm)/bin/clang-tidy" "/usr/local/bin/clang-tidy"
```

Install on Debian (Buster) with:
```shell
sudo apt install clang-tidy
```


### Installation
Move to the directory, where this git repository was cloned to.

For a cleaner workspace, all build activity is done in a separate directory:
```shell
mkdir build && cd build
```

Then the build environment can be configured and the code built with:
```shell
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j5
```
(The `-j` parameter tells the make system to run multiple compilers in parallel. You can use `j = $num_CPU_cores + 1` as a starting point and double that number, if your CPU uses hyper-threading.)

If you want to build a version with debug symbols, re-configure CMake (and run make again):
```shell
cmake -DCMAKE_BUILD_TYPE=Debug ..
```
**Note**: Your IDE might have support for CMake built in, like VS Code with a Build button and selector for Debug and Release configuration in the bottom bar.

The executable can be installed (to `usr/local/bin` on Linux) with:
```shell
sudo make install

# (optional) copy the example config
cp udpmqttgw.example.conf /etc/udpmqttgw.conf

# (optional) symlink the config to the home directory
ln -s /etc/udpmqttgw.conf /home/root/udpmqttgw.conf

# (optional) install a systemd unit
cp udpmqttgw.service /etc/systemd/system
sudo systemctl daemon-reload
sudo systemctl enable udpmqttgw.service
sudo systemctl start udpmqttgw.service
```




## Usage
After installing the application, a configuration file must be prepared.
An example file `udpmqttgw.example.conf` is located at the root of this repository.
By default, the application searches for the file at `/etc/udpmqttgw.conf`.

The application can be started with:
```shell
udpmqttgw -c=/path/to/config -v
```

The configuration file path is optional and the `-v` flag increases the verbosity of the output.
All CLI parameters are explaied when calling the application with the `-h` flag.

The different config file options for the MQTT connection are named in the same way, as the Paho MQTT library uses them.
For a full documentation, what each option does, see [the Paho library documentation](https://www.eclipse.org/paho/files/mqttdoc/MQTTClient/html/struct_m_q_t_t_client__connect_options.html).
The TLS options can be found at the [MQTTClient_SSLOptions struct](https://www.eclipse.org/paho/files/mqttdoc/MQTTClient/html/struct_m_q_t_t_client___s_s_l_options.html).



## Debugging
Debug output (to stdout) of the MQTT library is controlled by environment variables:
```shell
export MQTT_C_CLIENT_TRACE=ON
export MQTT_C_CLIENT_TRACE_LEVEL=PROTOCOL # one of: ERROR, PROTOCOL, MINIMUM, MEDIUM and MAXIMUM (from least to most verbose)
```
