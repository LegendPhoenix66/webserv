NAME = webserv
SRC_DIR = src
OBJ_DIR = obj
CFILES = main.cpp \
		ParseConfig.cpp \
		ServerConfig.cpp \
		Location.cpp \
		HttpStatusCodes.cpp \
		ParseUtils.cpp \
		InvalidFormat.cpp \
		Logger.cpp \
		SignalHandler.cpp \
		Address.cpp \
		Socket.cpp \
		EventLoop.cpp \
		Connection.cpp \
		HttpParser.cpp \
		HttpResponse.cpp \
		Listener.cpp \
		Router.cpp \
		LoopUtils.cpp \
		ConnectionUtils.cpp
OFILES = $(addprefix $(OBJ_DIR)/,$(CFILES:.cpp=.o))
CC = c++
CFLAGS = -Wall -Werror -Wextra -std=c++98 -g

RED = \033[1;31m
GREEN = \033[1;32m
YELLOW = \033[1;33m
RESET = \033[0m

TOTAL_FILES = $(words $(CFILES))
COMPILED_FILES = 0

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@
	@$(eval COMPILED_FILES=$(shell echo $$(($(COMPILED_FILES)+1))))
	@echo "${YELLOW}[$$(($(COMPILED_FILES)*100/$(TOTAL_FILES)))%]${RESET}		${GREEN}Compiled${RESET} $(notdir $<) ${GREEN}with flags${RESET} $(CFLAGS)"

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

asan:
	@$(MAKE) CFLAGS='-Wall -Werror -Wextra -std=c++98 -g -fsanitize=address -fno-omit-frame-pointer' all

test:
	@bash tests/mandatory_test.sh

re: fclean $(NAME)

.PHONY: all clean fclean test asan re
