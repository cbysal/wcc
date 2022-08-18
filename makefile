CC = g++
CFLAGS = -g -fdiagnostics-color=always -Wall -std=c++17 -Werror $(CFLAGS_EX)
TARGET_DIR = target
ARCH = $(shell uname -m)

compiler: testbench $(TARGET_DIR) $(TARGET_DIR)/ASM.o $(TARGET_DIR)/ASMItem.o $(TARGET_DIR)/ASMOptimizer.o \
	$(TARGET_DIR)/ASMParserAdd.o $(TARGET_DIR)/ASMParserBasic.o $(TARGET_DIR)/ASMParserCmp.o \
	$(TARGET_DIR)/ASMParserDiv.o $(TARGET_DIR)/ASMParserMod.o $(TARGET_DIR)/ASMParserMov.o \
	$(TARGET_DIR)/ASMParserMul.o $(TARGET_DIR)/ASMParserSub.o $(TARGET_DIR)/ASMWriter.o $(TARGET_DIR)/AST.o \
	$(TARGET_DIR)/ASTOptimizer.o $(TARGET_DIR)/BasicBlock.o $(TARGET_DIR)/ColoringRegAllocator.o \
	$(TARGET_DIR)/Compiler.o $(TARGET_DIR)/GlobalData.o $(TARGET_DIR)/HardCoding.o $(TARGET_DIR)/IR.o \
	$(TARGET_DIR)/IRItem.o $(TARGET_DIR)/IROptimizer.o $(TARGET_DIR)/IRParser.o $(TARGET_DIR)/LexicalParser.o \
	$(TARGET_DIR)/LinearRegAllocator.o $(TARGET_DIR)/Reg.o $(TARGET_DIR)/RegFile.o $(TARGET_DIR)/SSAOptimizer.o \
	$(TARGET_DIR)/Symbol.o $(TARGET_DIR)/SyntaxParser.o $(TARGET_DIR)/Token.o
	$(CC) $(CFLAGS) $(TARGET_DIR)/ASM.o $(TARGET_DIR)/ASMItem.o $(TARGET_DIR)/ASMOptimizer.o \
	$(TARGET_DIR)/ASMParserAdd.o $(TARGET_DIR)/ASMParserBasic.o $(TARGET_DIR)/ASMParserCmp.o \
	$(TARGET_DIR)/ASMParserDiv.o $(TARGET_DIR)/ASMParserMod.o $(TARGET_DIR)/ASMParserMov.o \
	$(TARGET_DIR)/ASMParserMul.o $(TARGET_DIR)/ASMParserSub.o $(TARGET_DIR)/ASMWriter.o $(TARGET_DIR)/AST.o \
	$(TARGET_DIR)/ASTOptimizer.o $(TARGET_DIR)/BasicBlock.o $(TARGET_DIR)/ColoringRegAllocator.o \
	$(TARGET_DIR)/Compiler.o $(TARGET_DIR)/GlobalData.o $(TARGET_DIR)/HardCoding.o $(TARGET_DIR)/IR.o \
	$(TARGET_DIR)/IRItem.o $(TARGET_DIR)/IROptimizer.o $(TARGET_DIR)/IRParser.o $(TARGET_DIR)/LexicalParser.o \
	$(TARGET_DIR)/LinearRegAllocator.o $(TARGET_DIR)/Reg.o $(TARGET_DIR)/RegFile.o $(TARGET_DIR)/SSAOptimizer.o \
	$(TARGET_DIR)/Symbol.o $(TARGET_DIR)/SyntaxParser.o $(TARGET_DIR)/Token.o -o compiler

$(TARGET_DIR)/ASM.o: src/backend/ASM.cpp src/backend/ASM.h
	$(CC) $(CFLAGS) -c src/backend/ASM.cpp -o $(TARGET_DIR)/ASM.o

$(TARGET_DIR)/ASMItem.o: src/backend/ASMItem.cpp src/backend/ASMItem.h
	$(CC) $(CFLAGS) -c src/backend/ASMItem.cpp -o $(TARGET_DIR)/ASMItem.o

