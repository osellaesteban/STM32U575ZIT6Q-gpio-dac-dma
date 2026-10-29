# Multiplexor de estímulos (STM32U575 + DS8R + PWD5F60)

Firmware para la NUCLEO-U575ZI-Q que multiplexa la salida de un estimulador
monocanal Digitimer DS8R sobre 8 canales. Cada canal usa un puente completo
PWD5F60, que además fija la polaridad del pulso en los electrodos.

La aplicación de PC (sujeto, experimento, umbrales, protocolos, monitor y
registro de sesiones) está en [pc_app/](pc_app/README.md).

## Arquitectura

| Módulo | Función |
|---|---|
| `st_stimulator.c/.h` | Modelo: parámetros por canal (habilitación, amplitud, polaridad, etiqueta) y del frame (período, secuencia, temporización). Valida la configuración. No depende del hardware. |
| `st_engine.c/.h` | Motor de tiempo real: TIM2 (32 bits, tick de 1 µs) en output compare. Cada interrupción escribe GPIO (una sola escritura a BSRR) y DAC, y programa el evento siguiente. |
| `st_protocol.c/.h` | Protocolo de comandos ASCII, independiente del hardware. |
| `st_comm.c/.h` | USART1 (VCP del ST-LINK) con buffers circulares por interrupción en recepción y transmisión: escribir nunca bloquea el bucle principal. |
| `st_monitor.c/.h` | Monitor de corriente: ADC1 muestrea continuamente a 500 kS/s, calcula estadísticas por fase de cada pulso y envía la forma de onda a pedido. |
| `st_pulse_stats.c/.h` | Parte del monitor que no depende del hardware: ventanas de muestras, estadísticas y formato de los reportes. |
| `st_hw_config.h` | Mapeo de pines, baudrate y prioridades de interrupción. |

Un **frame** (período `PERIOD`) se divide en *slots* equiespaciados, uno por
entrada de la secuencia `SEQ`. Un canal puede repetirse en la secuencia para
estimularlo más seguido. En cada slot:

```
t=0          SETTLE        +TRIG          DELAY+PW/2 (BIPH)    DELAY+PW+TAIL
 |-------------|--------------|-----------------|--------------------|---- ... siguiente slot
 CHx=1, POL,   TRIG ↑         TRIG ↓            POL/NPOL invertidos  CHx=0, DAC=0
 DAC=amplitud  (el DS8R muestrea la amplitud en el flanco)
```

Los canales deshabilitados o con amplitud 0 saltean el slot completo (no se
dispara el DS8R: la salida del DAC con buffer no llega a 0 V exactos).

### ¿Por qué ISR y no DMA?

El prototipo original movía tablas fijas por DMA con TIM2. Se reemplazó por
output compare con interrupción porque:

- Hay pocos eventos por slot (4 o 5) y el DS8R admite como máximo 1 kHz de
  disparos, así que la carga de CPU es despreciable. La latencia de la ISR a
  160 MHz es < 1 µs, frente a pulsos de 50 a 2000 µs.
- Los cambios de amplitud, habilitación y polaridad en tiempo real se leen
  de forma consistente al comienzo de cada slot. Con tablas DMA había que
  resolver el doble buffer entre varios canales de DMA.
- Los tiempos (settle, ancho del trigger, cierre del puente) se derivan de la
  configuración y no del índice en una tabla.

Si a futuro una fuente controlada por tensión necesita una forma de onda por
pulso, la ISR puede lanzar un DMA del DAC en el evento de trigger.

## Cableado esperado

Todas las líneas están en **GPIOE** (las escribe una sola instrucción BSRR).

| Señal | Pin | Destino |
|---|---|---|
| CH0..CH7 | PE0, PE2, PE3, PE4, PE5, PE6, PE7, PE8 | PWM1 = PWM2 del PWD5F60 de cada canal |
| POL | PE9 | IN1 de todos los puentes |
| NPOL | PE10 | IN2 de todos los puentes |
| TRIG | PE11 | Entrada de trigger del DS8R (TTL; usar buffer para el cable BNC) |
| ARM | PE12 | SD1 = SD2 de todos los puentes |
| DAC1_OUT1 | PA4 | Entrada de control del DS8R (0–10 V) mediante un amplificador ×3 |
| ADC1_IN8 | PA3 | Salida del amplificador aislado que mide el shunt de corriente (0–3,3 V) |

