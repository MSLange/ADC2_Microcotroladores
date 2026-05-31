#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "lwip/apps/mqtt.h"
#include "bsp/board.h"
#include <string.h>
#include "hardware/adc.h"

//Tópico MQTT para envio da temperatura
#define TOPICO_TEMPERATURA "/Mateus/240025564/temperatura"
//Tópico MQTT para controle do LED
#define TOPICO_LED "/Mateus/240025564/led"

#define END_MQTT "35.172.255.228" //emqx broker mqtt
//#define END_MQTT "54.36.178.49" //mosquitto broker mqtt

/*
 * Estrutura para armazenar as informações do cliente MQTT
 * identificador, usuário e senha...
 */
struct mqtt_connect_client_info_t info_cliente =
{
    "",   /Identificador do Usuário/
    NULL, /Usuário/
    NULL, /Senha/
    0,    /Tempo de keep alive/
    NULL, /Tópico do Ultimo desejo/
    NULL, /Mensagem do Ultimo desejo/
    0,    /Quality Of Service do ultimo desejo/
    0     /Ultimo Desejo Retentivo/
};


/*
 * Callback (Evento/Interrupção) mqtt_dados_recebidos_cb, utilizada para realizar a leitura dos 
 * dados recebidos através dos tópicos MQTT
 * ----- Parâmetros -----
 * 
 * void *arg -> dados genéricos definidos pelo usuário, podem ser qualquer coisa, para ver onde esses
 * são definidos procure a referência REF01, neste código
 * 
 * const u8_t *dados -> são explicitamente os dados recebidos em forma de texto ASCII, é um buffer
 * contínuo de dados, deve ser lido da posição 0 até o comprimento indicado no parâmetro 'comprimento'
 * 
 * u16_t comprimento -> é o tamanho dos dados recebidos pelo MQTT, delimita a leitura do Buffer 'dados'
 * 
 * u8_t flags -> sinalizam bits de controle referentes a mensagem MQTT, raramente utilizados emprojetos
 * simples como é o caso
 * 
 */
static void mqtt_dados_recebidos_cb(void *arg,
                                    const uint8_t *dados,
                                    uint16_t tamanho,
                                    uint8_t flags){

    char buffer[20];
    
    //Copia os dados recebidos para uma string
    memcpy(buffer, dados, tamanho);
    buffer[tamanho] = '\0';

    printf("recebendo dados: %s\n", buffer);

    //Converte a mensagem recebida para inteiro
    int valor = atoi(buffer);

    if(valor > 0){
        //Define o intervalo de piscagem do led
        intervalo_led = valor;
        printf("Intervalo LED: %d segundos\n", intervalo_led);
    }
    else{
        //Desliga o led
        intervalo_led = 0;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
        printf("LED desligado\n");
    }
}


/*
 * Callback (Evento/Interrupção) mqtt_recebendo_publicacao_cb, sinaliza em qual tópico os dados
 * serão recebidos/transmitidos.
 * ----- Parâmetros -----
 *
 * void *arg -> dados genéricos definidos pelo usuário, podem ser qualquer coisa, para ver onde esses
 * são definiddos procure a referência REF01, neste código
 * 
 * const char *topico -> string terminada em 0, contendo o nome do tópico qu está transmitindo/recebendo
 * dados.
 * 
 * u32_t tamanho -> tamanho da string 'tópico', utilizada apenas em situações de manipulação de strings
 * para evitar estouro de pilha.
 * 
 */
static void mqtt_recebendo_publicacao_cb(void *arg, const char *topico, uint32_t tamanho){
    printf("recebendo mensagem no topico %s\n", topico);
}

/*
 * Callback (Evento/Interrupção) mqtt_requisicao_cb, sinaliza as requisições de recepção/envio de dados via MQTT 
 * serão recebidos/transmitidos.
 * ----- Parâmetros -----
 * 
 * void *arg -> dados genéricos definidos pelo usuário, podem ser qualquer coisa, para ver onde esses
 * são definiddos procure a referência REF01, neste código
 * 
 * err_t erro -> parâmetro responsável pela sinalização do status de secusso/erro da requisição
 */
static void mqtt_requisicao_cb(void *arg, err_t error){
    printf("recebendo requisição %d\n", error);
}

static bool connectado_com_sucesso = false;
//Intervalo de piscagem do LED (segundos)
int intervalo_led = 0;
/*
 * Callback (Evento/Interrupção) mqtt_conectado_cb, sinaliza o momento em que o broker ("servidor") MQTT
 * aceita a conexão cliente (placa).
 * ----- Parâmetros -----
 * mqtt_client_t *cliente -> Estrutura que representa o cliente (placa), que foi conectado ao servidor;
 * 
 * void *arg -> dados genéricos definidos pelo usuário, podem ser qualquer coisa, para ver onde esses
 * são definiddos procure a referência REF01, neste código
 * 
 * mqtt_connection_status_t status-> parâmetro que contém o status da conexão do MQTT.
 * 
 */

