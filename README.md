<h1 align="center">
  LanSend
</h1>
<p align="center">
  跨平台端到端局域网文件传输工具
</p>

<p align="center">
  <img src="https://img.shields.io/badge/language-C%2B%2B23-yellow.svg">
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgreen.svg">
  <img src="https://img.shields.io/badge/network-boost.asio-blue.svg">
  <a href="https://deepwiki.com/CodeSoul-bytedance/LanSend"><img src="https://deepwiki.com/badge.svg" alt="Ask DeepWiki"></a>
</p>


## :pushpin: 简介

LanSend 是一个基于 C++23 开发的端到端的、安全的局域网文件传输工具。

## 工具链要求

- CMake 3.21 或更高版本
- C++23 兼容的编译器
- vcpkg 包管理器

## 环境设置

### 环境依赖

#### vcpkg

vcpkg 是 Microsoft 提供的开源跨平台 C/C++ 包管理器。

- **参考文档**: [vcpkg 入门指南](https://learn.microsoft.com/zh-cn/vcpkg/get_started/get-started?pivots=shell-powershell)
- **注意事项**: 请确保已安装 vcpkg 并设置了 `VCPKG_ROOT` 环境变量。项目配置时会自动调用 vcpkg 进行依赖管理。

#### Ninja

Ninja 是一个高效的构建系统，请确保已安装 Ninja 并将其添加到环境变量中。

- **安装指南**: 可通过包管理器（如 `brew`, `apt`, 或 `choco`）安装 Ninja。

#### Clang-Format

Clang-Format 用于格式化代码以保持风格一致性。

- **安装指南**: 请安装 Clang-Format 并根据项目需求进行配置。
- **使用建议**: 在提交代码前运行 Clang-Format 以确保代码风格符合规范。

### 提交检查 Hook

此项目使用 [pre-commit](https://pre-commit.com/) 进行提交检查，以确保代码风格一致性。请先安装 pre-commit 工具：

```bash
# 对于 Arch Linux
sudo pacman -S pre-commit
# 对于 Pipx 用户（跨平台）
pipx install pre-commit
```

然后，在克隆项目后，执行以下命令安装 pre-commit 钩子：

```bash
pre-commit install
```

> [!TIP]  
> 如果您发现工具提供的结果不可靠，可以使用 `git commit --no-verify` 暂时跳过提交检查。

## :triangular_ruler: 构建项目

本项目使用 CMake 预设进行构建，支持多种平台和配置。

### 在 macOS/Linux 上构建

**配置并构建 Debug 版本**:

```bash
cmake --preset=native
cmake --build --preset=native-debug
```

**配置并构建 Release 版本**:

```bash
cmake --preset=native
cmake --build --preset=native-release
```

### 在 Windows 上构建

**使用 MSVC x64 配置并构建 Debug 版本**:

```cmd
cmake --preset=msvc-x64
cmake --build --preset=msvc-x64-debug
```

**使用 MSVC x64 配置并构建 Release 版本**:

```cmd
cmake --preset=msvc-x64
cmake --build --preset=msvc-x64-release
```

**使用 MSVC x86 配置并构建 Debug 版本**:

```cmd
cmake --preset=msvc-x86
cmake --build --preset=msvc-x86-debug
```

**使用 MSVC x86 配置并构建 Release 版本**:

```cmd
cmake --preset=msvc-x86
cmake --build --preset=msvc-x86-release
```

## :open_file_folder: 可执行文件位置

构建完成后，可执行文件将位于以下位置：

- Debug 模式: `build/Debug/lansend`
- Release 模式: `build/Release/lansend`
