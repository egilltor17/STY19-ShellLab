# Makefile for the STY1 Shell Lab

TEAM = $(shell egrep '^ *\* *Group:' tsh.c | sed -e 's/\*//g' -e 's/ *Group: *//g' | sed 's/ *\([^ ].*\) *$$/\1/g')
USER_1 = $(shell egrep '^ *\* *User 1:' tsh.c | sed -e 's/\*//g' -e 's/ *User 1: *//g' | sed 's/ *\([^ ].*\) *$$/\1/g')
USER_2 = $(shell egrep '^ *\* *User 2:' tsh.c | sed -e 's/\*//g' -e 's/ *User 2: *//g' | sed 's/ *\([^ ].*\) *$$/\1/g')
USER_3 = $(shell egrep '^ *\* *User 3:' tsh.c | sed -e 's/\*//g' -e 's/ *User 3: *//g' | sed 's/ *\([^ ].*\) *$$/\1/g')

ifeq "$(TEAM)" "GROUPNAME"
	TEAM = "NONE"
endif
ifeq "$(USER_1)" "RU-LOGIN"
	USER_1 = "NONE"
endif
ifeq "$(USER_2)" ""
	USER_2 = "NONE"
endif
ifeq "$(USER_3)" ""
	USER_3 = "NONE"
endif

VERSION = 1
HANDINDIR = /labs/sty19/.handin/shlab
DRIVER = ./sdriver.pl
TSH = ./tsh
TSHREF = ./tshref
TSHARGS = "-p"
CC = gcc
CFLAGS = -Wall -O2 -std=gnu11
FILES = $(TSH) ./myspin ./mysplit ./mystop ./myint

all: $(FILES)

##################
# Handin your work
##################
handin:
	@echo "Team: \"$(TEAM)\""
	@echo "User 1: \"$(USER_1)\""
	@echo "User 2: \"$(USER_2)\""
	@echo "User 3: \"$(USER_3)\""
	@if [ "$(TEAM)" == "NONE" ]; then echo "Team name missing, please add it to the tsh.c file."; exit 1; fi
	@if [ "$(USER_1)" != "NONE" ]; then getent passwd $(USER_1) > /dev/null; if [ $$? -ne 0 ]; then echo "User $(USER_1) does not exist on Skel."; exit 2; fi; fi
	@if [ "$(USER_2)" != "NONE" ]; then getent passwd $(USER_2) > /dev/null; if [ $$? -ne 0 ]; then echo "User $(USER_2) does not exist on Skel."; exit 3; fi; fi
	@if [ "$(USER_3)" != "NONE" ]; then getent passwd $(USER_3) > /dev/null; if [ $$? -ne 0 ]; then echo "User $(USER_3) does not exist on Skel."; exit 4; fi; fi
	cp tsh.c "$(HANDINDIR)/$(USER)/$(TEAM)-$(VERSION)-tsh.c"

##################
# Regression tests
##################

# Run tests using the student's shell program
test01:
	$(DRIVER) -t trace01.txt -s $(TSH) -a $(TSHARGS)
test02:
	$(DRIVER) -t trace02.txt -s $(TSH) -a $(TSHARGS)
test03:
	$(DRIVER) -t trace03.txt -s $(TSH) -a $(TSHARGS)
test04:
	$(DRIVER) -t trace04.txt -s $(TSH) -a $(TSHARGS)
test05:
	$(DRIVER) -t trace05.txt -s $(TSH) -a $(TSHARGS)
test06:
	$(DRIVER) -t trace06.txt -s $(TSH) -a $(TSHARGS)
test07:
	$(DRIVER) -t trace07.txt -s $(TSH) -a $(TSHARGS)
test08:
	$(DRIVER) -t trace08.txt -s $(TSH) -a $(TSHARGS)
test09:
	$(DRIVER) -t trace09.txt -s $(TSH) -a $(TSHARGS)
test10:
	$(DRIVER) -t trace10.txt -s $(TSH) -a $(TSHARGS)
test11:
	$(DRIVER) -t trace11.txt -s $(TSH) -a $(TSHARGS)
test12:
	$(DRIVER) -t trace12.txt -s $(TSH) -a $(TSHARGS)
test13:
	$(DRIVER) -t trace13.txt -s $(TSH) -a $(TSHARGS)
test14:
	$(DRIVER) -t trace14.txt -s $(TSH) -a $(TSHARGS)
test15:
	$(DRIVER) -t trace15.txt -s $(TSH) -a $(TSHARGS)
test16:
	$(DRIVER) -t trace16.txt -s $(TSH) -a $(TSHARGS)

# Run the tests using the reference shell program
rtest01:
	$(DRIVER) -t trace01.txt -s $(TSHREF) -a $(TSHARGS)
rtest02:
	$(DRIVER) -t trace02.txt -s $(TSHREF) -a $(TSHARGS)
rtest03:
	$(DRIVER) -t trace03.txt -s $(TSHREF) -a $(TSHARGS)
rtest04:
	$(DRIVER) -t trace04.txt -s $(TSHREF) -a $(TSHARGS)
rtest05:
	$(DRIVER) -t trace05.txt -s $(TSHREF) -a $(TSHARGS)
rtest06:
	$(DRIVER) -t trace06.txt -s $(TSHREF) -a $(TSHARGS)
rtest07:
	$(DRIVER) -t trace07.txt -s $(TSHREF) -a $(TSHARGS)
rtest08:
	$(DRIVER) -t trace08.txt -s $(TSHREF) -a $(TSHARGS)
rtest09:
	$(DRIVER) -t trace09.txt -s $(TSHREF) -a $(TSHARGS)
rtest10:
	$(DRIVER) -t trace10.txt -s $(TSHREF) -a $(TSHARGS)
rtest11:
	$(DRIVER) -t trace11.txt -s $(TSHREF) -a $(TSHARGS)
rtest12:
	$(DRIVER) -t trace12.txt -s $(TSHREF) -a $(TSHARGS)
rtest13:
	$(DRIVER) -t trace13.txt -s $(TSHREF) -a $(TSHARGS)
rtest14:
	$(DRIVER) -t trace14.txt -s $(TSHREF) -a $(TSHARGS)
rtest15:
	$(DRIVER) -t trace15.txt -s $(TSHREF) -a $(TSHARGS)
rtest16:
	$(DRIVER) -t trace16.txt -s $(TSHREF) -a $(TSHARGS)


# clean up
clean:
	rm -f $(FILES) *.o *~


check:
	@ls -l $(HANDINDIR)/$(USER)
