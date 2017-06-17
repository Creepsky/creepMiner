LOGGING   := terminal

MODULES   := sphlib nxt
SRC_DIR   := $(addprefix src/,$(MODULES)) src
BUILD_DIR := $(addprefix bin/obj/,$(MODULES)) bin/obj

POCO_MODULES := Net Foundation NetSSL_OpenSSL openssl Crypto Util JSON
POCO_DIR     := $(addprefix Poco/,$(POCO_MODULES))

SRC       := $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.cpp))
OBJ       := $(patsubst src/%.cpp,bin/obj/%.o,$(SRC))
INCLUDES  := $(addprefix -I,$(SRC_DIR)) $(addprefix -I,$(POCO_DIR))

EXECUTABLE        := creepMiner
EXECUTABLE_PATH   := bin/$(EXECUTABLE)

cflags_logging.service  := -DLOG_SERVICE
cflags_logging.terminal := -DLOG_TERMINAL

CFLAGS    := -O3 -march=native -std=c++11 -Wall -D_REENTRANT -DNDEBUG ${cflags_logging.${LOGGING}}
CC        := $(CXX)
LDFLAGS   := -L/usr/local/lib -pthread -lPocoFoundation -lPocoNetSSL -lPocoCrypto -lPocoUtil -lPocoNet -lPocoJSON

vpath %.cpp $(SRC_DIR)

define make-goal
$1/%.o: %.cpp
	$(CC) $(CFLAGS) $(INCLUDES) -c $$< -o $$@
endef

.PHONY: all checkdirs clean

all: checkdirs bin/$(EXECUTABLE)
	
$(EXECUTABLE_PATH): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXECUTABLE_PATH) $(OBJ) $(LDFLAGS)
    
checkdirs: $(BUILD_DIR)

$(BUILD_DIR):
	@mkdir -p $@
    
clean:
	@rm -rf $(BUILD_DIR)/*.o

$(foreach bdir,$(BUILD_DIR),$(eval $(call make-goal,$(bdir))))
