int main(void) {
    lfs_t lfs;
    Config cfg = {0};

    // TODO: mount LittleFS here
    // lfs_mount(&lfs, &cfg, ...);

    if (load_config(&cfg, &lfs) == 0) {
        printf("TFT controller: %s\n", cfg.tft.controller);
        printf("Resolution: %dx%d\n", cfg.tft.width, cfg.tft.height);
        printf("Touch interface: %s\n", cfg.touch.interface);
        printf("Advanced serial: %d\n", cfg.other.advanced_serial_commands);
    }

    // TODO: use cfg to initialize TFT, touch, etc.

    // lfs_unmount(&lfs);
    return 0;
}
