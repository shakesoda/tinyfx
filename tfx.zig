// this wrapper is incomplete, and may yet be awful to use!
// for anything not exposed with a convenience wrappt can be accessed via raw.*
pub const raw = @cImport({
    @cInclude("tinyfx.h");
});
const std = @import("std");

pub const Debug = struct {
    pub const print = raw.tfx_debug_print;
    pub const blit_rgba = raw.tfx_debug_blit_rgba;
    pub const blit_pal = raw.tfx_debug_blit_pal;
    pub const set_palette = raw.tfx_debug_set_palette;
};
pub const UniformType = enum {
    Int,
    Vec4,
    Mat4,
};
pub const Program = struct {
    handle: raw.tfx_program,
    pub inline fn create(src: []const u8, attribs: [*][]const u8, attrib_count: usize) anyerror!Program {
        // alternative...
        // pub inline fn create(src: []const u8, attribs: [*c][*c]const u8) anyerror!Program {
        // var attribs: [*c][*c]const u8 = &[_:null]?[*:0]const u8 {
        //     "a_position",
        //     null
        // };
        // var handle = raw.tfx_program_len_new(src.ptr, @intCast(c_int, src.len), src.ptr, @intCast(c_int, src.len), attribs, -1)
        var handle = raw.tfx_program_len_new(src.ptr, @intCast(c_int, src.len), src.ptr, @intCast(c_int, src.len), @ptrCast([*c][*c]const u8, attribs), @intCast(c_int, attrib_count));
        if (handle == 0) {
            return error.ShaderCompileFailure;
        }
        return Program{ .handle = handle };
    }
};
pub const Uniform = struct {
    handle: raw.tfx_uniform,
    pub inline fn create(name: [:0]const u8, utype: UniformType, count: i32) Uniform {
        const real_type = switch (utype) {
            .Int => @intToEnum(raw.tfx_uniform_type, raw.TFX_UNIFORM_INT),
            .Vec4 => @intToEnum(raw.tfx_uniform_type, raw.TFX_UNIFORM_VEC4),
            .Mat4 => @intToEnum(raw.tfx_uniform_type, raw.TFX_UNIFORM_MAT4),
        };
        var handle = raw.tfx_uniform_new(name, real_type, @intCast(c_int, count));
        return .{ .handle = handle };
    }
};
pub const ComponentType = enum {
    Float,
};
pub const VertexFormat = struct {
    format: raw.tfx_vertex_format,
    pub inline fn start() VertexFormat {
        return VertexFormat{ .format = raw.tfx_vertex_format_start() };
    }
    pub inline fn add(self: *VertexFormat, slot: u8, count: usize, normalized: bool, component: ComponentType) void {
        var real_type = switch (component) {
            .Float => @intToEnum(raw.tfx_component_type, raw.TFX_TYPE_FLOAT),
        };
        raw.tfx_vertex_format_add(&self.format, slot, count, normalized, real_type);
    }
    pub inline fn end(self: *VertexFormat) void {
        raw.tfx_vertex_format_end(&self.format);
    }
};
pub const Buffer = raw.tfx_buffer;
pub fn TransientBuffer(t: var) type {
    return struct {
        ptr: [*]align(1) t,
        handle: raw.tfx_transient_buffer,
        pub fn create(fmt: *VertexFormat, count: u16) TransientBuffer(t) {
            var tb = raw.tfx_transient_buffer_new(&fmt.format, count);
            return TransientBuffer(t){
                .handle = tb,
                .ptr = @ptrCast([*]align(1) t, tb.data),
            };
        }
    };
}
// var tb = tfx.TransientBuffer(f32).create(&fmt, 3);
// tb.ptr[0] = -1.0;
// tb.ptr[3] = -1.0;
// tb.ptr[4] = -1.0;
// tb.ptr[7] = -1.0;
// tb.ptr[2] = depth;
// tb.ptr[5] = depth;
// tb.ptr[8] = depth;
// tb.ptr[1] = 3.0;
// tb.ptr[6] = 3.0;
pub const ResetFlags = struct {
    pub const None = raw.TFX_RESET_NONE;
    pub const DebugOverlay = raw.TFX_RESET_DEBUG_OVERLAY;
    pub const DebugOverlayStats = raw.TFX_RESET_DEBUG_OVERLAY_STATS;
    pub const ReportGPUTimings = raw.TFX_RESET_REPORT_GPU_TIMINGS;
};
pub const State = struct {
    pub const Default = raw.TFX_STATE_DEFAULT;
    pub const RGBWrite = raw.TFX_STATE_RGB_WRITE;
    pub const DepthWrite = raw.TFX_STATE_DEPTH_WRITE;
};
pub const TextureFlags = struct {
    pub const FilterPoint = raw.TFX_TEXTURE_FILTER_POINT;
    pub const FilterLinear = raw.TFX_TEXTURE_FILTER_LINEAR;
    pub const CPUWritable = raw.TFX_TEXTURE_CPU_WRITABLE;
    pub const GenMips = raw.TFX_TEXTURE_GEN_MIPS;
    pub const ReserveMips = raw.TFX_TEXTURE_RESERVE_MIPS;
    pub const Cube = raw.TFX_TEXTURE_CUBE;
    pub const MSAASample = raw.TFX_TEXTURE_MSAA_SAMPLE;
    pub const MSAAX2 = raw.TFX_TEXTURE_MSAA_X2;
    pub const MSAAX4 = raw.TFX_TEXTURE_MSAA_X4;
    pub const External = raw.TFX_TEXTURE_EXTERNAL;
};
pub const TextureFormat = struct {
    pub const RGB565 = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_RGB565);
    pub const RGBA8 = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_RGBA8);
    pub const SRGB8 = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_SRGB8);
    pub const SRGB8_A8 = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_SRGB8_A8);
    pub const RGB10A2 = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_RGB10A2);
    pub const RG11B10F = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_RG11B10F);
    pub const RGB16F = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_RGB16F);
    pub const RGBA16F = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_RGBA16F);
    pub const RGB565_D16 = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_RGB565_D16);
    pub const RGBA8_D16 = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_RGBA8_D16);
    pub const RGBA8_D24 = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_RGBA8_D24);
    pub const R16F = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_R16F);
    pub const R32UI = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_R32UI);
    pub const R32F = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_R32F);
    pub const RG16F = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_RG16F);
    pub const RG32F = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_RG32F);
    pub const D16 = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_D16);
    pub const D24 = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_D24);
    pub const D32 = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_D32);
    pub const D32F = @intToEnum(raw.tfx_format, raw.TFX_FORMAT_D32F);
};
pub const PlatformData = struct {
    pub inline fn create(gl_version: c_int, gl_get_proc_address: var) raw.tfx_platform_data {
        var pd = std.mem.zeroes(raw.tfx_platform_data);
        pd.context_version = gl_version;
        pd.gl_get_proc_address = gl_get_proc_address;
        return pd;
    }
};
pub const ViewFlags = struct {
    pub const None = raw.TFX_VIEW_NONE;
    pub const Invalidate = raw.TFX_VIEW_INVALIDATE;
    pub const Flush = raw.TFX_VIEW_FLUSH;
    pub const SortSequential = raw.TFX_VIEW_SORT_SEQUENTIAL;
    pub const Default = raw.TFX_VIEW_DEFAULT;
};
pub const View = struct {
    id: u8,
    pub inline fn setName(self: *const View, name: [:0]const u8) void {
        raw.tfx_view_set_name(self.id, name);
    }
    pub inline fn setFlags(self: *const View, flags: c_int) void {
        raw.tfx_view_set_flags(self.id, @intToEnum(raw.tfx_view_flags, flags));
    }
    pub inline fn setClearColor(self: *const View, color: u32) void {
        raw.tfx_view_set_clear_color(self.id, color);
    }
    pub inline fn setClearDepth(self: *const View, depth: f32) void {
        raw.tfx_view_set_clear_depth(self.id, depth);
    }
    pub inline fn setCanvas(self: *const View, canvas: *raw.tfx_canvas, layer: i32) void {
        raw.tfx_view_set_canvas(self.id, canvas, @intCast(c_int, layer));
    }
    pub inline fn setViewports(self: *const View, count: usize, viewports: [*][*]u16) void {
        raw.tfx_view_set_viewports(self.id, @intCast(c_int, count), @ptrCast([*]?[*]u16, viewports));
    }
};

