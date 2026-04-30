#获取当前目录下的所有源文件项目
file(GLOB SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/*cc)
list(REMOVE_ITEM SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/main.cc)
if (APPLE)
    list(REMOVE_ITEM SRC_FILE ${CMAKE_CURRENT_SOURCE_DIR}/EPollPoller.cc)
endif()


# 创建静态库，便于 macOS 最终链接阶段统一解析跨模块符号
add_library(src_lib STATIC ${SRC_FILE})

#创建可执行文件
add_executable(main  main.cc)

#链接必要的库
target_link_libraries(main src_lib memory_lib log_lib src_lib ${LIBS})
