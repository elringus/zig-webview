const utl = @import("utilities.zig");

pub fn embedWebview() !void {
    var windows: [100]HWND = undefined;
    try enumVisibleWindows(&windows);
    const selected_idx = utl.read_int(usize) catch 0;
    const hwnd = windows[selected_idx];

    if (webview2_init(hwnd) != 0) return error.WebviewInitFailed;
    webview2_navigate("https://naninovel.com/editor");
}

fn enumVisibleWindows(out: []HWND) !void {
    var ctx: WindowEnumCtx = .{ .hwnds = undefined, .count = 0 };
    if (EnumWindows(onWindowEnum, &ctx) == 0)
        return error.EnumWindowsFailed;
    for (0..@min(ctx.hwnds.len, out.len)) |idx|
        out[idx] = ctx.hwnds[idx];
}

fn onWindowEnum(hwnd: HWND, lp: WNDENUM_LP) BOOL {
    const ctx = lp orelse return 0;
    if (ctx.count >= ctx.hwnds.len) return 0;

    if (!isWindowVisible(hwnd)) return 1;
    var class_buf: [512]u8 = undefined;
    var title_buf: [512]u8 = undefined;
    const class = getWindowClass(hwnd, &class_buf) orelse return 1;
    const title = getWindowTitle(hwnd, &title_buf) orelse return 1;

    utl.print("{} class: {s} title: {s}\n", .{ ctx.count, class, title });
    ctx.hwnds[ctx.count] = hwnd;
    ctx.count += 1;
    return 1;
}

fn isWindowVisible(hwnd: HWND) bool {
    return IsWindowVisible(hwnd) == 1;
}

fn getWindowClass(hwnd: HWND, buf: []u8) ?[]const u8 {
    const len: usize = @intCast(GetClassNameA(hwnd, &buf[0], @intCast(buf.len)));
    return if (len == 0) null else buf[0..len];
}

fn getWindowTitle(hwnd: HWND, buf: []u8) ?[]const u8 {
    const len: usize = @intCast(GetWindowTextA(hwnd, &buf[0], @intCast(buf.len)));
    return if (len == 0) null else buf[0..len];
}

extern fn EnumWindows(cb: WNDENUM_CB, lp: WNDENUM_LP) BOOL;
extern fn GetWindowTextA(hwnd: HWND, lpString: *CHAR, nMaxCount: INT) INT;
extern fn GetClassNameA(hwnd: HWND, lpString: *CHAR, nMaxCount: INT) INT;
extern fn IsWindowVisible(hwnd: HWND) BOOL;

extern fn webview2_init(hwnd: HWND) i32;
extern fn webview2_navigate(url: [*:0]const u8) void;
extern fn webview2_set_visibility(visible: BOOL) void;

const BOOL = i32;
const CHAR = u8;
const INT = i32;
const HWND = ?*u8;
const WNDENUM_CB = ?*const fn (hwnd: HWND, lp: WNDENUM_LP) BOOL;
const WNDENUM_LP = ?*WindowEnumCtx;
const WindowEnumCtx = struct { hwnds: [1024]HWND, count: usize };
