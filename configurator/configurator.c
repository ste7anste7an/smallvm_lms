static void trim(char *str) {
    char *end;
    while (*str == ' ' || *str == '\t') str++;
    end = str + strlen(str) - 1;
    while (end > str && (*end == '\r' || *end == '\n' || *end == ' ' || *end == '\t'))
        *end-- = '\0';
}

static void parse_line(char *line, Config *cfg, const char *section) {
    char key[64], value[64];
    if (sscanf(line, "%63[^=]=%63s", key, value) != 2) return;
    trim(key); trim(value);

    if (strcmp(section, "tft") == 0) {
        if (strcmp(key, "controller") == 0) strncpy(cfg->tft.controller, value, sizeof(cfg->tft.controller));
        else if (strcmp(key, "mosi") == 0) cfg->tft.mosi = atoi(value);
        else if (strcmp(key, "miso") == 0) cfg->tft.miso = atoi(value);
        else if (strcmp(key, "sck") == 0) cfg->tft.sck = atoi(value);
        else if (strcmp(key, "cs") == 0) cfg->tft.cs = atoi(value);
        else if (strcmp(key, "dc") == 0) cfg->tft.dc = atoi(value);
        else if (strcmp(key, "rst") == 0) cfg->tft.rst = atoi(value);
        else if (strcmp(key, "orientation") == 0) cfg->tft.orientation = atoi(value);
        else if (strcmp(key, "color") == 0) cfg->tft.color = atoi(value);
        else if (strcmp(key, "backlight") == 0) cfg->tft.backlight = atoi(value);
        else if (strcmp(key, "width") == 0) cfg->tft.width = atoi(value);
        else if (strcmp(key, "height") == 0) cfg->tft.height = atoi(value);
    } else if (strcmp(section, "touch") == 0) {
        if (strcmp(key, "interface") == 0) strncpy(cfg->touch.interface, value, sizeof(cfg->touch.interface));
        else if (strcmp(key, "spi_mode") == 0) strncpy(cfg->touch.spi_mode, value, sizeof(cfg->touch.spi_mode));
        else if (strcmp(key, "shared_cs") == 0) cfg->touch.shared_cs = atoi(value);
        else if (strcmp(key, "miso") == 0) cfg->touch.miso = atoi(value);
        else if (strcmp(key, "mosi") == 0) cfg->touch.mosi = atoi(value);
        else if (strcmp(key, "sck") == 0) cfg->touch.sck = atoi(value);
        else if (strcmp(key, "cs") == 0) cfg->touch.cs = atoi(value);
        else if (strcmp(key, "orientation") == 0) cfg->touch.orientation = atoi(value);
    } else if (strcmp(section, "i2c") == 0) {
        if (strcmp(key, "sda") == 0) cfg->touch.sda = atoi(value);
        else if (strcmp(key, "scl") == 0) cfg->touch.scl = atoi(value);
    } else if (strcmp(section, "other") == 0) {
        if (strcmp(key, "advanced_serial_commands") == 0)
            cfg->other.advanced_serial_commands = atoi(value);
    }
}


int load_config(Config *cfg, lfs_t *lfs) {
    lfs_file_t file;
    char line[128];
    char section[32] = "";

    if (lfs_file_open(lfs, &file, CONFIG_FILE, LFS_O_RDONLY) < 0) {
        printf("Failed to open config file\n");
        return -1;
    }

    while (lfs_file_read(lfs, &file, line, sizeof(line) - 1) > 0) {
        line[sizeof(line) - 1] = '\0';
        trim(line);
        if (line[0] == '#' || strlen(line) == 0) continue;

        if (line[0] == '[') {
            sscanf(line, "[%31[^]]", section);
            continue;
        }
        parse_line(line, cfg, section);
    }

    lfs_file_close(lfs, &file);
    return 0;
}
