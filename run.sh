#==================================================================================================
#Author Name:          Zach Hofmeister
#Author Email:         zachhof@me.com
#==================================================================================================

#!/bin/bash

echo -e "Removing previously compiled code..."
rm grep

echo -e "Compiling grep05.c..."
clang grep05.c -g -o grep

echo -e "\nRunning: ./grep [cC]ar edtest.txt in.txt"
./grep [cC]ar edtest.txt in.txt
echo -e "\nRunning: grep [cC]ar edtest.txt in.txt"
grep [cC]ar edtest.txt in.txt --color=always

echo -e "\nRunning: ./grep [yadt]$ edtest.txt in.txt"
./grep [yadt]$ edtest.txt in.txt
echo -e "\nRunning: grep [yadt]$ edtest.txt in.txt"
grep [yadt]$ edtest.txt in.txt --color=always

echo -e "\nRunning: ./grep ^[hHcC] edtest.txt in.txt"
./grep ^[hHcC] edtest.txt in.txt
echo -e "\nRunning: grep ^[hHcC] edtest.txt in.txt"
grep ^[hHcC] edtest.txt in.txt --color=always

echo -e "\nRunning: ./grep wo edtest.txt in.txt"
./grep wo edtest.txt in.txt
echo -e "\nRunning: grep wo edtest.txt in.txt"
grep wo edtest.txt in.txt --color=always

echo -e "\nThe script file will terminate."