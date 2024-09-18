.PHONY: debug release prep all clean
OBJ := libtree.so main.o
EXE := main

DBGDIR := debug
DBGEXE := $(DBGDIR)/$(EXE)
DBGOBJ := $(addprefix $(DBGDIR)/, $(OBJ))
debug: CFLAGS += -DDEBUG -g

RELDIR := release
RELEXE := $(RELDIR)/$(EXE)
RELOBJ := $(addprefix $(RELDIR)/, $(OBJ))
release: CFLAGS += -O3

all: debug release

clean:
	rm -rf debug/ release/

prep:
	@mkdir -p $(DBGDIR) $(RELDIR)

debug: prep $(DBGEXE)

$(DBGEXE): $(DBGOBJ)
	$(CC) ${CPPFLAGS} ${CFLAGS} -o $@ $^

$(DBGDIR)/%.so: %.c
	$(CC) ${CPPFLAGS} ${CFLAGS} -shared -fPIC -c -o $@ $^

$(DBGDIR)/%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $^

release: prep $(RELEXE)

$(RELEXE): $(RELOBJ)
	$(CC) ${CPPFLAGS} ${CFLAGS} -o $@ $^

$(RELDIR)/%.so: %.c
	$(CC) ${CPPFLAGS} ${CFLAGS} -shared -fPIC -c -o $@ $^

$(RELDIR)/%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $^

