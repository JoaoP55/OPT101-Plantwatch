#include <SPI.h>
#include "RF24.h"

//  NRF24 
#define CE_PIN 7
#define CSN_PIN 8

RF24 radio(CE_PIN, CSN_PIN);

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
}; Pacote pacote;

// Definição da Struct DadosGlobais, usará os dados recebidos pelos 2 emissores, fara uso apenas dos valores de temperatura e umidade do ambiente e umidade do solo
struct DadosGlobais {
  float tempAr;
  float umidAr;
  float umidSolo;
}; DadosGlobais central;


bool recebeuAr = false; // Variáveis de controle, só irá enviar os dados para a página web quando as duas varáveis forem verdadeiras(ou seja recebeu os dados dos dois arduinos emissores)
bool recebeuSolo = false;
uint64_t address = 0x3030303030LL; // Igual em todas as placas

// CARRIER SENSING
bool isCanalOcupado() { //Função de verificar se a transmissão no meio
  radio.startListening(); // Começa a ouvir
  delayMicroseconds(140); // Delay para poder começar a ouvir certinho antes de fazer a verificação
  bool ocupado = radio.testRPD(); // Guarda na variável "ocupado" se já tem algo sendo transmitido
  radio.stopListening(); // Sai do modo RX e entra no modo de transmissão TX
  return ocupado; // Retorna para quem irá chamar a função, se o meio está ou não ocupado
}

// SETUP
void setup() {
  Serial.begin(115200); // Define a faixa que será usada do monitor serial
  Serial.println("=== RECEPTOR CENTRAL ==="); // Print de Controle

  if (!radio.begin()) { // Se não encontrar módulo do NRF24, printa o erro para controle
    Serial.println("NRF24 nao encontrado!");
    while (1);
  }
  radio.setPALevel(RF24_PA_LOW);
  radio.setChannel(100); // Canal que será usado para mandar os pacotes
  radio.setPayloadSize(sizeof(Pacote)); // Seta o tamanho do PayLoad para o tamanho do struct Pacote
  radio.setAutoAck(false); // AutoAck = False
  radio.setCRCLength(RF24_CRC_16);
  radio.setDataRate(RF24_250KBPS);
  radio.openWritingPipe(address);
  radio.openReadingPipe(1, address);

  radio.startListening(); // inicia em modo RX (modo de recepção)
  delay(100); // Delay entre o tempo de início do rádio para indicar ao usuário que o rádio está ligado
  Serial.println("Aguardando sensores..."); //Print de controle, indica que o rádio iniciou, e está esperando chegar algum pacote dos arduinos com sensores
}

