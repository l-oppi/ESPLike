idf_component_register(SRCS "json.c" "nvs_sync.c" "dns_server.c" "http_app.c" "wifi_manager.c"
                    INCLUDE_DIRS "include"
                    EMBED_FILES "code.js" "index.html" "style.css"
                    REQUIRES nvs_flash esp_http_server wpa_supplicant mdns)
