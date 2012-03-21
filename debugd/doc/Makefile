SRCS := $(shell echo *.md)
OBJS := $(patsubst %.md,%.html,$(SRCS))

all: $(OBJS)

%.html: %.md
	markdown $^ > $@