void loop() {
  if (radio.available()) {
    radio.read(&pacote, sizeof(pacote)); // Recebe e armazena a mensagem recebida

    if (pacote.tipo == MSG_RTS) { // Verifica o tipo da mensagem recebida (RTS)
      Serial.print("\nRTS recebido do ID "); // Print de controle
      Serial.println(pacote.id); // Print de controle, ID de quem enviou o pacote

      Pacote cts; // Cria um pacote CTS, para responder o RTS recebido
      cts.tipo = MSG_CTS; // Define o tipo do pacote que está sendo criado como resposta para CTS
      cts.id = pacote.id; // Coloca o mesmo ID
      cts.verif = 23; // E o código de verificação

      if (isCanalOcupado()) { // Faz o Carring Sensing antes de enviar o CTS
        Serial.println("Canal ocupado! CTS adiado..."); // Print de controle, caso já tenha alguém transmitindo
        delay(random(50, 150)); // Delay
        if (isCanalOcupado()) { // Tenta uma vez mais antes de desistir
          Serial.println("Canal ainda ocupado! Ignorando RTS."); // Print de controle, caso ainda tenha alguém transmitindo
          radio.startListening(); // Volta para o modo RX (receptor)
          return; // Volta para o início do loop
        }
      }

      radio.stopListening(); // Muda para o modo TX (transmissor)
      delayMicroseconds(200); // Delay
      radio.flush_tx(); // Limpa o buffer de transmissão

      bool okCTS = radio.write(&cts, sizeof(cts)); // Guarda na variável okCTS, se o pacote CTS foi enviado
      if (okCTS) {  
        Serial.println("CTS enviado"); // Se sim, print de controle
      } else {
        Serial.println("Falha CTS"); // Se não print de controle
      }
      delayMicroseconds(200); // Delay
      radio.startListening(); // Muda o modo RX (receptor)
      radio.flush_rx(); // Limpa o buffer de recepção
    }

    else if (pacote.tipo == MSG_DADOS) { // Verifica o tipo da mensagem recebida (Dados)
      Serial.print("DADOS recebidos do ID "); // Print de controle
      Serial.println(pacote.id); // Print de controle, ID de quem enviou o pacote

      if (pacote.id == 3) { // Se o ID do pacote recebido for == 3 (Sensor DTH11 (temperatura e umidade do ar/ambiente))
        central.tempAr = pacote.temperatura; // Armazena o valor da temperatura do ambiente na Struct central
        central.umidAr = pacote.umidade; // Armazena o valor da umidade de ambiente na Struct central
        recebeuAr = true; // Muda o valor da variável recebeu ar para true
      }

      else if (pacote.id == 12) { // Se o ID do pacote recebido for == 12 (Sensor umidade do solo)
        central.umidSolo = pacote.umidade; // Armazena o valor da umidade do solo na Struct central
        recebeuSolo = true; // Muda o valor da variável recebeu ar para true
      }

      Pacote ack; // Cria um pacote ACK, para responder os Dados recebidos
      ack.tipo = MSG_ACK; // Define o tipo do pacote que está sendo criado como resposta para ACK
      ack.id = pacote.id; // Coloca o mesmo ID do pacote recebido
      ack.verif = 23; // E o código de verificação

      if (isCanalOcupado()) { // Faz o Carring Sensing antes de enviar o CTS
        Serial.println("Canal ocupado! ACK adiado..."); // Print de controle, caso já tenha alguém transmitindo
        delay(random(50, 150)); // Delay
        if (isCanalOcupado()) { // Tenta uma vez mais antes de desistir
          Serial.println("Canal ainda ocupado! ACK nao enviado."); // Print de controle, caso ainda tenha alguém transmitindo
          radio.startListening(); // Volta para o modo RX (receptor)
          return;  // Volta para o início do loop
        }
      }

      //Caso o ACK seja enviado:
      radio.stopListening(); // Entra no modo TX (transmissor)
      delayMicroseconds(200); // Delay
      radio.flush_tx(); // Limpa o buffer de transmissão
      bool okACK = radio.write(&ack, sizeof(ack)); // Guarda na variável okACK, se o pacote ACK foi enviado
      if (okACK) { 
        Serial.println("ACK enviado"); // Print de controle, se o ACK for enviado
      } else {
        Serial.println("Falha ACK"); // Print de controle, se o ACK for não for enviado
      }

      delayMicroseconds(200); // Delay
      radio.startListening(); // Entra no modo de RX(recepção)
      radio.flush_rx(); // Limpa o buffer de recepção

      if (recebeuAr && recebeuSolo) { // Faz o print de controle para das variáveis que foram recebidas e armazenadas pelo Arduino Receptor. Somente quando chega os 2 pacotes do emissores
        Serial.println("\n================================");
        Serial.println(" RELATORIO CENTRAL ");
        Serial.println("================================");
        Serial.print("Temperatura Ar : ");
        Serial.print(central.tempAr);
        Serial.println(" C");
        Serial.print("Umidade Ar     : ");
        Serial.print(central.umidAr);
        Serial.println(" %");
        Serial.print("Umidade Solo   : ");
        Serial.print(central.umidSolo);
        Serial.println(" %");
        Serial.println("================================\n");
        Serial.print("{\"temp\":");
        Serial.print(central.tempAr);
        Serial.print(",\"air\":");
        Serial.print(central.umidAr);
        Serial.print(",\"soil\":");
        Serial.print(central.umidSolo);
        Serial.println("}");
        recebeuAr = false;
        recebeuSolo = false;
      }
    }
  }
}
