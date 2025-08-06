NAME = webserv
CFILES = src/main.cpp \
		src/ParseConf.cpp \
		src/ServerConf.cpp \
		src/Location.cpp
OBJ_DIR = obj
OFILES = $(addprefix $(OBJ_DIR)/,$(CFILES:.cpp=.o))
CC = c++
CFLAGS = -Wall -Werror -Wextra -std=c++98

RED = \033[1;31m
GREEN = \033[1;32m
YELLOW = \033[1;33m
RESET = \033[0m

TOTAL_FILES = $(words $(CFILES))
COMPILED_FILES = 0

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@
	@$(eval COMPILED_FILES=$(shell echo $$(($(COMPILED_FILES)+1))))
	@echo "${YELLOW}[$$(($(COMPILED_FILES)*100/$(TOTAL_FILES)))%]${RESET}		${GREEN}Compiled${RESET} $< ${GREEN}with flags${RESET} $(CFLAGS)"

$(NAME): $(OFILES)
	@$(CC) $(CFLAGS) $(CFILES) -o $(NAME)
	@echo "${YELLOW}[COMPLETED]${RESET}	${GREEN}Created executable${RESET} $(NAME)"

all: $(NAME)

clean:
	@rm -rf $(OBJ_DIR)
	@echo "${RED}Deleted directory${RESET} $(OBJ_DIR) ${RED}containing${RESET} $(CFILES:.cpp=.o)"

fclean: clean
	@rm -f $(NAME)
	@echo "${RED}Deleted executable${RESET} $(NAME)"

re: fclean $(NAME)

.PHONY: all clean fclean re
