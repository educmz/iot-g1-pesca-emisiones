
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_NeoPixel.h>
#include <RTClib.h>

// Pines del Gateway
#define TFT_CS   5
#define TFT_DC   27
#define TFT_RST  26

#define BTN_PIN  25
#define LED_PIN  32

#define I2C_SDA  21
#define I2C_SCL  22

Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);
Adafruit_NeoPixel pixel(1, LED_PIN, NEO_GRB + NEO_KHZ800);
RTC_DS1307 rtc;

enum Estado {
  NORMAL,
  ALERTA,
  CRITICO
};

Estado estado = NORMAL;

bool rtcDisponible = false;
bool reconocida = false;
bool ultimoBoton = HIGH;
bool botonEstable = HIGH;

unsigned long cambioBoton = 0;
unsigned long ultimaPantalla = 0;

void actualizarLED() {
  uint32_t color;

  if (estado == NORMAL) {
    color = pixel.Color(0, 150, 0);
  } else if (estado == ALERTA) {
    color = pixel.Color(150, 100, 0);
  } else {
    color = pixel.Color(150, 0, 0);
  }

  pixel.setPixelColor(0, color);
  pixel.show();
}

void actualizarPantalla() {
  tft.fillScreen(ILI9341_BLACK);

  tft.fillRect(0, 0, 320, 38, ILI9341_NAVY);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(12, 10);
  tft.print("G1 PESCA");

  tft.setTextSize(1);
  tft.setCursor(12, 48);
  tft.print("NODO 5 - GATEWAY FOG");

  tft.setCursor(12, 68);
  tft.print("PRUEBA LOCAL - SIN MQTT");

  tft.setTextSize(2);
  tft.setCursor(12, 96);

  if (estado == NORMAL) {
    tft.setTextColor(ILI9341_GREEN);
    tft.print("NORMAL");
  } else if (estado == ALERTA) {
    tft.setTextColor(ILI9341_YELLOW);
    tft.print("ALERTA");
  } else {
    tft.setTextColor(ILI9341_RED);
    tft.print("CRITICO");
  }

  tft.setTextSize(1);
  tft.setTextColor(ILI9341_WHITE);
  tft.setCursor(12, 134);

  if (reconocida) {
    tft.print("Alerta reconocida");
  } else {
    tft.print("Boton: reconocer alerta");
  }

  tft.setCursor(12, 160);
  tft.print("MQTT: PENDIENTE");

  tft.setCursor(12, 184);

  if (rtcDisponible) {
    DateTime ahora = rtc.now();
    char fecha[30];

    snprintf(fecha, sizeof(fecha),
      "%02d/%02d/%04d %02d:%02d:%02d",
      ahora.day(), ahora.month(), ahora.year(),
      ahora.hour(), ahora.minute(), ahora.second()
    );

    tft.print(fecha);
  } else {
    tft.print("RTC no detectado");
  }

  tft.setCursor(12, 220);
  tft.print("Serial: N normal, A alerta, C critico");
}

void cambiarEstado(Estado nuevo) {
  estado = nuevo;
  reconocida = false;

  actualizarLED();
  actualizarPantalla();

  Serial.println("Estado de prueba actualizado");
}

void setup() {
  Serial.begin(115200);

  pinMode(BTN_PIN, INPUT_PULLUP);

  Wire.begin(I2C_SDA, I2C_SCL);
  SPI.begin(18, 19, 23, TFT_CS);

  pixel.begin();
  pixel.setBrightness(80);
  pixel.clear();
  pixel.show();

  tft.begin();
  tft.setRotation(1);

  rtcDisponible = rtc.begin();

  if (rtcDisponible) {
    Serial.println("RTC detectado");
  } else {
    Serial.println("ERROR: RTC no detectado");
  }

  actualizarLED();
  actualizarPantalla();

  Serial.println("G1 PESCA - GATEWAY INICIADO");
  Serial.println("N = Normal");
  Serial.println("A = Alerta");
  Serial.println("C = Critico");
}

void loop() {
  // Comandos de prueba desde Serial Monitor
  if (Serial.available()) {
    char comando = Serial.read();

    if (comando == 'n' || comando == 'N') {
      cambiarEstado(NORMAL);
    }

    if (comando == 'a' || comando == 'A') {
      cambiarEstado(ALERTA);
    }

    if (comando == 'c' || comando == 'C') {
      cambiarEstado(CRITICO);
    }
  }

  // Pulsador con antirrebote
  bool lectura = digitalRead(BTN_PIN);

  if (lectura != ultimoBoton) {
    cambioBoton = millis();
  }

  if (millis() - cambioBoton > 50) {
    if (lectura != botonEstable) {
      botonEstable = lectura;

      if (botonEstable == LOW) {
        reconocida = true;
        Serial.println("Pulsador detectado: reconocimiento");
        actualizarPantalla();
      }
    }
  }

  ultimoBoton = lectura;

  // Actualización de hora
  if (millis() - ultimaPantalla >= 1000) {
    ultimaPantalla = millis();
    actualizarPantalla();
  }
}
