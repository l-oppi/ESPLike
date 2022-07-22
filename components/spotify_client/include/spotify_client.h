#pragma once

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"

#include "esp_http_client.h" 
#include "esp_tls.h"
#include "time_manager.h"
#include "cJSON.h"

#define MAX_SONG_TITLE_LENGTH       (64U)
#define MAX_SONG_ID_LENGTH          (22U)
#define MAX_PLAYLIST_ID_LENGTH      (22U)
#define MAX_PLAYLIST_NAME_LENGTH    (16U)
#define MAX_ARTIST_NAME_LENGTH      (64U)

#define SPOTIFY_HOST "api.spotify.com"
#define SPOTIFY_ACCOUNTS_HOST "accounts.spotify.com"

// Fingerprint for "*.spotify.com" as of May 17th, 2022
#define SPOTIFY_FINGERPRINT "4A 44 71 F7 6A 8D D4 BD 54 E9 0E 3D E8 6C A6 E0 00 27 BA D5"

// Fingerprint for "*.scdn.co" as of May 17th, 2022
#define SPOTIFY_IMAGE_SERVER_FINGERPRINT "08 9F D9 6B 46 93 45 43 29 07 BE AD 76 CF 6B 44 3B AE 95 F6"

#define SPOTIFY_TIMEOUT 200
#define SPOTIFY_TOKEN_TIMEOUT_SEC 3600

#define SPOTIFY_NAME_CHAR_LENGTH 100
#define SPOTIFY_URI_CHAR_LENGTH 40
#define SPOTIFY_URL_CHAR_LENGTH 70

#define SPOTIFY_DEVICE_ID_CHAR_LENGTH 45
#define SPOTIFY_DEVICE_NAME_CHAR_LENGTH 80
#define SPOTIFY_DEVICE_TYPE_CHAR_LENGTH 30

#define SPOTIFY_TOKEN_ENDPOINT "/api/token"
#define SPOTIFY_CURRENTLY_PLAYING_ENDPOINT "/v1/me/player/currently-playing"
#define SPOTIFY_PLAYER_ENDPOINT "/v1/me/player"
#define SPOTIFY_DEVICES_ENDPOINT "/v1/me/player/devices"
#define SPOTIFY_PLAY_ENDPOINT "/v1/me/player/play"
#define SPOTIFY_SEARCH_ENDPOINT "/v1/search"
#define SPOTIFY_PAUSE_ENDPOINT "/v1/me/player/pause"
#define SPOTIFY_VOLUME_ENDPOINT "/v1/me/player/volume?volume_percent="
#define SPOTIFY_SHUFFLE_ENDPOINT "/v1/me/player/shuffle?state="
#define SPOTIFY_REPEAT_ENDPOINT "/v1/me/player/repeat?state="
#define SPOTIFY_NEXT_TRACK_ENDPOINT "/v1/me/player/next"
#define SPOTIFY_PREVIOUS_TRACK_ENDPOINT "/v1/me/player/previous"
#define SPOTIFY_TRACKS_ENDPOINT "/v1/me/tracks"
#define SPOTIFY_SEEK_ENDPOINT "/v1/me/player/seek"

#define SPOTIFY_NUM_ALBUM_IMAGES 3
#define SPOTIFY_MAX_NUM_ARTISTS 5

#define SPOTIFY_ACCESS_TOKEN_LENGTH 309

typedef enum repeat_options_t
{
  repeat_track,
  repeat_context,
  repeat_off
} repeat_options_t;

typedef struct spotify_image_t
{
  int height;
  int width;
  const char *url;
} spotify_image_t;

typedef struct spotify_device_t
{
  const char *id;
  const char *name;
  const char *type;
  bool is_active;
  bool is_restricted;
  bool is_private_session;
  int volumePercent;
} spotify_device_t;

typedef struct player_details_t
{
  spotify_device_t device;
  long progress_ms;
  bool is_playing;
  repeat_options_t repeate_state;
  bool shuffle_state;
} player_details_t;

typedef struct spotify_artist_t
{
  const char *artist_name;
  const char *artist_uri;
} spotify_artist_t;

typedef struct search_result_t
{
  const char *album_name;
  const char *album_uri;
  const char *track_name;
  const char *track_uri;
  spotify_artist_t artists[SPOTIFY_MAX_NUM_ARTISTS];
  spotify_image_t album_images[SPOTIFY_NUM_ALBUM_IMAGES];
  int num_artists;
  int num_images;
} search_result_t;

typedef struct currently_playing_t
{
  spotify_artist_t artists[SPOTIFY_MAX_NUM_ARTISTS];
  int num_artists;
  const char *album_name;
  const char *album_uri;
  const char *track_name;
  const char *track_uri;
  spotify_image_t album_images[SPOTIFY_NUM_ALBUM_IMAGES];
  int num_images;
  bool is_playing;
  long progress_ms;
  long duration_ms;
} currently_playing_t;

typedef struct spotify_access_t
{
  bool is_fresh;
  char client_id[128];
  char client_secret[128];
  char refresh_token[256];
  uint32_t token_expiration_time;
  char access_token[256];
} spotify_access_t;

void spotify_init(void);
bool spotify_refresh_access_token(void);
bool spotify_get_player_details(player_details_t *player_details);
