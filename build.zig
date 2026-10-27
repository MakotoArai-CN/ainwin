const std = @import("std");

pub fn build(b: *std.Build) void {
    // The plugin is a plain console exe (JSON-RPC on stdin/stdout). GNU ABI
    // keeps the static mingw build reproducible on machines without MSVC.
    const target = b.standardTargetOptions(.{
        .default_target = .{ .cpu_arch = .x86_64, .os_tag = .windows, .abi = .gnu },
    });
    const optimize = b.option(std.builtin.Optimize, "optimize", "Optimization mode") orelse .fast;

    const exe = b.addExecutable(.{
        .name = "ainwin",
        .root_module = b.createModule(.{
            .target = target,
            .optimize = optimize,
            .link_libc = true,
            .link_libcpp = true,
        }),
    });
    exe.root_module.addIncludePath(b.path("vendor"));
    exe.root_module.addCSourceFile(.{
        .file = b.path("main.cpp"),
        .flags = &.{
            "-std=c++17",
            "-DUNICODE",
            "-D_UNICODE",
            "-DNOMINMAX",
            "-DWIN32_LEAN_AND_MEAN",
            "-Wno-macro-redefined",
        },
    });
    exe.root_module.link_libcpp = true;
    // onnxruntime.dll is loaded with LoadLibrary at run time, so there is no
    // import library to link and the plugin still starts without the model.
    const install = b.addUpdateSourceFiles();
    install.addCopyFileToSource(exe.getEmittedBin(), "ainwin.exe");
    b.getInstallStep().dependOn(&install.step);
}
