#ifndef CONFIGPARSER_H_INCLUDED
#define CONFIGPARSER_H_INCLUDED

#define CONFIG_DEFAULT ((const char*) 1)

void parse_config_file(const char *file);
void parse_default_config_file();

#endif
