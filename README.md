# InternetProtocol CLI

Master branch is a versatile console aplication to do quick tests with thirdparty libraries before make port to the unreal-dev branch. It can also be used as a header-only library to integrate into other projects.

## Sponsor me

<p align="center">
  <a href="https://www.paypal.com/donate?hosted_button_id=L48BPZ4VVCN6Q"><img src="https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif"></a>
</p>
<p align="center">
  <a href="https://nubank.com.br/pagar/1bcou4/5D6eezlHdm"><img src="https://logodownload.org/wp-content/uploads/2020/02/pix-bc-logo.png" width="128"></a>
</p>

### Suported Platforms

---

- [x] Windows

- [X] Linux

- [ ] Mac(Help Wanted)

## Download and Install

---

- [Git](https://git-scm.com)

- [CMake 3.25 or above](https://cmake.org/download/)

- [VCPKG](https://vcpkg.io/en/)

- Windows only
  - Build With Visual Studio 2022
    - [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/)
    - 👇 Install the following workloads:
    - Game Development with C++
    - MSVC v142 or above | x64/x86
    - C++ 2015/2022 redistributable update

- Linux only
  - Build With GNU ```install with package manager```
    - GCC 9.x or above
    - make 
    - m4 
    - autoconf 
    - automake 

### Roadmap

---

- [ ] TCP server/client

- [ ] UDP server/client

- [ ] Http server

- [X] Http client

- [ ] Websocket server

- [ ] Async json read/writte for big datas.

- [ ] Openssl support

## Setup Repository

---

```shell
git clone https://github.com/Cesio137/InternetProtocol.git
```

## Building the Engine

---

#### Setup Enviroment Variables
VCPKG_ROOT
* Setup VCPKG  
  * Create a variable called `VCPKG_ROOT` if do not exist:
    * ```Path
      <Path to VCPKG>/x.x.x/
      ```

#### Windows

* Setup Project.
  * Create a `build` folder and open terminal inside.
  * Commands to generate project
    * ```bash
      cmake .. --preset=Windows_Debug-x64
      cmake .. --preset=Windows_Release-x64
      ```

#### Linux

* Setup Project.
  
  * Create a `build` folder and open terminal inside.
  
  * Commands to generate project
    
    * ```bash
      cmake .. --preset=Unix_Debug-x64
      cmake .. --preset=Unix_Release-x64
      ``` 

### Bug Reporting Template:
```
**Detailed description of issue**
Write a detailed explanation of the issue here.

**Steps To Reproduce:**
1: Detailed Steps to reproduce the issue 
2: Clear steps
3: Etc

**Expected Results:**
A description of what should happen.

**Actual Results:**
A description of what actually happens.
```

# Thirdparty libraries
- [Asio non-boost 1.30.2](https://think-async.com/Asio/)

