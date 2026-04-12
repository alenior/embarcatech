# Projeto Final - Sistema de Alarme IoT (BitDogLab + Raspberry Pi Pico W)

## 1) Objetivo do projeto

Este projeto implementa um **sistema de alarme IoT** com monitoramento local e remoto, baseado em Raspberry Pi Pico W.

### Funcionalidades principais

- Detecção de **movimento** (PIR) e **som** (ADC/microfone).
- Estados do alarme: **DISARMED**, **ARMED** e **TRIGGERED**.
- Feedback local por:
  - **OLED SSD1306** (status/mensagens),
  - **LED RGB + buzzer** (efeitos de estado),
  - **Matriz WS2812 5x5** via PIO.
- Integração com **Firebase Realtime Database** via **HTTPS/TLS** para:
  - envio de eventos,
  - atualização de status do dispositivo,
  - leitura de comandos remotos (arm/disarm).

## 2) Arquitetura resumida

- Firmware principal em C (`src/projeto_final.c`) inicializa Wi-Fi, Firebase, sensores, UI local e loop de tarefas.
- Módulos dedicados:
  - `wifi.c`: conexão e reconexão Wi-Fi,
  - `firebase.c`: cliente TLS (lwIP + mbedTLS), fila de eventos e polling de controle,
  - `alarm.c`: máquina de estados e integração dos atuadores,
  - `audio.c` e `pir.c`: detecção sonora e de movimento,
  - `display.c`/`ssd1306.c` e `matrix.c`: interface visual local.

## 3) Estrutura de dados no Firebase

Este firmware utiliza, por padrão, os caminhos abaixo:

- Eventos: `alarm/<timestamp>`
- Status atual: `alarm_status/current`
- Controle remoto: `alarm_control/current`

### Payloads esperados

#### 3.1 Status enviado pelo dispositivo (`alarm_status/current`)

```json
{
  "status": "DESARMADO",
  "state": "DISARMED",
  "ts": 12345678,
  "device_id": "bitdoglab-picow",
  "fw_version": "1.0.0",
  "wifi_ok": true
}
```

#### 3.2 Comando remoto consumido pelo dispositivo (`alarm_control/current`)

```json
{
  "desired_state": "ARMED",
  "updated_at": 1712345678
}
```

> `desired_state` deve ser `ARMED` ou `DISARMED`.

## 4) Dependências

## 4.1 Firmware (Pico W)

- **Raspberry Pi Pico SDK** (compatível com o projeto CMake).
- **CMake >= 3.13**.
- Toolchain ARM (por exemplo `arm-none-eabi-gcc`).
- Bibliotecas do SDK já usadas no link do projeto:
  - `pico_stdlib`, `pico_cyw43_arch_lwip_poll`,
  - `pico_lwip_mbedtls`, `pico_mbedtls`,
  - `hardware_adc`, `hardware_dma`, `hardware_i2c`, `hardware_pio`,
  - `hardware_watchdog`, `hardware_sync`.

## 4.2 Backend Firebase Functions (opcional)

Pasta `functions/`:

- **Node.js 24**,
- Firebase CLI,
- Dependências npm:
  - `firebase-admin`,
  - `firebase-functions`,
  - `axios`.

## 4.3 Aplicação Flutter (app de monitoramento/controle)
>
> O código da aplicação Flutter tem integração com Firebase Realtime Database.

Dependências recomendadas no `pubspec.yaml` da app Flutter:

- `firebase_core`
- `firebase_database`
- `flutter_riverpod` **ou** `provider` (estado da UI)
- `intl` (formatação de data/hora)

A app deve observar em tempo real, no mínimo:

- `alarm_status/current` (status do dispositivo),
- `alarm` (histórico de eventos),
- e escrever em `alarm_control/current` para enviar comandos remotos.

## 5) Instalação e configuração

## 5.1 Clonar e entrar na pasta do projeto

```bash
git clone <repo-url>
cd bitdoglab/meu_projeto/projeto_final/projeto_final
```

## 5.2 Configurar credenciais e parâmetros

Edite `include/config.h`:

- `WIFI_SSID`
- `WIFI_PASS`
- `FIREBASE_HOST`
- `FIREBASE_AUTH_TOKEN`
- caminhos/pinos, se necessário.

## 5.3 Definir o caminho do Pico SDK

Exemplo Linux/macOS:

```bash
export PICO_SDK_PATH=/caminho/para/pico-sdk
```

## 6) Compilação e geração do firmware

```bash
cmake -S . -B build
cmake --build build -j
```

Após o build, serão gerados artefatos como `.uf2` (via `pico_add_extra_outputs`).

## 7) Gravação e execução no Pico W

1. Coloque a placa em modo BOOTSEL.
2. Copie o arquivo `.uf2` gerado para o dispositivo montado.
3. Reinicie e acompanhe logs por USB serial (stdio USB está habilitado no projeto).

## 8) Execução do backend Firebase Functions (opcional)

Na pasta do projeto:

```bash
cd functions
npm ci
npm run lint
npm run serve
```

Deploy (quando necessário):

```bash
npm run deploy
```

## 9) Integração com a aplicação Flutter

### 9.1 Configuração inicial da app

1. Adicione o projeto Firebase (Android/iOS/Web) no app Flutter.
2. Inicialize Firebase no `main.dart` com `Firebase.initializeApp()`.
3. Configure regras do Realtime Database para leitura/escrita conforme o fluxo do app e do dispositivo.

### 9.2 Leitura de status em tempo real

- Faça subscribe em `alarm_status/current`.
- Priorize o campo `state` para lógica de UI (`DISARMED`, `ARMED`, `TRIGGERED`).
- Use `status` para texto amigável ao usuário (ex.: `ALERTA - SOM`).

### 9.3 Envio de comando remoto

Escreva em `alarm_control/current`:

```json
{
  "desired_state": "DISARMED",
  "updated_at": 1712345678
}
```

## 10) Mapeamento de pinos (padrão)

- PIR: GP8
- Microfone (ADC): GP28
- Botões: GP5 (A), GP6 (B)
- Joystick: GP26 (X), GP27 (Y), GP22 (botão)
- LED RGB: GP13 (R), GP11 (G), GP12 (B)
- Matriz WS2812: GP7
- Buzzer: GP21
- OLED I2C: SDA GP14, SCL GP15

## 11) Solução de problemas

- **Erro CMake: `PICO_SDK_PATH` não definido**
  - Defina a variável de ambiente antes de configurar o projeto.
- **Status não atualiza na app Flutter**
  - Verifique se a app escuta `alarm_status/current`.
  - Verifique se a app usa o campo correto (`state` para lógica, `status` para exibição).
- **Comandos remotos não aplicam**
  - Confirme formato de `alarm_control/current` com `desired_state` e `updated_at`.
- **Sem conexão Firebase**
  - Confira token, host e conectividade Wi-Fi.

## 12) Próximos passos sugeridos

- Adicionar testes automatizados de parsing JSON e máquina de estados.
- Criar documentação da app Flutter no próprio repositório (ou link para repositório dedicado).
- Adicionar guia de calibração de áudio por ambiente.
