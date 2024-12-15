# Makefile for WaveCraft

# Compiler and flags
CC = gcc
CFLAGS = -Wall -g

# Directories
SRCDIR = src
OBJDIR = obj
BINDIR = bin

# Source files and object files
SRCS := $(wildcard $(SRCDIR)/*.c)
OBJS := $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.o,$(SRCS))

# Target executable
TARGET = wavecraft

# Libraries and their paths
LIBS = -lsndfile -lmp3lame -lasound -lm

# Default rule
all: directories $(TARGET)

# Rule to build the target executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LIBS) -o $(BINDIR)/$@

# Rule to compile source files into object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to create necessary directories
directories:
	mkdir -p $(OBJDIR) $(BINDIR)

# Clean up generated files
clean:
	rm -rf $(OBJDIR) $(BINDIR) $(TARGET)

# Very clean, including removing any generated output files
distclean: clean
	rm -rf output/*

.PHONY: all clean distclean directories
