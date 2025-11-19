const std = @import("std");

pub fn print(comptime fmt: []const u8, args: anytype) void {
    std.debug.print(fmt, args);
}

pub fn read_int(comptime T: type) !T {
    var buf: [32]u8 = undefined;
    var reader = std.fs.File.stdin().reader(&buf);
    const input = try reader.interface.takeDelimiterExclusive('\n');
    const input_trimmed = std.mem.trim(u8, input, &std.ascii.whitespace);
    return try std.fmt.parseInt(usize, input_trimmed, 10);
}
