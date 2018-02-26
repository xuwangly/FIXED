#!/bin/sh
tmp=0
while true
do  
	tmp=$(($tmp+1))
	echo "          第 $tmp 次拍照"
	adb shell sendevent /dev/input/event1 1 116 1
	adb shell sendevent /dev/input/event1 0 0 0
	adb shell sendevent /dev/input/event1 1 116 0
	adb shell sendevent /dev/input/event1 0 0 0 
	sleep 0.001  
done 