Según la tabla de verdad del PWD5F60 (DS12543), con SD=1 un medio puente
enciende su lado alto solo si IN=1 y PWM=1; en cualquier otro caso enciende
el bajo. Por lo tanto:

- `CHx=0`: los dos lados bajos encendidos. Los electrodos quedan en corto
  contra DS8R−, sin camino desde DS8R+, y se recargan los bootstrap.
- `CHx=1, POL=1`: OUT1 = DS8R+, OUT2 = DS8R− (polaridad positiva).
- `CHx=1, NPOL=1`: polaridad invertida.
- `ARM=0`: todos los MOSFET apagados (estado seguro, alta impedancia).

Las entradas del PWD5F60 tienen pull-down interno, así que en reset quedan apagados.

**Importante:**

1. **Aislación.** La GND de los puentes es la salida negativa del DS8R, que
   es la parte aplicada al paciente. Las líneas CHx/POL/NPOL/ARM tienen que
   pasar por aisladores digitales (con salida baja por defecto) y el VCC de
   los PWD5F60 (10–20 V) tiene que salir de un DC/DC aislado. Si se conectan
   directo, la GND de la Nucleo y de la PC queda unida al paciente y se anula
   la aislación del DS8R. TRIG y la entrada de control van al lado no aislado
   del DS8R.
2. **El DS8R tiene que estar en modo MONOFÁSICO POSITIVO.** Con el DS8R en
   bifásico, durante la fase negativa (DS8R+ < DS8R−) los diodos de cuerpo de
   todos los medios puentes conducen y desvían la corriente de recuperación
   (lado bajo encendido → OUT → diodo del lado alto → DS8R+). Esa fase no
   llega al electrodo. La polaridad, la alternancia y el bifásico los genera
   el puente.
3. El modo `BIPH` invierte el puente en la mitad del pulso del DS8R (`PW`
   debe ser el ancho total). Durante el tiempo muerto del PWD5F60 (0,1–0,25 µs
   con RDT = 0) la fuente de corriente queda en circuito abierto. **Es
   experimental: hay que verificarlo con osciloscopio sobre una carga
   resistiva antes de usarlo en personas.**
4. Medir el retardo trigger → salida del DS8R y ajustar `DELAY`; el puente
   tiene que estar cerrado durante todo el pulso (`TAIL` es el margen).
5. Verificar en el manual de la NUCLEO-U575ZI-Q (UM2861) que PE7..PE12 estén
   libres en los conectores que se usen.

## Monitor de corriente

### Hardware

- Los pines **SENSE1 y SENSE2** de todos los PWD5F60 (fuentes de los MOSFET
  del lado bajo) se unen a **un único shunt** contra la GND de los puentes
  (DS8R−). Como conduce un solo canal por vez, un canal de ADC alcanza, y la
  corriente atraviesa el shunt siempre en el mismo sentido sin importar la
  polaridad (también en `BIPH`).
- El shunt debe dar ≲1 V a la corriente máxima (le resta tensión de
  compuerta al lado bajo); por ejemplo 10 Ω para un rango de 100 mA.
- El shunt está del lado del paciente: su tensión llega a PA3 a través de
  un **amplificador aislado** (tipo AMC1311 o ACPL-C87) que entregue 0–3,3 V.
- Opcional, protección por hardware: uno de los comparadores del PWD5F60
  compara el shunt contra un umbral y su salida CPOUT (colector abierto)
  baja SD.

### Cómo se sincroniza

TIM6 dispara el ADC1 cada 2 µs (500 kS/s, 12 bits) y un GPDMA circular
guarda las muestras en un buffer de 16384 (32 ms). TIM6 cuenta con el mismo
prescaler que TIM2 y arranca junto con él, así que la muestra *k* se tomó en
el instante *(k+1)·2 µs* de TIM2. El motor informa los instantes de cada
pulso (cierre del puente, trigger, inicio y fin de cada fase, apertura) y el
bucle principal toma las muestras de cada fase sin leer la posición del DMA.
Cada 10 ms se verifica que la posición del DMA coincida con la esperada; si
no coincide, `STATUS` informa `MSYNC=1` y los datos no son confiables.

