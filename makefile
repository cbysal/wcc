CC = g++
CFLAGS = -g -fdiagnostics-color=always -Wall -std=c++17 -Werror
TARGET_DIR = target

compiler: $(TARGET_DIR) $(TARGET_DIR)/compiler.o $(TARGET_DIR)/AST.o $(TARGET_DIR)/IR.o $(TARGET_DIR)/IRItem.o \
	$(TARGET_DIR)/IRParser.o $(TARGET_DIR)/LexicalParser.o $(TARGET_DIR)/Symbol.o $(TARGET_DIR)/SyntaxParser.o \
	$(TARGET_DIR)/Token.o $(TARGET_DIR)/Type.o
	$(CC) $(CFLAGS) $(TARGET_DIR)/compiler.o $(TARGET_DIR)/AST.o $(TARGET_DIR)/IR.o $(TARGET_DIR)/IRItem.o \
	$(TARGET_DIR)/IRParser.o $(TARGET_DIR)/LexicalParser.o $(TARGET_DIR)/Symbol.o $(TARGET_DIR)/SyntaxParser.o \
	$(TARGET_DIR)/Token.o $(TARGET_DIR)/Type.o -o compiler

test_syntax_parser: $(TARGET_DIR) $(TARGET_DIR)/test_syntax_parser.o $(TARGET_DIR)/SyntaxParser.o \
	$(TARGET_DIR)/LexicalParser.o $(TARGET_DIR)/AST.o $(TARGET_DIR)/Symbol.o $(TARGET_DIR)/Token.o
	$(CC) $(CFLAGS) $(TARGET_DIR)/test_syntax_parser.o $(TARGET_DIR)/SyntaxParser.o $(TARGET_DIR)/LexicalParser.o \
	$(TARGET_DIR)/AST.o $(TARGET_DIR)/Symbol.o $(TARGET_DIR)/Token.o -o test_syntax_parser

test_lexical_parser: $(TARGET_DIR) $(TARGET_DIR)/test_lexical_parser.o $(TARGET_DIR)/LexicalParser.o $(TARGET_DIR)/Token.o
	$(CC) $(CFLAGS) $(TARGET_DIR)/test_lexical_parser.o $(TARGET_DIR)/LexicalParser.o $(TARGET_DIR)/Token.o -o \
	test_lexical_parser
	
$(TARGET_DIR)/compiler.o: src/compiler.cpp
	$(CC) $(CFLAGS) -c src/compiler.cpp -o $(TARGET_DIR)/compiler.o

$(TARGET_DIR)/test_lexical_parser.o: test/test_lexical_parser.cpp
	$(CC) $(CFLAGS) -c test/test_lexical_parser.cpp -o $(TARGET_DIR)/test_lexical_parser.o

$(TARGET_DIR)/test_syntax_parser.o: test/test_syntax_parser.cpp
	$(CC) $(CFLAGS) -c test/test_syntax_parser.cpp -o $(TARGET_DIR)/test_syntax_parser.o

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

$(TARGET_DIR)/Type.o: src/frontend/Type.cpp src/frontend/Type.h
	$(CC) $(CFLAGS) -c src/frontend/Type.cpp -o $(TARGET_DIR)/Type.o

$(TARGET_DIR):
	mkdir -p $(TARGET_DIR)

clean:
	rm target -rf
	rm compiler -f