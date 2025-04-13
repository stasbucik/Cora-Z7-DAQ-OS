import mmap
import os
import sys

if len(sys.argv) < 2:
   print("need an argument")
   exit()

num = int(sys.argv[1])

fileno = os.open('/dev/mem', os.O_RDONLY)
try:
   mem = mmap.mmap(fileno, 1024 * 128, offset=0x46000000, access=mmap.ACCESS_READ)
   try:
      print(mem.read(num))
   except Exception as e:
      print("mmap.read failed")
      print(e)
   finally:
      mem.close()
except Exception as e:
   print("mmap failed")
   print(e)
finally:
   os.close(fileno)
