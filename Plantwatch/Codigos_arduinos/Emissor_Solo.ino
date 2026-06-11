#include <SPI.h>
#include "RF24.h"

// Sensor do Solo 
#define PINO_ANALOGICO_SOLO A0
#define PINO_DIGITAL_SOLO 2

// NRF24
#define CE_PIN 7
#define CSN_PIN 8

// Tipos de mensagens para o MACAW
#define MSG_RTS   1
#define MSG_CTS   2
#define MSG_DADOS 3
#define MSG_ACK   4

// Definição da Struct Pacote, que será usado como a mensagem a ser enviada e recebida
struct Pacote {
  uint8_t tipo;
  uint8_t id;
  uint8_t verif;
  float temperatura;
  float umidade;
}; Pacote dados;


RF24 radio(CE_PIN, CSN_PIN);
uint64_t address = 0x3030303030LL; // Igual em todas as placas

// CARRIER SENSING
bool isCanalOcupado() { //Função de verificar se a transmissão no meio
  radio.startListening(); // Começa a ouvir
  delayMicroseconds(140); // Delay para poder começar a ouvir certinho antes de fazer a verificação
  bool ocupado = radio.testRPD(); // Guarda na variável "ocupado" se já tem algo sendo transmitido
  radio.stopListening(); // Sai do modo RX e entra no modo de transmissão TX
  return ocupado; // Retorna para quem irá chamar a função, se o meio está ou não ocupado
}

void setup() {
  Serial.begin(115200); // Define a faixa que será usada do monitor serial
  Serial.println("=== TRANSMISSOR SOLO ==="); // Print de Controle
  pinMode(PINO_ANALOGICO_SOLO, INPUT);
  pinMode(PINO_DIGITAL_SOLO, INPUT);
  randomSeed(analogRead(A2));

  if (!radio.begin()) { // Se não encontrar módulo do NRF24, printa o erro para controle
    Serial.println("NRF24 nao encontrado!");
    while (1);
  }
  radio.setPALevel(RF24_PA_LOW);
  radio.setChannel(37); // Canal que será usado para mandar os pacotes
  radio.setPayloadSize(sizeof(Pacote)); // Seta o tamanho do PayLoad para o tamanho do struct Pacote
  radio.setAutoAck(false); // AutoAck = False
  radio.setCRCLength(RF24_CRC_16);
  radio.setDataRate(RF24_250KBPS);
  radio.openWritingPipe(address);
  radio.openReadingPipe(1, address);

  radio.stopListening(); // inicia em modo TX (modo de transmissão)
  delay(100); // Delay entre o tempo de início do rádio para indicar ao usuário que o rádio está ligado
  Serial.println("Radio iniciado!"); //Print de controle, indica que o rádio iniciou
}
void loop() {
  int leitura = analogRead(PINO_ANALOGICO_SOLO); // Lê o valor do sensor do solo
  float umidadeSolo = map(leitura, 1023, 200, 0, 100); // Converte o valor para porcentagem

  if (umidadeSolo < 0) umidadeSolo = 0; // Garantir que os valor continuem em 0%
  if (umidadeSolo > 100) umidadeSolo = 100; // Garantir que os valor continuem em 100%

  dados.tipo = MSG_DADOS;
  dados.id = 12;
  dados.verif = 23;
  dados.temperatura = 0;
  dados.umidade = umidadeSolo;

  Pacote rts;
  rts.tipo = MSG_RTS;
  rts.id = dados.id;
  rts.verif = 23;

  // ENVIO RTS
  Serial.println("\nChecando canal...");
  if (isCanalOcupado()) {
    Serial.println("Canal ocupado! Aguardando...");
    delay(random(100, 400));
    return;
  }

  radio.stopListening();
  delayMicroseconds(200);
  radio.flush_tx();
  radio.flush_rx();
  Serial.println("Canal livre. Enviando RTS...");
  bool okRTS = radio.write(&rts, sizeof(rts));
  if (!okRTS) {
    Serial.println("Falha RTS");
    delay(random(300,1000));
    return;
  }

  // AGUARDANDO CTS
  radio.startListening();
  delayMicroseconds(200);
  radio.flush_rx();
  bool recebeuCTS = false;
  unsigned long inicio = millis();
  while (millis() - inicio < 1000) {
    if (radio.available()) {
      Pacote resposta;
      radio.read(&resposta, sizeof(resposta));
      if (resposta.tipo == MSG_CTS &&
          resposta.id == dados.id) {
        recebeuCTS = true;
        break;
      }
    }
  }
  if (!recebeuCTS) {
    Serial.println("Timeout CTS");
    radio.stopListening();
    delay(random(300,1000));
    return;
  }
  Serial.println("CTS recebido");

  // ENVIO DOS DADOS
  radio.stopListening();
  delayMicroseconds(200);
  radio.flush_tx();
  Serial.println("Enviando dados...");
  bool okDados = radio.write(&dados, sizeof(dados));
  if (!okDados) {
    Serial.println("Falha envio dados");
    delay(random(300,1000));
    return;
  }

  // ESPERA DO ACK
  radio.startListening();
  delayMicroseconds(200);
  radio.flush_rx();

  bool recebeuACK = false;
  inicio = millis();
  while (millis() - inicio < 1000) {
    if (radio.available()) {
      Pacote resposta;
      radio.read(&resposta, sizeof(resposta));
      if (resposta.tipo == MSG_ACK &&
          resposta.id == dados.id) {
        recebeuACK = true;
        break;
      }
    }
  }
  radio.stopListening();
  if (recebeuACK) {
    Serial.println("ACK recebido!");
    Serial.print("Umidade Solo: ");
    Serial.print(umidadeSolo);
    Serial.println(" %");
  } else {
    Serial.println("Timeout ACK");
  }
  delay(3000);
}
