const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe_mod = b.createModule(.{
        .target = target,
        .optimize = optimize,
    });

    exe_mod.addCSourceFile(.{
        .file = b.path("src/main.cpp"),
        .flags = &.{ "-std=c++20", "-O3", "-Wno-deprecated-literal-operator" },
    });

    exe_mod.addIncludePath(b.path("include"));
    exe_mod.link_libcpp = true;

    if (target.result.os.tag == .windows) {
        exe_mod.linkSystemLibrary("ws2_32", .{});
        exe_mod.linkSystemLibrary("shell32", .{});
        exe_mod.linkSystemLibrary("user32", .{});
        exe_mod.linkSystemLibrary("gdi32", .{});
        exe_mod.linkSystemLibrary("winhttp", .{});
    } else {
        exe_mod.linkSystemLibrary("curl", .{});
        exe_mod.linkSystemLibrary("pthread", .{});
    }

    const exe = b.addExecutable(.{
        .name = "NativeChats",
        .root_module = exe_mod,
    });

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());

    const run_step = b.step("run", "Ejecutar NativeChats");
    run_step.dependOn(&run_cmd.step);
}
