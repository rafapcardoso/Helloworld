/* https://github.com/sankarcheppali/esp_idf_esp32_posts/blob/master/tcp_client/main/tcp_client.c */
#include <stdio.h>
#include <string.h>    //strlen
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "nvs_flash.h"

//ADC
//https://github.com/espressif/esp-idf/blob/a0416e9351677cc3d7fe2a608bd2e54da155344e/examples/peripherals/adc/main/adc1_example_main.c
#include <driver/adc.h>
#include "esp_adc_cal.h"


#define SSID "Mi & Rafa"
#define PASSPHARSE "09092009"
#define MESSAGE "HelloTCPServer"
#define TCPServerIP "192.168.1.4"


//PARAMETROS DE CONEXAO
static int TCP_Connection;
int FLAG_Sem_Conexao;

//LEITURA DE DADOS
int FREQUENCIA = 10; //Hz

//CONVERSOR ADC
#define DEFAULT_VREF    1100        //Use adc2_vref_to_gpio() to obtain a better estimate
static esp_adc_cal_characteristics_t *adc_chars;
static const adc_channel_t channel = ADC_CHANNEL_0;     //GPIO34 if ADC1, GPIO14 if ADC2
static const adc_atten_t atten = ADC_ATTEN_DB_0;
static const adc_unit_t unit = ADC_UNIT_1;


static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
static const char *TAG="tcp_client";

void wifi_connect(){
    wifi_config_t cfg = {
        .sta = {
            .ssid = SSID,
            .password = PASSPHARSE,
        },
    };
    ESP_ERROR_CHECK( esp_wifi_disconnect() );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &cfg) );
    ESP_ERROR_CHECK( esp_wifi_connect() );
}

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    esp_log_level_set("wifi", ESP_LOG_NONE); // disable wifi driver logging
    tcpip_adapter_init();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}

void TCP_Close()
{
    close(TCP_Connection);
    ESP_LOGI(TAG, "Conexao fechada");
    //vTaskDelay(5000 / portTICK_PERIOD_MS);
}

void TCP_Enviar(char mensagem[])
{
    if( write(TCP_Connection , mensagem , strlen(mensagem)) < 0)
    {
        ESP_LOGE(TAG, "... Send failed \n");
        TCP_Close();
        /* continue; */
    }
    ESP_LOGI(TAG, "... socket send success");
}


void tcp_client(void *pvParam){
    ESP_LOGI(TAG,"tcp_client task started \n");
    struct sockaddr_in tcpServerAddr;
    tcpServerAddr.sin_addr.s_addr = inet_addr(TCPServerIP);
    tcpServerAddr.sin_family = AF_INET;
    tcpServerAddr.sin_port = htons( 23 );

    int  r;
    char recv_buf[64];
    //while(1){
        xEventGroupWaitBits(wifi_event_group,CONNECTED_BIT,false,true,portMAX_DELAY);
        TCP_Connection = socket(AF_INET, SOCK_STREAM, 0);
        if(TCP_Connection < 0) {
            ESP_LOGE(TAG, "... Failed to allocate socket.\n");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            //continue;
        }
        ESP_LOGI(TAG, "... allocated socket\n");
         if(connect(TCP_Connection, (struct sockaddr *)&tcpServerAddr, sizeof(tcpServerAddr)) != 0) {
            ESP_LOGE(TAG, "... socket connect failed errno=%d \n", errno);
            close(TCP_Connection);
            vTaskDelay(4000 / portTICK_PERIOD_MS);
            //continue;
        }
        ESP_LOGI(TAG, "... connected \n");

        TCP_Enviar("Testando Envio");

        if( write(TCP_Connection , MESSAGE , strlen(MESSAGE)) < 0)
        {
            ESP_LOGE(TAG, "... Send failed \n");
            TCP_Close();
            //continue;
        }


        do {
            bzero(recv_buf, sizeof(recv_buf));
            r = read(TCP_Connection, recv_buf, sizeof(recv_buf)-1);
            for(int i = 0; i < r; i++) {
                putchar(recv_buf[i]);
            }
        } while(r > 0);
        ESP_LOGI(TAG, "... done reading from socket. Last read return=%d errno=%d\r\n", r, errno);

        /*
        close(TCP_Connection);
        ESP_LOGI(TAG, "... new request in 5 seconds");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        */

    //}
    //ESP_LOGI(TAG, "...tcp_client task closed\n");
}

static void check_efuse()
{
    //Check TP is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        printf("eFuse Two Point: Supported\n");
    } else {
        printf("eFuse Two Point: NOT supported\n");
    }

    //Check Vref is burned into eFuse
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        printf("eFuse Vref: Supported\n");
    } else {
        printf("eFuse Vref: NOT supported\n");
    }
}

static void print_char_val_type(esp_adc_cal_value_t val_type)
{
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        printf("Characterized using Two Point Value\n");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        printf("Characterized using eFuse Vref\n");
    } else {
        printf("Characterized using Default Vref\n");
    }
}


void app_main()
{

	//==== CONVERSOR ADC INTERNO
    //Check if Two Point or Vref are burned into eFuse
    check_efuse();
    //Configure ADC
	adc1_config_width(ADC_WIDTH_BIT_12);
	adc1_config_channel_atten(channel, atten);
    //Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);
    print_char_val_type(val_type);


	//==== CONEXAO WIFI
	FLAG_Sem_Conexao = 1;

    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_event_group = xEventGroupCreate();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK( ret );
    initialise_wifi();

    //==== CONEXAO TCP
    xTaskCreate(&tcp_client,"tcp_client",4048,NULL,5,NULL);

    //Main Loop
    int i = 0;

    while(1){

    	//Verificar conexao TCP
        while(TCP_Connection < 0) {
        	//Tentar conexao até obter sucesso para continuar
            ESP_LOGI(TAG,"Tentando re-conexao TCP...");
        	xTaskCreate(&tcp_client,"tcp_client",4048,NULL,5,NULL);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }

        //Continuar somente se existir conexao
        if(TCP_Connection > 0) {

        	//MEDIR TENSAO
			uint32_t adc_reading = 0;
			adc_reading = adc1_get_raw((adc1_channel_t)channel);

			//Convert adc_reading to voltage in mV
			uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
			printf("Raw: %d\tVoltage: %dmV\n", adc_reading, voltage);
			vTaskDelay(pdMS_TO_TICKS(1000));


            char *str1 = "Tensao";

            char seq[10];
            sprintf(seq, "%d", i);

            char *str2 = ": ";

            char volt[18];
            sprintf(volt, "%d", voltage);

            char * str3 = (char *) malloc(1 + strlen(str1)+ strlen(seq)+strlen(str2)+strlen(volt) );
            strcpy(str3, str1);
            strcat(str3, seq);
            strcat(str3, str2);
            strcat(str3, volt);
            printf("%s", str3);

        	TCP_Enviar(str3);
        	TCP_Enviar("\n");
        }
        i++;
        vTaskDelay(1000 / FREQUENCIA);
    }

    //TCP_Close();

}
