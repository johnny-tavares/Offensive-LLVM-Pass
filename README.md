# LLVM Kernel Backdoor Pass

- This project implements a custom LLVM pass designed to modify Linux kernel source code during compilation. 
- This guide details the integration into the LLVM source tree, the build process using Ninja, and the configuration required to compile the Linux kernel with this custom toolchain.
- Testing was done with version 6.18.0-rc6 for linux, and 22.0.0 for LLVM.

## 1. LLVM Integration

### Directory Structure

Ensure the source code is placed in the following directory:

```
llvm/lib/Transforms/Backdoor/
```

### CMake Configuration

You must link the new folder in the build system.

#### Modify `llvm/lib/Transforms/CMakeLists.txt`

Add the following line at the end of the file to include your new folder:

```cmake
add_subdirectory(Backdoor)
```

### Pass Registration

To make the pass visible to the Pass Manager, modify `llvm/lib/Passes/PassRegistry.def`.

Find the section for `MODULE_PASS` and add your pass:

```cpp
MODULE_PASS("backdoor", BackdoorPass())
```

**Note:** Ensure the header file for your pass is included in `llvm/lib/Passes/PassBuilder.cpp` if strictly required for your version of LLVM, though `PassRegistry.def` is the primary registry point. For this build, there's a header file at  "llvm/include/llvm/Transforms/Utils/Backdoor.h" 

## 2. Building the Custom Clang

Use `ninja` to build the modified Clang compiler.

1. Navigate to your build directory (e.g., `llvm-project/build`).

2. Configure the build (if not already done):

```bash
cmake -G Ninja ../llvm
```

3. Build Clang:

```bash
ninja
```

## 3. Linux Kernel Configuration

Before building the kernel, you must configure the environment to use the custom Clang and modify the kernel configuration to avoid build errors.

### Environment Setup

Export the `PATH` so the kernel build system uses your custom compiled Clang and LLVM tools:

```bash
export PATH=/path/to/your/llvm-project/build/bin:$PATH
```

### Kernel Menuconfig

The kernel doesn't compile with Netfilter for whatever reason, so let's disable it.

1. Run the configuration menu:

```bash
make LLVM=1 menuconfig
```

2. Navigate through Networking Support -> Networking Options -> Netfilter, and disable it.

3. Save and Exit.

## 4. Building the Kernel

Build the kernel using the custom LLVM toolchain.

```bash
make LLVM=1 -j$(nproc)
```

## 5. Usage Verification

Once built, it can be used in a QEMU virtual machine, where you need to manually create the attacker and their GID in the init file.

## 6. Limitations/Upgrades
This project didn't implement a user-modifiable value for the magic number for the backdoor, but also has some other drawbacks to be aware of. The following are potential upgrades:

- To not hardcode the offset for finding the GID from the credential struct, as this isn't consistent between kernel versions.
- There may be other functions that are better to attack that aren't "cap_capable()", for example, "cap_task_prctl()". It should be possible to leverage this function to directly modify the credential structure. 
