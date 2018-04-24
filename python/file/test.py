import array
arr = []
arrgyro = []

fp = open("accel.raw")

lines = fp.readlines()
fp.close()

for i in range(len(lines)):
	#print lines[i]
	#if(i % 1 == 0):
	line = [float(x) for x in lines[i].split()]
	arr.append(line)
	#print arr


fp = open("gyro.raw")
lines = fp.readlines()
fp.close()

for i in range(len(lines)):
	#print lines[i]
	#if(i % 1 == 0):
	line = [float(x) for x in lines[i].split()]
	#print line
	arrgyro.append(line)
		#print arrgyro

#print arr
#print arrgyro

for i in range(len(arr)):
	#if(i % 1 == 0):
	a = arr[i][0] /1000000000
	for j in range(len(arrgyro)):
		if(int(a) == int(arrgyro[j][0]/1000000000)):
			arr[i].append(arrgyro[j][1])
			arr[i].append(arrgyro[j][2])
			arr[i].append(arrgyro[j][3])
			break

#print arr

fp = open("out.data" ,"w")
for i in range(len(arr)):
	fp.write(str(int(arr[i][0])))
	fp.write(" ")
	fp.write(str(arr[i][1]))
	fp.write(" ")
	fp.write(str(arr[i][2]))
	fp.write(" ")
	fp.write(str(arr[i][3]))
	fp.write(" ")
	fp.write(str(arr[i][4]))
	fp.write(" ")
	fp.write(str(arr[i][5]))
	fp.write(" ")
	fp.write(str(arr[i][6]))
	fp.write(" ")
	fp.write("\n")
fp.close()
#	str(arr[i])

