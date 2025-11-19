pub fn main() !void {
    switch (@import("builtin").os.tag) {
        .windows => try @import("windows.zig").embedWebview(),
        .macos => {},
        else => return error.UnsupportedOS,
    }
}
