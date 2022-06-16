CC = g++
CFLAGS = -g -fdiagnostics-color=always -Wall -std=c++17 -Werror
TARGET_DIR = target
ARCH = $(shell uname -m)

TEST_CASES=$(patsubst %.sy,%,$(wildcard ./test_case/functional/*.sy))

compiler: $(TARGET_DIR) $(TARGET_DIR)/compiler.o $(TARGET_DIR)/ASM.o $(TARGET_DIR)/AST.o $(TARGET_DIR)/ASMItem.o \
	$(TARGET_DIR)/ASMParser.o $(TARGET_DIR)/IR.o $(TARGET_DIR)/IRItem.o $(TARGET_DIR)/IRParser.o \
	$(TARGET_DIR)/LexicalParser.o $(TARGET_DIR)/Symbol.o $(TARGET_DIR)/SyntaxParser.o $(TARGET_DIR)/Token.o
	$(CC) $(CFLAGS) $(TARGET_DIR)/compiler.o $(TARGET_DIR)/ASM.o $(TARGET_DIR)/AST.o $(TARGET_DIR)/ASMItem.o \
	$(TARGET_DIR)/ASMParser.o $(TARGET_DIR)/IR.o $(TARGET_DIR)/IRItem.o $(TARGET_DIR)/IRParser.o \
	$(TARGET_DIR)/LexicalParser.o $(TARGET_DIR)/Symbol.o $(TARGET_DIR)/SyntaxParser.o $(TARGET_DIR)/Token.o -o compiler

$(TARGET_DIR)/compiler.o: src/compiler.cpp src
	$(CC) $(CFLAGS) -c src/compiler.cpp -o $(TARGET_DIR)/compiler.o

$(TARGET_DIR)/ASM.o: src/backend/ASM.cpp src/backend/ASM.h
	$(CC) $(CFLAGS) -c src/backend/ASM.cpp -o $(TARGET_DIR)/ASM.o

$(TARGET_DIR)/ASMItem.o: src/backend/ASMItem.cpp src/backend/ASMItem.h
	$(CC) $(CFLAGS) -c src/backend/ASMItem.cpp -o $(TARGET_DIR)/ASMItem.o

$(TARGET_DIR)/ASMParser.o: src/backend/ASMParser.cpp src/backend/ASMParser.h
	$(CC) $(CFLAGS) -c src/backend/ASMParser.cpp -o $(TARGET_DIR)/ASMParser.o

$(TARGET_DIR)/AST.o: src/frontend/AST.cpp src/frontend/AST.h
	$(CC) $(CFLAGS) -c src/frontend/AST.cpp -o $(TARGET_DIR)/AST.o

$(TARGET_DIR)/IR.o: src/frontend/IR.cpp src/frontend/IR.h
	$(CC) $(CFLAGS) -c src/frontend/IR.cpp -o $(TARGET_DIR)/IR.o

$(TARGET_DIR)/IRItem.o: src/frontend/IRItem.cpp src/frontend/IRItem.h
	$(CC) $(CFLAGS) -c src/frontend/IRItem.cpp -o $(TARGET_DIR)/IRItem.o

$(TARGET_DIR)/IRParser.o: src/frontend/IRParser.cpp src/frontend/IRParser.h
	$(CC) $(CFLAGS) -c src/frontend/IRParser.cpp -o $(TARGET_DIR)/IRParser.o

$(TARGET_DIR)/LexicalParser.o: src/frontend/LexicalParser.cpp src/frontend/LexicalParser.h
	$(CC) $(CFLAGS) -c src/frontend/LexicalParser.cpp -o $(TARGET_DIR)/LexicalParser.o

$(TARGET_DIR)/Symbol.o: src/frontend/Symbol.cpp src/frontend/Symbol.h
	$(CC) $(CFLAGS) -c src/frontend/Symbol.cpp -o $(TARGET_DIR)/Symbol.o

$(TARGET_DIR)/SyntaxParser.o: src/frontend/SyntaxParser.cpp src/frontend/SyntaxParser.h
	$(CC) $(CFLAGS) -c src/frontend/SyntaxParser.cpp -o $(TARGET_DIR)/SyntaxParser.o

$(TARGET_DIR)/Token.o: src/frontend/Token.cpp src/frontend/Token.h
	$(CC) $(CFLAGS) -c src/frontend/Token.cpp -o $(TARGET_DIR)/Token.o

$(TARGET_DIR):
	mkdir -p $(TARGET_DIR)

testbench:
	$(shell if [ $(ARCH) = "x86_64" ]; then $(CC) $(CFLAGS) tools/testbench_for_x86.cpp -o testbench; fi)
	$(shell if [ $(ARCH) = "armv7l" ]; then $(CC) $(CFLAGS) tools/testbench_for_arm.cpp -o testbench; fi)

clean:
	rm target -rf
	rm compiler testbench test test.c test.s out -f
