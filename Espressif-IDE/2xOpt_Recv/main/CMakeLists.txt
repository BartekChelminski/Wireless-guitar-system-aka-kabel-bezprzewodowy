# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS "main.c" "Setup_codec.cpp"         # list the source files of this component
    INCLUDE_DIRS "."        # optional, add here public include directories
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES nvs_flash log freertos esp_wifi esp_netif driver Codec_lib esp_timer        # optional, list the public requirements (component names)
    PRIV_REQUIRES       # optional, list the private requirements
)
