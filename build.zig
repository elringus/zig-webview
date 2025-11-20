const std = @import("std");

const targets: []const std.Target.Query = &.{
    .{ .cpu_arch = .x86_64, .os_tag = .windows, .abi = .msvc },
    // .{ .cpu_arch = .aarch64, .os_tag = .macos },
};

pub fn build(b: *std.Build) void {
    for (targets) |t| buildTarget(b, t);
}

fn buildTarget(b: *std.Build, t: std.Target.Query) void {
    const target = b.resolveTargetQuery(t);
    const optimize = b.standardOptimizeOption(.{});

    const root = b.createModule(.{
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });
    root.addIncludePath(b.path("src/include"));
    root.addCSourceFile(.{
        .file = b.path("src/webview_shim.cpp"),
        .flags = &[_][]const u8{"-std=c++17"},
    });

    const exe = b.addExecutable(.{
        .name = "webview",
        .root_module = root,
    });

    exe.linkLibC();
    exe.linkSystemLibrary("user32");
    exe.linkSystemLibrary("ole32");
    exe.addObjectFile(b.path("src/include/WebView2Loader.dll.lib"));

    b.installArtifact(exe);
}
