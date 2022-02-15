LDLIBS=-lrt -lm -pthread -lstdc++
CFLAGS=-Wall -O3
CXXFLAGS=-Wall -O3 -std=c++20

TARGETS=numa-pingpong

CPUS=`cd /sys/devices/system/cpu; ls | grep 'cpu[0-9]\+' | sed -e s/cpu//g`

CPU_PAIRS=$(foreach c0,$(CPUS),$(foreach c1,$(CPUS),$(echo $(c0),$(c1))))


all: $(TARGETS)

run:
	echo $(CPUS)
	echo $(CPU_PAIRS)

numa-pingpong: numa-pingpong.o ticket_mutex.o

dep:
	makedepend -- -Y -- *.c *.cpp

clean:
	rm -f $(TARGETS) *.o *~ *.bak

# DO NOT DELETE

ticket_lock.o: ticket_lock.h
ticket_mutex.o: ticket_mutex.h
numa-pingpong.o: ticket_mutex.h
