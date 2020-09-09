# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_SRCS 变量
aux_source_directory(. DIR_LIB_SRCS)
# 生成链接库
add_library (app_fifo STATIC ${DIR_LIB_SRCS})
#install (TARGETS business ARCHIVE DESTINATION ${CMAKE_SOURCE_DIR}/install/lib)
