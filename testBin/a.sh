#!/system/bin/sh
#tmp=0
#while true
#do
#	sleep 1
	sendevent /dev/input/event1 1 116 1
	sendevent /dev/input/event1 1 116 0
	sendevent /dev/input/event1 0 0 0
#	tmp=$(($tmp+1))
#	echo "          第 $tmp 次拍照"
#	adb shell sendevent /dev/input/event0 1 114 1
#	adb shell sendevent /dev/input/event0 0 0 0
# 	sleep 0.3
#	adb shell sendevent /dev/input/event0 1 114 0
#	adb shell sendevent /dev/input/event0 0 0 0 
#	sleep 5   
#done 