Las ventanas de cada fase se calculan a partir de `DELAY` y `PW`, así que
conviene medir el retardo real del DS8R con `WAVE` y ajustar `DELAY`.

### Reporte por pulso (`MON 1`)

Una línea por pulso entregado, en cuentas del ADC (12 bits, sin convertir):

```
P <frame> <slot> <canal> <amp> <base> <pol> <n> <max> <media> <mediana> <min> [<pol> <n> <max> <media> <mediana> <min>]
```

- `base`: mediana de las muestras entre el cierre del puente y el trigger,
  es decir, el cero de corriente (offset del amplificador aislado).
- Un grupo por fase (dos en `BIPH`). `pol` es `+` o `-` y `n` es la cantidad
  de muestras usadas. La mediana es la inferior si `n` es par.
- `MONTRIM <us>` descarta ese tiempo al principio y al final de cada fase,
  para que los flancos no afecten el mínimo y la media (por defecto 4 µs).
- Si el puerto no da abasto, se descartan reportes en lugar de frenar la
  estimulación. `STATUS` cuenta los pulsos sin analizar (`MLOST`) y las
  líneas descartadas (`TXDROP`).

### Forma de onda a pedido (`WAVE [canal]`)

Captura el próximo pulso entregado (de cualquier canal, o del indicado),
desde el cierre hasta la apertura del puente (máx. 4096 muestras):

```
WH <frame> <slot> <canal> <amp> <pol> <bifásico> <ts_us> <n> <i_trig> <i_fase1> <i_inversión> <i_fin>
WD <índice> <v> <v> ... (24 valores por línea)
WE
```

Los índices `i_*` son posiciones dentro de la captura (`i_inversión` = 0 si
no es bifásico). Hasta que termina el envío, otro `WAVE` responde `ERR BUSY`.

## Configuración del DS8R

- Modo monofásico, polaridad positiva, ancho de pulso = `PW`.
- Control de amplitud externo (BNC trasero, 0–10 V proporcional al rango
  configurado). Elegir el menor rango que sirva: 10 V en la entrada = 100 %
  del rango.
- `AMP` está en por mil del fondo de escala (0..1000). `DACFS` es la cuenta
  del DAC que produce 10 V a la salida del amplificador (calibrar).
- Máximo 1000 disparos/s en total con el firmware estándar: el firmware
  rechaza configuraciones con slots de menos de 1 ms.

## Protocolo (USART1 / VCP, 921600 8N1)

Una línea por comando (`\r`, `\n` o `\r\n`), una línea de respuesta: `OK ...`
o `ERR <CHANNEL|RANGE|TIMING|ARG|BUSY|UNAVAILABLE|UNKNOWN|LENGTH>`. Las palabras clave no
distinguen mayúsculas. Los argumentos se separan con espacios o comas.

| Comando | Cuándo | Descripción |
|---|---|---|
| `PING` | siempre | `OK PONG` |
| `INFO` | siempre | Versión y límites |
| `STATUS` | siempre | `OK RUN=<0/1> FRAMES=<n> WDOG=<ms> MON=<0/1> TRIM=<us> MLOST=<n> TXDROP=<n> MSYNC=<0/1>` |
| `GET` | siempre | Configuración del frame |
| `CH <n>` | siempre | `OK CH=n EN= AMP= POL= LABEL=` |
| `CH <n> EN <0/1>` | tiempo real | Habilita el canal |
| `CH <n> AMP <0..1000>` | tiempo real | Amplitud en ‰ del fondo de escala |
| `CH <n> POL <POS/NEG/ALT/BIPH>` | tiempo real | Polaridad |
| `CH <n> LABEL <texto>` | tiempo real | Etiqueta (máx. 15 caracteres, sin espacios) |
| `AMPS a0 a1 ...` | tiempo real | Amplitudes de los primeros canales en un comando (se validan todas antes de aplicar) |
| `ENS e0 e1 ...` | tiempo real | Habilitación de los primeros canales |
| `PERIOD <us>` | detenido | Período del frame |
| `SEQ c0 c1 ...` | detenido | Secuencia de canales (máx. 32 slots) |
| `TIMING <settle> <trig> <delay> <tail>` | detenido | Temporización del slot en µs |
| `PW <us>` | detenido | Ancho de pulso configurado en el DS8R (50..2000) |
| `DACFS <cuentas>` | detenido | Calibración del DAC |
| `DEFAULT` | detenido | Configuración por defecto |
| `RUN [frames]` | detenido | Arranca; sin argumento o 0 = continuo |
| `STOP` | siempre | Detiene y lleva todo al estado seguro |
| `WDOG <ms>` | siempre | Si estimula y no llega ningún comando en `ms`, se detiene (0 = desactivado) |
| `MON <0/1>` | siempre | Reporte de estadísticas por pulso |
| `MONTRIM <us>` | siempre | Tiempo descartado en los bordes de cada fase (0..500) |
| `WAVE [canal]` | siempre | Envía la forma de onda del próximo pulso (`ERR UNAVAILABLE` si el ADC no inicializó) |

