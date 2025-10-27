NAME = webserv
SRC_DIR = src
OBJ_DIR = obj

# Explicit source list (42 subject prefers listing files)
CFILES = \
	main.cpp \
	core/Server.cpp \
	core/EventLoop.cpp \
	core/Listener.cpp \
	core/Connection.cpp \
	net/Socket.cpp \
	net/Address.cpp \
	http/HttpRequest.cpp \
	http/HttpResponse.cpp \
	http/HttpParser.cpp \
	http/MimeTypes.cpp \
	routing/Router.cpp \
	routing/StaticFileHandler.cpp \
	config/Config.cpp \
	config/ConfigParser.cpp \
	util/Logger.cpp \
	util/SignalHandler.cpp

OFILES = $(addprefix $(OBJ_DIR)/,$(CFILES:.cpp=.o))
CC = c++
CFLAGS = -Wall -Werror -Wextra -std=c++98 -g -MMD -MP #-fsanitize=address
INCLUDES = -Iinc

RED = \033[1;31m
GREEN = \033[1;32m
YELLOW = \033[1;33m
RESET = \033[0m

TOTAL_FILES = $(words $(CFILES))
COMPILED_FILES = 0

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
	@$(eval COMPILED_FILES=$(shell echo $$(($(COMPILED_FILES)+1))))
	@echo "${YELLOW}[$$(($(COMPILED_FILES)*100/$(TOTAL_FILES)))%]${RESET}		${GREEN}Compiled${RESET} $(notdir $<) ${GREEN}with flags${RESET} $(CFLAGS) $(INCLUDES)"

-include $(OFILES:.o=.d)

$(NAME): $(OFILES)
	@$(CC) $(CFLAGS) $(OFILES) -o $(NAME)
	@echo "${YELLOW}[COMPLETED]${RESET}	${GREEN}Created executable${RESET} $(NAME)"

all: $(NAME)

clean:
	@rm -rf $(OBJ_DIR)
	@echo "${RED}Deleted directory${RESET} $(OBJ_DIR) ${RED}containing${RESET} $(notdir $(patsubst %.cpp, %.o, $(CFILES)))"

fclean: clean
	@rm -f $(NAME)
	@echo "${RED}Deleted executable${RESET} $(NAME)"

re: fclean $(NAME)

asan:
	@$(MAKE) CFLAGS='-Wall -Werror -Wextra -std=c++98 -g -fsanitize=address -fno-omit-frame-pointer' all

.PHONY: all clean fclean re asan


# Test targets
# Run the mandatory test suite (requires WSL bash, curl, python3)
test:
	@bash tests/mandatory_test.sh

# AddressSanitizer build then run tests
# Note: server will still be named 'webserv' but built with ASan flags.
test-asan:
	@$(MAKE) asan
	@bash tests/mandatory_test.sh
