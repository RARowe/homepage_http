# Homepage HTTP Server (HHTTPS)

Simple, threaded HTTP server with blocking I/O, but multiple thread workers.

Also serves a favico.ico by linking the favico into the executable binary.

To run on Linux, use:
```bash
ld -b binary -r favico.ico -o favico.o\
&& gcc main.c favico.o\
	-fsanitize=address\
	-D_GNU_SOURCE\
	-z noexecstack\
&& ./a.out
```