Mensajes asíncronos: `EVT READY ...`, `EVT DONE FRAMES=<n>`, `EVT WDOG`,
`EVT STOP BUTTON`, y los del monitor (`P ...`, `WH`/`WD`/`WE`). El botón azul de la Nucleo detiene la estimulación.

Ejemplo (2 canales a 40 Hz cada uno, el 1 alternando polaridad, 100 frames):

```
SEQ 0 1
PERIOD 25000
PW 300
CH 0 AMP 150
CH 1 AMP 200
CH 1 POL ALT
ENS 1 1
RUN 100
```

## Cambios pendientes en CubeMX (.ioc)

El código de `main.c` se modificó a mano. Antes de regenerar con CubeMX hay
que reflejar estos cambios en el `.ioc`; si no, se pierden:

- **Reloj:** MSI 4 MHz → PLL1 (M=1, N=80, R=2) → SYSCLK 160 MHz, escala de
  tensión 1, latencia de flash 4. Si se regenera sin este cambio, el firmware
  sigue funcionando: el prescaler de TIM2 se calcula en tiempo de ejecución.
- **DAC1:** trigger = None. El motor lo reconfigura igual al iniciar, con el
  modo de interfaz HF automático, obligatorio con HCLK > 80 MHz.
- **GPDMA1:** se pueden borrar los linked lists de CH10/CH11. Al hacerlo,
  borrar también `DACVals`/`GPIOEVals` en `main.c` (hoy solo los usa
  `linked_list.c`, que ya no se llama).
- **No** habilitar en el NVIC de CubeMX las interrupciones de TIM2 ni USART1:
  sus handlers están en `st_engine.c` y `st_comm.c`.
- **Monitor:** ADC1, TIM6 y GPDMA1 canal 0 se configuran por registros en
  `st_monitor.c` (el HAL de ADC no está en el proyecto). No asignarlos a otra
  cosa en CubeMX. Si en el futuro se agrega ADC1 al `.ioc`, conviene migrar
  `adcInit()` al HAL generado.
- **VDDA:** el firmware llama a `HAL_PWREx_EnableVddA()`. En el STM32U5 el
  ADC y el DAC no funcionan hasta que se quita la aislación de VDDA, y el
  código original no lo hacía, así que es probable que el DAC nunca haya
  sacado tensión.
- **Baudrate:** el `.ioc` dice 115200 y el firmware lo cambia a 921600 al
  iniciar (`ST_COMM_BAUDRATE`), porque los reportes por pulso no entran en
  115200 a frecuencias altas.

## Tests en PC

`Tests/host/run_tests.sh` compila el modelo, el protocolo y el motor contra
registros simulados (`Tests/host/fake/main.h`) y simula TIM2 con pasos de 1 µs.
Verifica la temporización de cada evento, la alternancia, el bifásico, el
salto de canales deshabilitados o con amplitud 0, el fin tras N frames, el
desborde del contador de 32 bits y estos invariantes: nunca dos canales a la
vez y nunca trigger sin un puente cerrado. También verifica los registros
de pulso que el motor entrega al monitor, las ventanas de muestras (incluso
al desbordar TIM2), las estadísticas contra una referencia ordenada y el
formato de los reportes.

```
sh Tests/host/run_tests.sh            # CC=gcc por defecto
```
