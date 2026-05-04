CXX = g++
CXXFLAGS = -Wall -O2
LDFLAGS = -lrt -lpthread

TARGET = sync
SRCS = main.cpp queue.cpp workers.cpp
OBJS = $(SRCS:.cpp=.o)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: clean
