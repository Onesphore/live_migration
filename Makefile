DIR := ${CURDIR}
# ---- flags
# compiler used
CC=gcc
# compiler flags
CFLAGS=-g3 -O0 -fno-stack-protector # no optimization
# static library flags.
# "before_main()" is not called in hello, hence 
# we need to mention it to the linker.
LFLAGS=-static -L${DIR} -lckpt -Wl,-u,before_main
# -end


default: hello live_migrate

hello: hello.c libckpt.a
	${CC} ${CFLAGS} ${LFLAGS} -o hello hello.c
libckpt.a: ckpt.o
	ar -rcs libckpt.a ckpt.o
ckpt.o: ckpt.c
	${CC} ${CFLAGS} -c -o ckpt.o ckpt.c

# ./restart myckpt will read the ckpt image "myckpt" into
# the myrestart address space. And then hello will restart
# from where it left off.
# -lm: for the math library.
live_migrate: live_migrate.c
	${CC} ${CFLAGS} -static \
        -Wl,-Ttext-segment=6400000 -Wl,-Tdata=6500000 -Wl,-Tbss=6600000 \
         -o live_migrate live_migrate.c -lm


# Will make testing our code faster
check: libckpt.a quicksort live_migrate
	(sleep 1 && kill -12 `pgrep -n quicksort` && \
	sleep 1 && pkill -9 -n quicksort) & 
	./quicksort < war-and-peace.txt | less

clean:
	rm -rf quicksort libckpt.a live_migrate ckpt.o




