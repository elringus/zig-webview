const utl = @import("utilities.zig");
const c = @cImport({
    @cDefine("_WIN32_WINNT", "0x0A00"); // target Windows 10
    @cInclude("EventToken.h");
    @cInclude("WebView2.h");
});

pub fn embedWebview() !void {
    var windows: [100]HWND = undefined;
    try enumVisibleWindows(&windows);
    const selected_idx = utl.read_int(usize) catch 0;
    const hwnd = windows[selected_idx];

    try initWebviewEnv();
    try createWebviewForHwnd(hwnd);
}

fn initWebviewEnv() !void {
    const COINIT_APARTMENTTHREADED: u32 = 0x2;
    const hr = c.CoInitializeEx(null, COINIT_APARTMENTTHREADED);
    if (hr != 0) return error.CoInitializeFailed;
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

fn createWebviewForHwnd(_hwnd: ?*u8) !void {
    _ = _hwnd;

    // Load the WebView2 loader DLL dynamically
    const dllName = "WebView2Loader.dll";
    var wide: [256]u16 = undefined;
    const name_bytes = dllName[0..];
    if (name_bytes.len + 1 > wide.len) return error.LoadLibraryFailed;
    var i: usize = 0;
    while (i < name_bytes.len) : (i += 1) {
        wide[i] = @as(u16, name_bytes[i]);
    }
    wide[name_bytes.len] = 0;
    const h = LoadLibraryW(&wide[0]);
    if (h == null) return error.LoadLibraryFailed;

    const proc_name = "CreateCoreWebView2EnvironmentWithOptions";
    const p = GetProcAddress(h, proc_name);
    if (p == null) return error.ProcNotFound;

    // TODO: implement full COM completed handlers in Zig.
    return error.NotImplemented;
}

extern fn EnumWindows(cb: WNDENUM_CB, lp: WNDENUM_LP) BOOL;
extern fn GetWindowTextA(hwnd: HWND, lpString: *CHAR, nMaxCount: INT) INT;
extern fn GetClassNameA(hwnd: HWND, lpString: *CHAR, nMaxCount: INT) INT;
extern fn IsWindowVisible(hwnd: HWND) BOOL;
extern fn GetLastError() INT;

extern fn LoadLibraryW(lpLibFileName: [*:0]const u16) ?*u8;
extern fn GetProcAddress(hModule: ?*u8, lpProcName: [*:0]const u8) ?*u8;
extern fn GetClientRect(hwnd: ?*u8, lpRect: *c.RECT) c.BOOL;

const BOOL = i32;
const CHAR = u8;
const INT = i32;
const HWND = ?*u8;
const WNDENUM_CB = ?*const fn (hwnd: HWND, lp: WNDENUM_LP) BOOL;
const WNDENUM_LP = ?*WindowEnumCtx;
const WindowEnumCtx = struct { hwnds: [1024]HWND, count: usize };
