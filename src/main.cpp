#include "app.hpp"

void setup()
{
    Serial.begin(115200);
    Serial.setRxBufferSize(4100);
    Serial.println();
    esp_log_level_set("*", ESP_LOG_NONE);

    // WiFi.begin("Autopeepal", "autopeepal@2021");
    // while (WiFi.status() != WL_CONNECTED)
    // {
    //     vTaskDelay(pdMS_TO_TICKS(500));
    // }
    // DATA_COM.println(WiFi.localIP());

    if (xTaskCreate(UART_Task, "UART_Task", 1024*4, NULL, tskIDLE_PRIORITY + 3, NULL) != pdTRUE)
    {
        DATA_COM.println("Failed to Create UART_Task");
    }

    // if (xTaskCreate(rtl_interface_Task, "rtl_interface_Task", 1024*4, NULL, tskIDLE_PRIORITY + 3, NULL) != pdTRUE)
    // {
    //     DATA_COM.println("Failed to Create rtl_interface_Task");
    // }

    // if (xTaskCreate(WIFI_Task, "WIFI_Task", 1024*8, NULL, tskIDLE_PRIORITY + 5, NULL) != pdTRUE)
    // {
    //     DATA_COM.println("Failed to Create WIFI_Task");
    // }

    if (xTaskCreate(eth_Task, "eth_Task", 1024*10, NULL, tskIDLE_PRIORITY + 5, NULL) != pdTRUE)
    {
        DATA_COM.println("Failed to Create eth_Task");
    }

    if (xTaskCreate(CAN_TASK, "CAN_Task", 1024 * 4, NULL, tskIDLE_PRIORITY + 6, NULL) != pdTRUE)
    {
        DATA_COM.println("Failed to Create CAN_Task");
    }

    if (xTaskCreate(IO_OPERATION_TASK, "IO_OPERATION_TASK", 1024 * 2, NULL, tskIDLE_PRIORITY + 1, NULL) != pdTRUE)
    {
        DATA_COM.println("Failed to Create IO_OPERATION_TASK");
    }
}

void loop()
{
    vTaskDelay(500 / portTICK_PERIOD_MS);
    vTaskDelete(NULL);
}