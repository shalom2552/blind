
SRC := blind-typing.c
BIN := blind

all:
	gcc -o $(BIN) $(SRC)

clean:
	$(RM) $(BIN)
