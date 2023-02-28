all:np_single_proc np_simple

np_single_proc : np_single_proc.cpp single_builtins.cpp
	g++ -o np_single_proc np_single_proc.cpp single_builtins.cpp

np_simple : np_simple.c simple_builtins.c
	gcc -o np_simple np_simple.c simple_builtins.c

clean:
	rm -f np_single_proc
	rm -f np_simple
