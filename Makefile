all: clean test
test:
	${CC} -o test ${CFLAGS} test.c ${LDFLAGS} -L. -L.. -lpthread -lusb-1.0 -lusbcanfd
	${CC} -o testLin ${CFLAGS} testLin.c ${LDFLAGS} -L. -L.. -lpthread -lusb-1.0 -lusbcanfd
	${CC} -o test_uds ${CFLAGS} test_uds.c ${LDFLAGS} -L. -L.. -lpthread -lusb-1.0 -lusbcanfd
clean:
	rm -vf test
	rm -vf testLin
