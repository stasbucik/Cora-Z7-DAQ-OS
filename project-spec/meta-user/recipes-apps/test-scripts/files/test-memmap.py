#  Copyright 2025, University of Ljubljana
#
#  This file is part of Cora-Z7-DAQ-OS.
#  Cora-Z7-DAQ-OS is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by the Free Software Foundation,
#  either version 3 of the License, or any later version.
#  Cora-Z7-DAQ-OS is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
#  FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
#  You should have received a copy of the GNU General Public License along with Cora-Z7-DAQ-OS.
#  If not, see <https://www.gnu.org/licenses/>.

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
