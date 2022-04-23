add_subdirectory(third_party/inja)
target_compile_definitions(inja INTERFACE INJA_NOEXCEPTION)

# Tooling 
add_subdirectory(tooling)
