#include "DHT.h"
#include <SPI.h>
#include "RF24.h"

// DHT
#define DHTPIN 2
#define DHTTYPE DHT11

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

DHT dht(DHTPIN, DHTTYPE);
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
  Serial.println("=== TRANSMISSOR AR (COM CARRIER SENSE) ==="); // Print de Controle
  dht.begin(); // Inicia o sensor DTH
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
  float h = dht.readHumidity(); // Faz a leitura de umidade do ambiente com o sensor DTH11 e guarda na váriavel "h"
  float t = dht.readTemperature(); // Faz a leitura de temperatura do ambiente com o sensor DTH11 e guarda na váriavel "t"

  if (isnan(h) || isnan(t)) { // Faz o controle da captura dos valores de "h" e "t"
    Serial.println("Erro DHT11");
    delay(2000);
    return;
  }

  // Cria o pacote que será enviado
  dados.tipo = MSG_DADOS; // Tipo do pacote (dados)
  dados.id = 3; // ID de controle (número do arduino)
  dados.verif = 23; // Outro número de controle (2 formas de controle de pacote)
  dados.temperatura = t; // Guarda o valor de temperatura no pacote
  dados.umidade = h; // Guarda o valor de umidade no pacote

  // ENVIANDO RTS COM CARRIER SENSE
  Pacote rts; // Pacote RTS
  rts.tipo = MSG_RTS; // Tipo do pacote (rts)
  rts.id = dados.id; // ID de controle (número do arduino)
  rts.verif = 23; // Outro número de controle (2 formas de controle de pacote)

  Serial.println("\nChecando canal para enviar RTS..."); // Print de controle do Carrier Sense
  if (isCanalOcupado()) { // Chama a função isCanalOcupado (Carrier Sensing), para verificar se o meio estpa ocupado e recebe o resultado em bool (true e false)
    Serial.println("[CS] Canal ocupado! Abortando RTS."); // Print de controle (Caso o canal esteja ocupado)
    delay(random(100, 400)); // Espera um tempo aleatório, porém ele só tentará fazer o envio do RTS novamente quando executar a função loop novamente
    return;
  }

  // (isCanalOcupado() deixa o rádio em stopListening = modo TX)
  radio.flush_tx(); // Limpar o buffer de transmissão
  radio.flush_rx(); // Limpar o buffer de recepção 
  delayMicroseconds(200); // Delay antes de transmitir

  Serial.println("Canal livre. Enviando RTS..."); // Print de controle, para envio do RTS
  bool okRTS = radio.write(&rts, sizeof(rts)); // Guarda na variável okRTS se o pacote RTS foi enviado
  if (!okRTS) {
    Serial.println("Falha fisica RTS"); // Print de controle caso pacote RTS não seja enviado
    delay(random(300, 1000)); // Espera um tempo 
    return; // Começa do ínicio do loop
  }

  // AGUARDANDO CTS
  radio.flush_rx(); // Limpa o buffer de recepção
  radio.startListening(); // Começa a ouvir

  bool recebeuCTS = false; // Define a variável recebeuCTS para false (controle)
  unsigned long inicio = millis(); // Salva o momento de ínicio da espera do CTS

  while (millis() - inicio < 1000) { // (millis() - inicio) é o tempo que já passou desde que começou a esperar o CTS, quando isso ultrapassa 1000ms (1 segundo) sem receber resposta, o while termina e cai no Timeout CTS.
    if (radio.available()) {
      Pacote resposta; // Cria o Pacote resposta
      radio.read(&resposta, sizeof(resposta)); // Escreve a resposta recebida no Pacote resposta

      if (resposta.tipo == MSG_CTS && // Faz o tratamento do pacote recebido, verifica o tipo do pacote recebido
          resposta.id   == dados.id && // Verifica o ID do pacote recebido
          resposta.verif == 23) { // Verifica o número de verificação do pacote recebido
        recebeuCTS = true; // Muda a variável recebeuCTS para true, indicando que o CTS foi recebido
        break; // Sai do while (millis() - inicio < 1000)
      }
    }
  }

  if (!recebeuCTS) { //Controle, caso não tenha recebido o CTS
    Serial.println("Timeout CTS"); // Print de controle
    radio.stopListening(); // Para de ouvir 
    delay(random(300, 1000)); // Delay
    return; // Volta ao início do loop
  }

  Serial.println("CTS recebido"); // Print de controle

  // ENVIANDO DADOS 
  radio.stopListening(); // Sai do modo RX para poder transmitir
  radio.flush_tx(); // Limpa do buffer de transmissão
  delayMicroseconds(200); // Delay

  Serial.println("Enviando dados..."); // Print de controle, envio dos dados
  bool okDados = radio.write(&dados, sizeof(dados)); // Guarda na variável okDados se o Pacote dados foi enviado
  if (!okDados) { // Se o Pacote dados não foi enviado
    Serial.println("Falha envio dados"); // Print de controle
    delay(random(300, 1000)); // Delay
    return; // Volta ao início do loop
  }

  // AGUARDANDO ACK 
  radio.flush_rx(); // Limpa o buffer de recepção
  radio.startListening(); // Começa a ouvir

  bool recebeuACK = false; // Variável de controle de recebimento do ACK
  inicio = millis(); // Salva o momento de ínicio da espera do ACK

  while (millis() - inicio < 1000) { // millis() - inicio é o tempo que já passou desde que começou a esperar o ACK, quando isso ultrapassa 1000ms (1 segundo) sem receber resposta, o while termina e cai no Timeout ACK.
    if (radio.available()) {
      Pacote resposta; // Cria o Pacote resposta
      radio.read(&resposta, sizeof(resposta)); // Guarda no Pacote resposta os dados que foram recebidos

      if (resposta.tipo  == MSG_ACK && // Faz o tratamento do pacote recebido, verifica o tipo do pacote recebido
          resposta.id    == dados.id && // Verifica o ID do pacote recebido
          resposta.verif == 23) { // Verifica o número de verificação do pacote recebido
        recebeuACK = true; // Muda a variável recebeuACK para true, indicando que o ACK foi recebido
        break; // Sai do while (millis() - inicio < 1000)
      }
    }
  }

  radio.stopListening(); // Sai do modo RX

  if (recebeuACK) { // Se receber o ACK
    Serial.println("ACK recebido!"); // Print de controle
    Serial.print("Temperatura: "); // Print de controle
    Serial.println(t); // Print de controle
    Serial.print("Umidade: "); // Print de controle
    Serial.println(h); // Print de controle
  } else {
    Serial.println("Timeout ACK"); // Print de controle, caso não receba o ACK
  }

  delay(5000); // Delay

} // Volta para o ínicio do loop
