#批量删除制定类型的文件
find $1 -name "*~" -type f -print -exec rm -rf {} \;
