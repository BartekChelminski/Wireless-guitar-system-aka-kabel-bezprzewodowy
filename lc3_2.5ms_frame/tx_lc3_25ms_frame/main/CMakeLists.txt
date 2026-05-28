# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS "main.c" "Setup_codec.cpp"
    INCLUDE_DIRS "."
    PRIV_INCLUDE_DIRS
    REQUIRES nvs_flash log freertos esp_wifi esp_netif driver Codec_lib liblc3
    PRIV_REQUIRES
)