pub inline fn setPlatformData(pd: raw.tfx_platform_data) void {
    raw.tfx_set_platform_data(pd);
}
pub inline fn reset(w: u32, h: u32, flags: c_int) void {
    raw.tfx_reset(@intCast(u16, w), @intCast(u16, h), @intToEnum(raw.tfx_reset_flags, flags));
}
pub const shutdown = raw.tfx_shutdown;
pub const setBuffer = raw.tfx_set_buffer;
pub inline fn setTransientBuffer(tb: var) void {
    raw.tfx_set_transient_buffer(tb.handle);
}
pub inline fn setTexture(uniform: *Uniform, tex: *raw.tfx_texture, slot: u8) void {
    raw.tfx_set_texture(&uniform.handle, tex, slot);
}
pub const setState = raw.tfx_set_state;
pub const setCallback = raw.tfx_set_callback;
pub inline fn setUniform(uniform: *Uniform, data: [*]f32, count: i32) void {
    raw.tfx_set_uniform(&uniform.handle, data, @intCast(c_int, count));
}
pub inline fn submit(view: View, program: Program, retain: bool) void {
    return raw.tfx_submit(view.id, program.handle, retain);
}
pub inline fn touch(view: View) void {
    return raw.tfx_touch(view.id);
}
pub inline fn getView(viewid: u8) View {
    return .{ .id = viewid };
}
pub inline fn frame() raw.tfx_stats {
    return raw.tfx_frame();
}
