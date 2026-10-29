# StimMux — aplicación de PC

Aplicación de escritorio (Python + PySide6) para operar el multiplexor de
estímulos: datos del sujeto y del experimento, configuración de canales,
medición de umbrales, control manual, protocolos por bloques, monitor de
corriente y registro completo de cada sesión.

## Instalación y uso

Requiere Python 3.10 o superior.

```bash
pip install -r requirements.txt
```
```bash
python -m stimmux
```

En Windows también se puede usar `run.bat`. En la lista de puertos aparece
**Simulador**: una placa simulada en la misma aplicación (mismo protocolo
que el firmware 0.3, con corriente sintética) para probar todo sin hardware.

La placa se conecta por el puerto virtual del ST-LINK a **921600 baud**
(firmware 0.3 o posterior).

## Flujo de trabajo

1. **Sujeto**: código seudonimizado (obligatorio), año de nacimiento, sexo,
   lateralidad, peso, talla, condición clínica y notas. Se puede guardar y
   cargar desde la biblioteca de sujetos. No se guarda nombre ni documento.
2. **Experimento y DS8R**:
   - estudio, protocolo, operador, código de ética y consentimiento;
   - los valores cargados a mano en el DS8R (rango, ancho de pulso, modo,
     límite);
   - la calibración: mA por cuenta del ADC, DACFS y recorte de bordes.
   - La aplicación no estimula si el DS8R no está en **monofásico positivo**.
3. **Canales**: músculo o nervio, lado, polaridad, electrodos (tipo, tamaño,
   ubicación), impedancia, umbrales, nivel de trabajo y **límite de seguridad**.
   - *Medir umbrales*: abre una rampa por pasos en un canal. El operador
     marca *Sensitivo*, *Motor* y *Máximo tolerado*; este último detiene la
     rampa.
   - *Límite = máximo tolerado* copia ese valor como límite del canal.
4. **Iniciar sesión** (barra superior): crea la carpeta de la sesión. Sin
   sesión abierta no se puede estimular, así todo queda registrado.
5. **Estimulación**:
   - *Temporización*: frecuencia de frame y secuencia de slots. Muestra la
     frecuencia resultante de cada canal y valida las mismas reglas que el
     firmware.
   - *Control manual*: amplitud en mA por canal, nunca por encima del límite.
   - *Protocolo por bloques* (ver abajo).
6. **Monitor**: corriente medida por pulso y por canal en el tiempo, tabla
   con media, mediana, mínimo y máximo, y captura de la forma de onda
   completa del próximo pulso.
7. **Marcadores** (barra superior, Ctrl+M): anotaciones con marca de tiempo.
8. **Cerrar sesión**: guarda y cierra los archivos.

**DETENER** (botón rojo o **Esc** desde cualquier pestaña) corta la
estimulación de inmediato.

### Protocolo por bloques

| Tipo | Parámetros |
|---|---|
| Rampa | duración, valores de inicio y fin |
| Meseta | duración, valor |
| Descanso | duración (todos los canales en 0) |
| Repetir | repite los N bloques anteriores la cantidad de veces indicada (se puede anidar) |

- Cada bloque usa **% del nivel de trabajo** de cada canal o **mA absolutos**.
  Con %, el mismo protocolo sirve para cualquier sujeto.
- Los valores se escriben como un número para todos los canales o como una
  lista por canal separada por `;` (por ejemplo `20; 35,5; 0`).
- Antes de ejecutar se verifica que ningún bloque supere el límite de
  ningún canal ni asigne corriente a un canal deshabilitado.

## Seguridad

- **Límites:** cada amplitud se limita al menor entre el límite del canal,
  el rango del DS8R y su límite de corriente, tanto en manual como en
  protocolo. La rampa de umbrales tiene su propio tope.
- **Watchdog:** mientras se estimula, la aplicación activa el watchdog del
  firmware (1 s) y lo mantiene con consultas cada 250 ms. Si la aplicación
  se cuelga o se desconecta el cable, la placa se detiene sola.
- **Confirmación:** antes de iniciar se muestra un resumen (sujeto, DS8R,
  canales y límites) y un aviso si falta el consentimiento.
- **Corte inmediato:** el botón azul de la Nucleo y la tecla Esc detienen
  todo.

## Archivos

Por defecto en `Documentos/StimMux` (se cambia en *Archivo → Carpeta de datos*):

```
sujetos/S012.json                     sujeto reutilizable
configuraciones/*.stimcfg.json        plantilla: temporización, DS8R, canales, protocolo
sesiones/2026-10-29_183303_S012_rampa-meseta/
    session.json     sujeto, experimento, DS8R, canales (con umbrales), calibración,
                     protocolo, versión de firmware (se reescribe ante cada cambio)
    pulses.csv       una fila por pulso: corrida, tiempo, canal, amplitud pedida,
                     y por fase signo, n, máx/media/mediana/mín en cuentas y en mA
    events.csv       marcadores, umbrales, inicio y fin de corridas, avisos de la placa
    commands.log     todo lo enviado y recibido (auditoría)
    waves/wave_NNNN.csv   formas de onda pedidas
    session.h5       (opcional) exportación a HDF5
```

- *Archivo → Cargar sesión anterior* recupera sujeto, experimento y
  configuración de una sesión para repetirla. Siempre graba en una carpeta
  nueva, así una sesión no se sobrescribe nunca.
- *Archivo → Exportar sesión a HDF5* copia una carpeta de sesión a un único
  archivo: `pulses` como tabla, `events`, un dataset por forma de onda y el
  `session.json` como atributo.
- En `pulses.csv`, `t_run_s` es el tiempo del pulso según la placa
  (frame × período + slot) desde el inicio de la corrida, y `t_host_s` es
  cuando llegó a la PC.

## Estructura del código

| Módulo | Función |
|---|---|
| `model.py` | Clases de datos y JSON versionado (tolera campos nuevos o faltantes). |
| `units.py` | Conversiones mA ↔ ‰ ↔ cuentas y validaciones (mismas reglas que el firmware). |
| `fwproto.py` | Comandos y parseo de respuestas, reportes `P` y formas de onda `WH/WD/WE`. |
| `transport.py`, `simulator.py` | Puerto serie real y placa simulada. |
| `device.py` | Conexión: limita los comandos en vuelo y asocia cada respuesta con su comando. |
| `runner.py` | Expansión, validación y ejecución de protocolos por bloques. |
| `storage.py` | Carpeta de sesión, carga y exportación HDF5. |
| `controller.py` | Lógica de la aplicación y reglas de seguridad. |
| `ui/` | Ventanas (PySide6 + pyqtgraph). |

## Tests

```bash
python -m unittest discover -s tests -v
```

Cubren el modelo, las unidades, el parseo, los protocolos, el simulador, el
almacenamiento con HDF5 y una sesión completa en la ventana real con el
simulador (umbrales, manual, protocolo y forma de onda).

## Pendiente

- Control en tiempo real desde otro dispositivo (LSL o UDP/TCP a través de
  la app), cuando se defina.
- Calibración asistida de `mA por cuenta` y `DACFS` con una carga resistiva
  conocida.
