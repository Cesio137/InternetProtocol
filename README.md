# InternetProtocol 

InternetProtocol is a versatile header only library to use with C++ 17 projects. You can also find the port for unreal in the unreal-dev branch.

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

- [CMake 3.20 or above](https://cmake.org/download/)

- Windows only
  - Build With Visual Studio 2019/2022 is more recommended
    - [Visual Studio](https://visualstudio.microsoft.com/downloads/)
    - 👇 Install the following workloads:
    - Desktop development with C++ ```Visual studio```'
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

- [ ] TCP server

- [X] TCP client

- [ ] UDP server

- [X] UDP client

- [ ] Http server

- [X] Http client

- [ ] Websocket server

- [X] Websocket client

- [ ] SSL support

## How to use

- Clone the repository or download a stable tag release.

- Copy content inside include folder to your project.

- Add [ASIO NON-BOOST](https://sourceforge.net/projects/asio/files/asio/) header to your project.

## Setup Repository

---

```shell
git clone https://github.com/Cesio137/InternetProtocol.git
```

## Setup project

---

* Setup Project.
  * Create a `build` folder and open terminal inside.
  * Commands to generate project
    * ```bash
      cmake .. --preset=debug
      cmake .. --preset=release
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

