idf_component_register(SRCS "main.c" INCLUDE_DIRS "." REQUIRES sf04 driver esp_timer app_update)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes)
