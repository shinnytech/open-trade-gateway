NAME := open-trade-gateway
CXX_SRCS := $(wildcard src/*.cpp src/ctp/*.cpp src/sim/*.cpp)
CXX_OBJS := $(patsubst src/%,obj/%,$(CXX_SRCS:.cpp=.o))

CXXFLAGS += -std=c++17 -pthread -g -O2 -flto -Icontrib/include/ -Isrc/
LDFLAGS += -Lcontrib/lib
LDLIBS += -lssl -lcrypto -lwebsockets -l:thosttraderapi.so -lcurl

all: directories bin/$(NAME)

directories:
	@mkdir -p bin
	@mkdir -p obj

bin/$(NAME): $(CXX_OBJS)
	$(CXX) -o $@ $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS)

obj/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -o $@ $(CPPFLAGS) $(CXXFLAGS) -c $<

clean:
	@$(RM) -rf bin obj

install: all
	install -d /etc/$(NAME)
	install -d /var/local/$(NAME)
	install -m 777 -d /var/log/$(NAME)
	install -m 755 bin/$(NAME) /usr/local/bin/
	install -m 755 contrib/lib/*.so /usr/local/lib/
	install -m 644 conf/* /etc/$(NAME)/
