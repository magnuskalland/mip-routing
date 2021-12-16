CCFLAGS				= -g -std=gnu11 -Wall -Wextra
VALGRINDFLAGS 		= --track-fds=yes --leak-check=full --show-leak-kinds=all --track-origins=yes
VALGRINDOUTPUTFILE 	= misc/valgrind-out.txt

VALGRINDOUTPUT 		= --log-file="$(VALGRINDOUTPUTFILE)"

BUILD 				= .build/
HEADERDIR 			= headers/
SOURCEDIR			= source/
SOCKETSDIR 			= /.sockets/
PROJECTNAME 		= mip-routing

DAEMON 				= mip_daemon
ROUTING				= mip_routing
CLIENT 				= ping_client
SERVER 				= ping_server

MIP 				= mip
MIPARP 				= mip_arp
MIPDEBUG 			= mip_debug
UTILS 				= utils
COMMON 				= common
STRUCTS				= structs
QUEUE				= queue

C_FILES					= $(wildcard $(SOURCEDIR)*.c)
H_FILES					= $(wildcard $(HEADERDIR)*.h)
O_FILES 				= $(subst $(SOURCEDIR), $(BUILD), $(SOURCEDIR)$(C_FILES:.c=.o))
CLIENT_EXECUTABLES 		= $(DAEMON) $(ROUTING) $(CLIENT)
SERVER_EXECUTABLES 		= $(DAEMON) $(ROUTING) $(SERVER) 

A_SOCKNAME 			= $(SOCKETSDIR)host-a
B_SOCKNAME 			= $(SOCKETSDIR)host-b
C_SOCKNAME 			= $(SOCKETSDIR)host-c
D_SOCKNAME 			= $(SOCKETSDIR)host-d
E_SOCKNAME 			= $(SOCKETSDIR)host-e

A_DAEMON_ARGS 		= $(A_SOCKNAME) 10
B_DAEMON_ARGS 		= $(B_SOCKNAME) 20
C_DAEMON_ARGS 		= $(C_SOCKNAME) 30
D_DAEMON_ARGS 		= $(D_SOCKNAME) 40
E_DAEMON_ARGS 		= $(E_SOCKNAME) 50

A_ROUTING_ARGS 		= -d $(A_SOCKNAME) 10
B_ROUTING_ARGS 		= -d $(B_SOCKNAME) 20
C_ROUTING_ARGS 		= -d $(C_SOCKNAME) 30
D_ROUTING_ARGS 		= -d $(D_SOCKNAME) 40
E_ROUTING_ARGS 		= -d $(E_SOCKNAME) 50

A_ARGS 				= 25 "this is a ping from A!" $(A_SOCKNAME)
B_ARGS 				= 25 "this is a ping from B!" $(B_SOCKNAME)
C_ARGS 				= 25 "this is a ping from C!" $(C_SOCKNAME)
D_ARGS 				= 25 "this is a ping from D!" $(D_SOCKNAME)
E_ARGS 				= 25 "this is a ping from E!" $(E_SOCKNAME)

S_SOCKNAME			= $(SOCKETSDIR)server
S_DAEMON_ARGS		= -d $(S_SOCKNAME) 25
S_ROUTING_ARGS		= -d $(S_SOCKNAME) 25
S_ARGS				= $(S_SOCKNAME)

# files not directly associated to the executables
BIN = $(BUILD)$(MIP).o $(HEADERDIR)$(MIP).h $(BUILD)$(MIPARP).o $(HEADERDIR)$(MIPARP).h $(BUILD)$(MIPDEBUG).o $(HEADERDIR)$(MIPDEBUG).h $(BUILD)$(UTILS).o $(HEADERDIR)$(UTILS).h $(BUILD)$(COMMON).o $(HEADERDIR)$(COMMON).h $(BUILD)$(QUEUE).o $(HEADERDIR)$(QUEUE).h $(HEADERDIR)$(STRUCTS).h

#O_FILES current target: prerequisite 
# $@: $^ ($< is first prerequisite)

all: make-dirs $(O_FILES) $(EXECUTABLES)

