add_subdirectory(third_party/inja)
target_compile_definitions(inja INTERFACE INJA_NOEXCEPTION)
add_subdirectory(third_party/pugixml)
target_compile_definitions(pugixml-static PUBLIC PUGIXML_NO_EXCEPTIONS)

# Tooling 
add_subdirectory(tooling)
