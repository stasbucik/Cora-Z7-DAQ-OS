import time

data = bytes()
file = open('/dev/daqdrv', 'rb')
for i in range(0,8):
    data = data + file.read(1024)
file.close()
print(data)