# link object files into executables
$(SOURCEDIR)$(DAEMON): $(BUILD)$(DAEMON).o $(HEADERDIR)$(DAEMON).h $(BIN)
	@echo "Linking $^";
	@sudo gcc $(CCFLAGS) $^ -o $(DAEMON)

$(SOURCEDIR)$(ROUTING): $(BUILD)$(ROUTING).o $(HEADERDIR)$(ROUTING).h $(BIN)
	@echo "Linking $^";
	@sudo gcc $(CCFLAGS) $^ -o $(ROUTING)

$(SOURCEDIR)$(SERVER): $(BUILD)$(SERVER).o $(HEADERDIR)$(SERVER).h $(BIN)
	@echo "Linking $^";
	@sudo gcc $(CCFLAGS) $^ -o $(SERVER)

$(SOURCEDIR)$(CLIENT): $(BUILD)$(CLIENT).o $(HEADERDIR)$(CLIENT).h $(BIN)
	@echo "Linking $^";
	@sudo gcc $(CCFLAGS) $^ -o $(CLIENT)

# run rules

runa: $(CLIENT_EXECUTABLES)
	sudo ./$(DAEMON) $(A_DAEMON_ARGS)
	sleep 0.1
	sudo ./$(ROUTING) $(A_ROUTING_ARGS)
	sleep 2
	sudo ./$(CLIENT) $(A_ARGS)

runb: $(CLIENT_EXECUTABLES)
	sudo ./$(DAEMON) $(B_DAEMON_ARGS)
	sleep 0.1
	sudo ./$(ROUTING) $(B_ROUTING_ARGS)
	sleep 2
	sudo ./$(CLIENT) $(B_ARGS)

runc: $(CLIENT_EXECUTABLES)
	sudo ./$(DAEMON) $(C_DAEMON_ARGS)
	sleep 0.1
	sudo ./$(ROUTING) $(C_ROUTING_ARGS)
	sleep 2
	sudo ./$(CLIENT) $(C_ARGS)

rund: $(CLIENT_EXECUTABLES)
	sudo ./$(DAEMON) $(D_DAEMON_ARGS)
	sleep 0.1
	sudo ./$(ROUTING) $(D_ROUTING_ARGS)
	sleep 2
	sudo ./$(CLIENT) $(D_ARGS)

rune: $(CLIENT_EXECUTABLES)
	sudo ./$(DAEMON) $(E_DAEMON_ARGS)
	sleep 0.1
	sudo ./$(ROUTING) $(E_ROUTING_ARGS)
	sleep 2
	sudo ./$(CLIENT) $(E_ARGS)

runs: $(SERVER_EXECUTABLES)
	sudo ./$(DAEMON) $(S_DAEMON_ARGS)
	sleep 0.1
	sudo ./$(ROUTING) $(S_ROUTING_ARGS)
	sudo ./$(SERVER) $(S_ARGS)

# compile C files into object files
$(BUILD)$(DAEMON).o: $(SOURCEDIR)$(DAEMON).c
	@echo "Compiling $^";
	@sudo gcc $(CCFLAGS) -c $^ -o $@ 

$(BUILD)$(SERVER).o: $(SOURCEDIR)$(SERVER).c
	@echo "Compiling $^";
	@sudo gcc $(CCFLAGS) -c $^ -o $@ 

$(BUILD)$(CLIENT).o: $(SOURCEDIR)$(CLIENT).c
	@echo "Compiling $^";
	@sudo gcc $(CCFLAGS) -c $^ -o $@ 

$(BUILD)$(MIP).o: $(SOURCEDIR)$(MIP).c
	@echo "Compiling $^";
	@sudo gcc $(CCFLAGS) -c $^ -o $@ 

$(BUILD)$(MIPARP).o: $(SOURCEDIR)$(MIPARP).c
	@echo "Compiling $^";
	@sudo gcc $(CCFLAGS) -c $^ -o $@ 

$(BUILD)$(MIPDEBUG).o: $(SOURCEDIR)$(MIPDEBUG).c
	@echo "Compiling $^";
	@sudo gcc $(CCFLAGS) -c $^ -o $@

