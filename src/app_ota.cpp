#include "app_ota.hpp"
#include <Arduino.h>
#include "app.hpp"

#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include <string>
#include <stdlib.h>

extern String OTA_URL;

char cert[] = 
"-----BEGIN CERTIFICATE-----\r\n"
"MIIEUzCCAzugAwIBAgIJAJCeuK5zlTyeMA0GCSqGSIb3DQEBCwUAMIG/MQswCQYD\r\n"
"VQQGEwJJTjEUMBIGA1UECAwLTWFoYXJhc2h0cmExDTALBgNVBAcMBFB1bmUxJTAj\r\n"
"BgNVBAoMHEF1dG9wZWVwYWwgU2VydmljZSBTb2x1dGlvbnMxHzAdBgNVBAsMFklu\r\n"
"Zm9ybWF0aW9uIFRlY2hub2xvZ3kxFjAUBgNVBAMMDTE1OS44OS4xNjcuNzIxKzAp\r\n"
"BgkqhkiG9w0BCQEWHHVqandhbC5iaXN3YXNAYXV0b3BlZXBhbC5jb20wHhcNMjEw\r\n"
"OTA4MTIyMzQ4WhcNMjIwOTA4MTIyMzQ4WjCBvzELMAkGA1UEBhMCSU4xFDASBgNV\r\n"
"BAgMC01haGFyYXNodHJhMQ0wCwYDVQQHDARQdW5lMSUwIwYDVQQKDBxBdXRvcGVl\r\n"
"cGFsIFNlcnZpY2UgU29sdXRpb25zMR8wHQYDVQQLDBZJbmZvcm1hdGlvbiBUZWNo\r\n"
"bm9sb2d5MRYwFAYDVQQDDA0xNTkuODkuMTY3LjcyMSswKQYJKoZIhvcNAQkBFhx1\r\n"
"amp3YWwuYmlzd2FzQGF1dG9wZWVwYWwuY29tMIIBIjANBgkqhkiG9w0BAQEFAAOC\r\n"
"AQ8AMIIBCgKCAQEA4Cl3kJTVd/lqE3v7Sx+jjCDnhj2in0Y5oPLiDfClXETjjFgA\r\n"
"OaPTMT9d21h2l+QPgRnjOHQvpYo5D8z6rr7q0itgp6h/nob0VUibQ442FVChnnM4\r\n"
"46QXpJl0KAI0ngVaMVAFv7/azCS5imL/qVz10UHsgNhgw9FP6HxaWyd7ihosAWmi\r\n"
"a0jFZj1BU+k/uvyC0XmQ69cfuKp3WdlSMoyBieWnXGUXr18F0xd0UE20vXrZYQfn\r\n"
"kxO28IK+jmU1EAM4ZPRWWt5k1eOLHJhD6lQsgRAwaGmWmd1+GC/35Q0IX4uF91Ev\r\n"
"3WuwebIUGXzuTembRAQKArZ+TSVPXPPa65INKQIDAQABo1AwTjAdBgNVHQ4EFgQU\r\n"
"jmyqMg9PvUo5q0PmIIOApNEnI7IwHwYDVR0jBBgwFoAUjmyqMg9PvUo5q0PmIIOA\r\n"
"pNEnI7IwDAYDVR0TBAUwAwEB/zANBgkqhkiG9w0BAQsFAAOCAQEAs7C7ZWxO+Q1W\r\n"
"oau2m1KCFYa3FtMqTECtqpGP/0otrMIzQ5p5Ew/G/ChYAImw5DB/cHsb/HCjLCPK\r\n"
"5UB6YJaiK2MQYzRUv1bYrUHcwBzmoeYeHlJ/9/r83U6dkxKgWs/ANrAKRmYZidnC\r\n"
"bxklOkyrtZuUzP/ek56rh2m+O865VG7iGLp0EHJhKk7vmsBzyAPB3Y+KiCj9+WCE\r\n"
"4q+Eylhog9C3zeJ4+Fj/vGroynxY66DVSauWgcDir3iy8U8M24QXkWDK9ifjB396\r\n"
"4mM7tKezdcKAzzJFxIxFKG9vZo4lXHS/iwVN6F3fOnKeGQskMeTCkF3a88z7K0on\r\n"
"PXo/U+7asg==\r\n"
"-----END CERTIFICATE-----\r\n";

size_t total_size, percent, idx = 0;


esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
  switch (evt->event_id)
  {
  case HTTP_EVENT_ERROR:
    log_e("Server Donwnload ERROR");
    APP_SendRespToFrame(1, 1, NULL, 0, COM_CHANNEL);
    break;
  case HTTP_EVENT_ON_CONNECTED:
    // Serial.println("HTTPS OTA Start...");
    APP_SendRespToFrame(0, 0, NULL, 0, COM_CHANNEL);
    break;
  case HTTP_EVENT_ON_HEADER:
    if (strcmp("Content-Length", evt->header_key) == 0)
    {
      total_size = atoi(evt->header_value);
      // Serial.printf("Content-Length: %d\r\n", total_size);
    }
    break;
  case HTTP_EVENT_ON_DATA:
    idx += evt->data_len;
    percent = (idx * 100) / total_size;
    // Serial.printf("Progress: %d\r\n", percent);
    break;
    default:
    break;
  }
  return ESP_OK;
}

void ota_task(void *args)
{
  esp_http_client_config_t config;
  config.url = OTA_URL.c_str();
  config.cert_pem = cert;
  config.event_handler = _http_event_handler;
  config.skip_cert_common_name_check = true;

  esp_err_t ret = esp_https_ota(&config);

  if (ret == ESP_OK)
  { 
    APP_SendRespToFrame(0, 0, NULL, 0, COM_CHANNEL);  
  }
  else
    APP_SendRespToFrame(1, 1, NULL, 0, COM_CHANNEL);

  vTaskDelay(2000 / portTICK_PERIOD_MS);
  esp_restart();

  while (1)
  {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}
