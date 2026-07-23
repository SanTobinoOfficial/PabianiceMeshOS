idf_component_register(
    SRCS "pkt.c" "dedup.c" "presence.c" "mesh.c"
    INCLUDE_DIRS "include"
    REQUIRES radio esp_timer
)
