add_rules("mode.debug", "mode.release")
set_languages("c11", "c++17")
set_warnings("all", "error")
add_requires("boost")

target("kld")
    set_kind("binary")
    add_files("src/*.cpp")
    add_packages("boost")
