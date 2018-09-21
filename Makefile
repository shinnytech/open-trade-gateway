NAME := open-trade-gateway
CXX_SRCS := $(wildcard src/*.cpp src/ctp/*.cpp src/sim/*.cpp)
CXX_OBJS := $(patsubst src/%,obj/%,$(CXX_SRCS:.cpp=.o))
CXX_DEPS := $(CXX_OBJS:%.o=%.d)

CXXFLAGS += -std=c++17 -pthread -g -O2 -flto -Icontrib/include/ -Isrc/
LDFLAGS += -Lcontrib/lib
LDLIBS += -lssl -lcrypto -lthosttraderapi -lcurl -lstdc++fs 

.PHONY: all clean install

all: bin/$(NAME)

bin/$(NAME): $(CXX_OBJS)
	@mkdir -p $(@D)
	$(CXX) -o $@ $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS)

obj/%.o: src/%.cpp
	@mkdir -p $(@D)
	$(CXX) -o $@ -MMD -MP $(CPPFLAGS) $(CXXFLAGS) -c $<

-include $(CXX_DEPS)

clean:
	@$(RM) -rf bin obj

install: all
	install -d /etc/$(NAME)
	install -m 777 -d /var/local/lib/$(NAME)
	install -m 777 -d /var/log/$(NAME)
	install -m 755 bin/$(NAME) /usr/local/bin/
	install -m 755 contrib/lib/*.so /usr/local/lib/
	install -m 644 conf/* /etc/$(NAME)/
