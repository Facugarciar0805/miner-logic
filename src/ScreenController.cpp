#include <math.h>
#include <ScreenController.h>

ScreenController::ScreenController() : u8g2(U8G2_R0, U8X8_PIN_NONE) {
}

void ScreenController::inicializar() {
  u8g2.begin();
}
void ScreenController::mostrarSpinner(uint8_t paso, const char* texto) {
  u8g2.clearBuffer();

  // 1. Dibujamos el texto centrado en la parte inferior
  u8g2.setFont(u8g2_font_ncenB08_tr); // Fuente estándar
  int anchoTexto = u8g2.getStrWidth(texto);
  u8g2.drawStr((128 - anchoTexto) / 2, 55, texto); // Centrado dinámico

  // 2. Matemáticas del círculo giratorio
  int centroX = 64; 
  int centroY = 25; 
  int radio = 12;

  for (int i = 0; i < 8; i++) {
    float angulo = i * (PI / 4.0);
    int x = centroX + radio * cos(angulo);
    int y = centroY + radio * sin(angulo);
    
    if (i == (paso % 8)) {
      u8g2.drawDisc(x, y, 2.5); // El punto que "avanza" es más grueso
    } else {
      u8g2.drawPixel(x, y);     // El resto de los puntos
    }
  }

  u8g2.sendBuffer();
}

void ScreenController::mostrarTickExito(const char* texto) {

  u8g2.clearBuffer();

  // 1. Dibujamos el ícono del Tilde usando una fuente especial de íconos
  // El tamaño 4x es de 32x32 píxeles
  u8g2.setFont(u8g2_font_open_iconic_check_4x_t);
  
  // El caracter 64 ('@') corresponde al ícono de confirmación en esta fuente
  // Lo centramos en X=48, Y=40
  u8g2.drawGlyph(48, 40, 64); 

  // 2. Dibujamos el texto de abajo
  u8g2.setFont(u8g2_font_ncenB08_tr);
  int anchoTexto = u8g2.getStrWidth(texto);
  u8g2.drawStr((128 - anchoTexto) / 2, 58, texto);

  u8g2.sendBuffer();
}

// Spinner con workId y nonce actual — se llama en el loop de minería
void ScreenController::mostrarMinando(uint8_t paso, uint32_t workId, uint32_t nonce) {
  u8g2.clearBuffer();

  // Spinner centrado arriba
  int centroX = 64, centroY = 22, radio = 12;
  for (int i = 0; i < 8; i++) {
    float angulo = i * (PI / 4.0);
    int x = centroX + radio * cos(angulo);
    int y = centroY + radio * sin(angulo);
    if (i == (paso % 8)) u8g2.drawDisc(x, y, 2.5);
    else                  u8g2.drawPixel(x, y);
  }

  // Línea 1: "Minando #<workId>"
  char linea1[24];
  snprintf(linea1, sizeof(linea1), "Minando #%lu", (unsigned long)workId);
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(linea1)) / 2, 44, linea1);

  // Línea 2: nonce actual
  char linea2[24];
  snprintf(linea2, sizeof(linea2), "nonce: %lu", (unsigned long)nonce);
  u8g2.drawStr((128 - u8g2.getStrWidth(linea2)) / 2, 58, linea2);

  u8g2.sendBuffer();
}

// Spinner con conteo de aprobaciones — se llama mientras consensusOpen == true
void ScreenController::mostrarConsenso(uint8_t paso, int aprobaciones, int requeridas) {
  u8g2.clearBuffer();

  int centroX = 64, centroY = 22, radio = 12;
  for (int i = 0; i < 8; i++) {
    float angulo = i * (PI / 4.0);
    int x = centroX + radio * cos(angulo);
    int y = centroY + radio * sin(angulo);
    if (i == (paso % 8)) u8g2.drawDisc(x, y, 2.5);
    else                  u8g2.drawPixel(x, y);
  }

  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth("Consenso...")) / 2, 44, "Consenso...");

  char aprobStr[16];
  snprintf(aprobStr, sizeof(aprobStr), "%d / %d aprobados", aprobaciones, requeridas);
  u8g2.drawStr((128 - u8g2.getStrWidth(aprobStr)) / 2, 58, aprobStr);

  u8g2.sendBuffer();
}

// Tick de éxito con workId y tiempo — se muestra ~3s y luego vuelve al idle
void ScreenController::mostrarResuelta(uint32_t workId, unsigned long elapsedMs) {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_open_iconic_check_4x_t);
  u8g2.drawGlyph(48, 40, 64);

  char info[28];
  snprintf(info, sizeof(info), "#%lu en %lums", (unsigned long)workId, elapsedMs);
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(info)) / 2, 58, info);

  u8g2.sendBuffer();
}