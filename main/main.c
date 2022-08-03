#include <stdio.h>
#include <string.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "spotify_client.h"
#include "wifi_manager.h"

static bool internet_connection =  false;
// static const char TAG[] = "main";


void monitoring_task(void *pvParameter)
{	
	player_details_t player_details;
	currently_playing_t currently_playing;
	spotify_get_player_details(&player_details);
	vTaskDelay(pdMS_TO_TICKS(1000));
	spotify_get_current_playing(&currently_playing);
	if (currently_playing.is_playing)
		spotify_pause();
	vTaskDelay(pdMS_TO_TICKS(1000));
	spotify_play("spotify:album:5ht7ItJgpBH7W6vJ5BqpPr", 5, 0, player_details.device.id);
	vTaskDelay(pdMS_TO_TICKS(1000));
	spotify_change_volume(25, player_details.device.id);
	for(;;){
		// ESP_LOGI(TAG, "free heap: %d",esp_get_free_heap_size());
		vTaskDelay(pdMS_TO_TICKS(10000));
	}
}

void cb_connection_ok(void *pvParameter)
{
	internet_connection = true;	
}

void init_system()
{	
	/* start the wifi manager */
	wifi_manager_start();

	/* register a callback as an example to how you can integrate your code with the wifi manager */
	wifi_manager_set_callback(WM_EVENT_STA_GOT_IP, &cb_connection_ok);
	while(!internet_connection){
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
	spotify_init();
	vTaskDelay(pdMS_TO_TICKS(1000));
}

void app_main()
{
	init_system();

	xTaskCreate(&monitoring_task, "monitor", (2048*8), NULL, 1, NULL);
}
