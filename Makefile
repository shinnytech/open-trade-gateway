NAME := open-trade-gateway
CXX_SRCS := $(wildcard src/*.cpp src/ctp/*.cpp)
CXX_OBJS := $(patsubst src/%,obj/%,$(CXX_SRCS:.cpp=.o))

CXXFLAGS += -Wall -Wextra -std=c++17 -pthread -g -O2 -flto -Icontrib/include/ -Isrc/
LDFLAGS += -Lcontrib/lib
LDLIBS += -lssl -lcrypto -lwebsockets -l:thosttraderapi.so -lcurl

all: directories $(NAME)

directories:
	@mkdir -p bin
	@mkdir -p obj

$(NAME): $(CXX_OBJS)
	$(CXX) -o bin/$@ $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS)

obj/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -o $@ $(CPPFLAGS) $(CXXFLAGS) -c $<

clean:
	@$(RM) -rf bin obj
