APP = daqsrv-udp

# Add any other object files to this list below
APP_OBJS = daqsrv-udp.o

all: build

build: $(APP)

$(APP): $(APP_OBJS)
	$(CXX) -o $@ $(APP_OBJS) $(LDFLAGS) $(LDLIBS)
clean:
	rm -f $(APP) *.o