$(TARGET_DIR)/ASMOptimizer.o: src/backend/ASMOptimizer.cpp src/backend/ASMOptimizer.h
	$(CC) $(CFLAGS) -c src/backend/ASMOptimizer.cpp -o $(TARGET_DIR)/ASMOptimizer.o

$(TARGET_DIR)/ASMParserAdd.o: src/backend/ASMParserAdd.cpp src/backend/ASMParser.h
	$(CC) $(CFLAGS) -c src/backend/ASMParserAdd.cpp -o $(TARGET_DIR)/ASMParserAdd.o

$(TARGET_DIR)/ASMParserBasic.o: src/backend/ASMParserBasic.cpp src/backend/ASMParser.h
	$(CC) $(CFLAGS) -c src/backend/ASMParserBasic.cpp -o $(TARGET_DIR)/ASMParserBasic.o

$(TARGET_DIR)/ASMParserCmp.o: src/backend/ASMParserCmp.cpp src/backend/ASMParser.h
	$(CC) $(CFLAGS) -c src/backend/ASMParserCmp.cpp -o $(TARGET_DIR)/ASMParserCmp.o

$(TARGET_DIR)/ASMParserDiv.o: src/backend/ASMParserDiv.cpp src/backend/ASMParser.h
	$(CC) $(CFLAGS) -c src/backend/ASMParserDiv.cpp -o $(TARGET_DIR)/ASMParserDiv.o

$(TARGET_DIR)/ASMParserMod.o: src/backend/ASMParserMod.cpp src/backend/ASMParser.h
	$(CC) $(CFLAGS) -c src/backend/ASMParserMod.cpp -o $(TARGET_DIR)/ASMParserMod.o

$(TARGET_DIR)/ASMParserMov.o: src/backend/ASMParserMov.cpp src/backend/ASMParser.h
	$(CC) $(CFLAGS) -c src/backend/ASMParserMov.cpp -o $(TARGET_DIR)/ASMParserMov.o

$(TARGET_DIR)/ASMParserMul.o: src/backend/ASMParserMul.cpp src/backend/ASMParser.h
	$(CC) $(CFLAGS) -c src/backend/ASMParserMul.cpp -o $(TARGET_DIR)/ASMParserMul.o

$(TARGET_DIR)/ASMParserSub.o: src/backend/ASMParserSub.cpp src/backend/ASMParser.h
	$(CC) $(CFLAGS) -c src/backend/ASMParserSub.cpp -o $(TARGET_DIR)/ASMParserSub.o

$(TARGET_DIR)/ASMWriter.o: src/backend/ASMWriter.cpp src/backend/ASMWriter.h
	$(CC) $(CFLAGS) -c src/backend/ASMWriter.cpp -o $(TARGET_DIR)/ASMWriter.o

$(TARGET_DIR)/AST.o: src/frontend/AST.cpp src/frontend/AST.h
	$(CC) $(CFLAGS) -c src/frontend/AST.cpp -o $(TARGET_DIR)/AST.o

$(TARGET_DIR)/ASTOptimizer.o: src/frontend/ASTOptimizer.cpp src/frontend/ASTOptimizer.h
	$(CC) $(CFLAGS) -c src/frontend/ASTOptimizer.cpp -o $(TARGET_DIR)/ASTOptimizer.o

$(TARGET_DIR)/BasicBlock.o: src/backend/BasicBlock.cpp src/backend/BasicBlock.h
	$(CC) $(CFLAGS) -c src/backend/BasicBlock.cpp -o $(TARGET_DIR)/BasicBlock.o

$(TARGET_DIR)/Compiler.o: src/Compiler.cpp src
	$(CC) $(CFLAGS) -c src/Compiler.cpp -o $(TARGET_DIR)/Compiler.o

$(TARGET_DIR)/ColoringRegAllocator.o: src/backend/ColoringRegAllocator.cpp src/backend/ColoringRegAllocator.h
	$(CC) $(CFLAGS) -c src/backend/ColoringRegAllocator.cpp -o $(TARGET_DIR)/ColoringRegAllocator.o

