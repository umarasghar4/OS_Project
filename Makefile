CC = gcc
CFLAGS = -Wall -Wextra -pthread
LDFLAGS = -lrt

all: dispatcher ingester processor reporter

dispatcher: dispatcher.c common.h
	$(CC) $(CFLAGS) -o dispatcher dispatcher.c $(LDFLAGS)

ingester: ingester.c common.h
	$(CC) $(CFLAGS) -o ingester ingester.c $(LDFLAGS)

processor: processor.c common.h
	$(CC) $(CFLAGS) -o processor processor.c $(LDFLAGS)

reporter: reporter.c common.h
	$(CC) $(CFLAGS) -o reporter reporter.c $(LDFLAGS)

clean:
	rm -f dispatcher ingester processor reporter
	rm -rf logs/* report.txt report.csv .dispatcher.pid