$(BUILD)$(UTILS).o: $(SOURCEDIR)$(UTILS).c
	@echo "Compiling $^";
	@sudo gcc $(CCFLAGS) -c $^ -o $@ 
	
$(BUILD)$(COMMON).o: $(SOURCEDIR)$(COMMON).c
	@echo "Compiling $^";
	@sudo gcc $(CCFLAGS) -c $^ -o $@ 

$(BUILD)$(ROUTING).o: $(SOURCEDIR)$(ROUTING).c
	@echo "Compiling $^";
	@sudo gcc $(CCFLAGS) -c $^ -o $@

$(BUILD)$(QUEUE).o: $(SOURCEDIR)$(QUEUE).c
	@echo "Compiling $^";
	@sudo gcc $(CCFLAGS) -c $^ -o $@

# valgrind
vala: $(CLIENT_EXECUTABLES)
	sudo rm -f $(VALGRINDOUTPUTFILE)
	sudo ./$(DAEMON) $(A_DAEMON_ARGS)
	sleep 0.1
	sudo ./$(ROUTING) $(A_ROUTING_ARGS) 
	sleep 2
	sudo ./$(CLIENT) $(A_ARGS)

valb: $(CLIENT_EXECUTABLES)
	@sudo rm -f $(VALGRINDOUTPUTFILE)
	sudo ./$(DAEMON) $(B_DAEMON_ARGS)
	sleep 0.1
	sudo ./$(ROUTING) $(B_ROUTING_ARGS)
	sleep 2
	sudo ./$(CLIENT) $(B_ARGS)

valc: $(CLIENT_EXECUTABLES)
	@sudo rm -f $(VALGRINDOUTPUTFILE)
	sudo ./$(DAEMON) $(C_DAEMON_ARGS)
	sleep 0.1
	sudo ./$(ROUTING) $(C_ROUTING_ARGS)
	sleep 2
	sudo ./$(CLIENT) $(C_ARGS)

vald: $(CLIENT_EXECUTABLES)
	@sudo rm -f $(VALGRINDOUTPUTFILE)
	sudo ./$(DAEMON) $(D_DAEMON_ARGS)
	sleep 0.1
	sudo valgrind $(VALGRINDOUTPUT) $(VALGRINDFLAGS) ./$(ROUTING) $(D_ROUTING_ARGS)
	sleep 2
	sudo ./$(CLIENT) $(D_ARGS)

vale: $(CLIENT_EXECUTABLES)
	@sudo rm -f $(VALGRINDOUTPUTFILE)
	sudo ./$(DAEMON) $(E_DAEMON_ARGS)
	sleep 0.1
	sudo ./$(ROUTING) $(E_ROUTING_ARGS)
	sleep 2
	sudo ./$(CLIENT) $(E_ARGS)

vals: $(SERVER_EXECUTABLES)
	@sudo rm -f $(VALGRINDOUTPUTFILE)
	sudo ./$(DAEMON) $(S_DAEMON_ARGS)
	sleep 0.1
	sudo ./$(ROUTING) $(S_ROUTING_ARGS)
	sudo ./$(SERVER) $(S_ARGS)

# remove run files
clean:
	@echo "Removing $(BUILD)* and $(SOCKETSDIR)"
	@sudo rm -rf $(BUILD)* $(SOCKETSDIR)* $(VALGRINDOUTPUTFILE) $(CLIENT_EXECUTABLES) $(SERVER_EXECUTABLES)

make-dirs:
	@sudo mkdir -p $(BUILD) $(HEADERDIR) $(SOCKETSDIR)

assembler: $(C_FILES)
	sudo gcc $(CCFLAGS) -S -fverbose-asm -O2 $< -o $(BUILD)/$(PROJECTNAME).asm 

print-files:
	@echo "C_FILES			$(subst $(SOURCEDIR), , $(C_FILES))"
	@echo "H_FILES			$(subst $(HEADERDIR), , $(H_FILES))"
	@echo "O_FILES		    $(subst $(BUILD), , $(O_FILES))"
	@echo "EXECUTABLES		  $(CLIENT_EXECUTABLES) $(SOURCEDIR)$(SERVER)"
