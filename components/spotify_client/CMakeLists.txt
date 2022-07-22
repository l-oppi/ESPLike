idf_component_register(SRCS "spotify_client.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_http_client json time_manager esp-tls)
