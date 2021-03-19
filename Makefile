
### Application-specific constants
APP_NAME := location

### Environment constants 

ARCH ?=
CROSS_COMPILE ?=
CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar

LCFLAGS = $(CFLAGS) -fPIC -Iinc -Ilibmqtt/src

OBJDIR = obj

INCLUDES = $(wildcard inc/*.h) 

### linking options

LLIBS := -lcurl -lpthread -lpaho-mqtt3a -lm

### general build targets

all: $(APP_NAME)

clean:
	rm -f $(OBJDIR)/*.o
	rm -f $(APP_NAME)

### Sub-modules compilation

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/%.o: src/%.c $(INCLUDES) | $(OBJDIR)
	$(CC) -g -c $(LCFLAGS) $< -o $@

### Main program compilation and assembly

$(APP_NAME): $(OBJDIR)/parson.o $(OBJDIR)/lgwmm.o $(OBJDIR)/utilities.o $(OBJDIR)/mapwize_api.o $(OBJDIR)/location.o | $(OBJDIR)
	$(CC) -g $^ -o $@ $(LLIBS)

### test programs

### EOF