//Executada quando a Pico conecta ao broker MQTT
static void mqtt_conectado_cb(mqtt_client_t *cliente, void *arg, mqtt_connection_status_t status) {
    printf("Cliente MQTT conectado com o status %d\n", status);

    if (status == MQTT_CONNECT_ACCEPTED) {
        printf("Conexão MQTT aceita\n");
        connectado_com_sucesso = true;
        //Inscreve a placa no tópico de controle do LED 
        mqtt_sub_unsub(cliente, TOPICO_LED,
                        0,
                       mqtt_requisicao_cb,
                       NULL,
                       1);
    }
}

float ler_temperatura() {
    //Leitura do ADC
    uint16_t valor = adc_read();
    //Conversão para tensão
    const float conversao = 3.3f / (1 << 12);

    float tensao = valor * conversao;
    //Fórmula fornecida pela Raspberry Pi
    float temperatura =
        27.0f - (tensao - 0.706f) / 0.001721f;

    return temperatura;
}

int main()
{
    stdio_init_all();
    sleep_ms(5000);
    
    // Initialise the Wi-Fi chip
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed\n");
        return -1;
    }

    // Ligando o Wifi como cliente 
    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wifi\n");

    if(cyw43_arch_wifi_connect_timeout_ms("mateus_", "00550066", CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("Erro ao conectar ao wifi\n");
        return -1;
    } else {
        printf("Conectado!!!!\n");
        uint8_t end_ip = (uint8_t) &(cyw43_state.netif[0].ip_addr.addr);
        printf("Endereço IP: %d.%d.%d.%d\n", end_ip[0], end_ip[1], end_ip[2], end_ip[3]);
    }

    /*
    * Callback (Evento/Interrupção) mqtt_conectado_cb, sinaliza o momento em que o broker ("servidor") MQTT
    * aceita a conexão cliente (placa).
    * ----- Parâmetros -----
    * mqtt_client_t *cliente -> Estrutura que representa o cliente (placa), que foi conectado ao servidor;
    * 
    * void *arg -> dados genéricos definidos pelo usuário, podem ser qualquer coisa, para ver onde esses
    * são definiddos procure a referência REF01, neste código
    * 
    * mqtt_connection_status_t status-> parâmetro que contém o status da conexão do MQTT.
    * 
    */
    ip_addr_t end_mqtt;
    ip4addr_aton(END_MQTT, &end_mqtt);

    mqtt_client_t *mqtt_cliente = mqtt_client_new();

    mqtt_set_inpub_callback(mqtt_cliente, &mqtt_recebendo_publicacao_cb, &mqtt_dados_recebidos_cb, NULL);

    err_t houve_erro = mqtt_client_connect(mqtt_cliente, &end_mqtt, 1883, &mqtt_conectado_cb, NULL, &info_cliente);

    if (houve_erro != ERR_OK){
        printf("Falha na requisião de conexão MQTT\n");
        return 1;
    }

    // Example to turn on the Pico W LED
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);

    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);

    while (true) {
    //Mantém a pilha Wi-Fi/MQTT funcionando 
    cyw43_arch_poll();

    if(board_button_read()) {

        if(connectado_com_sucesso) {
            //Lê a temperatura
            float temperatura = ler_temperatura();

            char mensagem[50];
            //Monta a mensagem
            sprintf(mensagem,
                    "Temperatura: %.2f C",
                    temperatura);

            printf("%s\n", mensagem);
            //Publica no tópico MQTT
            mqtt_publish(
                mqtt_cliente, TOPICO_TEMPERATURA,
                mensagem,
                strlen(mensagem),
                0,
                0,
                mqtt_requisicao_cb,
                NULL
            );
        }

        sleep_ms(300);
    }

    static absolute_time_t ultimo_toggle;
    static bool estado_led = false;
    //Se recebeu um intervalo válido
    if(intervalo_led > 0) {
        //Verfica se chegou no momento de alternar
        if(absolute_time_diff_us(
                ultimo_toggle,
                get_absolute_time())
            >= intervalo_led * 1000000) {
            //Inverte o estado do led
            estado_led = !estado_led;

            cyw43_arch_gpio_put(
                CYW43_WL_GPIO_LED_PIN,
                estado_led
            );
            //Atualiza o instate da última troca
            ultimo_toggle = get_absolute_time();
        }
    }

    sleep_ms(10);
}