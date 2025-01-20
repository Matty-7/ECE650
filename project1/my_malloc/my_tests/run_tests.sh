#!/bin/bash
# 先编译你的my_malloc.c成libmymalloc.so
# 再分别用 -DFF 或 -DBF 编译 test1.c ~ test9.c
for i in {1..9}
do
  echo "===== test$i FF ====="
  gcc test$i.c -L. -lmymalloc -I. -o test$i-ff -DFF
  ./test$i-ff
  echo

  echo "===== test$i BF ====="
  gcc test$i.c -L. -lmymalloc -I. -o test$i-bf -DBF
  ./test$i-bf
  echo
done
