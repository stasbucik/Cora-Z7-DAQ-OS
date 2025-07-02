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

APP = recv-udp

INCLUDE_FLAGS = -I $(RECIPE_MATPLOTLIBCPP) \
				-I $(RECIPE_SYSTEM_INCLUDE) \
				-I $(RECIPE_PYTHON_INCLUDE) \
				-I $(RECIPE_NUMPY_INCLUDE)

LINK_LIBS = -l$(RECIPE_PYTHON_VERSION)

all: build

build: $(APP)

$(APP):
	$(CXX) -o $@ cpp/recv-udp.cpp -std=c++11 $(INCLUDE_FLAGS) $(LDFLAGS) $(LDLIBS) $(LINK_LIBS)
clean:
	rm -f $(APP) *.o
