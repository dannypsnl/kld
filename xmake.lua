add_rules("mode.debug", "mode.release")
set_languages("c11", "c++17")
set_warnings("all", "error")

target("kld")
    set_kind("binary")
    add(find_packages("vcpkg::elfio"))
    add_files("src/*.cpp")
