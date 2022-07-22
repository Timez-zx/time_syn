CFLAGS= -g -Wall -O0

#standard c implementation
FILENAME=slave.c
OUT=slave.out

HEADER=	libs/

default: $(FILENAME) $(HEADER)
	$(CC)  $(CFLAGS) $(FILENAME) -o $(OUT) 

clean:
	$(RM) $(OUT)
	$(RM) -r $(OUT).dSYM
	$(RM) $(TIMESTAMP_OUT)
	$(RM) -r $(TIMESTAMP_OUT).dSYM
	$(RM) $(NETMAP_OUT)
	$(RM) -r $(NETMAP_OUT).dSYM