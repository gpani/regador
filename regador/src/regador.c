/* Copyright 2017, Gessica Paniagua
 * Copyright 2017, Pablo Ridolfi
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* incluyo encabezados del programa:
 * picoapi y api para el modulo esp8266
 */
#include "picoapi.h"
#include "esp8266.h"

/* incluyo encabezados estandar de C que voy a usar */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* defino constantes
 * longitud de string de comando (128)
 * puerto UDP 8000
 * datos de la red wi-fi a la cual conectarse (ssid y password)
 */
#define BUF_LEN  128
#define UDP_PORT 8000
#define AP_SSID "AndroidAP"
#define AP_PSWD "advt1067"

/* constantes de riego
 * intervalo de riego: 24 horas
 * tiempo bomba encendida: 60 segundos
 */
#define INTERVALO_RIEGO     (24*3600*10)
#define INTERVALO_ENCENDIDO (60*10)

/* variables para riego
 * contador de tiempo y contador de bomba encendida
 */
uint32_t contador = 0;
uint32_t bomba_encendida = INTERVALO_ENCENDIDO;

/* programa principal */
int main(void)
{
	/* variables locales
	 * string para comando y flag (bool) de modo manual
	 * rv es una variable para ver el valor retornado por las funciones
	 */
	char buf[BUF_LEN];
	bool manual = false;
	espStatus_e rv;

	/* imprimo por consola local mensaje de bienvenida y estado */
	printString("\r\nESP8266+picoCIAA REGADOR AUTOMATICO\r\n");
	printString("Iniciando... ");

	do {
		/* inicializo el ESP */
		rv = espInit(ESP_STATION);
		if (rv == ESP_OK) {
			printString("OK.\r\n");
		}
		else {
			printString("ERROR.\r\n");
			pausems(1000);
		}
	} while (rv != ESP_OK);

	/* me conecto a la red wi-fi */
	printString("Connecting to "); printString(AP_SSID);
	printString("... ");
	rv = espConnectToAP(AP_SSID, AP_PSWD);
	if (rv == ESP_OK) {
		printString("OK.\r\n");
	}
	else {
		printString("ERROR.\r\n");
	}

	/* imprimo informacion sobre la ip del módulo */
	bzero(buf, BUF_LEN);
	rv = espGetIP(buf, BUF_LEN);

	if (rv == ESP_OK) {
		printString("IP info: "); printString(buf);
		printString("\r\n");
	}

	/* inicio socket UDP 8000 para recibir comandos */
	rv = espStartUDPListener(UDP_PORT);
	if (rv == ESP_OK) {
		printString("Listening on UDP port "); printInteger(UDP_PORT);
		printString(".\r\n");
	}

	while (1) {
		/* prendo y apago LED verde de la pico */
		picoDigitalToggle(LED_G);

		/* espero 100 milisegundos */
		pausems(100);

		/* me fijo si llegaron datos por el socket UDP */
		rv = espGetData(buf, BUF_LEN);
		if (rv > 0) {
			/* obtengo comando recibido por UDP */
			buf[rv] = 0;
			espSendData(buf, rv);
			printString("Recv: "); printString(buf);
			printString("\r\n");

			/* comparo contra los diferentes comandos posibles:
			 * prender
			 * apagar
			 * estado
			 * bomba
			 *
			 * y envío una respuesta
			 */
			if (strstr(buf, "prender")) {
				picoDigitalWrite(LED_B, LED_ON);
				manual = true;
			}
			if (strstr(buf, "apagar")) {
				picoDigitalWrite(LED_B, LED_OFF);
				manual = false;
			}
			if (strstr(buf, "estado")) {
				sprintf(buf, "cont:%u bomba:%d %d sens:%d manual:%d",
						contador, !picoDigitalRead(LED_B), bomba_encendida, picoDigitalRead(P8_8), manual);

				pausems(1000);
				espSendData(buf, strlen(buf));
			}
			if (strstr(buf, "bomba=")) {
				bomba_encendida = atoi(buf+6)*10;
			}
		}

		/* si se detecta la tierra seca (P8_8) */
		if (picoDigitalRead(P8_8)) {
			/* prendo led rojo de la picoCIAA */
			picoDigitalWrite(LED_R, LED_ON);
			if ((manual == false) && (contador == 0)) {
				/* si pasó el tiempo, empiezo a regar
				 * la bomba y el led azul de la pico están en el mismo pin (P8_10)
				 */
				picoDigitalWrite(LED_B, LED_ON);
				contador = INTERVALO_RIEGO;
			}
		}
		else {
			picoDigitalWrite(LED_R, LED_OFF);
		}

		/* en cuanto pasa el tiempo de bomba encendida, la apago */
		if (((INTERVALO_RIEGO - contador) > bomba_encendida) && (manual == false)) {
			picoDigitalWrite(LED_B, LED_OFF);
		}

		/* voy contando el tiempo hasta el próximo riego */
		if (contador > 0) {
			contador --;

			/* cada 1 segundo muestro estado por consola local */
			if ((contador % 10)==0) {
				sprintf(buf, "cont:%u bomba:%d sens:%d manual:%d\r",
						contador, !picoDigitalRead(LED_B), picoDigitalRead(P8_8), manual);
				printString(buf);

			}

			/* cada 10 segundos muestro ip del módulo */
			if ((contador % 100)==0) {
				bzero(buf, BUF_LEN);
				rv = espGetIP(buf, BUF_LEN);

				if (rv == ESP_OK) {
					printString("IP info: "); printString(buf);
					printString("\r\n");
				}
			}
		}
	}
}
