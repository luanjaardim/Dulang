const std = @import("std");
const heap = std.heap;
const fs = std.fs;
const mem = std.mem;

const stderr = std.io.getStdErr().writer();

pub fn main() !void {
    var arena = heap.ArenaAllocator.init(heap.page_allocator);
    defer arena.deinit();

    const arena_alok = arena.allocator();

    const args = try std.process.argsAlloc(arena_alok);
    if(args.len < 2) {
        _ = try stderr.writeAll("Error! You must pass the program to be compiled as an argument!");
        return error.MissingCommandLineArg;
    }
    const len = args[1].len;

    if(len < 6 or mem.eql(u8, args[1][len-6..len-1], ".dulan")) {
        std.debug.print("{s}\n", .{args[1][args[1].len-6..]});
        _ = try stderr.writeAll("Error! Expected a Dulang file!");
        return error.ExpectedDulangFile;
    }
    const file_name = args[1][0..args[1].len-6];

    //
    // Parse file
    //

    if(fs.Dir.openFile(fs.cwd(), args[1], .{})) |fd| {
        fd.close();

        const destPath = try mem.concat(arena_alok, u8, &[_][]const u8{file_name, ".asm"});
        const file = try fs.cwd().createFile(destPath, .{});
        defer file.close();

        _ = try file.writeAll(";; at the moment just a comment in nasm x86_64\n");

        //
        // Genereate Dulang code
        //

    } else |_| {
        _ = try stderr.writeAll("Error! The passed file does not exist!");
        return error.FileDoesNotExist;
    }
}

test "create file and allocate args" {
    var arena = heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();

    const arena_alok = arena.allocator();

    const args = try std.process.argsAlloc(arena_alok);
    std.debug.print("{}\n", .{args.len});

    const destPath = try mem.concat(arena_alok, u8, &[_][]const u8{args[0], ".asm"});
    const file = try fs.cwd().createFile(destPath, .{ .read = true });
    defer file.close();

    _ = try file.writeAll("oi oi\n");
}
