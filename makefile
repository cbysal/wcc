CC = g++
CFLAGS = -g -fdiagnostics-color=always -Wall -std=c++17 -Werror
TARGET_DIR = target

test_grammar_parser: $(TARGET_DIR) $(TARGET_DIR)/test_grammar_parser.o $(TARGET_DIR)/GrammarParser.o \
	$(TARGET_DIR)/LexicalParser.o $(TARGET_DIR)/AST.o $(TARGET_DIR)/Symbol.o $(TARGET_DIR)/Token.o
	$(CC) $(CFLAGS) $(TARGET_DIR)/test_grammar_parser.o $(TARGET_DIR)/GrammarParser.o $(TARGET_DIR)/LexicalParser.o \
	$(TARGET_DIR)/AST.o $(TARGET_DIR)/Symbol.o $(TARGET_DIR)/Token.o -o $(TARGET_DIR)/test_grammar_parser

test_lexical_parser: $(TARGET_DIR) $(TARGET_DIR)/test_lexical_parser.o $(TARGET_DIR)/LexicalParser.o $(TARGET_DIR)/Token.o
	$(CC) $(CFLAGS) $(TARGET_DIR)/test_lexical_parser.o $(TARGET_DIR)/LexicalParser.o $(TARGET_DIR)/Token.o -o \
	$(TARGET_DIR)/test_lexical_parser
	
$(TARGET_DIR)/test_lexical_parser.o: test/test_lexical_parser.cpp
	$(CC) $(CFLAGS) -c test/test_lexical_parser.cpp -o $(TARGET_DIR)/test_lexical_parser.o

$(TARGET_DIR)/test_grammar_parser.o: test/test_grammar_parser.cpp
	$(CC) $(CFLAGS) -c test/test_grammar_parser.cpp -o $(TARGET_DIR)/test_grammar_parser.o

$(TARGET_DIR)/LexicalParser.o: src/frontend/LexicalParser.cpp
	$(CC) $(CFLAGS) -c src/frontend/LexicalParser.cpp -o $(TARGET_DIR)/LexicalParser.o

$(TARGET_DIR)/GrammarParser.o: src/frontend/GrammarParser.cpp
	$(CC) $(CFLAGS) -c src/frontend/GrammarParser.cpp -o $(TARGET_DIR)/GrammarParser.o

$(TARGET_DIR)/AST.o: src/frontend/AST.cpp
	$(CC) $(CFLAGS) -c src/frontend/AST.cpp -o $(TARGET_DIR)/AST.o

$(TARGET_DIR)/AssignStmtAST.o: src/frontend/ast/AssignStmtAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/AssignStmtAST.cpp -o $(TARGET_DIR)/AssignStmtAST.o

$(TARGET_DIR)/BinaryExpAST.o: src/frontend/ast/BinaryExpAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/BinaryExpAST.cpp -o $(TARGET_DIR)/BinaryExpAST.o

$(TARGET_DIR)/BlankStmtAST.o: src/frontend/ast/BlankStmtAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/BlankStmtAST.cpp -o $(TARGET_DIR)/BlankStmtAST.o

$(TARGET_DIR)/BlockAST.o: src/frontend/ast/BlockAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/BlockAST.cpp -o $(TARGET_DIR)/BlockAST.o

$(TARGET_DIR)/BreakStmtAST.o: src/frontend/ast/BreakStmtAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/BreakStmtAST.cpp -o $(TARGET_DIR)/BreakStmtAST.o

$(TARGET_DIR)/ConstDefAST.o: src/frontend/ast/ConstDefAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/ConstDefAST.cpp -o $(TARGET_DIR)/ConstDefAST.o

$(TARGET_DIR)/ContinueStmtAST.o: src/frontend/ast/ContinueStmtAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/ContinueStmtAST.cpp -o $(TARGET_DIR)/ContinueStmtAST.o

$(TARGET_DIR)/ExpStmtAST.o: src/frontend/ast/ExpStmtAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/ExpStmtAST.cpp -o $(TARGET_DIR)/ExpStmtAST.o

$(TARGET_DIR)/FloatLiteralAST.o: src/frontend/ast/FloatLiteralAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/FloatLiteralAST.cpp -o $(TARGET_DIR)/FloatLiteralAST.o

$(TARGET_DIR)/FuncCallAST.o: src/frontend/ast/FuncCallAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/FuncCallAST.cpp -o $(TARGET_DIR)/FuncCallAST.o

$(TARGET_DIR)/FuncDefAST.o: src/frontend/ast/FuncDefAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/FuncDefAST.cpp -o $(TARGET_DIR)/FuncDefAST.o

$(TARGET_DIR)/FuncFParamAST.o: src/frontend/ast/FuncFParamAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/FuncFParamAST.cpp -o $(TARGET_DIR)/FuncFParamAST.o

$(TARGET_DIR)/IfStmtAST.o: src/frontend/ast/IfStmtAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/IfStmtAST.cpp -o $(TARGET_DIR)/IfStmtAST.o

$(TARGET_DIR)/InitValAST.o: src/frontend/ast/InitValAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/InitValAST.cpp -o $(TARGET_DIR)/InitValAST.o

$(TARGET_DIR)/IntLiteralAST.o: src/frontend/ast/IntLiteralAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/IntLiteralAST.cpp -o $(TARGET_DIR)/IntLiteralAST.o

$(TARGET_DIR)/LValAST.o: src/frontend/ast/LValAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/LValAST.cpp -o $(TARGET_DIR)/LValAST.o

$(TARGET_DIR)/ReturnStmtAST.o: src/frontend/ast/ReturnStmtAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/ReturnStmtAST.cpp -o $(TARGET_DIR)/ReturnStmtAST.o

$(TARGET_DIR)/RootAST.o: src/frontend/ast/RootAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/RootAST.cpp -o $(TARGET_DIR)/RootAST.o

$(TARGET_DIR)/UnaryExpAST.o: src/frontend/ast/UnaryExpAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/UnaryExpAST.cpp -o $(TARGET_DIR)/UnaryExpAST.o

$(TARGET_DIR)/VarDefAST.o: src/frontend/ast/VarDefAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/VarDefAST.cpp -o $(TARGET_DIR)/VarDefAST.o

$(TARGET_DIR)/WhileStmtAST.o: src/frontend/ast/WhileStmtAST.cpp
	$(CC) $(CFLAGS) -c src/frontend/ast/WhileStmtAST.cpp -o $(TARGET_DIR)/WhileStmtAST.o

$(TARGET_DIR)/Symbol.o: src/frontend/Symbol.cpp
	$(CC) $(CFLAGS) -c src/frontend/Symbol.cpp -o $(TARGET_DIR)/Symbol.o

$(TARGET_DIR)/Token.o: src/frontend/Token.cpp
	$(CC) $(CFLAGS) -c src/frontend/Token.cpp -o $(TARGET_DIR)/Token.o

$(TARGET_DIR):
	mkdir -p $(TARGET_DIR)

clean:
	rm target -rf
	rm test_lexical_parser test_grammar_parser -rf