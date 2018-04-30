project(lib_img_reduce_project)

include_directories ("${PROJECT_SOURCE_DIR}/src")
include_directories ("${PROJECT_SOURCE_DIR}/..")
add_library(img_reduce SHARED img_reduce.c)

install(TARGETS img_reduce DESTINATION lib)
install(FILES img_reduce.h DESTINATION include)

