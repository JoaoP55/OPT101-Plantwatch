# PlantWatch

Monitor de plantas com Arduino — Temperatura, Umidade do Ar e Umidade do Solo.

---

## Estrutura do projeto

```
plantwatch/
├── Codigos_arduinos
|      ├── Emissor_Ar.ino
|      ├── Emissor_Solo.ino
|      └── Receptor.ino
├── app.py          ← Backend Flask (roda no PC)
├── index.html      ← Frontend (aberto pelo Flask)
├── plants.json     ← Banco de dados das plantas
└── requirements.txt
```

---

## Instalação (Linux)

### 1. Instale as dependências Python

```bash
pip install flask flask-cors pyserial
```

### 2. Carregue o código no Arduino

1. Abra o **Arduino IDE**
2. Instale a biblioteca **https://github.com/adafruit/Adafruit_Sensor/releases/tag/1.1.15**
   Instale a biblioteca **https://github.com/adafruit/dht-sensor-library**
   - Menu → Sketch → Include Library → add .ZIP library → (As duas bibliotecas acima)

### 3. Rode o backend

```bash
cd plantwatch
python app.py
```

### 4. Acesse o site

Abra no navegador: **http://localhost:5000**

---

## Permissão serial no Linux

Se der erro de permissão na porta serial:

```bash
sudo usermod -a -G dialout $USER
# Faça logout e login novamente, ou:
sudo chmod a+rw /dev/ttyACM0
```

---

## Como usar

1. Cadastre uma planta com nome e faixas ideais de temperatura/umidade
2. Clique na planta na lista lateral
3. O site mostra os valores atuais e indica se algo está fora do ideal
4. Alertas explicam exatamente o que precisa ser feito
