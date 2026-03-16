load("@rules_cc//cc:defs.bzl", "CcInfo", "cc_common")

def _system_headers_impl(ctx):
    out_dir = ctx.actions.declare_directory("staging/include")

    cmd = "mkdir -p {out_dir} && ".format(out_dir = out_dir.path)
    cmd += "cp -rL /usr/include/linux {out_dir}/ && ".format(out_dir = out_dir.path)
    cmd += "cp -rL /usr/include/x86_64-linux-gnu/asm {out_dir}/ && ".format(out_dir = out_dir.path)
    cmd += "cp -rL /usr/include/asm-generic {out_dir}/ && ".format(out_dir = out_dir.path)

    # Detect paths from inputs
    root_path = ctx.files.root_headers[0].path
    include_root = root_path[:root_path.find("include") + len("include")]

    types_base = include_root + "/llvm-libc-types"
    macros_base = include_root + "/llvm-libc-macros"

    # Copy root headers and common header
    cmd += "cp -L {include_root}/*.h {out_dir}/ && ".format(out_dir = out_dir.path, include_root = include_root)

    # Copy LLVM-libc internal types and macros so generated headers can find them via relative paths
    cmd += "cp -rL {types_base} {out_dir}/ && ".format(out_dir = out_dir.path, types_base = types_base)
    cmd += "cp -rL {macros_base} {out_dir}/ && ".format(out_dir = out_dir.path, macros_base = macros_base)

    # Copy generated headers
    for f in ctx.files.generated_headers:
        # f.path is something like bazel-out/.../staging/generated_include/sys/stat.h
        # We want to put it in staging/include/sys/stat.h
        rel_path = f.path.split("staging/generated_include/")[1]
        dest = out_dir.path + "/" + rel_path
        cmd += "mkdir -p $(dirname {dest}) && cp -L {src} {dest} && ".format(
            dest = dest,
            src = f.path,
        )

    cmd += "true"  # End the command chain

    ctx.actions.run_shell(
        outputs = [out_dir],
        inputs = ctx.files.types_dir + ctx.files.macros_dir + ctx.files.root_headers + ctx.files.generated_headers,
        command = cmd,
        use_default_shell_env = True,
    )
    return [
        DefaultInfo(files = depset([out_dir])),
        CcInfo(
            compilation_context = cc_common.create_compilation_context(
                includes = depset([out_dir.path]),
            ),
        ),
    ]

system_headers = rule(
    implementation = _system_headers_impl,
    attrs = {
        "types_dir": attr.label_list(allow_files = True),
        "macros_dir": attr.label_list(allow_files = True),
        "root_headers": attr.label_list(allow_files = True),
        "generated_headers": attr.label_list(allow_files = True),
    },
)
