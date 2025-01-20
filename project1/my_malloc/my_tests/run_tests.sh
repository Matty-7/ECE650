#!/bin/bash
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
