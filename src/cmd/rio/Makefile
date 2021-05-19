BIN = rio

INCLUDES = -I/usr/X11R6/include
LDFLAGS = -L/usr/X11R6/lib
LIBS = -lXext -lX11

# use "CFLAGS='-DDEBUG -DDEBUG_EV' make" if you want to use rio -debug
CFLAGS += -DSHAPE -DCOLOR -Wall -Werror -pedantic

OBJS = main.o key.o color.o event.o manage.o menu.o client.o grab.o cursor.o error.o

all: $(BIN)

$(BIN): $(OBJS)
	$(CC) -o $(BIN) $(OBJS) $(LIBS) $(INCLUDES) $(LDFLAGS)

.c.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $<

clean:
	rm -f $(BIN) *.o
