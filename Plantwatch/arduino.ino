/*
  PlantWatch - Leitura de Sensores
  ================================
  Sensores utilizados:
    - DHT22: temperatura e umidade do ar
    - Sensor resistivo de umidade do solo (analógico)

  Ligações:
    DHT22  → pino digital 2
    Solo   → pino analógico A0

  Saída Serial (9600 baud):
    Envia JSON a cada 2 segundos no formato:
    {"temp":25.4,"air":60.2,"soil":45}
*/

#include <DHT.h>

#define DHTPIN 2
#define DHTTYPE DHT22
#define SOIL_PIN A0

DHT dht(DHTPIN, DHTTYPE);

void setup() {
  Serial.begin(9600);
  dht.begin();
}

void loop() {
  delay(2000);

  float temp = dht.readTemperature();
  float air  = dht.readHumidity();

  // Leitura do solo: 0 (molhado) a 1023 (seco)
  // Convertemos para 0-100% (100 = muito úmido)
  int raw  = analogRead(SOIL_PIN);
  int soil = map(raw, 1023, 0, 0, 100);

  // Verifica leitura válida do DHT
  if (isnan(temp) || isnan(air)) {
    Serial.println("{\"error\":\"DHT read failed\"}");
    return;
  }

  // Envia JSON pela serial
  Serial.print("{\"temp\":");
  Serial.print(temp, 1);
  Serial.print(",\"air\":");
  Serial.print(air, 1);
  Serial.print(",\"soil\":");
  Serial.print(soil);
  Serial.println("}");
}
