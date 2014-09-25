CFLAGS    := -O3 -march=native -std=c++11 -Wall -D_REENTRANT
CC        := g++ $(CFLAGS)
LD        := g++ -pthread

MODULES   := rapidjson sphlib nxt
SRC_DIR   := $(addprefix src/,$(MODULES)) src
BUILD_DIR := $(addprefix bin/,$(MODULES)) bin

SRC       := $(foreach sdir,$(SRC_DIR),$(wildcard $(sdir)/*.cpp))
OBJ       := $(patsubst src/%.cpp,bin/%.o,$(SRC))
INCLUDES  := $(addprefix -I,$(SRC_DIR))

EXECUTABLE:= burstminer

vpath %.cpp $(SRC_DIR)

define make-goal
$1/%.o: %.cpp
	$(CC) $(INCLUDES) -c $$< -o $$@
endef

.PHONY: all checkdirs clean

all: checkdirs bin/$(EXECUTABLE)
	
bin/$(EXECUTABLE): $(OBJ)
	$(LD) $^ -o $@
    
checkdirs: $(BUILD_DIR)

$(BUILD_DIR):
	@mkdir -p $@
    
clean:
	@rm -rf $(BUILD_DIR)

$(foreach bdir,$(BUILD_DIR),$(eval $(call make-goal,$(bdir))))
