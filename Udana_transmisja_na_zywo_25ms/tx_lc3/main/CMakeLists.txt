idf_component_register(
    SRCS "main.c" "Setup_codec.cpp"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash log freertos esp_wifi esp_netif driver Codec_lib esp_timer liblc3 esp_ringbuf
)
