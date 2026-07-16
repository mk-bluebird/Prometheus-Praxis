fn main() {
    println!("cargo:rerun-if-changed=../../ker/ker_oplus_geom_min_max.cpp");
    println!("cargo:rerun-if-changed=../../ker/ker_oplus_geom_min_max.hpp");

    cc::Build::new()
        .cpp(true)
        .file("../../ker/ker_oplus_geom_min_max.cpp")
        .flag_if_supported("-std=c++20")
        .compile("ker_oplus_geom_min_max");
}
