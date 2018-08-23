NAME := open-trade-gateway
CXX_SRCS := $(wildcard src/*.cpp src/ctp/*.cpp)
CXX_OBJS := ${CXX_SRCS:.cpp=.o}

CXXFLAGS += -Wall -Wextra -std=c++17 -pthread -g -O2 -flto -Icontrib/include/ -Isrc/
LDFLAGS += -Lcontrib/lib
LDLIBS += -lwebsockets -l:thosttraderapi.so -lcurl

all: $(NAME)

$(NAME): $(CXX_OBJS)
	$(CXX) -o $@ $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS)

%.o: %.cpp
	$(CXX) -o $@ $(CPPFLAGS) $(CXXFLAGS) -c $<

clean:
	$(RM) $(CXX_OBJS) $(NAME)
