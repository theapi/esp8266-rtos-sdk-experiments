
#ifndef DENBIT_H_
#define DENBIT_H_
#ifdef __cplusplus
extern "C" {
#endif


#define DENBIT_GREEN     13 // D7
#define DENBIT_RED       12 // D6
#define DENBIT_RGB_RED   15 // D8
#define DENBIT_RGB_GREEN 14 // D5
#define DENBIT_RGB_BLUE  10 // SD3
#define DENBIT_SW1 2
#define DENBIT_SW2 16

#define DENBIT_OUTPUT_PIN_SEL ( (1ULL<<DENBIT_GREEN) | (1ULL<<DENBIT_RED) | (1ULL<<DENBIT_RGB_RED) | (1ULL<<DENBIT_RGB_GREEN) | (1ULL<<DENBIT_RGB_BLUE))

void denbit_config();

#ifdef __cplusplus
}
#endif
#endif /* DENBIT_H_ */
