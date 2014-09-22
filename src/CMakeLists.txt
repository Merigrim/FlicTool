set(FlicTool_SOURCE_FILES main.cc Bitmap.cc Flic.cc)

add_executable(FlicTool ${FlicTool_SOURCE_FILES})

target_link_libraries(FlicTool ${LIBS})