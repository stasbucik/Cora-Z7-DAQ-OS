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
