COMMON_NAME := libopen-trade-common.so
CTP_NAME := open-trade-ctp
CTPSE_NAME := open-trade-ctpse
SIM_NAME := open-trade-sim
MD_NAME := open-trade-mdservice
GATEWAY_NAME := open-trade-gateway

COMMON_SRCS := $(wildcard open-trade-common/*.cpp)
CTP_SRCS:= $(wildcard open-trade-ctp/*.cpp)
CTPSE_SRCS:= $(wildcard open-trade-ctpse/*.cpp)
SIM_SRCS:= $(wildcard open-trade-sim/*.cpp)
MD_SRCS:= $(wildcard open-trade-mdservice/*.cpp)
GATEWAY_SRCS:= $(wildcard open-trade-gateway/*.cpp)

COMMON_OBJS := $(patsubst open-trade-common/%,obj/common/%,$(COMMON_SRCS:.cpp=.o))
CTP_OBJS := $(patsubst open-trade-ctp/%,obj/ctp/%,$(CTP_SRCS:.cpp=.o))
CTPSE_OBJS := $(patsubst open-trade-ctpse/%,obj/ctpse/%,$(CTPSE_SRCS:.cpp=.o))
SIM_OBJS := $(patsubst open-trade-sim/%,obj/sim/%,$(SIM_SRCS:.cpp=.o))
MD_OBJS := $(patsubst open-trade-mdservice/%,obj/md/%,$(MD_SRCS:.cpp=.o))
GATEWAY_OBJS := $(patsubst open-trade-gateway/%,obj/gateway/%,$(GATEWAY_SRCS:.cpp=.o))

COMMON_DEPS := $(COMMON_OBJS:%.o=%.d)
CTP_DEPS := $(CTP_OBJS:%.o=%.d)
CTPSE_DEPS := $(CTPSE_OBJS:%.o=%.d)
SIM_DEPS := $(SIM_OBJS:%.o=%.d)
MD_DEPS := $(MD_OBJS:%.o=%.d)
GATEWAY_DEPS := $(GATEWAY_OBJS:%.o=%.d)

CXXFLAGS += -std=c++17 -pthread -g -O2 -flto -Icontrib/include/ \
-Iopen-trade-common/
LDFLAGS += -Lcontrib/lib
LDLIBS += -lssl -lcrypto -lcurl -lboost_system -lstdc++fs -lrt

.PHONY: all clean install

all: bin/$(COMMON_NAME) bin/$(CTP_NAME) bin/$(CTPSE_NAME) bin/$(SIM_NAME) bin/$(MD_NAME) bin/$(GATEWAY_NAME)

bin/$(COMMON_NAME): $(COMMON_OBJS)
	@mkdir -p $(@D)
	$(CXX) -fPIC -shared -o $@ $^

obj/common/%.o: open-trade-common/%.cpp
	@mkdir -p $(@D)
	$(CXX) -o $@ -MMD -MP $(CPPFLAGS) $(CXXFLAGS) -c $<

-include $(COMMON_DEPS)

LDFLAGS += -Lbin/

LDLIBS +=-lopen-trade-common

LDLIBS +=-lboost_thread -lboost_filesystem -lboost_regex -lboost_chrono

LDLIBS_CTP = $(LDLIBS) -lthosttraderapi 

LDLIBS_CTPSE = $(LDLIBS) -lthosttraderapise 

bin/$(CTP_NAME):$(CTP_OBJS)
	@mkdir -p $(@D)
	$(CXX) -o $@ $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS_CTP) 
	
obj/ctp/%.o: open-trade-ctp/%.cpp
	@mkdir -p $(@D)
	$(CXX) -o $@ -MMD -MP $(CPPFLAGS) $(CXXFLAGS) -c $<

-include $(CTP_DEPS)

bin/$(CTPSE_NAME):$(CTPSE_OBJS)
	@mkdir -p $(@D)
	$(CXX) -o $@ $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS_CTPSE) 

obj/ctpse/%.o: open-trade-ctpse/%.cpp
	@mkdir -p $(@D)
	$(CXX) -o $@ -MMD -MP $(CPPFLAGS) $(CXXFLAGS) -c $<

-include $(CTPSE_DEPS)

bin/$(SIM_NAME):$(SIM_OBJS)
	@mkdir -p $(@D)
	$(CXX) -o $@ $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS) 

obj/sim/%.o: open-trade-sim/%.cpp
	@mkdir -p $(@D)
	$(CXX) -o $@ -MMD -MP $(CPPFLAGS) $(CXXFLAGS) -c $<

-include $(SIM_DEPS)

bin/$(MD_NAME):$(MD_OBJS)
	@mkdir -p $(@D)
	$(CXX) -o $@ $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS)

obj/md/%.o: open-trade-mdservice/%.cpp
	@mkdir -p $(@D)
	$(CXX) -o $@ -MMD -MP $(CPPFLAGS) $(CXXFLAGS) -c $<

-include $(MD_DEPS)

bin/$(GATEWAY_NAME):$(GATEWAY_OBJS)
	@mkdir -p $(@D)
	$(CXX) -o $@ $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS)

obj/gateway/%.o: open-trade-gateway/%.cpp
	@mkdir -p $(@D)
	$(CXX) -o $@ -MMD -MP $(CPPFLAGS) $(CXXFLAGS) -c $<

-include $(GATEWAY_DEPS)

clean:
	@$(RM) -rf bin obj

install: all
	install -d /etc/$(GATEWAY_NAME)
	install -m 777 -d /var/local/lib/$(GATEWAY_NAME)
	install -m 777 -d /var/log/$(GATEWAY_NAME)
	install -m 755 bin/$(COMMON_NAME) /usr/local/bin/
	install -m 755 bin/$(CTP_NAME) /usr/local/bin/
	install -m 755 bin/$(CTPSE_NAME) /usr/local/bin/
	install -m 755 bin/$(SIM_NAME) /usr/local/bin/
	install -m 755 bin/$(MD_NAME) /usr/local/bin/	
	install -m 755 bin/$(GATEWAY_NAME) /usr/local/bin/
	install -m 755 contrib/lib/*.so /usr/local/lib/
	install -m 644 conf/* /etc/$(GATEWAY_NAME)/