$(TARGET_DIR)/GlobalData.o: src/GlobalData.cpp src/GlobalData.h
	$(CC) $(CFLAGS) -c src/GlobalData.cpp -o $(TARGET_DIR)/GlobalData.o

$(TARGET_DIR)/HardCoding.o: src/backend/HardCoding.cpp src/backend/HardCoding.h
	$(CC) $(CFLAGS) -c src/backend/HardCoding.cpp -o $(TARGET_DIR)/HardCoding.o

$(TARGET_DIR)/IR.o: src/frontend/IR.cpp src/frontend/IR.h
	$(CC) $(CFLAGS) -c src/frontend/IR.cpp -o $(TARGET_DIR)/IR.o

$(TARGET_DIR)/IRItem.o: src/frontend/IRItem.cpp src/frontend/IRItem.h
	$(CC) $(CFLAGS) -c src/frontend/IRItem.cpp -o $(TARGET_DIR)/IRItem.o

$(TARGET_DIR)/IROptimizer.o: src/frontend/IROptimizer.cpp src/frontend/IROptimizer.h
	$(CC) $(CFLAGS) -c src/frontend/IROptimizer.cpp -o $(TARGET_DIR)/IROptimizer.o

$(TARGET_DIR)/IRParser.o: src/frontend/IRParser.cpp src/frontend/IRParser.h
	$(CC) $(CFLAGS) -c src/frontend/IRParser.cpp -o $(TARGET_DIR)/IRParser.o

$(TARGET_DIR)/LexicalParser.o: src/frontend/LexicalParser.cpp src/frontend/LexicalParser.h
	$(CC) $(CFLAGS) -c src/frontend/LexicalParser.cpp -o $(TARGET_DIR)/LexicalParser.o

$(TARGET_DIR)/LinearRegAllocator.o: src/backend/LinearRegAllocator.cpp src/backend/LinearRegAllocator.h
	$(CC) $(CFLAGS) -c src/backend/LinearRegAllocator.cpp -o $(TARGET_DIR)/LinearRegAllocator.o

$(TARGET_DIR)/Reg.o: src/backend/Reg.cpp src/backend/Reg.h
	$(CC) $(CFLAGS) -c src/backend/Reg.cpp -o $(TARGET_DIR)/Reg.o

$(TARGET_DIR)/RegFile.o: src/backend/RegFile.cpp src/backend/RegFile.h
	$(CC) $(CFLAGS) -c src/backend/RegFile.cpp -o $(TARGET_DIR)/RegFile.o

$(TARGET_DIR)/SSAOptimizer.o: src/frontend/SSAOptimizer.cpp src/frontend/SSAOptimizer.h
	$(CC) $(CFLAGS) -c src/frontend/SSAOptimizer.cpp -o $(TARGET_DIR)/SSAOptimizer.o

$(TARGET_DIR)/Symbol.o: src/frontend/Symbol.cpp src/frontend/Symbol.h
	$(CC) $(CFLAGS) -c src/frontend/Symbol.cpp -o $(TARGET_DIR)/Symbol.o

$(TARGET_DIR)/SyntaxParser.o: src/frontend/SyntaxParser.cpp src/frontend/SyntaxParser.h
	$(CC) $(CFLAGS) -c src/frontend/SyntaxParser.cpp -o $(TARGET_DIR)/SyntaxParser.o

$(TARGET_DIR)/Token.o: src/frontend/Token.cpp src/frontend/Token.h
	$(CC) $(CFLAGS) -c src/frontend/Token.cpp -o $(TARGET_DIR)/Token.o

$(TARGET_DIR):
	mkdir -p $(TARGET_DIR)

testbench: tools/testbench_for_x86.cpp tools/testbench_for_arm.cpp
	$(shell if [ $(ARCH) = "x86_64" ]; then $(CC) $(CFLAGS) tools/testbench_for_x86.cpp -o testbench; fi)
	$(shell if [ $(ARCH) = "armv7l" ]; then $(CC) $(CFLAGS) tools/testbench_for_arm.cpp -o testbench; fi)

clean:
	rm target -rf
	rm Compiler testbench test test.c test.s out -f
