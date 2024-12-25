# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -I$(CURDIR)/../include  # Include path to the header directory
LDFLAGS = 


# Directories
SRC_DIR = $(CURDIR)/../src
EXAMPLES_DIR = $(CURDIR)/examples
INCLUDE_DIR = $(CURDIR)/include


# Files
LIB_SRC = $(SRC_DIR)/libekio.c
LIB_OBJ = $(SRC_DIR)/libekio.o

# Automatically find all main.c files and create corresponding executables
EXAMPLE_SRCS = $(wildcard $(EXAMPLES_DIR)/*/main.c)
EXECUTABLES = $(EXAMPLE_SRCS:$(EXAMPLES_DIR)/%/main.c=$(EXAMPLES_DIR)/%/output)

# Default target
all: $(EXECUTABLES)

# Compile libekio.c into libekio.o
$(LIB_OBJ): $(LIB_SRC)
	$(CC) -c $(CFLAGS) -o $@ $<

# Compile and link main.c with libekio.o to create the executable
$(EXAMPLES_DIR)/%/output: $(EXAMPLES_DIR)/%/main.c $(LIB_OBJ)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# Clean build files
clean:
	rm -f $(SRC_DIR)/*.o $(EXECUTABLES)

# Rebuild everything
rebuild: clean all