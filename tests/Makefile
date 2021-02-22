all:
	gcc  test.c test_helpers.c  -o test_suite `pkg-config --cflags --libs check` -lsession
	gcc  -Wall -pg perf_test.c  -o perf_test -lsession
