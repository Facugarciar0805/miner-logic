#ifndef SCREEN_CONTROLLER_H
#define SCREEN_CONTROLLER_H

#include <U8g2lib.h>
#include <Wire.h>

class ScreenController {
  private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

  public:
    ScreenController();
    void inicializar();
    void mostrarSpinner(uint8_t paso, const char* texto = "Conectando...");
    void mostrarTickExito(const char* texto = "¡Exito!");

    void mostrarMinando(uint8_t paso, uint32_t workId, uint32_t nonce);
    void mostrarConsenso(uint8_t paso, int aprobaciones, int requeridas);
    void mostrarResuelta(uint32_t workId, unsigned long elapsedMs);
};

#endif