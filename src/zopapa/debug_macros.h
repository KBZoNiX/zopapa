
/* Macros de DEBUG. 
 *  DBG_PRINT(...) -> Serial.print(...)
 *  DBG_PRINTLN(...) -> Serial.println(...)
 *  DBG_PRINT_F(...) -> Serial.print(F(...))
 *  DBG_PRINTLN_F(...) -> Serial.println(F(...))
 *
 *  Las variantes _F no copian las cadenas de texto a RAM, ahorrando memoria (no sirven para imprimir variables)
 *
 *  Sólo incluye el Serial.print... si:
 *  #define DEBUG 1
*/

#if DEBUG == 1
  #define DBG_PRINT(...) Serial.print(__VA_ARGS__)
  #define DBG_PRINTLN(...) Serial.println(__VA_ARGS__)
  #define DBG_PRINT_F(...) Serial.print(F(__VA_ARGS__))
  #define DBG_PRINTLN_F(...) Serial.println(F(__VA_ARGS__))
#else
  #define DBG_PRINT(...)
  #define DBG_PRINTLN(...)
  #define DBG_PRINT_F(...)
  #define DBG_PRINTLN_F(...)
#endif
