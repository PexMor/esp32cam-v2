/* SPDX-License-Identifier: MIT */

#define HTTP_HOST             "srv.iot"
// using the unencrypted http !!!
#define HTTP_PORT             80
#define HTTP_PATH             "/save/"

// Configure the values below
// #define SSID                  "<wifi-ssid>"
// #define SSID_PW               "<wifi-password>"

#ifndef SSID
#error "Please set the SSID (name of the Wi-Fi network )"
#endif

#ifndef SSID_PW
#error "Please set the SSID_PW (password for Wi-Fi network)"
#endif

// mime frame magic number and headers for jpg
#define MIME_BOUNDARY "405e903e-4f49-4933-91f1-76fb6cae1d23"
#define VER "1.0-2021/06/09"
#define ID "catcam"
