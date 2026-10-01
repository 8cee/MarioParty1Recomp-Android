# Android cross-build helper: expose the in-tree freetype target to RmlUi.
if(NOT TARGET freetype)
    message(FATAL_ERROR "FindFreetype(Android): in-tree freetype target is missing")
endif()
if(NOT TARGET Freetype::Freetype)
    add_library(Freetype::Freetype INTERFACE IMPORTED GLOBAL)
    set_target_properties(Freetype::Freetype PROPERTIES
        INTERFACE_LINK_LIBRARIES freetype
        INTERFACE_INCLUDE_DIRECTORIES "${freetype_SOURCE_DIR}/include;${freetype_BINARY_DIR}/include")
endif()
set(FREETYPE_FOUND TRUE)
set(Freetype_FOUND TRUE)
