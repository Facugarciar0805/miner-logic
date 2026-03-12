#include <Arduino.h>
#include <mbedtls/md.h>

// --- Configuración Global ---
String datos = "Bloque_Simulado_#106";
uint32_t nonce = 1;
uint32_t sessionStartTime = 0; 
const int LOTE_SIZE = 500; // Procesamos 500 hashes antes de informar y ceder el CPU

// Prototipos
String minarBloque(String datos, uint32_t nonce);

void setup() {
  Serial.begin(115200);
  sessionStartTime = millis(); 
  Serial.println("\n>>> Sistema de Minería Profesional Iniciado <<<");
  Serial.println("Dificultad objetivo: '0000' (1 entre 65,536 probabilidades)");
}

void loop() {
  uint32_t batchStart = micros(); // Tiempo exacto al iniciar el lote
  
  for (int i = 0; i < LOTE_SIZE; i++) {
    // 1. Ejecutamos la minería usando el acelerador de hardware del ESP32
    String result = minarBloque(datos, nonce);
    
    // 2. Verificación de dificultad (Ajustada a 4 ceros para el reto real)
    if (result.startsWith("0000")) { 
      uint32_t sessionEndTime = millis();
      float totalSeconds = (sessionEndTime - sessionStartTime) / 1000.0;
      
      // Cálculo de Hashrate final al momento del éxito
      float hashStartTime = micros(); // Medimos un solo hash para el log final
      minarBloque(datos, nonce);
      float finalIndividualTime = (micros() - hashStartTime) / 1000000.0;

      Serial.println("\n\n########################################");
      Serial.println("##        ¡BLOQUE MINADO!             ##");
      Serial.println("########################################");
      Serial.print("  Nonce Ganador: "); Serial.println(nonce);
      Serial.print("  Hash:          "); Serial.println(result);
      Serial.println("----------------------------------------");
      Serial.print("  TIEMPO TOTAL:  "); Serial.print(totalSeconds, 2); Serial.println(" s");
      Serial.print("  INTENTOS:      "); Serial.println(nonce);
      Serial.print("  ULTIMO HASH:   "); Serial.print(finalIndividualTime, 6); Serial.println(" s");
      Serial.println("########################################");
      Serial.println("Simulación finalizada. Reinicie para otra prueba.");

      while(true) { delay(1000); } // Frenado de seguridad para el Watchdog
    }
    
    nonce++;
  }
  
  uint32_t batchEnd = micros();
  
  // 3. Alimentamos al Watchdog y permitimos tareas de red (WiFi/MQTT)
  yield(); 

  // 4. Cálculo de Hashrate del lote (H/s)
  float seconds = (batchEnd - batchStart) / 1000000.0;
  float hashrate = LOTE_SIZE / seconds;

  Serial.printf("Lote: %.2f H/s | Tiempo lote: %.4f s | Nonce: %d\n", hashrate, seconds, nonce);}

/**
 * Implementación de SHA-256 usando la API genérica de mbedtls
 */
String minarBloque(String datos, uint32_t nonce){
    byte shaResult[32];
    mbedtls_md_context_t ctx;

    mbedtls_md_init(&ctx); // Limpia el contexto
    
    // Configura el algoritmo y reserva memoria dinámica
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0); 
    
    mbedtls_md_starts(&ctx); // Prepara el inicio del cálculo
    
    String payload = datos + String(nonce);
    // Alimenta los datos al motor de hardware
    mbedtls_md_update(&ctx, (const unsigned char*) payload.c_str(), payload.length()); 
    
    mbedtls_md_finish(&ctx, shaResult); // Cierra el hash y entrega los 32 bytes
    
    mbedtls_md_free(&ctx); // Libera la RAM para evitar fugas (Memory Leaks)

    // Conversión eficiente a Hexadecimal
    String hashHex = "";
    hashHex.reserve(64); // Pre-reserva memoria para ganar velocidad
    for (int i = 0; i < 32; i++) {
        char buf[3];
        sprintf(buf, "%02x", shaResult[i]);
        hashHex += buf;
    }
    return hashHex;
}