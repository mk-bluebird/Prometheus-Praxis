// crates/eco-wealth-kernel/build.rs

fn main() {
    // Rebuild if the C++ Lyapunov residual sources change.
    println!("cargo:rerun-if-changed=../../ker/lyapunov_residual_simd.cpp");
    println!("cargo:rerun-if-changed=../../ker/lyapunov_residual_simd.hpp");

    let mut build = cc::Build::new();

    build
        .cpp(true)
        .file("../../ker/lyapunov_residual_simd.cpp")
        // Use a modern C++ standard compatible with the project constraints.
        .flag_if_supported("-std=c++20")
        // Enable SIMD where supported; safe fallbacks are in the C++ code.
        .flag_if_supported("-mavx2")
        .flag_if_supported("-msse2")
        .compile("lyapunov_residual_simd");
}
